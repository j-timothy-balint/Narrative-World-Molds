#include "RuleSet.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include "FactorGraph.h"
#include <iostream>

#define EPSILON 0.00001 //We do 1e-5 because that's what we said in the paper (that no vertex from a rule-set that isn't required is above 1-1e-5). Basically a magic number


//Helper inline function to compare two sets that are known to be the same length
inline bool sameSet(const std::vector<Content*>& lhs, const std::vector<Content*>& rhs) {
	bool same = true;
	std::vector<bool> lb(lhs.size(), false);
	std::vector<bool> rb(rhs.size(), false);
	for (int i = 0; i < lhs.size() && same; i++) {
		for (int j = 0; j < rhs.size() && !lb[i]; j++) {
			if (!rb[j]) {
				if ((*lhs.at(i)) == (*rhs.at(j))) {
					rb[j] = true;
					lb[i] = true;
				}
			}
		}
		if (!lb[i])
			same = false;
	}
	return same;
}

RuleSet::RuleSet(const std::list<std::string>& csv_lines,bool dcs) :deep_copied_set(dcs) {
	//Because we are creating this one from a string, we have to create the vertices and factors
	std::vector<int> verts;//Gets the index of the vertices
	for (std::list<std::string>::const_iterator it = csv_lines.begin(); it != csv_lines.end(); it++) {
		unsigned int num_elements = (unsigned int)std::count((*it).begin(), (*it).end(), ',') + 1; //We add one for the last element
		size_t start = 0;
		Factor *fact = NULL;
		float support = 0;
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
						fact = new Factor(num_elements - 2, sub); //Subtract 2 to remove the factor and support
						factors.push_back(fact);
					}
				}
				else {
					//These are our vertices.
					//See if it exists
					int found_vert = this->getVertex(sub);
					if (found_vert == -1) {
						found_vert = this->getNumVertices();//Where we are will become our size
						Vertex * v = new Vertex(sub);
						this->addVertex(v);
					}
					verts.push_back(found_vert);
				}
				start = end + 1;//We skip the comma
			}
			else {
				//Here, we are at the last element, which is support
				support = (float)atof((*it).substr(start).c_str());
			}
		}
		if (fact->getNumComponents() == 1 && fact->getName() == "frequency"){ //Would like to not hard-code this, but am not sure if we can
			int v = verts.at(0); //Get the vertex
			//In this case, we append the frequency
			this->addFrequency(v, support + this->getFrequency(v));
		}
		else {
			//Now we determine whether to add it to the list or just change the support to the max support
			//For every vertex that matches factor
			bool found = false;
			for (unsigned int i = 0; i < this->ruleset.size() && !found; i++) {
				if (this->ruleset.at(i) == fact) {
					//Get the list of vertices at factor

					if (this->matchedFactor(i, verts)) { //If we have a match, then we are good and can do the right check
						this->probability.at(i) = support + this->probability.at(i);
						found = true;
					}
				}
			}
			if (!found) {
				this->ruleset.push_back(fact);
				//Here, we need to get the index of the vertex
				for (unsigned int i = 0; i < verts.size(); i++) {
					this->variables.insert(std::make_pair((int)this->ruleset.size() - 1, verts.at(i)));
				}
				this->probability[(int)this->ruleset.size() - 1] = support;
			}
		}
	}
}

//Copy Constructor for probabilities
RuleSet::RuleSet(const RuleSet &rhs, bool dcs):deep_copied_set(dcs) {
	//First set up the factors
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> rf = rhs.getAllFactors();
	for (std::list<Factor*>::const_iterator it = rf.first; it != rf.second; it++) {
		if (this->deep_copied_set) {
			this->factors.push_back(new Factor((*it)->getNumComponents(), (*it)->getName())); //For all of our things, we need to use these pointers instead, so search by factor name
		}
		else {
			this->factors.push_back((*it));
		}
	}
	for (int i = 0; i < rhs.getNumVertices(); i++) {
		Content *it = rhs.getContent(i);
		Content *new_content = new Content((*it), dcs);
		//Deep copying is done in addVertex
		this->addContent(new_content);
	}
	//This is where we have to use the new pointers from the left hand side for our pointer consistancy
	bool found = false;
	for (int i = 0; i < rhs.getNumRules(); i++) {
		//Get the rule factor for the right hand side
		Factor *fact = rhs.getFactor(i);
		if (this->deep_copied_set) {
			found = false;
			for (std::list<Factor*>::const_iterator it = this->factors.begin(); it != this->factors.end() && !found; it++) {
				if (*(*it) == *fact) {
					this->ruleset.push_back((*it));
					found = true;
				}
			}
		}
		else {
			this->ruleset.push_back(fact);
		}
		//Get their predicates
		std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> rrhs = rhs.getPredicate(i);
		//Here we can exploit the fact that our left hand side and right hand side should be equal in the vector positions for this copy no matter what
		//because it is a copy constructor
		for (std::multimap<int, int>::const_iterator it = rrhs.first; it != rrhs.second; it++) {
			this->variables.insert(std::make_pair(i, (*it).second));
		}
		//Finally, add in the probability
		this->probability[(int)this->ruleset.size() - 1] = rhs.getRuleProbability(i);
	}
}

RuleSet::~RuleSet() {
	if (this->deep_copied_set) {
		for (auto&& child : this->factors) { //efficient way to delete the objects in the pointer. Kind of gross (auto pointer) but okay
			delete child;
		}
	}
/*	for (auto&& child : this->vertices) { //Deleting the vertex will depend on if the child is deep copied
		delete child;
	}
	//clear out the actual pointers
	this->vertices.clear();
	this->factors.clear();
	this->ruleset.clear();
	this->variables.clear();
	this->probability.clear();*/
}

//MatchedFactor determines if a rule is the same based on the vertices
bool RuleSet::matchedFactor(int factor, const std::vector<int>& vars) {
	std::pair<std::multimap<int, int>::iterator, std::multimap<int, int>::iterator> range = this->variables.equal_range(factor);
	if (vars.size() != std::distance(range.first, range.second))
		return false;//If our ranges are off, then we are off
	for (unsigned int j = 0; j < vars.size(); j++) {
		//Deep copy does not matter here because we are pulling vertices from the same set
		bool matched = false;
		for (std::multimap<int, int>::iterator it = range.first; it != range.second; it++) {
			if ((*it).second == vars.at(j)) { //We have our match
					matched = true;
				}
			}
		if (!matched)
			return false;
	}
	return true;
}

//Helper function to do bayes rule on a conditional factor
//Our double factor appears as P(B|A) where B is our single factor and 
//P(A) is unchanging
//If it is P(A|B), and B is our changing factor, it doesn't actually change anything
//so we don't have to worry about it at this point
//Need to put update checks in this
void RuleSet::updateConditionalFactor(int double_factor, int vertex,float new_prob) {
	if(new_prob > EPSILON){
		//Get the conditional probability
		float prob = this->probability.at(double_factor);
			float b_prob = this->getFrequency(vertex);
			std::pair<std::multimap<int, int>::iterator, std::multimap<int, int>::iterator > range;
			//Now, lets get the unchanging probability of the single factor
			range = this->variables.equal_range(double_factor);
			if(std::distance(range.first,range.second) > 0 ) {
				range.first++;
				float a_prob = this->getFrequency((*range.first).second);
				float old_bayes_prob = (prob * a_prob) / b_prob;
				this->probability.at(double_factor) = (old_bayes_prob *b_prob) / new_prob; //If it's at zero we will have to figure it out
			}
		}
} 
//This updates a bi-conditional probabilty before setting the values
//double_factor_a is P(A|B) and double_factor_b is P(B|A)
void RuleSet::updateConditionalFactor(int double_factor_a,int double_factor_b, int a_single, int b_single, float a_new, float b_new) {
	float a_g_b = this->probability.at(double_factor_a);
	float b_g_a = this->probability.at(double_factor_b);
	float a = this->getFrequency(a_single);
	float b = this->getFrequency(b_single);
	float old_b_g_a = (a_g_b*b) / a;
	if (b_new > EPSILON) {
		this->probability.at(double_factor_a) = (old_b_g_a * a) / b_new;
	}
	else {
		this->probability.at(double_factor_a) = b_new;
	}
	
	float old_a_g_b = (b_g_a*a) / b;
	if (a_new > EPSILON) {
		this->probability.at(double_factor_b) = (old_a_g_b * b) / a_new;
	}
	else {
		this->probability.at(double_factor_b) = a_new;
	}
}
std::map<int, int> RuleSet::intersectionVertices(const RuleSet& rhs) const{
	std::map<int, int> result;
	std::vector<bool> found_vertices(rhs.getNumVertices(),false);//bitwise
	for (int l_v = 0; l_v < this->getNumVertices(); l_v++) {
		//For now, if we are the same vertex or a child, we are good
		//Basically, it comes down to what we consider equality in the vertex
		int skips = 0;
		int r_c = rhs.getContent(this->getContent(l_v), skips);
		//Makes sure we are not double counting
		while (r_c != -1 && found_vertices[r_c]) {
			skips++;
			r_c = rhs.getContent(this->getContent(l_v), skips);
		}
		if (r_c != -1) {
			found_vertices[r_c] = true;
			result[l_v] = r_c;
		}
	}
	return result;
}
std::map<int, int> RuleSet::intersectionRules(const RuleSet& rhs, const std::map<Factor*, Factor*>& fact_map, const std::map<int, int>& vert_map) const{//Look, its private and I can be responsible enough to run the verts and factors beforehand
	std::map<int, int> result;
	//But not responsible enough to check them
	if (fact_map.size() == 0 || vert_map.size() == 0)
		return result;
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> t_v_ids;
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> r_v_ids;
	for (std::map<Factor*, Factor*>::const_iterator fit = fact_map.begin(); fit != fact_map.end(); fit++) {
		//Get our possible common rules
		std::list<int> t_rules =this->getPredicates((*fit).first);
		std::list<int> r_rules = rhs.getPredicates((*fit).second);
		for (std::list<int>::const_iterator t_r = t_rules.begin(); t_r != t_rules.end(); t_r++) {
			bool found = false;
			std::vector<bool> bf(r_rules.size(), false);
			t_v_ids = this->getPredicate((*t_r));
			std::vector<Content*> t_verts;
			for (std::multimap<int, int>::const_iterator tv = t_v_ids.first; tv != t_v_ids.second; tv++) {
				t_verts.push_back(this->getContent((*tv).second));
			}
			int counter = 0;
			for (std::list<int>::const_iterator r_r = r_rules.begin(); r_r != r_rules.end() &&!found; r_r++) {
				if (!bf[counter]) {
					r_v_ids = rhs.getPredicate((*r_r));
					std::vector<Content*> r_verts;
					for (std::multimap<int, int>::const_iterator rv = r_v_ids.first; rv != r_v_ids.second; rv++) {
						Content *c = rhs.getContent((*rv).second);
						if (c == NULL) {
							Content *c = rhs.getContent((*rv).second);
						}
						r_verts.push_back(rhs.getContent((*rv).second));
					}
					//Now, we check to see if they are the same
					found = sameSet(t_verts, r_verts);
					//if they are, then we add it to the list
					if (found) {
						result[(*t_r)] = (*r_r);
						bf[counter] = true;
					}
				}
				counter++;
			}
		}
	}
	return result;
}
std::map<Factor*, Factor*> RuleSet::intersectionFactors(const RuleSet& rhs) const{
	std::map<Factor*, Factor*> result;
	bool deep = this->deep_copied_set || rhs.deep_copied_set;
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> l_factors = this->getAllFactors();
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> r_factors = rhs.getAllFactors();
	//First,we find the similar factors
	std::map<Factor*, std::pair<Factor*, Factor*> > factor_map;
	for (std::list<Factor*>::const_iterator f_l = l_factors.first; f_l != l_factors.second; f_l++) {
		Factor *r_fact = NULL;
		for (std::list<Factor*>::const_iterator rit = r_factors.first; rit != r_factors.second && r_fact == NULL; rit++) {
			if (deep) {
				if ((*(*rit)) == *(*f_l)) {//Go by what the equality operator says
					r_fact = (*rit);
				}
			}
			else {
				if ((*rit) == (*f_l)) {//pointers here
					r_fact = (*rit);
				}
			}
		}
		if (r_fact != NULL) {
			result[(*f_l)] = r_fact;
		}
	}
	return result;
}
//Normalizes everything by a constant factor
void RuleSet::normByValue(float denom) {
	this->normVertsByValue(denom);
	this->normRulesByValue(denom);
}
//Instead, we may have different factors (like, different counts) that we are using for our vertices
void RuleSet::normVertsByValue(float denom) {
	for (int i = 0; i < this->vertices.size(); i++) {
		this->vertices.at(i)->normalizeBase(denom);
	}
}
//And our ruleset
void RuleSet::normRulesByValue(float denom) {
	for (unsigned int i = 0; i < this->ruleset.size(); i++) {
		this->probability[i] /= denom;
	}

}

//Returns a pointer to the factor value from the position
Factor* RuleSet::getFactor(int factor_value) const {
	if (factor_value < 0 || factor_value >=(int)this->ruleset.size())
		return NULL;
	return this->ruleset.at(factor_value);
}
//Returns the pointer to the vertex at a given position
Vertex* RuleSet::getVertex(int pos) const {
	Content *c = this->getContent(pos);
	if(c != NULL)
		return c->getVertex();
	return NULL;//Otherwise, we give up
}
//Returns the index of the vertex at the given position
int RuleSet::getVertex(Vertex* vert) const {
	if (vert == NULL)
		return -1;
	if (this->deep_copied_set) { //Here, we are really banking on consistancy between deep_copied here and in the content
		return this->getVertex(vert->getName());
	}
	for (int i = 0; i < this->getNumVertices(); i++) {
		if (this->vertices.at(i)->getVertex() == vert)
				return i;
	}
	return -1;
}
//Basically the same as when we run by vertex, but no worry about dcs because we only have the name
int RuleSet::getVertex(const std::string& name) const {
	for (int i = 0; i < this->getNumVertices(); i++) {
		if (this->vertices.at(i)->getVertex()->getName() == name) {
			return i;
		}
	}
	return -1;
}
Content* RuleSet::getContent(int pos) const {
	if (pos < 0 || pos >= (int)this->vertices.size())
		return NULL;
	return this->vertices.at(pos);
}
Content* RuleSet::getContent(Vertex* vert,int skips) const {
	//Requires that we go through the list of content and 
	if (vert == NULL) {
		return NULL;
	}
	int counter = 0;
	for (std::vector<Content*>::const_iterator it = this->vertices.begin(); it != this->vertices.end(); it++) {
		if ((*it)->hasVertex(vert)) {
			if (counter == skips) {
				return (*it);
			}
			else {
				counter++;
			}
		}
	}
	return NULL;
}
//Gives us the position of the content based on the content
int RuleSet::getContent(Content* content,int skips) const{
	int counter = 0;
	for (int i = 0; i < this->getNumVertices(); i++) {
		if ((*content) == (*this->getContent(i)))
			if (counter == skips) {
				return i;
			}
			else {
				counter++;
			}
	}
	return -1;
}
//Returns the number of rules that match a given set
int RuleSet::getNumRules(Factor* fact) const {
	if (fact == NULL)
		return 0;
	//I have been really bad about doing deep copying of this data
	return (int)std::count(this->ruleset.begin(), this->ruleset.end(), fact);
}
//Returns the number of rules that match a given vertex pair
int RuleSet::getNumRules(const std::list<Vertex*>& vertices) const {
	int num_rules = 0;
	std::list<int> vert_num;
	//Do a vertex check here. If we don't have the vertices quit early
	for (std::list<Vertex*>::const_iterator v = vertices.begin(); v != vertices.end(); v++) { //This gets our vertices
		int found_value = this->getVertex((*v));
		if (found_value != -1) {
			vert_num.push_back(found_value);
		}
	}
	if (vert_num.size() != vertices.size())
		return num_rules;


	for (int i = 0; i < this->ruleset.size(); i++) {
		//Rule length check to save us some time
		if (this->getFactor(i)->getNumComponents() == vert_num.size()) {
			std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = this->getPredicate(i);

			bool found = true;
			for (std::multimap<int, int>::const_iterator it = range.first; found && it != range.second; it++) {
				if (std::find(vert_num.begin(), vert_num.end(), (*it).second) == vert_num.end()) {
					found = false;
				}
			}
			if (found) {
				num_rules++;
			}
		}
	}
	return num_rules;
}

//Loops are a special case in our system, and the Mold Generator handles it through expansion.
//Therefore, we need to know how many loops we aren't going to consider because of expansion.
int RuleSet::getNumLoops() const {
	int loops = 0;
	for (int i = 0; i < this->ruleset.size(); i++) {
		std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = this->variables.equal_range(i);
		bool loop = true;
		for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
			if ((*it).second != (*range.first).second)
				loop = false;
		}
		if (loop)
			loops++;
	}
	return loops;
}

//Determines if a one or two factor rule specifically exists in our system
//Used for TIV's in the generator as a slight way of speeding things up (instead of using getPredicate)
bool RuleSet::hasRule(Factor* f, Vertex* a, Vertex *b) const{
	std::list<Vertex*> v;
	v.push_back(a);
	v.push_back(b);
	if (this->getPredicate(f, v) != -1)
		return true;
	return false;

}
//Really quick helper function to determine if the vertex is in the ruleset
bool RuleSet::hasVertex(int rule_id, int vertex_id) const {
	if (rule_id < 0 || rule_id >= (int)this->ruleset.size() || vertex_id < 0)
		return false;
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = this->variables.equal_range(rule_id);
	for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {//basically, it has to be one of the predicates of the rule
		if ((*it).second == vertex_id)
			return true;
	}
	return false;
}

std::list<int> RuleSet::getPredicates(Factor* factor) const {
	std::list<int> result;
	for (int i = 0; i < (int)this->ruleset.size(); i++) {//We have a vector, so lets get those positions
		if ((*factor) == (*this->ruleset.at(i))) {
			result.push_back(i);
		}
	}
	return result;
}
std::list<int> RuleSet::getPredicates(Content* cont,int which) const {
	std::list<int> result;
	int var = this->getContent(cont);
	if (var == -1)
		return result;
	if (which < 0) { //in this case, we want undirected edges
		for (std::multimap<int, int>::const_iterator it = this->variables.begin(); it != this->variables.end(); it++) {
			if ((*it).second == var) {
				result.push_back((*it).first);
			}
		}
	}
	else {
		//And in this case, we have a direction to the edges
		int skips = 0;
		int cur_rule = -1;
		for (std::multimap<int, int>::const_iterator it = this->variables.begin(); it != this->variables.end(); it++) {
			if (cur_rule < (*it).first) {
				cur_rule = (*it).first;
				skips = 0;
			}
			if (skips == which) {
				if ((*it).second == var) {
					result.push_back((*it).first);
				}
			}
			skips++;
		}
	}
	return result;
}
//Returns the rule-predicates based on the factor. If pos = -1, we get all the rule-predicates, otherwise we are looking for a specific one in our ordering.
//If we do not find it or their is a problem, then we return a pair that is the ends of the multimap
std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator>  RuleSet::getPredicate(Factor* rule, int pos) const {
	if (pos < 0 || pos >=(int)this->ruleset.size()) {//No chance, so no point
		return std::make_pair(this->variables.end(), this->variables.end());//This is also what equal range returns when it doesn't find it
	}
	//Now, we see if we have it
	int skips = 0;
	for (int i = 0; i < (int)this->ruleset.size(); i++) {//We go through all rules instead of iterating as we are generally concerned with connecting the number to the predicate, because what we are trying to get is the predicate
		if (this->ruleset.at(i) == rule) {
			if (skips >= pos) {
				return this->getPredicate(i);
			}
			else {
				skips++;
			}
		}
	}
	//If we hit this point, we do not have it, and so return our falsehood
	return std::make_pair(this->variables.end(), this->variables.end());
}
//Gets all the predicates that match a given factor and vertex at a given position
//This returns the rules and other predicates at that position
//It's a bit sad that we gotta re-create the predicates, and we should think of a faster way to do this
std::multimap<int, int>  RuleSet::getAllPredicates(Factor* fact, Vertex* v, int pos) const {
	std::multimap<int, int> result;
	int vert_pos = this->getVertex(v);//Because getVertex is a deep copy, the vertices are deep-copied
	Factor* f = fact;
	if (this->deep_copied_set) {
		for (std::list<Factor*>::const_iterator it = this->factors.begin(); it != this->factors.end(); it++) {
			if ((*f) == *(*it)) {//de-reference so we can use value comparison instead of pointer comparison
				f = (*it);
			}
		}
	}
	//Now, we are set for deep-copying factors
	for (int i = 0; i < this->ruleset.size(); i++) {
		if (this->ruleset.at(i) == f) {
			std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range;
			range = this->variables.equal_range(i);
			std::multimap<int, int>::const_iterator it = range.first;
			advance(it, pos);
			if (it != range.second && (*it).second == vert_pos) {
				for (std::multimap<int, int>::const_iterator it2 = range.first; it2 != range.second; it2++) { //We don't know how many other predicates there are, so we go over all of them
					if(it != it2)
						result.insert(std::make_pair((*it2).first, (*it2).second));
				}
			}
		}
	}
	return result;
}
//Returns the rule-predicates based solely on a known rule (provided it exists)
std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator>  RuleSet::getPredicate(int factor_value) const {
	return this->variables.equal_range(factor_value);
}
//This returns the ruleset number based on a Factor and Vertex set
int RuleSet::getPredicate(Factor* fact, const std::list<Vertex*>& predicates) const {
	if (fact == NULL || predicates.size() == 0)
		return -1;//Default failure case
	//First, we loop through our factors and find a match
	for (int i = 0; i < (int)this->ruleset.size(); i++) {
		if (*(this->ruleset.at(i)) == *(fact)) { //Factor match, now lets test the vertices
			std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> verts = this->getPredicate(i);
			if (std::distance(verts.first, verts.second) == predicates.size()) { //Make sure they are the same size
				int matched_verts = 0;

				//Count the number of matches between the two 
				for (std::multimap<int, int>::const_iterator it = verts.first; it != verts.second; it++) {
					bool found = false;
					for (std::list<Vertex*>::const_iterator it2 = predicates.begin(); it2 != predicates.end() && !found; it2++) {
						if (this->deep_copied_set) {
							if (this->getVertex((*it).second)->getName() == (*it2)->getName()) {
								matched_verts++;
							}
						}
						else {
							if (this->getVertex((*it).second) == (*it2)) {
								matched_verts++;
							}
						}
					}
				}
				if (matched_verts == (int)predicates.size())
					return i;
			}
		}
	}
	return -1;
}
//Returns the rule position based on how many times we've seen this set of combinations
int RuleSet::getPredicate(const std::list<Vertex*>& predicates, int offset) const {
	int num_found = 0;
	for (int i = 0; i < (int)this->ruleset.size(); i++) {
		std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> verts = this->getPredicate(i);
		if (std::distance(verts.first, verts.second) == predicates.size()) { //Make sure they are the same size before we do a more expensive check
			int matched_verts = 0;
			//Count the number of matches between the two 
			for (std::multimap<int, int>::const_iterator it = verts.first; it != verts.second; it++) {
				bool found = false;
				for (std::list<Vertex*>::const_iterator it2 = predicates.begin(); it2 != predicates.end() && !found; it2++) {
					if (this->deep_copied_set) {
						if (this->getVertex((*it).second)->getName() == (*it2)->getName()) {
							matched_verts++;
						}
					}
					else {
						if (this->getVertex((*it).second) == (*it2)) {
							matched_verts++;
						}
					}
				}
			}
			if (matched_verts == (int)predicates.size()) { //If we matched all the vertices, then we have found a match (we have no factors to go on)
				if (num_found >= offset)
					return i;
				else
					num_found++;
			}
		}
	}
	return -1;
}
//Just like our above getting the predicates, we search based on factor
float RuleSet::getRuleProbability(Factor* rule, int pos) const{ //We were initially searching on factor rules and positions, but it fell out of favor for the exact rule
	if (pos < 0 || pos >=(int)this->probability.size()) {
		return 0.0;
	}
	int skips = 0;
	for (int i = 0; i < (int)this->ruleset.size(); i++) {
		if (this->ruleset.at(i) == rule) {
			if (pos >= skips) {
				return this->getRuleProbability(i);
			}
			else {
				skips++;
			}
		}
	}
	return 0.0;
}
//Our probability by the known ruleset. Our probability for failed value is 0.0
float RuleSet::getRuleProbability(int factor_value) const{
	std::map<int, float>::const_iterator it = this->probability.find(factor_value);
	if(it == this->probability.end())
		return 0.0; 
	return (*it).second;
}
//This is our special frequency probability
float RuleSet::getFrequency(int vertex,int which) const {
	if (vertex < 0 || vertex > this->vertices.size())
		return 0.0f;
	return this->vertices[vertex]->getProbability(which);
}
//Determines the maximum value in the probability of each rule-set
float RuleSet::getMaxProbability() const {
	float max = 0.0f;
	for (std::map<int, float>::const_iterator it = this->probability.begin(); it != this->probability.end(); it++) {
		if (max < (*it).second) {
			max = (*it).second;
		}
	}
	return max;
}
//Determines the maximum value in the frequency of each rule-set
float RuleSet::getMaxFrequency() const {
	float max = 0.0f;
	for (int i = 0; i < this->vertices.size();i++) { //The base probability is always greater than it's relative children, so we only need to check the base probability
		float prob = this->vertices.at(i)->getProbability(0);
		if (max < prob) {
			max = prob;
		}
	}
	return max;
}
RuleSet* RuleSet::intersection(const RuleSet& lhs, const RuleSet& rhs) const{
	bool deep = lhs.deep_copied_set || rhs.deep_copied_set;
	RuleSet* result = new RuleSet(deep);//The intersection of deep_copied is the one that is deep_copied
	std::map<Factor*, Factor*> f_map = lhs.intersectionFactors(rhs);
	std::map<int, int>         v_map = lhs.intersectionVertices(rhs);
	std::map<int, int>         r_map = lhs.intersectionRules(rhs, f_map, v_map);

	//Now, we build up what we have in common
	for (std::map<Factor*, Factor*>::const_iterator it = f_map.begin(); it != f_map.end(); it++) {
		result->addFactor((*it).first);
	}
	//We do second because we are subtracting out rhs in our operator -
	std::map<int, int> result_map;
	for (std::map<int, int>::const_iterator it = v_map.begin(); it != v_map.end(); it++) {
		result_map[(*it).second] = result->addContent(rhs.getContent((*it).second));
	}
	
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range; 
	for (std::map<int, int>::const_iterator it = r_map.begin(); it != r_map.end(); it++) {
		range = rhs.getPredicate((*it).second);
		std::list<int> vertices;
		for (std::multimap<int, int>::const_iterator v = range.first; v != range.second; v++) {
			vertices.push_back(result_map[(*v).second]);
		}
		result->addRule(rhs.getFactor((*it).second), vertices, rhs.getRuleProbability((*it).second));
	}
	return result;
}
//This does a probability subtraction on the intersection between LHS and RHS
//Note, the output of this is not a clean ruleset, and the ruleset should be probability cleaned after subtraction
RuleSet operator -(const RuleSet& lhs, const RuleSet& rhs) {
	RuleSet ans = RuleSet(lhs,lhs.deep_copied_set ||rhs.deep_copied_set); //We start with lhs, copied
	RuleSet *intersection = ans.intersection(lhs, rhs);//Get the intersection
	//Make a map between all common factors (because we cannot garentee sameness)
	//subtract out all common vertices
	for (int i = 0; i < intersection->getNumVertices(); i++) {
		Content *intc = intersection->getContent(i);
		Content *ansc = ans.getContent(ans.getContent(intc));
		if (ansc != NULL) { //Should never equal null, but just to be safe
			Content c = (*ansc - *intc);
			for (int j = 0; j < ansc->getNumContent(); j++) {
				ansc->addProbability(c.getProbability(j), j);
			}
			for (int j = 0; j < ansc->getNumLeafs(); j++) {
				ansc->addLeafProbability(j, 0, c.getLeafProbability(j, 0));
			}
		}
	}
	//Subtract out all common rules
	for (int i = 0; i < intersection->getNumRules(); i++) {
		std::list<Vertex*> v_list;
		std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = intersection->getPredicate(i);
		for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
			v_list.push_back(intersection->getVertex((*it).second));
		}
		int place = ans.getPredicate(intersection->getFactor(i), v_list);
		if (place != -1) {//again, shouldn't happen but we just want to make sure
			if (ans.getRuleProbability(place) - intersection->getRuleProbability(i) > 0.0)
				ans.setProbability(place, ans.getRuleProbability(place) - intersection->getRuleProbability(i));
			else
				ans.setProbability(place, 0.0f);
		}
	}
	//cleanup
	delete intersection;
	return ans;
}
//Performs an addition of the two rulesets. In essence, this is the addition of all probabilities
//betwen the two sets,
RuleSet RuleSet::operator +(const RuleSet& rhs) {
	bool deep = this->deep_copied_set || rhs.deep_copied_set;
	RuleSet result;
	result.deep_copied_set = deep;
	std::map<Factor*, Factor*> f_map = this->intersectionFactors(rhs);
	std::map<int, int>         v_map = this->intersectionVertices(rhs);
	std::map<int, int>         r_map = this->intersectionRules(rhs, f_map, v_map);
	std::map<Factor*, Factor*> new_f_map; //For knowing which new factors we have
	//Two maps, one for r, and one for the ones only in l
	std::map<int, int>         new_r_map; 
	std::map<int, int>         new_l_map;
	std::map<int, int>     new_rules_map;

	//Add in common factors
	for (std::map<Factor*, Factor*>::const_iterator fact = f_map.begin(); fact != f_map.end(); fact++) {
		new_f_map[(*fact).second] = result.addFactor((*fact).first);//rhs
	}
	//Because factors a list, they become a bit more of a pain than the other ones
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> all_factors = this->getAllFactors();
	for (std::list<Factor*>::const_iterator it = all_factors.first; it != all_factors.second; it++) {
		if (f_map.find((*it)) == f_map.end()) {//Not part of our intersection 
			new_f_map[(*it)] = result.addFactor((*it));

		}
	}
	//And of course, the right hand side
	all_factors = rhs.getAllFactors();
	for (std::list<Factor*>::const_iterator it = all_factors.first; it != all_factors.second; it++) {
		//not as clean a search
		bool found = false;
		for (std::map<Factor*, Factor*>::const_iterator mit = f_map.begin(); mit != f_map.end() && !found; mit++) {
			if ((*mit).second == (*it)) {
				found = true;
			}
		}
		if (!found) {
			new_f_map[(*it)] = result.addFactor((*it));
		}
	}
	//Now, we add in the additional vertices (right first, because we can more easily look up the left ones
	for (int i = 0; i < rhs.getNumVertices(); i++) {
		new_r_map[i] = result.addContent(rhs.getContent(i));
	}
	for (int i = 0; i < this->getNumVertices(); i++) {
		if (v_map.find(i) == v_map.end()) {
			new_l_map[i] = result.addContent(this->getContent(i));
		}
		else {
			if (this->getFrequency(i) > result.getFrequency(new_r_map[v_map[i]])) {
				result.addFrequency(new_r_map[v_map[i]], this->getFrequency(i));
			}
			new_l_map[i] = new_r_map[v_map[i]]; //Now we know where it is
		}
	}
	//Now, we add in the rules
	for (int i = 0; i < rhs.getNumRules(); i++) {
		std::list<int> vertices;
		std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = rhs.getPredicate(i);
		for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
			vertices.push_back(new_r_map[(*it).second]);
		}
		result.addRule(new_f_map[rhs.getFactor(i)], vertices, rhs.getRuleProbability(i));
	}
	//And the rules from the left hand side 
	for (int i = 0; i < this->getNumRules(); i++) {
		if (r_map.find(i) == r_map.end()) { //Not a common rule
			std::list<int> vertices;
			std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = this->getPredicate(i);
			for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
				vertices.push_back(new_l_map[(*it).second]);
			}
			result.addRule(this->getFactor(i), vertices, this->getRuleProbability(i));
		}
		else {//Is a common rule, so we #MAX our ruleset probability
			float p = this->getRuleProbability(i);
			if (rhs.getRuleProbability(r_map[i]) > p) {
				p = rhs.getRuleProbability(r_map[i]);
			}
			result.setProbability(r_map[i],p); //We load the rhs into it, so where-ever we are, we are
		}
	}
	return *(new RuleSet(result,result.deep_copied_set));
}
//Determines if two rulesets are equal to each other. Since Factors and Vertices can be in different order, we have 
//to check each and every one of them. This is very much eqivalent to the subtraction of probabilities, and improvements
//in that should be mirrored in this one
bool RuleSet::operator ==(const RuleSet &rhs)const {
	if (this->factors.size() != rhs.factors.size()) //First, we make sure we have the right sizes
		return false;

	if (this->vertices.size() != rhs.vertices.size()) //First, we make sure we have the right sizes
		return false;

	if (this->ruleset.size() != rhs.ruleset.size()) //First, we make sure we have the right sizes
		return false;

	//Then, we make sure the factors and vertices inside them are the same
	//Make a map between all common factors
	std::map<Factor*, Factor*> fcon;
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> lhsf = this->getAllFactors();
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> rhsf = rhs.getAllFactors();
	for (std::list<Factor*>::const_iterator lit = lhsf.first; lit != lhsf.second; lit++) {
		for (std::list<Factor*>::const_iterator rit = rhsf.first; rit != rhsf.second; rit++) {
			if (*(*lit) == *(*rit)) {
				fcon[(*rit)] = (*lit);
			}
		}
	}
	if (fcon.size() != this->factors.size())
		return false; //Then we didn't find one
	//Also make a map between all common vertices
	std::map<Vertex*, Vertex*> vcon;
	for (int i = 0; i < this->getNumVertices(); i++) {
		for (int j = 0; j < rhs.getNumVertices(); j++) {
			if (this->getVertex(i)->getName() == rhs.getVertex(j)->getName() && fabsf(this->getFrequency(i) - rhs.getFrequency(j)) < EPSILON) {
				vcon[rhs.getVertex(j)] = this->getVertex(i);
			}
		}
	}
	if (vcon.size() != this->vertices.size())
		return false; //Again, didn't find one
	//Then we check their rulesets
	std::list<Vertex*> verts;
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> rp;
	Factor* lf;
	for (int i = 0; i < rhs.getNumRules(); i++) {
		verts.clear();//Remove the pointers
		bool no_match = false;
		rp = rhs.getPredicate(i);
		for (std::multimap<int, int>::const_iterator it = rp.first; it != rp.second && !no_match; it++) {
			if (vcon.find(rhs.getVertex((*it).second)) != vcon.end()) //Make sure we have it
				verts.push_back(vcon[rhs.getVertex((*it).second)]);
			else {
				//Then it's definitly not a good one
				no_match = true;
			}
		}
		if (no_match)
			return false;
		else {
			lf = fcon[rhs.getFactor(i)];
			int connection = this->getPredicate(lf, verts);
			//Connection won't equal -1 at this point
			if (abs(this->getRuleProbability(connection) - rhs.getRuleProbability(i)) > EPSILON)
				return false;
		}
	}
	return true;
}
//Adds a rule to our total rule-set
bool RuleSet::addRule(Factor* fact, const std::list<Vertex*>& verts,float probability) {
	//Now, here we cannot assume that we are doing pointer comparisions. Therefore, we have to find the pointers in our rule-set here
	//if (this->hasRule(fact, verts.front(), verts.back()) != -1)
	//	return false; //We have it already
	//First, make sure the factor exists in our set. 
	bool found = false;
	for (std::list<Factor*>::const_iterator it = this->factors.begin(); it != this->factors.end(); it++) {
		if (this->deep_copied_set) {
			if (*(*it) == *(fact)) {
				found = true;
				fact = (*it);
			}
		}
		else {
			if((*it) == fact)
				found = true;
		}
	}if (!found) {
		if(this->deep_copied_set)
			fact = new Factor(fact->getNumComponents(), fact->getName());
		this->factors.push_back(fact);
	}
	int pos = (int)this->ruleset.size(); //This is our position
	this->ruleset.push_back(fact);
	//Now, find each of the vertices (if they exist) and add them to the multimap
	for (std::list<Vertex*>::const_iterator it = verts.begin(); it != verts.end(); it++) {
		Vertex* found_vertex;
		int found_number = this->getVertex((*it));
		if (found_number == -1) {
			//If it doesn't exist, we need to add it to the list of vertices
			if (this->deep_copied_set) {
				found_vertex = new Vertex((*it)->getName());
			}
			else {
				found_vertex = (*it);
			}
			Content* content = new Content(found_vertex,this->deep_copied_set);
			found_number = (int)this->vertices.size();
			this->vertices.push_back(content);
		}
		this->variables.insert(std::make_pair(pos, found_number));
	}

	//Finally, add in the probability
	this->probability[pos] = probability;
	return true;
}

bool RuleSet::addRule(Factor* fact, const std::list<int>& verts, float probability) {
	//Now, here we cannot assume that we are doing pointer comparisions. Therefore, we have to find the pointers in our rule-set here

	//First, make sure the factor exists in our set. 
	bool found = false;
	for (std::list<Factor*>::const_iterator it = this->factors.begin(); it != this->factors.end(); it++) {
		if (this->deep_copied_set) {
			if (*(*it) == *(fact)) {
				found = true;
				fact = (*it);
			}
		}
		else {
			if ((*it) == fact)
				found = true;
		}
	}if (!found) {
		if (this->deep_copied_set)
			fact = new Factor(fact->getNumComponents(), fact->getName());
		this->factors.push_back(fact);
	}
	int pos = (int)this->ruleset.size(); //This is our position
	this->ruleset.push_back(fact);
	//An important thing to remember here is that we have to know the vertices at this point and have already added them in
	for (std::list<int>::const_iterator it = verts.begin(); it != verts.end(); it++) {
		if ((*it) < 0 || (*it) >= (int)this->vertices.size()) //Shouldn't add it if it has no chance of existing
			return false;
		this->variables.insert(std::make_pair(pos, (*it)));
	}

	//Finally, add in the probability
	this->probability[pos] = probability;
	return true;
}
//We run through all the conditional probabilities and update them with the new frequency
bool RuleSet::updateFrequency(int vertex, float new_prob) {
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range;
	for (int i = 0; i < this->ruleset.size(); i++) {
		if (this->ruleset.at(i)->getExistance()) {//Only existance terms are changed by frequency. Seperation of content and structure
			range = this->variables.equal_range(i);
			if (std::distance(range.first, range.second) > 0) {
				if ((*range.first).second == vertex) { //We only care about the conditional of the existance
					this->updateConditionalFactor(i, vertex, new_prob);
				}
			}
		}
	}
	this->addFrequency(vertex, new_prob);
	return true;
}
//Removes the numbered system from the list
bool RuleSet::removeRule(int factor,bool cleanup) {
	if (factor < 0 || factor > (int)this->ruleset.size()) { //cant remove if we don't have anything to remove
		return false;
	}
	//Get the factor we are removing
	Factor *fact = this->ruleset[factor];
	this->ruleset.erase(this->ruleset.begin() + factor);
	this->probability.erase(this->probability.find(factor));
	//Finally, remove the variables
	this->variables.erase(factor);
	//The last thing we have to do for removal is move all the previous values over one, which will be an expensive operation
	for (int i = factor + 1; i <= this->ruleset.size(); i++) { //It's equal to here because ruleset has been changed already
		std::pair<std::multimap<int, int>::iterator, std::multimap<int, int>::iterator> range = this->variables.equal_range(i);
		while (range.first != range.second) {
			int new_val = (*range.first).first - 1;
			int new_vert = (*range.first).second;//This now pushes over our entire multi-map
			range.first = this->variables.erase(range.first);
			this->variables.insert(std::make_pair(new_val, new_vert));
		}
		//Need to move all the probabilities as well
		std::map<int, float>::iterator prob_it = this->probability.find(i);
		this->probability[i - 1] = (*prob_it).second;
		this->probability.erase(prob_it);
	}
	if (cleanup)
		this->cleanRuleSet();
	return true;
}

//Cleans up any vertices and factors that are not present in the ruleset
void RuleSet::cleanRuleSet(bool freq_based) {
	//First, we remove any extranious factors
	std::list<Factor*>::iterator it = this->factors.begin();
	while(it != this->factors.end()) {
		bool found = false;
		for (int i = 0; i < (int)this->ruleset.size() && !found; i++) {
			if (this->ruleset.at(i) == (*it)) {
				found = true;
			}
		}
		if (!found) {
			Factor *f = (*it);
			it = this->factors.erase(it);
			if (this->deep_copied_set)
				delete f;
		}
		else {
			it++;
		}
	}
	//Now, we remove any vertex that no longer exists in the sytem
	int vert = 0;
	while (vert < this->vertices.size()) {
		bool found = false;
		if(freq_based){
			if (this->getFrequency(vert) > EPSILON) {
				found = true;
			}
		}
		else { //Otherwise, we do it based on the connections
			for (std::multimap<int, int>::iterator it = this->variables.begin(); it != this->variables.end() && !found; it++) {
				if ((*it).second == vert) {
					found = true;
				}
			}
		}
		if (!found) {
			//We remove the vertex
			Content* v = this->vertices.at(vert);
			this->vertices.erase(this->vertices.begin() + vert);
			//Now, we go through the variable list and change the # of the vertices
			for (int i = vert; i <= this->vertices.size(); i++) {
				for (std::multimap<int, int>::iterator it = this->variables.begin(); it != this->variables.end(); it++) {
					if (i == (*it).second)
						(*it).second = i - 1;
				}
			}
			delete v;
		}
		else {
			vert++;
		}
	}
}
//Cleans up our ruleset by removing any unlikely probabilities from our system
void RuleSet::cleanRuleSetProb(bool freq_based) {
	//First, we go through all of our vertices frequency, setting the probability of each connecting factor to zero when
	//we are below that 
	for (int i = 0; i < this->vertices.size(); i++) {
			if (this->getFrequency(i) < EPSILON) {
				//Get all rules that are connected to this vertex and set their probabilities to zero
				for (int j = 0; j < this->ruleset.size(); j++) {
					std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range;
					range = this->variables.equal_range(j);
					bool found = false;
					for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
						if ((*it).second == i) {
							found = true;
						}
					}
					if (found) {
						this->probability[j] = 0.0f;
					}
				}
			}
	}
	//We first remove all rules that are less than our epsilon. This removes all those that we wanted to remove
	int i = 0;
	while (i < this->ruleset.size()) {
		if (this->getRuleProbability(i) < EPSILON) {
			/*std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = this->getPredicate(i);
			std::cout << "Removing " << this->getFactor(i)->getName() << ":";
			std::cout << this->getVertex(range.first->second)->getName() << " ";
			range.first++;
			std::cout << this->getVertex(range.first->second)->getName() << std::endl;*/
			this->removeRule(i, false);//Hold cleanup until the end
		}
		else {
			i++;
		}
	}
	this->cleanRuleSet(freq_based);//Clean ruleset from that, which removes the vertices as well!
}
//Adds a vertex to the set and returns that position that was just added to us
int RuleSet::addVertex(Vertex* vert) {
	Content *content = new Content(vert,this->deep_copied_set); //All the vertex stuff is done in Content
	this->vertices.push_back(content);
	return (int)this->vertices.size() - 1;
}
//This does a copy of all of our content
int RuleSet::addContent(Content* cont) {
	Content *content = new Content((*cont), this->deep_copied_set);
	this->vertices.push_back(content);
	return (int)this->vertices.size() - 1;
}
//Adds the factor to the set and returns the factor (more useful return when things are deepcopied
Factor* RuleSet::addFactor(Factor* fact) { 
	if (!this->deep_copied_set) {
		factors.push_back(fact);
		return fact;
	}
	Factor* f = new Factor(fact->getNumComponents(), fact->getName(), fact->getExistance());
	factors.push_back(f);
	return f;
}
Factor* RuleSet::replaceFactor(Factor* of, Factor* nf) {
	std::replace(this->factors.begin(), this->factors.end(), of, nf);
	//At some point, we need to do factor number check and connect those rules to the head
	return nf;
}
//This has much less of the restrictions than the other one
int RuleSet::replaceVertex(Vertex* ov, Vertex* nv) {
	//Here, we cannot care about deep copied sets and are prolly trying to change it from one to the other
	int old_pos = this->getVertex(ov);
	if (old_pos != -1) {
		this->vertices[old_pos]->changeVertex(nv);
	}
	return old_pos;
}
int RuleSet::replaceContent(int which, Content* nv) {
	if (which < 0 || which >(int)this->vertices.size())
		return -1;
	Content *old = this->vertices.at(which);
	if (this->deep_copied_set) {
		Content *c = new Content(*nv, this->deep_copied_set);
		this->vertices[which] = c;
	}
	this->vertices[which] = nv;
	delete old;//Deletes based on if it is a deep or shallow copy, which is what we want
	return which;
}
//Adds the frequency. which is meant for the distribution
void RuleSet::addFrequency(int vert, float prob, int which) {
	if (vert < 0 || vert >= this->getNumVertices())
		return;
	this->vertices[vert]->addProbability(prob, which);
}
/**************************************************************************************************************************************************************************************************************************************/
/* END RULE SET DEFINITION */
/**************************************************************************************************************************************************************************************************************************************/
//Finally, other functions that are important to parse the factor-graph
std::map<std::string, RuleSet*> parseCSVRuleSet(const std::string & file_name,bool individual_max) {
	std::ifstream myfile;
	std::string line;
	std::string room_name;
	std::list<std::string> lines;
	std::map<std::string, RuleSet*> graphs;
	float total_rooms;
	myfile.open(file_name);
	if (myfile.is_open()) {
		while (getline(myfile, line)) {
			//Here, we get the name, and then keep adding lines to our thing until we hit another one
			int num_elements = (int)std::count(line.begin(), line.end(), ',');
			if (num_elements < 2) { //In this case, we have a room
				if (!lines.empty()) { //We had a room we need to construct
					graphs[room_name] = new RuleSet(lines);
					if (!individual_max) {
						graphs[room_name]->normByValue(total_rooms);
					}
					else {
						graphs[room_name]->normVertsByValue(graphs[room_name]->getMaxFrequency());
						//graphs[room_name]->normRulesByValue(graphs[room_name]->getMaxProbability());
					}
					lines.clear();
				}
				if(line.find(",") != std::string::npos){ //In this case, we have the total rooms
				room_name = line.substr(0, line.find(','));
					total_rooms = (float)atof(line.substr(line.find(',') + 1).c_str());
				}
				else {
					room_name = line; //Otherwise, our room name is our line
				}
			}
			else {
				lines.push_back(line);
			}
		}
		myfile.close();
		graphs[room_name] = new RuleSet(lines);
		if (!individual_max) {
			graphs[room_name]->normByValue(total_rooms);
		}
		else {
			graphs[room_name]->normVertsByValue(graphs[room_name]->getMaxFrequency());
			graphs[room_name]->normRulesByValue(graphs[room_name]->getMaxProbability());
		}
	}
	return graphs;
}

//Calculates the difference between two factor sets. We will make the assumption that the factor sets are equal
//With this helper function, we assume that rhs's prob will always be less than lhs. Not a good general assumption,
//but for what it is used for, it is fine.
float calcDifference(RuleSet* lhs, RuleSet* rhs) {
	float total_diff = 0.0;
	int total_rules = lhs->getNumRules();
	for (int i = 0; i < total_rules; i++) {
		total_diff += lhs->getRuleProbability(i) - rhs->getRuleProbability(i);
	}
	return total_diff;
}

//Determines the ruleSet that has the highest change with tset.
RuleSet* getMaxSupriseRuleSet(const std::list<RuleSet*>& sets,RuleSet* tset) {
	RuleSet *max = NULL;
	float pdiff = 0.0f;
	for (std::list<RuleSet*>::const_iterator it = sets.begin(); it != sets.end(); it++) {
		if((*it) != tset){
			RuleSet res = (RuleSet)(*tset - *(*it));
			float new_diff = calcDifference(tset,&res);
			if (new_diff > pdiff) {
				max = (*it);
				pdiff = new_diff;
			}
		}
	}
	return max;
}
//Converts a rule-set into a json string of {vertices:[],rules:[],probability:[]}
std::string convertToJson(RuleSet* set) {
	std::stringstream result;
	//Vertices are changed to content
	result << "{\n"<<"\"vertices\":[\n";
	for (int i = 0; i < set->getNumVertices(); i++) {
		Content *vert = set->getContent(i);
		float prob = set->getFrequency(i);
		result << "{\"id\":" << "\"" << i << "\"," << "\"name\":" << "\"" << vert->getVertex()->getName() << "\"";
		result << ",\"Leaves\":[";
		for (int j = 0; j < vert->getNumLeafs(); j++) {
			result << "{"
				<<"\"name\":"<< "\"" << vert->getLeaf(j)->getName() << "\","
				<< "\"frequency\":" << vert->getLeafProbability(j, 0)<<" "
				<< "}";
			if (j < vert->getNumLeafs() - 1) {
				result << ",";
			}
		}
		result<< "]";
		result << ",\"frequency\":"<<prob<<"}";
		if (i < set->getNumVertices() - 1) {
			result << ",\n";
		}
		else {
			result << "\n";
		}
	}
	result << "],\n" << "\"rules\":[\n";
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range;
	for (int i = 0; i < set->getNumRules(); i++) {
		range = set->getPredicate(i);
		int source = -1;
		int target = -1;
		for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
			if (source == -1)
				source = (*it).second;
			else
				target = (*it).second;
		}
		if (source != -1 && target != -1) {
			result << "{\"id\":" << "\"" << i << "\"," << "\"source\":" << "\"" << source << "\"";
			result << ",\"target\":" << "\"" << target << "\"" << ",\"factor\":" << "\"" << set->getFactor(i)->getName() << "\"" << "}";
			if (i < set->getNumRules() - 1) {
				result << ",\n";
			}
			else {
				result << "\n";
			}
		}
	}
	result << "],\n" << "\"probability\":[\n";
	for (int i = 0; i < set->getNumRules(); i++) {
		result << "{\"id\":" << "\"" << i << "\"," << "\"value\":" << set->getRuleProbability(i) << "}";
		if (i < set->getNumRules() - 1) {
			result << ",\n";
		}
		else {
			result << "\n";
		}
	}
	result << "]}";
	return result.str();
}