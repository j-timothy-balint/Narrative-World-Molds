#include "NarrativeWorldMoldGenerator.h"
#include <set>
#include <fstream>
#include <iostream>
#include <cmath>
#define EPSILON 0.999999f
//Strips the quotes from a word
inline std::string stripQuotes(const std::string& word) {
	size_t start = word.find("'") + 1;
	size_t end = word.find("'", start);
	return word.substr(start, end - start);
}
//Determines if an item is an instanced copy of another by finding the last _ 
std::string extractParent(const std::string& name) {
	std::size_t finder = name.find_last_of("_");
	return name.substr(0, finder);
}

NarrativeWorldMoldGenerator::~NarrativeWorldMoldGenerator() {
	//We assume that vertices are cleaned in database a seperate way
	for(int i =0; i < this->motifs.size(); i++){
		delete this->motifs[i];
	}
	this->motifs.clear();
	this->location_motifs.clear();
	this->vertices.clear();
}
//If we need a similar object, we look to the siblings
Vertex* NarrativeWorldMoldGenerator::searchSiblings(Vertex* vert, int count) {
	if (vert == NULL)
		return NULL;
	Vertex* parent = vert->getEdge(0, false);
	if (parent == NULL)
		return NULL;
	Vertex *item = NULL;
	int num_found = 0;
	std::string name = vert->getName();
	if (isdigit(name.substr(name.find_last_of("_") + 1, 1).c_str()[0])) {
		name = name.substr(0,name.find_last_of("_") -1);
	}
	for (int i = 0; i < parent->getNumEdges() && item == NULL; i++) {
		if (parent->getEdge(0, true, i) != vert && !this->forbiddenObject(parent->getEdge(0,true,i)))//making sure we have a vertex and that it isn't a story item
			if (parent->getEdge(0, true, i)->getName().find(name) != std::string::npos) {//our sibling needs to be similar
				if (num_found == count)
					item = parent->getEdge(0, true, i);
				num_found++;
			}
	}
	return item;
}
//Goes through our factors to get the factor pos (id)
int NarrativeWorldMoldGenerator::getFactorPosition(Factor* fact) {
	if (fact == NULL)
		return -1;
	return db->getFactor(fact);
}
Vertex* NarrativeWorldMoldGenerator::createNewObject(Vertex* old_object, int counter) {
	Vertex* new_object;
	char name[100];
	sprintf_s(name, "%s_%d", old_object->getName().c_str(), counter); //Remember that this will be j, so it may not be the underscore number we wish, but it will work!
	new_object = new Vertex(name);
	old_object->getEdge(0, false)->addEdge(new_object, true);
	int new_id;
	if (!this->vertices.empty()) {
		new_id = -1;
		for (std::map<int, Vertex*>::const_reverse_iterator it = this->vertices.rbegin(); it != this->vertices.rend(); it++) {
			if (new_id < (*it).first) {
				new_id = (*it).first;
			}
		}
		new_id++;
	}
	else {
		new_id = this->db->getMaxObjectID()+1;
	}
	this->addVertex(new_id,new_object);
	return new_object;
}
bool NarrativeWorldMoldGenerator::forbiddenObject(Vertex* obj) {
	if (obj == NULL)
		return false; //How can it be forbidden if it doesn't exist
	int pos = this->getVertexNumber(obj);
	if (std::find(this->forbidden_objects.begin(), this->forbidden_objects.end(), pos) == this->forbidden_objects.end()) {
		return false;
	}
	return true;
}

//Is child actually a child of parent?
bool NarrativeWorldMoldGenerator::recursive(Vertex* parent, Vertex* child) {
	if (parent == NULL || child == NULL)
		return false;
	bool found = false;
	while (child != NULL && !found) {
		if (child == parent)
			found = true;
		child = child->getEdge(0, false);
	}
	return found;
}
bool NarrativeWorldMoldGenerator::recursive(int parent, int child) {
	if (parent == -1 || child == -1)
		return false;
	Vertex* p = this->getVertex(parent);
	Vertex* c = this->getVertex(child);
	return this->recursive(p, c);
}
int NarrativeWorldMoldGenerator::getVertexNumber(Vertex* vert, bool deep_copy) {
	int obj_id = -1;
	if (vert != NULL) {
		obj_id = this->db->getObjects(vert);
		for (std::map<int, Vertex*>::const_iterator it = this->vertices.begin(); obj_id == -1 && it != this->vertices.end(); it++) {
			if (deep_copy) {
				if ((*it).second->getName() == vert->getName())
					obj_id = (*it).first;
			}
			else {
				if ((*it).second == vert)
					obj_id = (*it).first;
			}
		}
	}
	return obj_id;
}
Vertex* NarrativeWorldMoldGenerator::getVertex(int obj_id) {
	Vertex* vert = NULL;
	if (obj_id != -1) {
		vert = this->db->getObjectByID(obj_id);
		if (vert == NULL) {
			if (this->vertices.find(obj_id) != this->vertices.end()) {
				vert = this->vertices.at(obj_id);
			}
		}
	}
	return vert;
}
Vertex* NarrativeWorldMoldGenerator::getVertex(const std::string& obj_name) {
	Vertex* vert = this->db->getObjectByID(this->db->getObjects(obj_name));
	for (std::map<int, Vertex*>::const_iterator it = this->vertices.begin(); vert == NULL && it != this->vertices.end(); it++) {
		if ((*it).second->getName() == vert->getName())
			vert = (*it).second;
	}
	return vert;
}

//We get back a new ruleset that has only the expansion
RuleSet* NarrativeWorldMoldGenerator::expand(RuleSet* set, int rule_num,const std::map<int,int>& dfs_selects) {
	//We have already identified the culprit at this time
	RuleSet* new_set = new RuleSet(false);
	float prob = set->getRuleProbability(rule_num);
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> var_range = set->getPredicate(rule_num);
	Factor* fact = set->getFactor(rule_num);
	Vertex* item = NULL;
	if (dfs_selects.find((*var_range.first).second) != dfs_selects.end()) {
		int val = (*var_range.first).second;
		val = dfs_selects.at(val);
		item = this->getVertex(val); //Now we are working with the dfs_selected vertex
	}
	if (item == NULL)
		return new_set;
	new_set->addVertex(item);
	int item_pos = (*var_range.first).second;
	//Now, we get all rules that have this connection
	std::list<int> connection_rules;
	for (int i = 0; i < set->getNumRules(); i++) {
		if (i != rule_num) {
			bool found = false;
			var_range = set->getPredicate(i);
			//Here, we do not count when the first is the same as the second, no matter what. This is because we will handle this in our combination
			//Where the multiple self loops will figure out the highest probability 
			for (std::multimap<int, int>::const_iterator it = var_range.first; it != var_range.second; it++) {
				if ((*it).second == item_pos) {
					found = !found;
				}
			}
			if (found)
				connection_rules.push_back(i);
		}
	}
	float rand_expand = (float)rand() / (float)RAND_MAX;
	int i = 0;
	std::list<Vertex*> variables;
	Vertex* new_item = NULL;
	while (i < 1) { //prob > rand_expand
		variables.clear();
		//At this stage, we have all the connection rules and the connection we need to duplicate
		//so, we keep creating new larger connections
		//First, we figure out if we have another object that would do it by searching the siblings
		//We want to do the same stratigy as we are doing for dfs_selection
		int j = 0;
		bool found = false;
		while(!found){
			//See if our new item is in the set
			new_item = this->searchSiblings(item, j);
			if (new_item == NULL) {
				//There is not one that exists for us, abort
				found = true;
			}
			else {
				int pos = new_set->getVertex(new_item);
				if (pos != -1) {
					new_item = NULL;
				}
				else {
					found = true;
				}
			}
			j++;
		}
		if (new_item == NULL) { //If it's not found, then we create a new one with the last counter of j
			//We need to make sure we aren't duplicating the name
			//if (isdigit(item->getName().substr(item->getName().find_last_of("_") + 1, 1).c_str()[0])) {
			//	std::cout << item->getName() << " " << j << std::endl;
			//}
			new_item = createNewObject(item, j);
		}
		//Next, we add in the new connection to the new ruleset
		variables.push_back(item);
		variables.push_back(new_item);
		new_set->addRule(fact, variables, prob);
		new_set->addFrequency(new_set->getVertex(item), set->getFrequency(item_pos));
		new_set->addFrequency(new_set->getVertex(new_item), set->getFrequency(item_pos));
		//And we connect the other rules to the new object as well
		for (std::list<int>::const_iterator it = connection_rules.begin(); it != connection_rules.end(); it++) {
			variables.clear();
			var_range = set->getPredicate((*it));
			for (std::multimap<int, int>::const_iterator it2 = var_range.first; it2 != var_range.second; it2++) {
				if ((*it2).second != item_pos) {
					variables.push_back(this->getVertex((*dfs_selects.find((*it2).second)).second)); //Again, we are working in dfs when we are expanding
				}
				else {
					variables.push_back(new_item);
				}
			}
			//Don't need frequency of the other items, those are used when we combine!
			new_set->addRule(set->getFactor((*it)), variables, set->getRuleProbability((*it))); 
		}
		rand_expand = (float)rand() / (float)RAND_MAX; //Now we try again
		i++;
	}
	return new_set;
}
//Our vertices are made up of story objects, which must be converted into motif-objects
RuleSet* NarrativeWorldMoldGenerator::convertToCommonItems(RuleSet* set, NarrativeWorldMold* recipe) {
	//Our factors and vertices should always be connected to something in our knowledge base. 
	//If we don't know about it, we can't do anything with it
	//Next, we do this for the vertices
	for (int i = 0; i < set->getNumVertices(); i++) {
		Vertex *v = set->getVertex(i);
		if (recipe->hasStoryItem(v)) { //If it is a story vertex,then we switch it
			set->replaceVertex(v, recipe->getShadowVertex(v));
		}
	}
	//Finally, return our set
	return set;
}
//The reverse of the common objects
RuleSet* NarrativeWorldMoldGenerator::convertToStoryItems(RuleSet* res,NarrativeWorldMold* recipe) {
	if (res == NULL || recipe== NULL)
		return res;
	//For each item in story set, find the connected vertex in res
	for (int i = 0; i < res->getNumVertices(); i++) {
		Vertex * story = res->getVertex(i);
		if(recipe->getStoryVertex(story) != NULL){
			res->replaceVertex(story, recipe->getStoryVertex(story));
		}
	}
	return res;
}
//Set is the set that the connecting vertex is in. Compares is the intersection between
//set and the motif that we want to connect to.
//Sometimes, the true vert is replaced by a parent. We want the actual connection to be on the vert, and not the true vert
RuleSet* NarrativeWorldMoldGenerator::Dijstra(RuleSet* set, RuleSet *compares, int vert, Content* true_vert) {
	std::vector<int> dist;
	std::vector<int> prev;
	std::vector<bool> seen; //Falls into a bit-wise vector
	dist.resize(set->getNumVertices(), 999999);
	prev.resize(set->getNumVertices(), -1);
	seen.resize(set->getNumVertices(), false);
	dist[vert] = 0; //vert is now the index from vertex
	std::list<int> range;
	int node = vert;
	while (node != -1) {
		seen[node] = true;
		//For each neighbor of node
		range = set->getPredicates(set->getContent(node),1);
		for (std::list<int>::iterator it = range.begin(); it != range.end(); it++) {
			int v = set->getPredicate((*it)).first->second;
			int alt = dist[node] + 1;//For unweighted, dist_between is 1
			if (alt < dist[v]) { //One interesting change would be to weight dijstra with 1-p where p is the probability of the link
				dist[v] = alt;
				prev[v] = node;
			}
		}
		//Get lowest unseen vertex
		node = -1;
		for (int i = 0; i < (int)set->getNumVertices(); i++) {
			if (!seen[i]) {
				if (node == -1 || dist[i] < dist[node]) {
					node = i;
				}
			}
		}
	}
	//std::list<int> prev_list(prev, prev + vertices.size()); //debug
	//std::list<int> dist_list(dist, dist + vertices.size());
	//Next, I determine the shortest list of connections
	//For each object in compare verts, we get the minimum distance to our source node
	//The minimum gives us the end connection we need to make
	int lowest_vertex = -1;
	int lowest_cost = 999999;
	for (int i = 0; i < compares->getNumVertices(); i++) {
		int match_set = set->getVertex(compares->getVertex(i));
		if (match_set != -1) {//if it exists
			//Get the distance
			if (dist[match_set] < lowest_cost) {
				lowest_vertex = match_set;
				lowest_cost = dist[match_set];
			}
		}
	}
	if (lowest_vertex < 0 || lowest_vertex == vert)
		return NULL;
	//Finally, get that path from a to b as a seperate connection provided from the shortest path
	//We do this by going through prev in the set to get the u,v pairs. 
	RuleSet *result = new RuleSet(false);
	std::list<int> previous_edges;
	std::set<int> previous_nodes; //should be fine as a vector and not a set
	while(lowest_vertex != vert && lowest_vertex >= 0) {
		std::list<Vertex*> items;
		items.push_back(set->getVertex(prev[lowest_vertex]));
		items.push_back(set->getVertex(lowest_vertex));
		int set_edge = set->getPredicate(items,0);
		//At this point, what we want is the inverse, the vertex from the index
		previous_edges.push_back(set_edge);
		previous_nodes.insert(lowest_vertex);//this prevents us from having the last node in there, so we don't mess up any of those properties
		lowest_vertex = prev[lowest_vertex];
	}
	if (lowest_vertex < 0) {//In this case, there actually is no path
		delete result;
		return NULL;
	}
	previous_nodes.insert(vert); //It doesn't insert the last one, so we do
	//now, we get an ordering for this
	std::vector<int> p_nodes;
	for (std::set<int>::const_iterator it = previous_nodes.begin(); it != previous_nodes.end(); it++) {
		p_nodes.push_back((*it));
		
	}
	previous_nodes.clear();
	for (std::vector<int>::const_iterator it = p_nodes.begin(); it != p_nodes.end(); it++) {
		if ((*it) == vert) {
			result->addContent(true_vert);
		}
		else {
			result->addContent(set->getContent((*it)));
		}
	}
	int addition_count = 0;
	for (std::list<int>::iterator it = previous_edges.begin(); it != previous_edges.end(); it++) {
		std::list<int> vert_ids;
		std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = set->getPredicate((*it));
		for (std::multimap<int, int>::const_iterator vit = range.first; vit != range.second; vit++) {
			bool found = false;
			for(int i=0; i < p_nodes.size() && !found; i++) {//Find the position (which should always be in there
				if ((*vit).second == p_nodes.at(i)) {
					vert_ids.push_back(i);
				}
			}
		}
		
		result->addRule(set->getFactor((*it)), vert_ids, set->getRuleProbability((*it)));
	}
	return result;
}
RuleSet* NarrativeWorldMoldGenerator::augmentRuleSet(RuleSet* superset, RuleSet* motif) {
	//First, we get the map of the intersection vertices
	std::map<int, int> matched_indices = superset->intersectionVertices((*motif));
	RuleSet* result = new RuleSet(*motif, false); //copy the ruleset
	for (std::map<int, int>::const_iterator it = matched_indices.begin(); it != matched_indices.end(); it++) { //l to v
		result->replaceContent((*it).second, superset->getContent((*it).first));
	}
	return result;
}
/*reverse distance matching methods*/
std::multimap<int, int> NarrativeWorldMoldGenerator::vertexMatches(RuleSet* lhs, RuleSet* rhs) {
	/*Matches all the vertices from the left hand side to all the vertices from the right hand side*/
	std::multimap<int, int> results;
	for (int i = 0; i < lhs->getNumVertices(); i++) {
		Vertex* v1 = lhs->getVertex(i);
		int v_i = this->getVertexNumber(v1);
		for (int j = 0; j < rhs->getNumVertices(); j++) {
			Vertex* v2 = rhs->getVertex(j);
			int v_j = this->getVertexNumber(v2);
			if (this->recursive(v1, v2) || this->recursive(v2, v1)) {
				results.insert(std::make_pair(v_i, v_j));
			}
		}
	}
	return results;
}
/*Here, we will find all possible edge matches between my left and right hand side. This is not
reflexive

*/
std::multimap<int, int> NarrativeWorldMoldGenerator::candidateMatches(RuleSet* lhs, RuleSet* rhs) {
	std::multimap<int, int> results;
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> lhs_range;
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> rhs_range;
	Vertex *lhs_1, *lhs_2, *rhs_1, *rhs_2;
	for (int i = 0; i < lhs->getNumRules(); i++) {
		for (int j = 0; j < rhs->getNumRules(); j++) {
			if (lhs->getFactor(i) == rhs->getFactor(j)) { //Here we check to make sure the delta is correct
														  //Now we check the vertices to see if they match
				lhs_range = lhs->getPredicate(i);
				lhs_1 = lhs->getVertex(lhs_range.first->second);
				lhs_range.first++;
				lhs_2 = lhs->getVertex(lhs_range.first->second);
				rhs_range = rhs->getPredicate(j);
				rhs_1 = rhs->getVertex(rhs_range.first->second);
				rhs_range.first++;
				rhs_2 = rhs->getVertex(rhs_range.first->second);
				if (this->recursive(lhs_1, rhs_1) || this->recursive(rhs_1, lhs_1)) {
					if (this->recursive(lhs_2, rhs_2) || this->recursive(rhs_2, lhs_2)) {
						results.insert(std::make_pair(i, j));
					}
				}
				else if (this->recursive(lhs_1, rhs_2) || this->recursive(rhs_2, lhs_1)) {
					if (this->recursive(lhs_2, rhs_1) || this->recursive(rhs_1, lhs_2)) {
						results.insert(std::make_pair(i, j));
					}
				}
			}
		}
	}
	return results;
}
//We are matching the right hand side to the left hand side, so we have to connect one of every right hand side
int NarrativeWorldMoldGenerator::matchedEdges(RuleSet* lhs, RuleSet* rhs) {
	int total_matches = 0;
	if (lhs == NULL || rhs == NULL)
		return total_matches;
	std::multimap<int, int> vertex_match = this->vertexMatches(lhs, rhs);
	std::multimap<int, int> edge_match = this->candidateMatches(lhs, rhs);
	bool *rhs_verts = new bool[rhs->getNumVertices()];
	std::map<int, int> edge_pointer;
	for (int i = 0; i < rhs->getNumVertices(); i++) {
		rhs_verts[i] = false;
	}
	bool *rhs_edges = new bool[rhs->getNumRules()];
	for (int i = 0; i < rhs->getNumRules(); i++) {
		rhs_edges[i] = false;
		edge_pointer[i] = 0;
	}
	for (int i = 0; i < rhs->getNumRules(); i++) {

	}
	for (int i = 0; i < rhs->getNumRules(); i++) {
		if (rhs_edges[i])
			total_matches++;
	}
	delete[] rhs_verts;
	delete[] rhs_edges;
	return total_matches;
}

RuleSet* NarrativeWorldMoldGenerator::buildConnections(int loc_id, RuleSet* diff, RuleSet* best_motif_match) {
	RuleSet *result = new RuleSet((*best_motif_match), false); //We copy because we are working with best motif match
	for (int i = 0; i < diff->getNumVertices(); i++) {
		//bool found = false;
		for (unsigned int j = 0; j < this->motifs.size(); j++) {
			//Determine if it belongs to the location, if so, test it
			if (j != loc_id) {
				RuleSet* motif = this->motifs.at(j);
				if (motif != NULL) {
					//Determine if it has it. If so, then we will run dijstra on the motif position vertex

					Content* content = motif->getContent(diff->getContent(i)->getVertex());
					if (content != NULL) {
						int pos = motif->getVertex(content->getVertex());
						RuleSet* intersection = motif->intersection((*motif), (*best_motif_match));
						if (intersection->getNumVertices() > 0) {
							//Then, we see if we get back with dijstra
							RuleSet* addition = Dijstra(motif, intersection, pos, diff->getContent(i)); //Same set, so should be fine, but also considers parents
							if (addition != NULL) {
								//Before entering, we have to set the new content to be the same uniqueness as the old content
								addition->getContent(diff->getContent(i)->getVertex())->setUnique(diff->getContent(i)->getUnique());
								RuleSet new_set  =(*result) + (*addition);
								delete result;
								result = new RuleSet(new_set,false);
								//Now, we make sure to set our uniqueness away, because we have the unique item already inset in the rule-set
								diff->getContent(i)->setUnique(false);
							}
							delete addition;
						}
						delete intersection;
					}
				}
			}
		}
	}
	return result;
}

//Determines if our database connection has the factor
bool NarrativeWorldMoldGenerator::hasFactor(Factor* fact) {
	if (db->getFactor(fact) != -1)
		return true;
	return false;
}
//Determines if we have the vertex when we are parsing the plot points
bool NarrativeWorldMoldGenerator::hasVertex(const std::string& vertex_name) {
	return this->getVertex(vertex_name) != NULL;
}
bool NarrativeWorldMoldGenerator::hasLocationMotif(int loc_id) {
	if (this->location_motifs.count(loc_id) > 0)
		return true;
	return false;
}

std::set<std::string> removeElements(const std::set<std::string>& start_set, std::multimap<int, std::string>& remove_set, int remove_point) {
	if (remove_set.find(remove_point) == remove_set.end()) {
		std::set<std::string> new_set = std::set<std::string>(start_set);
		return new_set;
	}
	//Otherwise, we got to see if there is any overlap
	std::set<std::string> new_set = std::set<std::string>();
	std::pair<std::multimap<int, std::string>::const_iterator, std::multimap<int, std::string>::const_iterator> range;
	range = remove_set.equal_range(remove_point);
	for (std::set<std::string>::const_iterator it = start_set.begin(); it != start_set.end(); it++) {
		bool found = false;
		for (std::multimap<int, std::string>::const_iterator mit = range.first; mit != range.second && !found; mit++) {
			if ((*mit).second == (*it))
				found = true;
		}
		if (!found)
			new_set.insert((*it));
	}
	return new_set;
}

void UnionElements(std::set<std::string>& start_set, std::multimap<int, std::string>& add_set, int add_point) {
	std::pair<std::multimap<int, std::string>::iterator, std::multimap<int, std::string>::iterator> range = add_set.equal_range(add_point);
	for (std::multimap<int, std::string>::iterator it = range.first; it != range.second; it++)
		start_set.insert((*it).second);
}

//Performs a union of two name sets
void UnionElements(std::set<std::string>& a_set, std::set<std::string>& b_set) {
	for (std::set<std::string>::const_iterator it = b_set.begin(); it != b_set.end(); it++) {
		a_set.insert((*it));
	}
}




//This is the code to build up the Narrative World Recipe temporial and locations. It may get moved to the Generator during a refactor
std::map<int, std::set<std::string> > backFowardTrack(std::multimap<int, std::string>& created, std::multimap<int, std::string>& used, std::multimap<int, std::string>& destroyed) {
	//First, we get the 
	std::map<int, std::set<std::string> > results;
	std::set<int> time_points;
	std::multimap<int, std::string>::const_iterator it;
	if (created.empty() && used.empty() && destroyed.empty())
		return results; //We have nothing here to return
	for (it = created.begin(); it != created.end(); it++) {
		time_points.insert((*it).first);
	}
	for (it = used.begin(); it != used.end(); it++) {
		time_points.insert((*it).first);
	}
	for (it = destroyed.begin(); it != destroyed.end(); it++) {
		time_points.insert((*it).first);
	}
	//Also add in the time_points -1 and 9999999 (beginning and end)
	//results[-1] = std::list<std::string>();
	time_points.insert(-1);
	results[999999] = std::set<std::string>();
	time_points.insert(999999);
	//Next, we backtrack through the list, where our set is time_point set unioned with the future set - the create set of that future set
	std::set<int>::const_reverse_iterator tp = time_points.rbegin();
	++tp;
	std::set<int>::const_reverse_iterator tpp1 = time_points.rbegin();
	while (tp != time_points.rend()) {
		results[(*tp)] = removeElements(results[(*tpp1)], created, (*tp)); //(future set minus the items that were created at the end of this action)
		UnionElements(results[(*tp)], used, (*tp)); //Connected with the items that were used in this action
		UnionElements(results[(*tp)], destroyed, (*tp)); //With the items that were destroyed in this action (because they need to exist to be destroyed)
		++tpp1;
		++tp;
	}
	std::set<int>::const_iterator ftp = time_points.begin();
	ftp++;
	std::set<int>::const_iterator ftpp1 = time_points.begin();
	while (ftp != time_points.end()) {
		std::set<std::string> foward = removeElements(results[(*ftpp1)], destroyed, (*ftpp1));
		UnionElements(foward, used, (*ftp));
		UnionElements(foward, created, (*ftpp1)); //With the items that were created in the last iteration
		UnionElements(results[(*ftp)], foward);
		ftpp1++;
		ftp++;
	}
	//finally, we go through each time-point and remove the ones where there is no change to the previous
	std::map<int, std::set<std::string> >::iterator rit = results.begin();
	rit++;
	std::map<int, std::set<std::string> >::iterator rit1 = results.begin();
	while (rit != results.end()) {
		//Determine if they are equal
		bool equal = true;
		//Break this out to make it easier to read
		std::set<std::string> sit = (*rit).second;
		std::set<std::string> sit1 = (*rit1).second;
		if (sit.size() != sit1.size())
			equal = false;
		for (std::set<std::string>::iterator it = sit.begin(); it != sit.end() && equal; it++) {
			if (std::find(sit1.begin(), sit1.end(), (*it)) == sit1.end())
				equal = false;
		}
		for (std::set<std::string>::iterator it = sit1.begin(); it != sit1.end() && equal; it++) {
			if (std::find(sit.begin(), sit.end(), (*it)) == sit.end())
				equal = false;
		}
		if (equal) {
			//Here,we want to remove it
			rit = results.erase(rit);
		}
		else {
			rit++;
			rit1++;
		}
	}
	return results;
}
//Determines the create, used, and destroyed set from the list of strings that we have
std::map<int, std::set<std::string> > createTemporialNarrative(const std::list<std::string>& change_json) {

	std::multimap<int, std::string> created;
	std::multimap<int, std::string> used;
	std::multimap<int, std::string> destroyed;
	int type = 0;
	//First, we loop through all items and get the list of time positions
	for (std::list<std::string>::const_iterator it = change_json.begin(); it != change_json.end(); it++) {
		size_t  start = (*it).find(":"); //The first one will say what we are dealing with
		if ((*it).find("Used") != std::string::npos) {
			type = 1;
		}
		else if ((*it).find("Destroyed") != std::string::npos) {
			type = 2;
		}
		start = (*it).find("[");
		size_t end = (*it).find("]");
		if (start + 1 != end) { //If this is the case, we have an empty set
			std::string sub = (*it).substr(start + 1, end - (start + 1));
			int num_points = (int)std::count(sub.begin(), sub.end(), ':');
			start = 0; //We start from the first of the substring, which is zero
			for (int i = 0; i < num_points; i++) {
				end = sub.find(":", start);
				int time = atoi(sub.substr(start, end - start).c_str());
				start = end + 1;
				end = sub.find(",", start);
				if (type == 0)
					created.insert(std::make_pair(time, stripQuotes(sub.substr(start, end - start))));
				else if (type == 1)
					used.insert(std::make_pair(time, stripQuotes(sub.substr(start, end - start))));
				else
					destroyed.insert(std::make_pair(time, stripQuotes(sub.substr(start, end - start))));
				start = end + 1;
			}
		}
	}
	//This determines all points AT THE BEGINNING OF THAT TIME POINT where something happens and we need to know what the state of the world looks like
	return backFowardTrack(created, used, destroyed);
}

NarrativeWorldMold* NarrativeWorldMoldGenerator::readInNarrativeWorldMold(const std::vector<std::string>& location_graph, const std::vector<std::string>& narrative_table,bool motif_only) {
	NarrativeWorldMold* result = new NarrativeWorldMold();
	//First, we will connect all the locations together
	for (int i = 0; i < location_graph.size(); i++) {
		std::string line = location_graph[i];
		size_t delim = line.find("-");
		if (delim != std::string::npos) {
			std::string first_room = line.substr(0, delim);
			std::string second_room = line.substr(delim + 1);
			if (result->getLocationID(first_room) == -1)
				result->addLocation(first_room);
			if (result->getLocationID(second_room) == -1)
				result->addLocation(second_room);
			result->addLocationConnection(first_room, second_room);
		}
	}
		
	//Next, we read in the full temporial structure and backtrack based on that
	if (!motif_only) {
		std::list<std::string> table;
		std::string change_name;
		for (int i = 0; i < narrative_table.size(); i++) {
			std::string line = narrative_table[i];
			if (line.find("}") != std::string::npos) {
				if (result->getLocationID(change_name) != -1) {
					RuleSet *superset = new RuleSet(false);
					std::map<int, std::set<std::string> > changeMap = createTemporialNarrative(table);
					for (std::map<int, std::set<std::string> >::iterator it = changeMap.begin(); it != changeMap.end(); it++) {
						RuleSet* set = new RuleSet(false);
						for (std::set<std::string>::iterator sit = (*it).second.begin(); sit != (*it).second.end(); sit++) {
							Vertex *vex = NULL;
							if (!result->hasStoryItem((*sit))) {
								//In this case, we find the parent and add it to the list
								vex = new Vertex((*sit));
								//See if we can give it a parent?
								std::string parent_name = extractParent((*sit));
								if (this->hasVertex(parent_name)) {
									//If we find it, we actually want to search the siblings
									Vertex * travel = this->getVertex(parent_name);
									//If our traveler is already a story item, then we should search the parent's siblings for one that isn't
									result->addStoryItem(vex, travel);
								}
								else {
									result->addStoryItem(vex, NULL);
								}
							}
							else {
								vex = result->getStoryVertex((*sit));
							}
							set->addVertex(vex);
							set->addFrequency(set->getVertex(vex), 1.0);
							if (superset->getVertex(vex) == -1) { //If it isn't in super-set
								superset->addVertex(vex);
								superset->addFrequency(superset->getVertex(vex), 1.0);
							}
						}
						result->addTemporialConstraint(change_name, (double)(*it).first, set);
					}
					if (superset->getNumVertices() > 0) {
						result->addSuperset(change_name, superset);
					}
					else {
						delete superset;
					}
				}
				table.clear();

			}
			else if (line.find("{") != std::string::npos) {
				//We are starting a new one
				size_t start = line.find("'") + 1;
				size_t end = line.find("'", start);
				change_name = line.substr(start, end - start);
			}
			else {
				table.push_back(line);
			}
		}
	}
	return result;
}
//We are going to assume that our superset is already converted to common items from the story items
RuleSet* NarrativeWorldMoldGenerator::genSuperSetLocation(int loc_id, RuleSet* superset,NarrativeWorldMold* recipe, bool with_addition) {
	
	//This is the preliminary
	//First, we go through and create a unioned set of all time_points to know what all we need
	if (superset == NULL) { //Motif only location
		std::pair< std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator > range = this->location_motifs.equal_range(loc_id);
		if (range.first == range.second)
			return NULL;
		else
			return this->motifs.at((*range.first).second);
	}
	//delete superset; //can't delete, vertices are now being used. Memory leak that needs fixing
	//What actually gives us the location
	//Next, we find the motif with the best match
	RuleSet* best_motif_match = NULL;
	float best_motif_score = -1.0f;
	std::pair< std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator > range = this->location_motifs.equal_range(loc_id);
	for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
		//Determine if it belongs to the location, if so, test it
		RuleSet* motif = this->motifs.at((*it).second);
		//TIV* motif = this->motifToSparseMatrix((*it).second,false);
		RuleSet *intersection = superset->intersection((*superset),(*motif));
		if (intersection != NULL) {
			float score  = this->cosineSim(intersection, superset, motif); 
				  score += this->cosineSimRules(intersection, superset, motif);
				  score /= 2.0f;
			if (score > best_motif_score) {
				best_motif_match = motif;//just changing a pointer
				best_motif_score = score;
			}
			delete intersection;
		}
	}
	if (best_motif_match == NULL) //At this case, we didn't find a motif that would work
		return superset;
	//Our connection contribution
	//Using the best motif match, add in the connections that are still needed
	RuleSet* result = augmentRuleSet(superset,best_motif_match); //we are messing with the ruleset, so it copies here
	RuleSet diff = (*superset) - (*result); //this is for our new way of augmenting the data-structure
	diff.cleanRuleSet(true);
	//With the intersection
	//The intersection tells us which objects we should be re-using and replacing
		if (diff.getNumVertices() > 0 && with_addition) {
			//We set the uniqueness of our super-set here simply because here is the only place where we want to make sure
			//that each item is being treated differently (so we will the same item multiple times if the story asks for it)
			for (int i = 0; i < diff.getNumVertices(); i++) {
				Content* c = diff.getContent(i);
				c->setUnique(true);
			}
			RuleSet* res = buildConnections(loc_id, &diff, result);
			delete result;
			result = res;
		}
	return result;

}

std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> NarrativeWorldMoldGenerator::getLocationMotif(int loc_id) const {
	return this->location_motifs.equal_range(loc_id);
}
