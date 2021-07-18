#include "GraphComplete.h"
#include "RuleSet.h"
#include <queue>

//Uses breadth first search (non-recursively) to determine if there is a path between two nodes in a graph
bool GraphComplete::pathBFS() {
	std::queue<Vertex*> queue;
	this->initNodeList();
	bool found = false;
	int* final_nodes = new int[this->finish.size()];
	std::list<Vertex*>::const_iterator it = this->finish.begin();
	for (unsigned int i = 0; i < this->finish.size(); i++) {
		final_nodes[i] = this->graph->getVertexPos((*it));
		it++;
	}
	//Start and end come from the factorgraph, so we can just use them
	queue.push(this->start);
	//Of course there isn't really a position, but we have a vector, so we use that
	while (!queue.empty() && !found) {
		Vertex *cur = queue.front();
		int pos = this->graph->getVertexPos(cur);
		if (!nodes[pos]) { //if we haven't seen it, we need to process it
			nodes[pos] = true;
			for (unsigned int i = 0; i < this->finish.size(); i++) {
				if (pos == final_nodes[i]) {
					found = true;
				}
			}
			if(!found){
				//This will add the same edges multiple times, which is okay because we have our check
				if (this->backwards <= 0) {
					std::vector<Vertex*> edges;
					edges = cur->getInEdges();
					for (std::vector<Vertex*>::const_iterator edge = edges.begin(); edge != edges.end() && !found; edge++) {
						queue.push((*edge));
					}
				}
				if(this->backwards >= 0){
					std::multimap<int, Vertex*> edges;
					edges = cur->getEdges();
					for (std::multimap<int, Vertex*>::const_iterator edge = edges.begin(); edge != edges.end() && !found; edge++) {
						queue.push((*edge).second);
					}
				}
			}
		}
		queue.pop();
	}
	//Finally, we need to clean up our mess
	delete[] nodes;
	delete[] final_nodes;
	while (!queue.empty()) {
		queue.pop();
	}
	return found; //What we are left with is knowing if a path exists
}

//Performs a probab
bool GraphComplete::pDFSGenerate(int pos,int* final_nodes) {
	bool found = false;
	for (unsigned int i = 0; i < this->finish.size(); i++) {
		if (pos == final_nodes[i]) {
			found = true;
		}
	}
	if (found) {
		this->nodes[pos] = true; //Mark that we have seen this one before we leave seeing this one
		return true;
	}
	if (this->nodes[pos]) { //already been here, so it's no use
		return false;
	}
	this->nodes[pos] = true; //Mark that we have seen this one
	if (this->backwards <= 0) {
		std::vector<Vertex*> edges;
		edges = graph->getContent(pos)->getVertex()->getInEdges();
		for (std::vector<Vertex*>::const_iterator edge = edges.begin(); edge != edges.end() && !found; edge++) {
			if (this->selected(graph->getContent((*edge))->calculateExternal())) { //Here is the probabalistic part. Right Now we will just do the nodes

				found = found || this->pDFSGenerate(graph->getVertexPos((*edge)), final_nodes);
			}
		}
	}
	if(this->backwards >= 0){
		std::multimap<int, Vertex*> edges;
		edges = graph->getContent(pos)->getVertex()->getEdges();
		for (std::multimap<int, Vertex*>::const_iterator edge = edges.begin(); edge != edges.end() && !found; edge++) {
			if (this->selected(graph->getContent((*edge).second)->calculateExternal())) { //Here is the probabalistic part. Right Now we will just do the nodes

				found = found || this->pDFSGenerate(graph->getVertexPos((*edge).second), final_nodes);
			}
		}
	}
	return found;
}

bool GraphComplete::selected(float probability) {
	return probability > (float)rand() / (float)RAND_MAX;
}

void GraphComplete::initNodeList() {
	this->nodes = new bool[this->graph->getNumVertices()];
	for (int i = 0; i < this->graph->getNumVertices(); i++) {
		this->nodes[i] = false; //We haven't seen any of them yet
	}
}
//This is our function that creates a probabalistic sub-graph based on our motif
//Vertex begin and end are most likely not from our factorgraph, but from the rule-set we 
//converted into a factor-graph
RuleSet* GraphComplete::completeGraph(Vertex* begin, Vertex* end) {
	//First, we set the beginning and ending vertices
	if (this->graph == NULL) return NULL;
	this->start  = this->graph->getVertex(begin->getName());
	if (!this->finish.empty()) {
		this->finish.clear();
	}
	this->finish.push_back(this->graph->getVertex(end->getName()));
	if (!this->pathBFS()) {
		this->start = NULL; //Our cleanup
		this->finish.clear();
		return NULL;
	}
	this->initNodeList();
	int* final_nodes = new int[this->finish.size()];
	std::list<Vertex*>::const_iterator it = this->finish.begin();
	for (unsigned int i = 0; i < this->finish.size(); i++) {
		final_nodes[i] = this->graph->getVertexPos((*it));
		it++;
	}
	this->pDFSGenerate(this->graph->getVertexPos(this->start), final_nodes);
	delete final_nodes;
	//Now, we collect all the rules
	RuleSet* out = new RuleSet(*(this->set));
	for (int i = 0; i < this->graph->getNumVertices(); i++) {
		if (!this->nodes[i]) { //Add the vertices
			out->updateFrequency(i, 0.0);
		}
	}
	this->ChooseXORProbabilities(out);
	out->cleanRuleSetProb(true);
	//clean up
	delete[] nodes;
	this->start = NULL;
	this->finish.clear();

	return out;
}

//This is our function that creates a probabalistic sub - graph based on our motif
//Vertex begin and end are most likely not from our factorgraph, but from the rule-set we 
//converted into a factor-graph. This is an overloaded function that takes
//in a list of vertices, where any of them can be the end
RuleSet * GraphComplete::completeGraph(Vertex * begin, const std::list<Vertex*>& end) {
	//First, we set the beginning and ending vertices
	if (this->graph == NULL) return NULL;
	this->start = this->graph->getVertex(begin->getName());
	if (!this->finish.empty()) {
		this->finish.clear();
	}
	for (std::list<Vertex*>::const_iterator it = end.begin(); it != end.end(); it++) {
		//Because we have two different factor graphs, we have to do the conversion
		this->finish.push_back(this->graph->getVertex((*it)->getName()));
	}
	if (!this->pathBFS()) {
		this->start = NULL; //Our cleanup
		this->finish.clear();
		return NULL;
	}
	this->initNodeList();
	int* final_nodes = new int[this->finish.size()];
	std::list<Vertex*>::const_iterator it = this->finish.begin();
	for (unsigned int i = 0; i < this->finish.size(); i++) {
		final_nodes[i] = this->graph->getVertexPos((*it));
		it++;
	}
	this->pDFSGenerate(this->graph->getVertexPos(this->start), final_nodes);
	delete final_nodes;
	//Now, we collect all the rules
	RuleSet* out = new RuleSet(*(this->set));
	for (int i = 0; i < this->graph->getNumVertices(); i++) {
		if (!this->nodes[i]) { //Add the vertices
			out->updateFrequency(i, 0.0);
		}
	}
	this->ChooseXORProbabilities(out);
	out->cleanRuleSetProb(true);
	//clean up
	delete[] nodes;
	this->start = NULL;
	this->finish.clear();

	return out;
}

RuleSet* GraphComplete::completeGraph(const std::list<Vertex*>& begin, const std::list<Vertex*>& end) {
	RuleSet *result = new RuleSet();
	for (std::list<Vertex*>::const_iterator it = begin.begin(); it != begin.end(); it++) {
		RuleSet *set = this->completeGraph((*it), end);
		if (set == NULL) { //A failure for one is a failure for all in a narrative world
			delete result;
			return NULL;
		}
		RuleSet new_set = (*result) + set;
		delete result;
		delete set;
		result = new RuleSet(new_set);
	}
	return result;
}
RuleSet* GraphComplete::completeGraph(const std::list<Vertex*>& begin, Vertex* end) {
	std::list<Vertex*> end_list;
	end_list.push_back(end);
	return this->completeGraph(begin, end_list);
}

RuleSet* GraphComplete::convertRuleSetNoExpand(RuleSet* set,bool cleanup) {
	RuleSet new_set = *set;
	if (cleanup)
		this->ChooseXORProbabilities(&new_set);
	new_set.cleanRuleSetProb();
	//Now, we go through and ground our objects
	for (int i = 0; i < new_set.getNumVertices(); i++) {
		if (new_set.getContent(i)->getNumLeafs() > 0)
			new_set.getContent(i)->select();
	}
	return new RuleSet(new_set);
}

void GraphComplete::ChooseXORProbabilities(RuleSet* set) {

	//First, we want for each vertex to determine only one support
	//which we do for each type of support!
	FactorGraph* fg = new FactorGraph(set, true);//Reverse it because we want targets
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> range = fg->getFactors();
	std::list<Factor*> orientations;//Each orientation is not independent
	std::list<Factor*> support; //Each support is independent
	for (std::list<Factor*>::const_iterator it = range.first; it != range.second; it++) {
		if ((*it)->getExistance()) {
			support.push_back((*it));
		}
		else if ((*it)->getName().find("orientation") != std::string::npos) {
			orientations.push_back((*it));
		}
	}

	std::map<int, Vertex*> vmap;//If I coded fg right, the list is equivalent to the ruleset list
	std::pair<std::vector<Content*>::const_iterator, std::vector<Content*>::const_iterator> vmap_range = fg->getVertices();
	std::vector<Content*>::const_iterator vmap_range_it = vmap_range.first;
	for (int i = 0; i < set->getNumVertices() && vmap_range_it != vmap_range.second; i++) {
		vmap[i] = (*vmap_range_it)->getVertex();
		vmap_range_it++;
	}

	//Begin removing supports 
	for (int i = 0; i < set->getNumVertices(); i++) {
		Vertex *v = vmap[i];
		//Here, we also determine if the content was a necessary item that is no longer one (and therefore we need INC)
		bool inc = false;
		if (set->getContent(i)->getNumLeafs() > 0 && set->getContent(i)->getLeaf(0)->getName() == "Story") {
			inc = true; //Guess this vertex was INC
		}
		std::pair<std::multimap<Vertex*, int>::const_iterator, std::multimap<Vertex*, int>::const_iterator> rule_range = fg->getRuleNumbers(v);
		std::pair<std::multimap<Vertex*, Factor*>::const_iterator, std::multimap<Vertex*, Factor*>::const_iterator> range = fg->getFactorEdges(v);
		for (std::list<Factor*>::iterator it = support.begin(); it != support.end(); it++) {
			int counter = 0;
			for (std::map<Vertex*, Factor*>::const_iterator fit = range.first; fit != range.second; fit++) {
				if ((*fit).second == (*it)) {
					counter++;
				}
			}
			if (counter > 1) {
				int keeper = rand() % counter;
				counter = 0;
				//Now we go through, and change the rule values to zero if it isn't a keeper
				std::multimap<Vertex*, int>::const_iterator pit = rule_range.first;
				for (std::multimap<Vertex*, Factor*>::const_iterator fit = range.first; fit != range.second && pit != rule_range.second; fit++, pit++) {
					if ((*fit).second == (*it)) {
						if (keeper != counter) {
							set->setProbability((*pit).second, 0.0);
						}
						else {
							//For keeper, there is a very special addition step that we have to perform
							//Basically, if we have a lost must-have (i.e, the frequency of the head node is zero)
							//then we must make sure that the good node is the must-have.
							//For must haves that stay, we get this for free in the generator
							if (inc) {//Should make these epsilon values
									  //get the connected object
								std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> head_range = set->getPredicate((*pit).second);
								set->addFrequency((*head_range.first).second, 1.0f);
							}
						}
						counter++;
					}
				}
			}
			else {
				//If the rule is a keeper, than we have to set the frequency 
				std::multimap<Vertex*, int>::const_iterator pit = rule_range.first;
				for (std::multimap<Vertex*, Factor*>::const_iterator fit = range.first; fit != range.second && pit != rule_range.second; fit++, pit++) {
					if ((*fit).second == (*it)) {
						//For keeper, there is a very special addition step that we have to perform
						//Basically, if we have a lost must-have (i.e, the frequency of the head node is zero)
						//then we must make sure that the good node is the must-have.
						//For must haves that stay, we get this for free in the generator
						//std::cout << "item is " << set->getVertex(i)->getName() << " with a frequency of " << set->getFrequency(i) << std::endl;
						if (inc) {//Should make these epsilon values
								  //get the connected object
							std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> head_range = set->getPredicate((*pit).second);
							//std::cout << "Setting the new must have " << set->getVertex((*head_range.first).second)->getName() << std::endl;
							set->addFrequency((*head_range.first).second, 1.0f);
						}
					}

				}
			}
		}
	}
	//End removing supports
	delete fg;
}