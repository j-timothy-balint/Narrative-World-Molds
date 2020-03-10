#include "FactorGraph.h"
#include "RuleSet.h"
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <set>

std::map<Vertex*, Vertex*> FactorGraph::intersectionVertices(const FactorGraph& rhs) const {
	std::map<Vertex*, Vertex*> result;
	std::pair<std::vector<Content*>::const_iterator, std::vector<Content*>::const_iterator> l_range = this->getVertices();
	for (std::vector<Content*>::const_iterator l_v = l_range.first; l_v != l_range.second; l_v++) {
		//For now, if we are the same vertex or a child, we are good
		//Basically, it comes down to what we consider equality in the vertex
		Vertex *r_v = rhs.getVertex((*l_v)->getVertex()->getName());
		if (r_v != NULL) {
			result[(*l_v)->getVertex()] = r_v;
		}
	}
	return result;
}
std::map<Factor*, Factor*> FactorGraph::intersectionFactors(const FactorGraph& rhs) const {
	std::map<Factor*, Factor*> result;
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> l_factors = this->getFactors();
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> r_factors = rhs.getFactors();
	//First,we find the similar factors
	std::map<Factor*, std::pair<Factor*, Factor*> > factor_map;
	for (std::list<Factor*>::const_iterator f_l = l_factors.first; f_l != l_factors.second; f_l++) {
		Factor *r_fact = NULL;
		for (std::list<Factor*>::const_iterator rit = r_factors.first; rit != r_factors.second && r_fact == NULL; rit++) {
				if ((*(*rit)) == *(*f_l)) {//Go by what the equality operator says
					r_fact = (*rit);
				}
		}
		if (r_fact != NULL) {
			result[(*f_l)] = r_fact;
		}
	}
	return result;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Public method definitions
/*Factor Graph Section*/
FactorGraph::FactorGraph() : num_edges(0){}

/*Parses the known output of factors */
FactorGraph::FactorGraph(const std::list<std::string>& csv_lines) : num_edges(0) {
	std::vector<Vertex*> verts;
	for (std::list<std::string>::const_iterator it = csv_lines.begin(); it != csv_lines.end(); it++) {
		
		unsigned int num_elements = (unsigned int)std::count((*it).begin(), (*it).end(), ',') + 1; //We add one for the last element
		size_t start = 0;
		Factor *fact = NULL;
		int support = 0;
		verts.clear();
		for (unsigned int i = 0; i < num_elements; i++) {
			size_t end = (*it).find(",", start);
			if (end != std::string::npos) {
				std::string sub = (*it).substr(start, end - start);
				//First is always our factor, last is always our count
				if (i == 0) {
					//We see if it is a known factor. If so, we use that Factor. If not, we create a new Factor
					bool found = false;
					for (std::list<Factor*>::const_iterator it2 = this->factors.begin(); it2 != this->factors.end() && !found; it2++) {
						if ((*it2)->getName() == sub) {
							fact = (*it2);
							found = true;
						}
					}
					if (!found) {
						fact = new Factor(num_elements - 2,sub);
						factors.push_back(fact);
					}
				}
				else {
					//These are our vertices.
					//See if it exists
					Vertex *v = this->getVertex(sub);
					if (v == NULL) {
						Content *cont = new Content(sub, true);
						vertices.push_back(cont);
						v = cont->getVertex();
					}
					verts.push_back(v);
				}
				start = end + 1;//We skip the comma
			}
			else {
				//Here, we are at the last element, which is support
				support = atoi((*it).substr(start).c_str());
			}
			
		}
		num_edges++;
		//Finally, we set everything up
		//First, we have to figure out if this is already in the graph. If so, we figure out the max instead
		bool found = false;
		std::pair<std::multimap<Vertex*, Factor*>::iterator, std::multimap<Vertex*, Factor*>::iterator > range; 
		range = this->graph.equal_range(verts.front());
		//And also for the range
		std::pair<std::multimap<Vertex*, float>::iterator, std::multimap<Vertex*, float>::iterator > prange;
		prange = this->prob.equal_range(verts.front());
		std::multimap<Vertex*, Factor*>::iterator it2 = range.first;
		std::multimap<Vertex*, float  >::iterator itp = prange.first;
		if(range.first != range.second){
			//We may have multiple factors, and so when we find one, we have to determine if the edge is the same
			for (unsigned int count_mapper = 0; count_mapper < (unsigned int)this->graph.count(verts.front()) && !found; count_mapper++) {
				if ((*it2).second == fact) {
					//Here, we loop through the vertices connected to the factor
					for(int f = 0; f < fact->getNumComponents()-1; f++)//Subtract one because we have the head
						if (verts.front()->getEdge(f,true,count_mapper) == verts[1]) {
							found = true;
						}
				}
				it2++;
				itp++;
			}

		}
		if (!found) {
			this->graph.insert(std::pair<Vertex*, Factor*>(verts.front(), fact));
			//We define the first edge as the outbound edge. Everything else is an inbound edge for now
			std::vector<Vertex*>::iterator it2 = verts.begin();
			it2++; //Here, we start at the not-front edge
			for (; it2 != verts.end(); it2++) {
				verts.front()->addEdge((*it2));
			}
			this->prob.insert(std::pair<Vertex*, float>(verts.front(), (float)support));
		}
		else {
			itp--; //We overstepped by one if we found a match
			if ((*itp).second < (float)support) {
				(*itp).second = (float)support;
			}
		}
	}
}
//Converts a ruleset into a factor graph representation
//Reverse reverses our in and out nodes
FactorGraph::FactorGraph(RuleSet* set,bool reverse){
	//First, we build the factors
	std::map<Factor*, Factor*> fmap;
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> fs = set->getAllFactors();
	for (std::list<Factor*>::const_iterator f = fs.first; f != fs.second; f++){
		//Copy the Factors
		Factor *fact = new Factor((*f)->getNumComponents(), (*f)->getName(),(*f)->getExistance());
		this->factors.push_back(fact);
		fmap[(*f)] = fact;
	}
	//Next, we build the vertices
	std::map<int, Vertex*> vmap; //rhs to lhs
	for (int i = 0; i < set->getNumVertices(); i++) {
		Content *vert = new Content((*set->getContent(i)),true);//Deep copy everything
		this->vertices.push_back(vert);
		vmap[i] = vert->getVertex();
	}
	this->num_edges = (int)vmap.size();
	//Now, for each rule, we create a connection between the vertices and rules
	for (int i = 0; i < set->getNumRules(); i++){
		Factor *f = fmap[set->getFactor(i)]; //as the factor in factor graph
		float prob = set->getRuleProbability(i);
		std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> vit = set->getPredicate(i);
		
		if (!reverse) {
			Vertex *head = vmap[vit.first->second];//as the vertex in factor graph
			int j = (int)this->prob.count(head);
			this->graph.insert(std::make_pair(head, f));
			this->prob.insert(std::make_pair(head, prob));
			this->rule_numbers.insert(std::make_pair(head, i));
			//skip one
			std::multimap<int, int>::const_iterator it = vit.first;
			advance(it, 1);
			for (it; it != vit.second; it++) {
				head->addEdge(vmap[(*it).second], true, j);
			}
		}
		else {
			//Here we break convention for now and assume head is the first and tail is the second (no three factor anything)
			Vertex *tail = vmap[vit.first->second];
			//int j = (int)this->prob.count(tail);//J is no longer the probability tail, but is instead the number of keys that we have
			vit.first++;
			Vertex *head = vmap[vit.first->second];
			int j = (int)head->getNumEdges();//This may be true for the above as well, as j is the graph'ed counter

			this->graph.insert(std::make_pair(head, f));
			this->prob.insert(std::make_pair(head, prob));
			this->rule_numbers.insert(std::make_pair(head, i));
			head->addEdge(tail, true, j);
		}
	}
}

FactorGraph::~FactorGraph(){
	for (auto&& child : this->vertices) { //efficient way to delete the objects in the pointer. Kind of gross (auto pointer) but okay
		delete child;
	}
	for (auto&& child : this->factors) { //efficient way to delete the objects in the pointer. Kind of gross (auto pointer) but okay
		delete child;
	}
	//clear out the actual pointers
	this->vertices.clear();
	this->factors.clear();
	this->graph.clear();
	this->prob.clear(); 
}
//Normalizes our graph by a constant value
void FactorGraph::normByValue(float val) {
	for(std::multimap<Vertex*, float>::iterator it = this->prob.begin(); it != this->prob.end(); it++) {
		(*it).second /= val;
	}
}
//Getters for the components of the factor graph. From the name, the factor edges from the vertex are possible
Vertex* FactorGraph::getVertex(const std::string& name) const{
	for (std::vector<Content*>::const_iterator it = this->vertices.begin(); it != vertices.end(); it++) {
		if ((*it)->getVertex()->getName() == name) {
			return (*it)->getVertex();
		}
	}
	return NULL;
}

Vertex* FactorGraph::getFactorVertices(Vertex* vert, Factor* fact,int offset) const{
	std::pair<std::multimap<Vertex*, Factor*>::const_iterator, std::multimap<Vertex*, Factor*>::const_iterator>  range = this->getFactorEdges(vert);
	int i = 0;
	for (std::multimap<Vertex*, Factor*>::const_iterator it = range.first; it != range.second; it++) {
		if ((*it).second == fact) {
			return vert->getEdge(offset,true,i);
		}
		i++;
	}
	return NULL;
}

float FactorGraph::getProbability(Vertex* vert, Factor* fact) {
	std::pair<std::multimap<Vertex*, Factor*>::const_iterator, std::multimap<Vertex*, Factor*>::const_iterator>  range = this->getFactorEdges(vert);
	std::pair<std::multimap<Vertex*, float>::iterator, std::multimap<Vertex*, float>::iterator>  prange = this->getFactorProb(vert);
	int i = 0;
	std::multimap<Vertex*, float>::iterator prob = prange.first;
	for (std::multimap<Vertex*, Factor*>::const_iterator it = range.first; it != range.second; it++) {
		if ((*it).second == fact) {
			return (*prob).second;
		}
		i++;
		prob++;
	}
	return 0.0;
}
std::pair<std::multimap<Vertex*, int>::const_iterator, std::multimap<Vertex*, int>::const_iterator> 
FactorGraph::getRuleNumbers(Vertex* vert) const {
	return this->rule_numbers.equal_range(vert);
}

std::pair<std::multimap<Vertex*, Factor*>::const_iterator, std::multimap<Vertex*, Factor*>::const_iterator> 
FactorGraph::getFactorEdges(Vertex* vert) const{
	return this->graph.equal_range(vert);
}
std::pair<std::multimap<Vertex*, float>::iterator, std::multimap<Vertex*, float>::iterator> 
FactorGraph::getFactorProb(Vertex* vert) {
	return this->prob.equal_range(vert);
}

int FactorGraph::getVertexPos(Vertex* vert){
	if (vert == NULL)
		return -1;
	for (int i = 0; i < this->vertices.size(); i++) {
		if (this->vertices[i]->getVertex() == vert) {
			return i;
		}
	}
	return -1;
}
Content* FactorGraph::getContent(Vertex* vert) {
	if (vert == NULL)
		return NULL;

	for (int i = 0; i < this->vertices.size(); i++) {
		if (this->vertices[i]->getVertex() == vert) {
			return this->vertices[i];
		}
	}
	return NULL;
}

//This returns the largest probabilty for a given factor-graph factor
float FactorGraph::getMaxProb(Vertex* vert) {
	if (vert == NULL) {
		return 0.0; //No prob for no factor
	}
	//Find the highest prob for the outbound connections
	float max = 0.0;
	std::pair<std::multimap<Vertex*, float>::iterator, std::multimap<Vertex*, float>::iterator> range;
	range = this->getFactorProb(vert);
	for (std::multimap<Vertex*, float>::iterator it = range.first; it != range.second; it++) {
		if ((*it).second > max) {
			max = (*it).second;
		}
	}
	//Now, we do the inbound edges, which are a bit more in-depth for that probability
	Vertex* vex;
	std::map<Vertex*, int> found; //we want to know how many we want to skip
	for (int i = 0; i < vert->getNumEdges(false); i++) { 
		//Get the position of the in-bound edge
		vex = vert->getEdge(i, false); //Should never be NULL unless we have bigger problems (inbound so no key)
		std::map<Vertex*, int>::iterator it = found.find(vex);
		if (it == found.end()) {
			found[vex] = 0;
		}
		int vert_pos = vex->getEdgePos(vert,true,found[vex]); //This should tell us the position in vex
		found[vex]++;
		if (vert_pos != -1) { //Means somewhere we failed
			//Get the range of values
			range = this->getFactorProb(vex); //c++ 11 says this is connected in order
			int dist = (int)std::distance(range.first, range.second);
			advance(range.first, vert_pos); //Should also not need to worry about it going over-board
			if ((*range.first).second > max) {
				max = (*range.first).second;
			}
		}
	}
	return max;
}
//Gets the highest possible probability for the total factor graph
float FactorGraph::getTotalMaxProb() {
	float max = 0.0;
	for (std::vector<Content*>::iterator it = this->vertices.begin(); it != this->vertices.end(); it++) {
		float test = this->getMaxProb((*it)->getVertex());
		if (test > max) {
			max = test;
		}
	}
	return max;
}
//Writes out the FactorGraph as a Json string that can be put into a Force Layout
std::string FactorGraph::graphToJson() {
	std::stringstream output;
	std::map<Vertex*, int> it_counter_map;
	output << "{\n\"nodes\":[";
	int i = 0;
	for (std::vector<Content*>::iterator it = this->vertices.begin(); it != this->vertices.end(); it++) {
		Vertex *v = (*it)->getVertex();
		it_counter_map[v] = i;
		output << "{\"id\":" << "\"" << i << "\"," << "\"name\":" << "\"" << v->getName() << "\"";
		output << ",\"frequency\":" << (*it)->getProbability(0) << "}";
		if (i < (int)this->vertices.size() - 1) {
			output << ",\n";
		}
		else {
			output << "\n";
		}
		i++;
	}
	output << "],\n\"links\":[\n";
	i = 0;
	std::map<Vertex*, int> found;
	std::multimap<Vertex*, float>::iterator pit = this->prob.begin();
	for (std::multimap<Vertex*, Factor*>::iterator it = this->graph.begin(); it != this->graph.end(); it++) {
		if (found.find((*it).first) == found.end()) {
			found[(*it).first] = 0;
		}
		for(int j = 0; j < (*it).second->getNumComponents() -1; j++){
			output << "{\"source\":\"" << it_counter_map[(*it).first] << "\",\"target\":\"";
			Vertex *connect = (*it).first->getEdge(j, true, found[(*it).first]);
			if (connect != NULL) {
				output << it_counter_map[connect] << "\",\"value\":" << (*pit).second;
			}
			else { //debug
				connect = (*it).first->getEdge(j, true, found[(*it).first]);
			}
			output <<",\"factor\":\"" <<(*it).second->getName().c_str()<< "\"}";

			if (i != this->graph.size() - 1 && j != (*it).second->getNumComponents()-1) // too much to double copy
				output << ",\n";
			else
				output << "\n";
		}
		found[(*it).first] ++;
		pit++;
		i++;
	}
	output << "]}\n";
	return output.str();
}

float FactorGraph::graphKernel(const FactorGraph& rhs) const {
	//First, we get the matching 
	std::map<Vertex*, Vertex*> vertices = this->intersectionVertices(rhs); //We break our set down into a smaller matching subset
	std::map<Vertex*, Vertex*>  rev_verts;
	std::map<Factor*, Factor*>  factors = this->intersectionFactors(rhs);
	if (vertices.size() == 0 || factors.size() == 0)
		return 0.0; //The score is zero when we have no match
	float result = 0.0;

	//Now, we assign an ordering to each and set up our normalization
	std::set<Vertex*> l_set; //This will basically be removing duplicates, which we can have on the rhs
	std::set<Vertex*> r_set;
	std::map<std::string, float> l_vert_count;
	std::map<std::string, float> r_vert_count;
	for (std::map<Vertex*, Vertex*>::const_iterator it = vertices.begin(); it != vertices.end(); it++) {
		rev_verts[(*it).second] = (*it).first;
		l_vert_count[(*it).first->getName()] = 0.0f;
		r_vert_count[(*it).second->getName()] = 0.0f;
		l_set.insert((*it).first);
		r_set.insert((*it).second);
	}
	//Now, we want an index mapping
	int counter = 0;
	std::map<Vertex*, int> l_num;
	for (std::set<Vertex*>::const_iterator it = l_set.begin(); it != l_set.end(); it++) {
		l_num[(*it)] = counter;
		counter++;
	}
	counter = 0;
	std::map<Vertex*, int> r_num;
	for (std::set<Vertex*>::const_iterator it = r_set.begin(); it != r_set.end(); it++) {
		r_num[(*it)] = counter;
		counter++;
	}
	//We get the normalized cost for each node set
	std::pair<std::vector<Content*>::const_iterator, std::vector<Content*>::const_iterator> verts = this->getVertices();
	for (std::vector<Content*>::const_iterator it = verts.first; it != verts.second; it++) {
		if (l_vert_count.find((*it)->getVertex()->getName()) != l_vert_count.end()) {
			l_vert_count[(*it)->getVertex()->getName()]+= 1.0f;
		}
	}
	for (std::map<std::string, float>::iterator it = l_vert_count.begin(); it != l_vert_count.end(); it++) {
		l_vert_count[(*it).first] = 1.0f / (*it).second;
	}
	verts = rhs.getVertices();
	for (std::vector<Content*>::const_iterator it = verts.first; it != verts.second; it++) {
		if (r_vert_count.find((*it)->getVertex()->getName()) != r_vert_count.end()) {
			r_vert_count[(*it)->getVertex()->getName()] += 1.0f;
		}
	}
	for (std::map<std::string, float>::iterator it = r_vert_count.begin(); it != r_vert_count.end(); it++) {
		r_vert_count[(*it).first] = 1.0f / (*it).second;
	}
	std::multimap<int, int> edges; //Edges determines if there is an edge from graph a matched vertex to graph b matched vertex
	for (std::map<Vertex*, Vertex*>::const_iterator v_it = vertices.begin(); v_it != vertices.end(); v_it++) {
		//Zeroth
		result += l_vert_count[(*v_it).first->getName()] * r_vert_count[(*v_it).second->getName()];
		//summing over all node pairs across the two graphs. To do so, we will go by the outnodes.
		for (int i = 0; i < (*v_it).first->getNumEdges(); i++) {
			Vertex* v = (*v_it).first->getEdge(0,true,i);
			int edge_pos = (*v_it).second->getEdgePos(v);
			if ( edge_pos != -1) {
				//Make sure that our factor matches up
				 //Then we have the link
				result += l_vert_count[v->getName()] * r_vert_count[(*v_it).second->getEdge(0, true, edge_pos)->getName()];
				//Add the edge to our list of edges
				edges.insert(std::make_pair(l_num[(*v_it).first], r_num[(*v_it).second->getEdge(0, true, edge_pos)]));
			}
		}
	}
	return result;
}
//Finally, other functions that are important to parse the factor-graph
std::map<std::string, FactorGraph*> parseCSVFactorGraph(const std::string & file_name) {
	std::ifstream myfile;
	std::string line;
	std::string room_name;
	std::list<std::string> lines;
	std::map<std::string, FactorGraph*> graphs;
	float total_rooms;
	myfile.open(file_name);
	if (myfile.is_open()) {
		while (getline(myfile, line)) {
			//Here, we get the name, and then keep adding lines to our thing until we hit another one
			int num_elements = (int)std::count(line.begin(), line.end(), ',');
			if (num_elements < 3) { //In this case, we have a room
				if (!lines.empty()) { //We had a room we need to construct
					graphs[room_name] = new FactorGraph(lines);
					graphs[room_name]->normByValue(total_rooms);
					//graphs[room_name]->normByValue(graphs[room_name]->getTotalMaxProb());
					lines.clear();
				}
				room_name   = line.substr(0, line.find(','));
				total_rooms = (float)atof(line.substr(line.find(',') + 1).c_str());
			}
			else {
				lines.push_back(line);
			}
		}
		myfile.close();
		graphs[room_name] = new FactorGraph(lines);
		graphs[room_name]->normByValue(total_rooms);
	}
	return graphs;
}