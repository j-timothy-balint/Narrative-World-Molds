#include "BaseDataStructures.h"
#include <cstdlib> //For rand()
/*Vertex Section*/
//Starts our basic Vertex, which at the beginning does not have any vertices or edges
Vertex::Vertex(std::string n) :name(n) {}

int Vertex::nextKey() {
	int counter = -1;
	for (std::multimap<int, Vertex*>::iterator it = this->out_edges.begin(); it != this->out_edges.end(); it = this->out_edges.upper_bound(it->first))
	{
		if (counter < (*it).first)
			counter = (*it).first;//Basically, we keep jumping until we find the maximum key
	}
	return counter + 1;
}
//Creates a map count of the names of all of our connections. Useful for equality
std::map<std::string, int> Vertex::countNodes(bool outbound) const{//This assumes that we are dealing with nodes that are made different but semantically the same (same name)
	std::map<std::string, int> counts;
	if (outbound){
		for (std::multimap<int, Vertex*>::const_iterator it = this->out_edges.begin(); it != this->out_edges.end(); this->out_edges.upper_bound((*it).first)){
			//Now, we get the vertices for each map
			std::pair<std::multimap<int, Vertex*>::const_iterator, std::multimap<int, Vertex*>::const_iterator> range = this->out_edges.equal_range((*it).first);
			std::string name = "";
			//Connect them together. For now, we should hope they are sorted
			for (std::multimap<int, Vertex*>::const_iterator vit = range.first; vit != range.second; vit++){
				name += (*vit).second->getName() + "$"; //We want to ignore factors, so we concat the factors we have together
			}
			if (counts.find(name) != counts.end()){
				counts[name] ++;
			}
			else{
				counts[name] = 1;
			}
		}
	}
	else{
		for (std::vector<Vertex*>::const_iterator it = this->in_edges.begin(); it != this->in_edges.end(); it++){
			if (counts.find((*it)->getName()) != counts.end()){
				counts[(*it)->getName()] ++;
			}
			else{
				counts[(*it)->getName()] = 1;
			}
		}
	}
	return counts;
}

//Adds an edge to the vertex graph. We maintain consistancy in our nodes by adding 
//the edge in both the inbound and outbound direction
void Vertex::addEdge(Vertex *edge, bool outbound, int key) {
	if (outbound) {
		if (key == -1) {//If we don't have a key to assign it to (we don't know or have the factor), we figure out the next key in our list
			key = this->nextKey();
		}
		this->out_edges.insert(std::pair<int, Vertex*>(key, edge));
		edge->addEdge(this, false); //Key does not matter here, because the vector id relates back to the key
	}
	else {
		this->in_edges.push_back(edge);
	}
}
//Gets the edge based on the name
Vertex* Vertex::getEdge(const std::string& name, bool outbound, int key) {
	if (outbound) {
		//If key is -1, then we are just looking for the first 
		if (key == -1) {
			for (std::multimap<int, Vertex*>::const_iterator it = this->out_edges.begin(); it != this->out_edges.end(); it++) {
				if ((*it).second->getName() == name) {
					return (*it).second;
				}
			}
		}
		else {
			std::pair<std::multimap<int, Vertex*>::const_iterator, std::multimap<int, Vertex*>::const_iterator> range;
			range = this->out_edges.equal_range(key);
			for (std::multimap<int, Vertex*>::const_iterator it = range.first; it != range.second; it++) {
				if ((*it).second->getName() == name) {
					return (*it).second;
				}
			}
		}
	}
	else {
		for (std::vector<Vertex*>::const_iterator it = this->in_edges.begin(); it != this->in_edges.end(); it++) {
			if ((*it)->getName() == name) {
				return (*it);
			}
		}
	}
	return NULL;
}
//Determines if the edge exists based on the pointer.
bool Vertex::getEdge(Vertex* edge, bool outbound, int key) {

	if (outbound) {
		std::pair<std::multimap<int, Vertex*>::const_iterator, std::multimap<int, Vertex*>::const_iterator> range;
		if (key == -1) {
			range = std::make_pair(this->out_edges.begin(), this->out_edges.end());
		}
		else {
			range = this->out_edges.equal_range(key);
		}
		for (std::multimap<int, Vertex*>::const_iterator it = range.first; it != range.second; it++) {
			if ((*it).second == edge)
				return true;
		}
	}
	else {
		std::vector<Vertex*>::const_iterator it;
		it = std::find(this->in_edges.begin(), this->in_edges.end(), edge);
		if (it != this->in_edges.end()) {
			return true;
		}
	}
	return false;
}

//Gets the edge based on the expected position
//This function may be a bit overloaded
Vertex* Vertex::getEdge(int offset, bool outbound, int key) {

	if (outbound) {
		std::multimap<int, Vertex*>::const_iterator it;
		if (key == -1) { //For outbound edges, we demand a key
			return NULL;
		}
		std::pair<std::multimap<int, Vertex*>::const_iterator, std::multimap<int, Vertex*>::const_iterator> range = this->out_edges.equal_range(key);
		int dist = (int)std::distance(range.first, range.second);
		if (offset >= std::distance(range.first, range.second)) {
			return NULL;
		}
		it = range.first;
		advance(it, offset);
		if (it == range.second)
			return NULL;
		return (*it).second;
	}
	else {
		std::vector<Vertex*>::const_iterator it;
		if (offset >= (int)this->in_edges.size()) {
			return NULL;
		}
		it = this->in_edges.begin();
		advance(it, offset);
		return (*it);
	}
	return NULL;
}

int Vertex::getEdgePos(const std::string& name, bool outbound, int num) {
	Vertex* vert = this->getEdge(name, outbound);
	return this->getEdgePos(vert, outbound, num);
}
//This function determines which position in the vertex list the edge is. Since it can be in there multiple times for different factors, 
//We have a skip (num). In essense, this returns the factor that the vertex is in, for both inbound and outbound edges
int Vertex::getEdgePos(Vertex* edge, bool outbound, int num) {
	if (edge == NULL) {
		return -1;
	}
	int skips = 0;
	int end;
	if (outbound) {
		//Gives the position in the multimap across all factors, even when we might be in different keys
		std::multimap<int, Vertex*>::iterator it = this->out_edges.begin();
		end = (int)this->out_edges.size();
		for (int i = 0; i < end; i++) {
			if ((*it).second == edge) { 
				if (skips >= num) {
					return (*it).first; //this in essense is giving the key of the numth outbound time this edge is in the graph
				}
				else {
					skips++;
				}
			}
			it++;
		}
	}
	else {
		std::vector<Vertex*>::iterator it = this->in_edges.begin();
		end = (int)this->in_edges.size();
		for (int i = 0; i < end; i++) {
			if ((*it) == edge) {
				if (skips >= num) {
					return i;
				}
				else {
					skips++;
				}
			}
			it++;
		}
	}
	return -1;
}
int Vertex::getNumEdges(Vertex* edge, bool outbound) const{
	int result = 0;
	if (outbound) {
		for (std::multimap<int, Vertex*>::const_iterator it = this->out_edges.begin(); it != this->out_edges.end(); it++) {
			if (edge == (*it).second) {
				result++;
			}
		}
	}
	else {
		for (int i = 0; i < (int)this->in_edges.size(); i++) {
			if (edge == this->in_edges[i]) {
				result++;
			}
		}
	}
	return result;
}
//Returns the length of the list container if we are looking at inbound, and the number of keys if we are looking at outbound
int Vertex::getNumEdges(bool outbound) const{
	if (outbound) {
		//stack overflow 11554932
		int counter = 0;
		for (std::multimap<int, Vertex*>::const_iterator it = this->out_edges.begin(); it != this->out_edges.end(); it = this->out_edges.upper_bound(it->first))
		{
			counter++; //count the number of times that we hit the upper bound
		}
		return counter;
	}
	else {
		return (int)this->in_edges.size();
	}
}

//Removes the edge from the list, either the inbound or outbound edge like when we add an edge
bool Vertex::removeEdge(Vertex *edge, bool outbound, int key) {
	if (edge == NULL) {
		return false;
	}
	if (outbound) {
		std::pair<std::multimap<int, Vertex*>::iterator, std::multimap<int, Vertex*>::iterator> range;
		if (key == -1) {
			range = std::make_pair(this->out_edges.begin(), this->out_edges.end());
		}
		else {
			range = this->out_edges.equal_range(key);
		}
		for (std::multimap<int, Vertex*>::iterator it = range.first; it != range.second; it++) {
			if ((*it).second == edge) {
				out_edges.erase(it);
				edge->removeEdge(this, false);
				return true;
			}
		}
	}
	else {
		std::vector<Vertex*>::iterator it = std::find(this->in_edges.begin(), this->in_edges.end(), edge);
		if (it == in_edges.end()) {
			return false;
		}
		in_edges.erase(it);
	}
	return true;
}


//This does a vertex comparision. It is no a full recursive one on the in and out nodes, as we only care about the objects (names)
//and not how they are situated in a graph. Therefore, we are really just doing name comparisions
bool Vertex::operator ==(const Vertex &rhs) const{
	//Quick name and number checks
	if (this->name != rhs.getName())
		return false;
	if (this->getNumEdges(true) != rhs.getNumEdges(true) || this->getNumEdges(false) != rhs.getNumEdges(false))
		return false;
	//Now, we do a comparision between the in-bound nodes, which is a bit less messy. 
	std::map<std::string, int> lhsmap = this->countNodes(false);
	std::map<std::string, int> rhsmap = rhs.countNodes(false);
	for (std::map<std::string, int>::const_iterator lit = lhsmap.begin(); lit != lhsmap.end(); lit++){
		if (rhsmap.find((*lit).first) == rhsmap.end())
			return false;
		if (rhsmap[(*lit).first] != (*lit).second)
			return false;
	}
	//And the out-bound nodes
	lhsmap = this->countNodes(true);
	rhsmap = rhs.countNodes(true);
	for (std::map<std::string, int>::const_iterator lit = lhsmap.begin(); lit != lhsmap.end(); lit++){
		if (rhsmap.find((*lit).first) == rhsmap.end())
			return false;
		if (rhsmap[(*lit).first] != (*lit).second)
			return false;
	}

	return true;
}

//Tells us how many objects we have for a given role position
int Operational::getNumRoles(int pos) const{ 
	std::pair<std::multimap<int, Vertex*>::const_iterator, std::multimap<int, Vertex*>::const_iterator> range;
	range = this->roles.equal_range(pos);
	return (int)std::distance(range.first,range.second); 
}
///////////////////////////////////////////////////////////////////////////////
//Our content Classes
Content::Content(Vertex* v, bool dcs):deep_copy(dcs), external(uniformExtern), internal(uniformIntern), selector(randomSelection) {
	if (this->deep_copy) {
		this->vertex = new Vertex(v->getName());
	}
	else {
		this->vertex = v;
	}
	this->probability.push_back(0.0);//Always have a single probability
} 
Content::~Content() {
	//All of this is based off of DCS
	if (this->deep_copy) {
		delete this->vertex;
	}
	this->probability.clear();
	for (auto&& child : this->leaf_nodes) {
		delete child; //We want to at least clear out these. They should correctly reflect dcs as well
	}
	this->leaf_nodes.clear();//Clear out the space for good measure
}
Content::Content(const Content& rhs):deep_copy(rhs.deep_copy) { //This is technically the same as below. It's redundency may not be needed
	if (this->deep_copy) {
		this->vertex = new Vertex(rhs.getVertex()->getName());
	}
	else {
		this->vertex = rhs.getVertex();
	}
	for (int i = rhs.getNumContent() - 1; i >= 0; i--) { //Reverse to remove alot of resize calls
		this->addProbability(rhs.getProbability(i),i);
	}
	//Now, we do this for all children (same reverse logic)
	for (int i = rhs.getNumLeafs() -1 ; i >=0 ; i--) {
		this->addLeaf(i,rhs.getLeaf(i));
		this->selection[i] = rhs.getSelection(i);
		for (int j = rhs.getNumLeafProbability(i) -1 ; j >= 0; j--) { //We also do not do checks in the copy constructor because we assume they were done else where
			this->addLeafProbability(i, j, rhs.getLeafProbability(i, j));
			
		}
	}
	//Don't forget our selection
	//Finally, connect the sampling function
	this->internal = rhs.internal;
	this->external = rhs.external;
	this->selector = rhs.selector;
}
Content::Content(const Content& rhs, bool dcs):deep_copy(dcs) {
	if (this->deep_copy) {
		this->vertex = new Vertex(rhs.getVertex()->getName());
	}
	else {
		this->vertex = rhs.getVertex();
	}
	for (int i = rhs.getNumContent() - 1; i >= 0; i--) { //Reverse to remove alot of resize calls
		this->addProbability(rhs.getProbability(i), i);
	}
	//Now, we do this for all children (same reverse logic)
	for (int i = rhs.getNumLeafs() - 1; i >= 0; i--) {
		this->addLeaf(i, rhs.getLeaf(i));
		this->selection[i] = rhs.getSelection(i);
		for (int j = rhs.getNumLeafProbability(i) - 1; j >= 0; j--) { //We also do not do checks in the copy constructor because we assume they were done else where
			this->addLeafProbability(i, j, rhs.getLeafProbability(i, j));
		}
	}
	//Finally, connect the sampling function
	this->internal = rhs.internal;
	this->external = rhs.external;
	this->selector = rhs.selector;
}

void Content::addProbability(float value, int which) {
	if (which < 0) { //Not cool for this to be done
		return;
	}
	if (which == 0) {//always allowed the base probability
		if (this->probability.size() == 0)
			this->probability.push_back(value);
		else
			this->probability[which] = value;
		return;
	}
	//Basically, we have a constraint that says we can't have multiple probabilities in both the parent and leaf objects (symmetric constraint)
	//So, we have to perform a check on the children to make sure we don't
	bool asymmetric = true; //We first assume no symmetry
	for (int i = 0; asymmetric && i < this->getNumLeafs(); i++) {
		asymmetric = asymmetric && (this->getNumLeafProbability(i) < 1);
	}
	if (asymmetric) {
		//If we can expand, then we do
		if (which >= this->getNumContent()) {
			this->probability.resize(which+1, 0); //Which is zero based, but our size sure isn't
		}
		this->probability[which] = value;
	}
}
void Content::addLeaf(int which, Vertex* vert) { //Here, we say that if we have leafs, we plan on having multiple leaves
	if (which < 0) { //Not cool for this to be done
		return;
	}
	if (which >= this->getNumLeafs()) {
		//We have to expand it in that case
		this->leaf_nodes.resize(which + 1, NULL);//Don't have vertices to fill in
		this->selection.resize(which + 1, -1);//Add this as -1 because we have not made a selection yet
	}
	if (this->leaf_nodes.at(which) == NULL) {
		this->leaf_nodes[which] = new Content(vert, this->deep_copy);
	}
}
void Content::addLeafProbability(int which_leaf, int which_prob, float probability) {
	if (which_leaf < 0 || which_prob < 0 || which_leaf >= this->getNumLeafs()) {//There are many reasons that we would not want to set the probability
		return;
	}
	if (which_prob == 0 || this->getNumContent() < 2) { //This is our asymmetry check
		this->leaf_nodes[which_leaf]->addProbability(probability, which_prob); //We are okay to do this with the asymmerty check because a leaf node shouldn't have children
	}
}

//For our getters and setters, I'm going to try not to re-write a bunch of primitive code
int Content::getNumLeafProbability(int which) const {
	if (which < 0 || which > this->getNumLeafs()) {
		return 0; //Our probability is zero for everything we don't know about
	}
	return this->leaf_nodes[which]->getNumContent();
}

//Getters for all of our information
float Content::getProbability(int which) const{
	if (which < 0 || which > this->getNumContent()) {
		return 0.0f;
	}
	return this->probability[which];
}
int Content::getSelection(int which) const {
	if (this->selection.size() == 0)
		return -1;
	if (which < 0 || which > this->getNumLeafs()) { //Selection mirrors attributes
		return -1;
	}
	return this->selection[which];
}
Vertex* Content::getLeaf(int which) const {
	if (which < 0 || which > this->getNumLeafs()) {
		return NULL; //Our probability is zero for everything we don't know about
	}
	return this->leaf_nodes[which]->getVertex();
}
int Content::getLeaf(Vertex* v) const {
	if (v == NULL)
		return -1;
	for (int i = 0; i < this->getNumLeafs(); i++) {
		if (this->leaf_nodes[i]->hasVertex(v))
			return i;
	}
	return -1;
}
float Content::getLeafProbability(int which_leaf, int which_prob) const {
	if (which_leaf < 0 || which_leaf > this->getNumLeafs()) {
		return 0.0f; //Our probability is zero for everything we don't know about
	}
	return this->leaf_nodes[which_leaf]->getProbability(which_prob);
}
void Content::select() {
	int* selections = this->selector(this->leaf_nodes);
	for (int i = 0; i <(int)leaf_nodes.size(); i++) {
		this->selection[i] = selections[i];
	}
	delete[] selections;
}
//This changes the information in our set
void Content::normalizeSet() {
	//Here, we are going to assume that everything is the same (i.e, everything is raw counts or everything is frequency based)
	float base = this->getProbability(0);
	for (int i = 1; i < this->getNumContent(); i++) { //start at 1 because our base is special
		this->addProbability(this->getProbability(i) / base, i);
	}
}
void Content::normalizeLeafs(bool norm_sets_first) { //Our children are a bit more problamatic, but we again have to assume our base is the same as our children and possible their set members
	if (norm_sets_first) {//Then, we assume that we are not working with a relative leaf set, and fix that first
		for (int i = 0; i < this->getNumLeafs(); i++) {
			this->leaf_nodes.at(i)->normalizeSet();
		}
	}
	//Now, we normalize our children. We constrain the children to be the total whole of the parent (because the children make up the type and we need to figure down)
	float total = 0.0f;
	for (int i = 0; i < this->getNumLeafs(); i++) {
		total += this->getLeafProbability(i, 0);
	}
	if (total >= 0.0f) {
		return; //Nothing to do here
	}
	for (int i = 0; i < this->getNumLeafs(); i++) {
		this->addLeafProbability(i, 0, this->getLeafProbability(i,0) / total);
	}
} 
void Content::changeVertex(Vertex* vert) {
	if (this->deep_copy) {
		delete this->vertex; //Get rid of it
		this->vertex = new Vertex(vert->getName());
	}
	else {
		this->vertex = vert;
	}
}
//Subtraction of probability between two sets of content
Content operator -(const Content& lhs, const Content& rhs) {
	Content result(lhs, lhs.deep_copy && rhs.deep_copy);
	if (result.deep_copy) {
		//Hopefully they are all normalized or not normalized (no mix)
		if (lhs.getVertex()->getName() == rhs.getVertex()->getName()) {
			for (int i = 0; i < lhs.getNumContent() && rhs.getNumContent(); i++) { //Go as long as we have one
				if (lhs.getProbability(i) - rhs.getProbability(i) > 0)
					result.addProbability(lhs.getProbability(i) - rhs.getProbability(i), i);
				else
					result.addProbability(0.0, i);
			}
		}
	}
	else {
		if (lhs.getVertex() == rhs.getVertex()) {
			for (int i = 0; i < lhs.getNumContent() && rhs.getNumContent(); i++) { //Go as long as we have one
				if (lhs.getProbability(i) - rhs.getProbability(i) > 0)
					result.addProbability(lhs.getProbability(i) - rhs.getProbability(i), i);
				else
					result.addProbability(0.0, i);
			}
		}
	}
	//Now, we go for each child that is the same, which has deep copy checks so we should be okay
	for (int i = 0; i < lhs.getNumLeafs(); i++) {
		int rhs_i = rhs.getLeaf(lhs.getLeaf(i));
		if (rhs_i != -1) { //Then we have a match
			for (int j = 0; j < lhs.getNumLeafProbability(i) && j < rhs.getNumLeafProbability(j); j++) {
				if (lhs.getLeafProbability(i, j) - rhs.getLeafProbability(rhs_i, j) > 0)
					result.addLeafProbability(i, j, lhs.getLeafProbability(i, j) - rhs.getLeafProbability(rhs_i, j));
				else
					result.addLeafProbability(i, j, 0.0);
			}
		}
	}
	result.normalizeLeafs(false); //We re-normalize the leafs in this case
	return result;
}

bool Content::hasVertex(Vertex* v) const {//Determines if we have the vertex, which may be either the vertex or a child
	if (this->deep_copy) {
		if (this->getVertex()->getName() == v->getName())
			return true;
		for (int i = 0; i < this->getNumLeafs(); i++) {
			if (this->getLeaf(i)->getName() ==v->getName())
				return true;
		}
	}
	else {
		if (this->getVertex() == v) {
			return true;
		}//Check the children as well, either one would have to be the child
		for (int i = 0; i < this->getNumLeafs(); i++) {
			if (this->getLeaf(i) == v)
				return true;
		}
	}
	return false;
}
//Equality is designed mainly for find, and what we are trying to find is the vertex. 
//Because of how we treat parent/child relations, we also check to see if one is the
//parent of the other (we also be reflexive
bool Content::operator ==(const Content &rhs)const { 
	
	return this->hasVertex(rhs.getVertex()) || rhs.hasVertex(this->getVertex()); //If we cannot make a connection, then they aren't equal
}


float uniformExtern(const std::vector<int>& selection,const std::vector<float>& vals) {
	float prob = 1.0f;
	if (selection.size() == 0 && vals.size() > 0) {
		return vals[0];
	}
	for (int i = 0; i < (int)vals.size(); i++) {
		if (selection[i] != 0) {
			prob *= vals[i];
		}
		else {
			prob *= (1 - vals[i]);
		}
	}
	return prob; //Otherwise, return the last good one
}

float uniformIntern(const std::vector<Content*>& items, const std::vector<int>& selection) {
	float prob = 1.0f;
	for (int i = 0; i < (int)selection.size(); i++) {
		if (selection[i] != -1) { //-1 means it wasn't selected
			prob *= items[i]->getProbability(selection[i]);
		}
	}
	return prob; //Otherwise, return the last good one
}
int *randomSelection(const std::vector<Content*>& items) {
	int *data = new int[items.size()];
	for (int i = 0; i < (int)items.size(); i++) {
		data[i] = rand() % items[i]->getNumContent();
	}
	return data;
}