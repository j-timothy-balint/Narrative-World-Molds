#include "FisherTwelveSceneSynth.h"
#include <cstdlib>

//Determines if the content c should be added to the smaller factor-graph
bool FisherSceneSynth::add(Content *c, FactorGraph* fg) {
	float add_val = c->calculateExternal(); //First, we get the probability of using this. For root objects, it means we are assuming that the roots are independent
	//Next, for each parent, we multiply the probability of the object being there
	for (int i = 0; i < c->getVertex()->getNumEdges(false); i++) {
		Vertex *parent = c->getVertex()->getEdge(i, false);
		std::pair<std::multimap<Vertex*, float>::const_iterator, std::multimap<Vertex*, float>::const_iterator> probs = fg->getFactorProb(parent);
		std::multimap<Vertex*, float>::const_iterator it = probs.first;
		//Next, we get the key of the c vertex, so that we can know which one we should advance to
		advance(it, parent->getEdgePos(c->getVertex(), true));
		int pos = fg->getVertexPos(parent);
		if (this->added[pos]) {
			//Gets the vertex probabilities for the given key
			add_val *= fg->getContent(parent)->calculateExternal() * (*it).second;//B(S) * T(S)

		}
		else {
			add_val *= (1.0f - fg->getContent(parent)->calculateExternal()) * (1.0f - (*it).second);//B(S) * T(S) when it isn't there
		}
	}
	//finally, determine if we keep it
	float sample = (float)rand() / (float)RAND_MAX;

	return add_val > sample;
}
//Creates a factor graph based on foward sampling. An important point that is implicit in 
//Fisher 12 is that 
RuleSet* FisherSceneSynth::fowardSample() {
	RuleSet *result = new RuleSet(true);
	FactorGraph* network = new FactorGraph(this->ruleset);
	std::queue<Content*> vert;
	std::map<int, int> vert_to_vert;
	std::pair<std::vector<Content*>::const_iterator, std::vector<Content*>::const_iterator> vertices = network->getVertices();
	//We clean everything out for this run as well
	this->added.clear();
	this->added.resize(std::distance(vertices.first, vertices.second), false);
	this->seen.clear();
	this->seen.resize(std::distance(vertices.first, vertices.second), false);
	//First, we get the root node (which let's hope there is one)
	for (std::vector<Content*>::const_iterator it = vertices.first; it != vertices.second; it++) {
		if ((*it)->getVertex()->getNumEdges(false) == 0) {
			vert.push((*it));
		}
	}
	while(vert.size() > 0){
		Content * c = vert.front();
		vert.pop();
		int old_pos = network->getVertexPos(c->getVertex());
		this->seen[old_pos] = true;
		if (this->add(c, network)) {
			int new_pos = result->addContent(c);
			vert_to_vert[old_pos] = new_pos;
			this->added[old_pos] = true;
			//We also add in all the vertex content that this is connected to
			for (int i = 0; i < c->getVertex()->getNumEdges(); i++) {
				Vertex* child = c->getVertex();
				if (!this->seen[network->getVertexPos(child->getEdge(0,true,i))]) { //This means we only try each vertex once to see if it's in, which is a better foward position
					//We actually only want to add this if we've seen all the parents (because then we know if they are used or not)
					bool good = true;
					for (int j = 0; j < child->getNumEdges(false) && good; j++) {
						if (!this->seen[network->getVertexPos(child->getEdge(j, false))]) {
							good = false;
						}
					}
					if (good) {
						vert.push(network->getContent(c->getVertex()->getEdge(0, true, i)));
					}
				}
			}
		}
	}
	delete network;
	//Since this is a content based one, we add in all possible connections from the seen content

	for (int i = 0; i < this->ruleset->getNumRules(); i++) {
		std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = this->ruleset->getPredicate(i);
		bool add = true;
		for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
			if (!this->added[(*it).second]) {
				add = false;
			}
		}
		if (add) {
			Factor *f = this->ruleset->getFactor(i);
			float prob = this->ruleset->getRuleProbability(i);
			std::list<int> content_pos;
			for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
				content_pos.push_back(vert_to_vert[(*it).second]);
			}
			result->addRule(f, content_pos, prob);
		}
	}
	return result;
}