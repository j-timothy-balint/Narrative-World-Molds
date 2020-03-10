#include "SetTransformation.h"
#include "BaseDataStructures.h"
#include "GluNet.h"

//Determines a list of actions that match a given set of vertex objects
std::list<int> FrameTransformation::getActionsFromObjSet(const std::list<Vertex*>& objs,const std::multimap<std::string,std::string>& adjectives) {
	
	std::map<int, int> action_counts;
	std::list<int> acts;
	std::list<std::string> names;
	for (std::list<Vertex*>::const_iterator it = objs.begin(); it != objs.end(); it++) {
		names.clear();
		Vertex *vert = (*it);
		while (vert != NULL) {
			std::string name = vert->getName().substr(0, vert->getName().find("."));
			names.push_back(name);
			std::pair<std::multimap<std::string, std::string>::const_iterator, std::multimap<std::string, std::string>::const_iterator> adj_range = adjectives.equal_range(name);
			for (std::multimap<std::string, std::string>::const_iterator adj_it = adj_range.first; adj_it != adj_range.second; adj_it++)
				names.push_back((*adj_it).second);
			vert = vert->getEdge(0, false);
		}
		std::list<int> actions = this->reader->framesFromFEs(names);
		for (std::list<int>::iterator cit = actions.begin(); cit != actions.end(); cit++) {
			if (action_counts.find((*cit)) == action_counts.end())
				action_counts[(*cit)] = 1;
			else
				action_counts[(*cit)] ++;
		}
	}
	for (std::map<int, int>::iterator it = action_counts.begin(); it != action_counts.end(); it++) {
		//if ((*it).second > this->reader->getFECore((*it).first)) {
			//Then it is a good one
			acts.push_back((*it).first);
		//}
	}

	return acts;
}

template <typename T, typename U> inline bool quickFind(typename std::multimap<T, U>::iterator begin, typename std::multimap<T, U>::iterator end, U input) {
	for (std::multimap<T, U>::iterator it = begin; it != end; it++) {
		if ((*it).second == input)
			return true;
	}
	return false;
}
std::map<std::string, float> FrameTransformation::totalJacComp(std::multimap<std::string, int> input) {//switch to glu-net ids
	std::map<std::string, float> output;
	std::list<std::string> names;
	//This is a very slow way of doing this, and there is alot of room for improvement
	//For each unique key, we get the left hand side of our data
	for (std::multimap<std::string, int>::iterator it = input.begin(); it != input.end(); it = input.upper_bound((*it).first)) { //cache this instead of ub search each time
		names.push_back((*it).first);
		output[(*it).first] = 0.0f;
	}
	for (std::list<std::string>::iterator it = names.begin(); it != names.end(); it++) {
		std::pair<std::multimap<std::string, int>::iterator, std::multimap<std::string, int>::iterator> a = input.equal_range((*it));
		int a_dist = std::distance(a.first, a.second) + 1;
		for (std::list<std::string>::iterator it2 = names.begin(); it2 != names.end(); it2++) {
			if ((*it) != (*it2)) { //Jaccard of the same two groups should be 1
				int intersect_count = 0;
				//jac = intersecion/union = int/(int + non_int count)
				std::pair<std::multimap<std::string, int>::iterator, std::multimap<std::string, int>::iterator> b = input.equal_range((*it2));
				int b_dist = std::distance(b.first, b.second) + 1;
				for (std::multimap<std::string, int>::iterator ait = a.first; ait != a.second; ait++) {
					if (quickFind<std::string,int>(b.first, b.second, (*ait).second)) {
						intersect_count++;
					}
				}
				float jac = (float)intersect_count / (float)(a_dist + b_dist - intersect_count);
				//std::cout << (*it) << ":" << (*it2) << ":" << jac << std::endl;
				if (jac > output[(*it)])
					output[(*it)] = jac;
			}
		}
	}
	return output;
}

/******************************************************
 *Our frame transformation functions
 *****************************************************/
//Resets all of our visited functions to false
void FrameTransformation::resetVisited(bool *visited) {
	for (unsigned int i = 0; i < this->vertices.size(); i++) {
		visited[i] = false;
	}
}
//Does our backtracking stuff that we need to do
void FrameTransformation::backtrack(int vert, node* nodes) {
	nodes[vert].cur_aff = nodes[vert].beg_aff;
	nodes[vert].visited = false;
}
int FrameTransformation::processVertex(int vert, node* nodes, bool* affordances,int length) {
	nodes[vert].visited = true; //Lets say we visited it
	bool found = false;
	while(nodes[vert].cur_aff != nodes[vert].end_aff && !found ) {
		int aff_pos = this->findFE(nodes[vert].cur_aff->second);
		if (!affordances[aff_pos]) {
			affordances[aff_pos] = true;
			found = true;
		}
		else {
			nodes[vert].cur_aff++;
		}
	}
	if (!found) {
		this->backtrack(vert,nodes);
		return length;
	}
	length++; //We are good for this
	for (unsigned int i = 0; i < this->vertices.size(); i++) {
		//Find the first unvisited connection
		if (this->connections[vert][i] && !nodes[i].visited) {
			length = this->processVertex(i, nodes, affordances,length);
		}
	}
	return length;
}
//Given a starting path vertex svert, find a path of length length (that is, has k checked off) 
std::list<int> FrameTransformation::findPath(int svert, int length) {
	//Make sure everthing is starting on an even kneel
	std::list<int> path;
	bool *affordances = new bool[this->frameFEs.size()];
	for (unsigned int i = 0; i < this->frameFEs.size(); i++) {
		affordances[i] = false;
	}
	node *nodes = new node[this->vertices.size()];
	//Get the starting position for each vertex that has a FE list attached
	for (unsigned int i = 0; i < this->vertices.size(); i++) {
		nodes[i].index = i;
		nodes[i].visited = false;
		nodes[i].beg_aff = this->annotations.lower_bound(i);
		nodes[i].cur_aff = this->annotations.lower_bound(i);
		nodes[i].end_aff = this->annotations.upper_bound(i);
	}
	//Now, until we have iterated through everything or found a path, we DFS through things
	//int cur_vert = svert;
	//int par_ver = -1;
	//int path_len = 0;
	//Either we make the path or we fail
	//while (cur_vert != -1 && path_len < length) {
	//}
	int len = processVertex(svert, nodes, affordances, 0);

	//Finally, our path becomes the list of visited nodes
	if (len >= length) {
		for (int i = 0; i < (int)this->vertices.size(); i++) {
			if (nodes[i].visited)
				path.push_back(i);
		}
	}
	delete[] affordances;
	delete[] nodes;
	return path;
} 

//Determines if a path is already in our set of all paths
bool FrameTransformation::matched(const std::list<int>& path) {
	bool matched = false;
	std::pair<std::multimap<int, Vertex*>::iterator, std::multimap<int, Vertex*>::iterator> range;
	for (std::multimap<int, Vertex*>::iterator it = this->all_paths.begin(); it != this->all_paths.end() && !matched; it = this->all_paths.upper_bound((*it).first)) {
		range = this->all_paths.equal_range((*it).first);
		if (std::distance(range.first, range.second) + 1 == path.size()) {
			//Because the end goal is to mine the factors for each of these, we do not care about which FE it was attached to
			bool found = true;
			for (std::list<int>::const_iterator it2 = path.begin(); it2 != path.end() && found; it2++) {
				if (!quickFind<int,Vertex*>(range.first, range.second, this->vertices.at((*it2)))) {
					found = false;
				}
			}
			if (found) {
				matched = true;
			}
		}
	}

	return matched;
}
int FrameTransformation::findFE(int fe) {
	for (int i = 0; i < this->frameFEs.size(); i++) {
		if (this->frameFEs.at(i) == fe)
			return i;
	}
	return -1;
}
//Adds a vertex to our list
void FrameTransformation::addVertex(Vertex* vert) {
	this->vertices.push_back(vert);
} 
  //Adds an annotation to our vertex
void FrameTransformation::addAnnotation(Vertex* vert, int fe) {
	for (unsigned int i = 0; i < this->vertices.size(); i++) {
		if (vert == this->vertices.at(i)) {
			this->annotations.insert(std::make_pair(i, fe));
			return;
		}
	}
	//We didn't find the vertex, so we add it to our list
	unsigned int i = this->vertices.size();
	this->vertices.push_back(vert);
	this->annotations.insert(std::make_pair(i, fe));
} 
//Adds a functional element ID from glu-net to the system. It also sets up our core and 
//non-core
void FrameTransformation::addFE(int fe, bool req) {
	this->frameFEs.push_back(fe);
	if (req) {
		this->num_req++;
	}
	else {
		this->num_opt++;
	}
}

void FrameTransformation::calcAllPaths() {
	this->all_paths.clear();
	Vertex *vert1, *vert2;
	this->connections = new bool*[this->vertices.size()];
	//Create the connections
	for (unsigned int i = 0; i < this->vertices.size(); i++) {
		this->connections[i] = new bool[this->vertices.size()];
		vert1 = this->vertices.at(i);
		for (unsigned int j = 0; j < this->vertices.size(); j++) {
			vert2 = this->vertices.at(j);
			if (vert1->getEdge(vert2) != NULL || vert1->getEdge(vert2, false) != NULL)
				this->connections[i][j] = true;
			else
				this->connections[i][j] = false;
		}
	}
	//Get the path for each vertex connection
	for (unsigned int i = 0; i < this->vertices.size(); i++) {
		for (int j = this->num_req; j < this->num_req + this->num_opt; j++) {
			std::list<int> path = this->findPath(i, j);
			if (!matched(path)) {
				//Add the path to all the paths
				for (std::list<int>::const_iterator it = path.begin(); it != path.end(); it++) {
					all_paths.insert(std::make_pair(i, this->vertices[(*it)]));
				}
			}
		}
	}
	//Destroy the connections
	for (unsigned int i = 0; i < this->vertices.size(); i++) {
		delete[] this->connections[i];
	}
	delete[] this->connections;

	this->connections = NULL;
}

int FrameTransformation::getNumPaths() {
	int counter = 0;
	for (std::multimap<int, Vertex*>::const_iterator it = this->all_paths.begin(); it != this->all_paths.end(); it = this->all_paths.upper_bound((*it).first)) {
		counter += 1;
	}
	return counter;
}