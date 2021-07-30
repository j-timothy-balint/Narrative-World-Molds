#include "KermaniSceneSynth.h"
#include <cstdlib> //For rand()
#include <iostream>
#include "FactorGraph.h"
#ifndef EPSILON
#define EPSILON 0.00001 //We do 1e-5 because that's what we said in the paper (that no vertex from a rule-set that isn't required is above 1-1e-5). Basically a magic number
#endif // !EPSILON

//This basic Kermani function assumes the leaves are all content to choose from
int* KermaniContentSelection(const std::vector<Content*>& items) {
	int *selection = new int[(int)items.size()];
	for (int i = 0; i < (int)items.size(); i++) {
		selection[i] = -1;
	}
	selection[rand() % items.size()] = 0;
	return selection;
}

float KermaniExternalFunction(const std::vector<int>& selection,const std::vector<float>& items) { //Everything should be the first probability for our content stuff
	return items[0];
}

int sample(int num_objects, int desired_objects) {
	//Samples based on the number of desired objects
	float ex1 = expf((float)(desired_objects - num_objects));
	float ex2 = expf((float)(num_objects - desired_objects));
	float d1 = ((float)rand()/(float)RAND_MAX)*(ex1/(ex1+1.0f));
	float d2 = ((float)rand() / (float)RAND_MAX)*(ex2 / (ex2 + 1.0f));
	float d3 = ((float)rand() / (float)RAND_MAX )* 0.3f;
	if (d1 > d2 && d1 > d3)
		return 0; //zero is add
	if (d2 > d1 && d2 > d3)
		return 1;//one is remove
	return 2;
}

void SamplingConstraint::propose(int desired_objects) {
	//We have a weird proposing function that attempts to bias proposal selection because Kermani et al. say that they do that
	int ptype =  sample(this->calcVerts(), desired_objects);
	if (ptype == 0) { 
		int f1 = rand() % this->set->getNumRules();
		this->addProposal(f1);
	}
	else if(ptype == 1){ 
		int f1 = rand() % this->set->getNumRules();
		this->removeProposal(f1);
	}
	else {
		int f1 = rand() % this->set->getNumRules();
		int f2 = rand() % this->set->getNumRules();
		if (f1 != f2) {
			if ((rules[f1] && !rules[f2]) || (!rules[f1] && rules[f2])) {
				if (rules[f1]) {
					this->removeProposal(f2);
					this->addProposal(f1);
				}
				else {
					this->removeProposal(f1);
					this->addProposal(f2);
				}
			}
		}
	}
}
int SamplingConstraint::calcVerts() {
	//Step function from Merell et al.
	int verts = 0;
	for (int i = 0; i < this->set->getNumVertices(); i++) {
		if (this->vertices[i])
			verts++;
	}
	return verts;
}
float SamplingConstraint::cost_function(int desired_objects) {// Remember, in this, the factor graph should be normalized in some respect
	if (this->vertices.size() == 0 || this->rules.size() == 0) {
		return 0.0; //Bad for it to be empty!
	}
	float total_prob = 1.0;
	if (this->calcVerts() == 0)
		return 0.0;
	
	float positive_prob = 1.0f;
	float negitive_prob = 1.0f;
	//They are using the law of total probability on noisy data. That makes me really uncomfortable, especially knowing the graphs that are generated
	for (int i = 0; i < this->set->getNumVertices(); i++) {
		float value = this->set->getContent(i)->calculateExternal();
		if (this->vertices[i]) {
			positive_prob *= value; //Our yes/no is now down in the external function
		}
		else {
			negitive_prob *= (1.0f - value); //This really biases towards only the strongest motif, and cuts down on variability
		}
	}
	total_prob = positive_prob *negitive_prob;
	/*for (int i = 0; i < this->set->getNumRules(); i++) {
		//We don't have single factors, so ignore them
		//std::cout <<i<<":"<< total_prob << std::endl;
		if (this->set->getFactor(i)->getNumComponents() > 1) {
			if (this->rules[i]) {//Has A and Has B
			
				total_prob *= this->set->getRuleProbability(i);
			//TBH, the numbers that they are getting for their total prob do not make sense. They assume things they shouldn't 
			//and switch between if these are joint probabilities or conditional probabilites. It's just bad
			}
			else {//can cause a float underflow for large motifs
				std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range;
				range = set->getPredicate(i);
				int u = (*range.first).second;
				range.first++;
				int v = (*range.first).second;
				total_prob *= (1.0f - this->set->getRuleProbability(i));
			}
		}
	}*/
	return total_prob; //May incorporate Beta into this
}

bool SamplingConstraint::accept(float new_cost, float old_cost) {
	if (old_cost == 0)//Explore until we have something to score
		return true;
	float ratio = new_cost / old_cost;
	if (ratio >= 1.0f) {
		return true;
	}
	else {
		float accept_prob =this->beta * (float)rand() / (float)(RAND_MAX);
		//if (ratio > accept_prob)
		//	std::cout << ratio <<":"<<accept_prob<<std::endl;
		return ratio > accept_prob;
	}
}


void SamplingConstraint::addProposal(int pos) {
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = this->set->getPredicate(pos);
	std::list<int> verts; //Note, we don't have to do the figuring between the two, because we compare by name in both getPredicate and addRule
	for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
		//Make sure we have these objects. 
		this->vertices[(*it).second] = true;
	}
	this->rules[pos] = true;
}

void SamplingConstraint::removeProposal(int pos) {
	this->rules[pos] = false;
	int u = -1;
	int v = -1;
	bool single_factor = false;
	if (this->set->getFactor(pos)->getNumComponents() == 1)
		single_factor = true;
	//This is a cleanup function
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range;
	range = set->getPredicate(pos);
	u = (*range.first).second;
	if (!single_factor) {
		range.first++;
		v = (*range.first).second;
	}
	//Next, we go through the rules twice, and determine if any other rules exist for the vertices (same as cleanup step)
	bool found = false;
	for (int i = 0; i < this->set->getNumRules(); i++) {
		if (this->rules[i]) {
			if (this->set->hasVertex(i, u)) {
				found = true;
			}
		}
	}
	this->vertices[u] = found;
	found = false;
	if (!single_factor) {
		for (int i = 0; i < this->set->getNumRules(); i++) {
			if (this->rules[i]) {
				if (this->set->hasVertex(i, v))
					found = true;
			}
		}
		this->vertices[v] = found;
	}
}

//We get back a new ruleset that has only the expansion, and can add it later
RuleSet* SamplingConstraint::expand(RuleSet* set, int rule_num) {
	float prob = set->getRuleProbability(rule_num);
	if (prob < EPSILON) {
		return NULL; //Its a dead rule, so we shouldn't be doing anything with this
	}
	//We have already identified the culprit rule at this time
	RuleSet* new_set = new RuleSet(false);
	
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> var_range = set->getPredicate(rule_num);
	Factor* fact = set->getFactor(rule_num);
	Content* item = set->getContent((*var_range.first).second); //We should now be looking at self links
	if (item == NULL)
		return new_set;
	new_set->addContent(item);
	int item_pos = (*var_range.first).second;
	//Now, we get all rules that have this connection
	std::list<int> connection_rules;
	std::map<int, int> variables;
	for (int i = 0; i < set->getNumRules(); i++) {
		if (i != rule_num) {
			bool found = false;
			var_range = set->getPredicate(i);
			//Here, we do not count when the first is the same as the second, no matter what. This is because we will handle this in our combination
			//Where the multiple self loops will figure out the highest probability 
			for (std::multimap<int, int>::const_iterator it = var_range.first; it != var_range.second; it++) {
				if ((*it).second == item_pos) { //For self loop, this goes back to false
					found = !found;
				}
				else {
					//We add the item if it isn't already there
					if (variables.find((*it).second) == variables.end()) {
						variables[((*it).second)] = new_set->addContent(set->getContent((*it).second));
					}
				}
			}
			if (found)
				connection_rules.push_back(i);
		}
	}
	//Now, we add all of those links that were part of those rules
	float rand_expand = (float)rand() / (float)RAND_MAX;
	std::list<int> adds;
	std::list<int> added_vars;
	added_vars.push_back(0);
	Vertex* new_item = NULL;
	int i = 1;
	while (i < 2) { //item->getProbability(i) > rand_expand
		//At this stage, we create a new content node with the probability of the next item
		Content *c = new Content(*item);
		//need to clear out all other probabilities because this is a single item
		c->addProbability(c->getProbability(i), 0);
		c->clearSetProbability();
		int new_pos = new_set->addContent(c);
		variables[item_pos] = new_pos;
		//Next, we add in the new connection to the new ruleset
		//Now, we add in all our variables that have come before us for the given rule
		for (std::list<int>::const_iterator olds = added_vars.begin(); olds != added_vars.end(); olds++) {
			adds.clear();
			adds.push_back((*olds));
			adds.push_back(new_pos);
			new_set->addRule(set->getFactor(rule_num), adds, set->getRuleProbability(rule_num));
		}
		//And we connect the other rules to the new object as well
		for (std::list<int>::const_iterator it = connection_rules.begin(); it != connection_rules.end(); it++) {
			adds.clear();
			var_range = set->getPredicate((*it));
			for (std::multimap<int, int>::const_iterator it2 = var_range.first; it2 != var_range.second; it2++) {
				adds.push_back(variables[(*it2).second]);//This is the pointer between objects
			}
			//Don't need frequency of the other items, those are used when we combine!
			new_set->addRule(set->getFactor((*it)), adds, set->getRuleProbability((*it)));
		}
		//Finally, we are done with this, so we add it to the list
		added_vars.push_back(new_pos);
		rand_expand = (float)rand() / (float)RAND_MAX; //Now we try again
		i++;
	}
	//Finally, we are done with it, so we flatten out the content set we used
	set->getContent(0)->clearSetProbability();
	return new_set;
}

//We get back a new ruleset that has only the expansion, and can add it later
RuleSet* SamplingConstraint::expand(RuleSet* set, Content *item) {
	int item_pos = set->getContent(item);
	Content *converter = item;
	if (item->getLeaf(0) != NULL && item->getLeaf(0)->getName() == "Story") {
		converter = new Content(*item);
		converter->changeVertex(item->getLeaf(1));
	}
	if (item->getProbability(0) < EPSILON || item->getNumContent() ==1 || item_pos == -1) {
		return NULL; //It is singular content, so we shouldn't be doing anything with this
	}
	RuleSet* new_set = new RuleSet(false);
	//Let's identify the rules the are self links and connection_rules
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> var_range;
	new_set->addContent(item);
	//Now, we get all rules that have this connection
	std::list<int> connection_rules;
	std::list<int> self_links;
	std::map<int, int> variables;
	for (int i = 0; i < set->getNumRules(); i++) {
			bool found = false;
			var_range = set->getPredicate(i);
			//Here, we do not count when the first is the same as the second, no matter what. This is because we will handle this in our combination
			//Where the multiple self loops will figure out the highest probability 
			for (std::multimap<int, int>::const_iterator it = var_range.first; it != var_range.second; it++) {
				if ((*it).second == item_pos) { 
					if (found) {
						self_links.push_back(i);
					}
					found = !found;//For self loop, this goes back to false
				}
				else {
					//We add the item if it isn't already there
					if (variables.find((*it).second) == variables.end()) {
						variables[((*it).second)] = new_set->addContent(set->getContent((*it).second));
					}
				}
			}
			if (found)
				connection_rules.push_back(i);
		}
	//Now, we add all of those links that were part of those rules
	float rand_expand = (float)rand() / (float)RAND_MAX;
	std::list<int> adds;
	std::list<int> added_vars;
	added_vars.push_back(0);
	Vertex* new_item = NULL;
	int i = 1;
	while (item->getProbability(i) > rand_expand) {
					//At this stage, we create a new content node with the probability of the next item
		Content *c = new Content(*converter);
		//need to clear out all other probabilities because this is a single item
		c->addProbability(c->getProbability(i), 0);
		c->clearSetProbability();
		int new_pos = new_set->addContent(c);
		variables[item_pos] = new_pos;
		//Next, we add in the new connection to the new ruleset
		for (std::list<int>::const_iterator it = self_links.begin(); it != self_links.end(); it++) {
			//Now, we add in all our variables that have come before us for the given rule
			for (std::list<int>::const_iterator olds = added_vars.begin(); olds != added_vars.end(); olds++) {
				adds.clear();
				adds.push_back((*olds));
				adds.push_back(new_pos);
				new_set->addRule(set->getFactor((*it)), adds, set->getRuleProbability((*it)));
			}
		}
		//And we connect the other rules to the new object as well
		for (std::list<int>::const_iterator it = connection_rules.begin(); it != connection_rules.end(); it++) {
			adds.clear();
			var_range = set->getPredicate((*it));
			for (std::multimap<int, int>::const_iterator it2 = var_range.first; it2 != var_range.second; it2++) {
				adds.push_back(variables[(*it2).second]);//This is the pointer between objects
			}
			//Don't need frequency of the other items, those are used when we combine!
			new_set->addRule(set->getFactor((*it)), adds, set->getRuleProbability((*it)));
		}
		//Finally, we are done with this, so we add it to the list
		added_vars.push_back(new_pos);
		rand_expand = (float)rand() / (float)RAND_MAX; //Now we try again
		i++;
	}
	//Finally, we are done with it, so we flatten out the content set
	item->clearSetProbability();
	return new_set;
}

RuleSet* SamplingConstraint::convertRuleSet(bool cleanup) {
	//Converts rule-sets with general content nodes into fully expanded nodes
	RuleSet new_set = *this->set;
	if(cleanup)
		ChooseXORProbabilities(&new_set);
	new_set.cleanRuleSetProb();
	for (int i = 0; i < set->getNumVertices(); i++) {
		if (set->getContent(i)->getNumContent() > 1) {
			RuleSet *addition = expand(set, set->getContent(i));
			if (new_set != NULL) {
				new_set = new_set + *addition;
			}
			delete addition;
		}
	}
	//Now that we are expanded, we change our selector function
	for (int i = 0; i < new_set.getNumVertices(); i++) {
		new_set.getContent(i)->changeSelectorFunction(KermaniContentSelection);
		new_set.getContent(i)->changeExternalFunction(KermaniExternalFunction);
	}
	//Now, we go through and ground our objects
	for (int i = 0; i < new_set.getNumVertices(); i++) {
		ground(new_set.getContent(i));
	}
	return new RuleSet(new_set);
}

RuleSet* SamplingConstraint::convertRuleSetNoExpand(bool cleanup) {
	RuleSet new_set = *this->set;
	if (cleanup)
		ChooseXORProbabilities(&new_set);
	new_set.cleanRuleSetProb();
	//Now, we go through and ground our objects
	for (int i = 0; i < new_set.getNumVertices(); i++) {
		ground(new_set.getContent(i));
	}
	return new RuleSet(new_set);
}
void SamplingConstraint::ground(Content* content) {
	if (content->getNumLeafs() == 0)
		return;
	content->select(); //Grounds into our selector function
	return;
}
//This implements the sampling system of Kermani et al.
RuleSet* SamplingConstraint::sampleConstraints(int num_objects) {
	//First clear out vertices and rules
	this->beta = 1.0f;
	float best_cost = this->cost_function(num_objects);
	std::vector<bool> best_verts;
	for (int i = 0; i < this->set->getNumVertices(); i++) {
		best_verts.push_back(false);
		this->vertices.push_back(false);
	}
	std::vector<bool> best_rules;
	for (int i = 0; i < this->set->getNumRules(); i++) {
		best_rules.push_back(false);
		this->rules.push_back(false);
	}
	if (best_verts.size() == 0 || best_rules.size() == 0) //Can't do anything if we don't have anything
		return NULL;
	for (int i = 0; i < this->iterations; i++) {
		this->propose(num_objects);
		float cost_cost = this->cost_function(num_objects);
		if (this->accept(cost_cost, best_cost)) {
			//std::cout << "Accepting old:" << best_cost << ",new:" << cost_cost << std::endl;
			//Here, we make the swap by deleting best and then deep copying cost
			best_cost = cost_cost;
			/*if (best_cost > 0) {
				std::cout << best_cost << std::endl;
			}*/
			for (int j = 0; j < this->set->getNumVertices(); j++) {
				best_verts[j] = this->vertices[j];
			}
			for (int j = 0; j < this->set->getNumRules(); j++) {
				best_rules[j] = this->rules[j];
			}
		}
		else {
			for (int j = 0; j < this->set->getNumVertices(); j++) {
				this->vertices[j] = best_verts[j];
			}
			for (int j = 0; j < this->set->getNumRules(); j++) {
				this->rules[j] = best_rules[j];
			}
		}
		//To cool the formula, every 10th of an iteration we cool the beta by a 10th
		if ((i + 1) % (this->iterations / 10) == 0)
			this->beta *= 10.0f;//We have the accept function reversed, so we reverse this
	}
	if (best_cost == 0.0) { //If we didn't converge on anything, it's a failure
		/*std::cout << "Did not converge" << std::endl;
		for (unsigned int i = 0; i < this->vertices.size(); i++) {
			if (vertices[i] == false && this->set->getFrequency(i) > 0.99999f) {
				std::cout << this->set->getVertex(i)->getName() << std::endl;
			}
		}*/
		this->vertices.clear();
		this->rules.clear();
		return NULL;
	}
	//else {
	//	std::cout << "Converged with score:" << best_cost << std::endl;
	//}
	RuleSet* best_set = new RuleSet(false);
	std::map<int, int> vert_map;
	//Add in the vertices
	int vert_count = 0;
	//bool cup_p = false;
	//bool cup_k = false;
	for (int i = 0; i < this->set->getNumVertices(); i++) {
		if (best_verts[i]) {
			//basically, if we have made a selection, we have to abide by it
			Content *c = this->set->getContent(i);
			Content *newc = new Content(*c);
			best_set->addContent(newc);
			/*if (c->getNumLeafs() > 0) {
				Vertex *v = NULL;
				for (int l_i = 0; l_i < c->getNumLeafs(); l_i++) {
					if (c->getSelection(l_i) != -1) {
						v = c->getLeaf(l_i);
					}
				}
				if (v != NULL) {
					best_set->addVertex(v);
				}
				else {
					best_set->addVertex(this->set->getVertex(i));
				}
			}
			else {
				best_set->addVertex(this->set->getVertex(i));
			}*/
			//if (this->set->getVertex(i)->getName() == "cup_prince")
			//	cup_p = true;
			//if (this->set->getVertex(i)->getName() == "cup_king")
			//	cup_k = true;
			best_set->addFrequency(vert_count, this->set->getFrequency(i));
			vert_map[i] = vert_count;
			vert_count++;
		}
	}
	//if (!cup_k) {
	//	std::cout << "Lost king cup in kermani iterations " <<best_cost<<std::endl;
	//}
	//if (!cup_p) {
	//	std::cout << "Lost prince cup in kermani iterations" <<best_cost<<std::endl;
	//}
	//Add in the rules
	for (int i = 0; i < this->set->getNumRules(); i++) {
		if (best_rules[i]) {
			std::list<int> verts;
			verts.clear();
			std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range;
			range = this->set->getPredicate(i);
			for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
				//This fixes a bug but should be fixed in removing rule. It's been fixed but I'll leave this just in case
				if(vert_map.find((*it).second) != vert_map.end())
					verts.push_back(vert_map[(*it).second]);
				else{
					//std::cout << "Adding " << (*it).second << " From Rule " << i << std::endl;
					best_set->addVertex(this->set->getVertex((*it).second));
					best_set->addFrequency(vert_count, this->set->getFrequency((*it).second));
					vert_map[(*it).second] = vert_count;
					vert_count++;
					verts.push_back(vert_map[(*it).second]);
				}
			}
			best_set->addRule(this->set->getFactor(i),verts, this->set->getRuleProbability(i));
		}
	}

	best_set->cleanRuleSet();
	this->vertices.clear();
	this->rules.clear();
	//Make sure we have something to show
	if (best_set->getNumVertices() > 0)
		return best_set;
	else
		return NULL;
}

//The idea of our XOR is currently hard-coded in
void SamplingConstraint::ChooseXORProbabilities(RuleSet* set) {
	//2-6-2020: We need to ensure that the additional vertex links do not have a negative impact on our search.
	//Since Kermani favors higher links, we set all relationships to required content (all with a probability of one)
	//to be 0.5 (so it doesn't positvely or negatively effect the relationship to be there)
	//In reality, we should just not count that relationship in our MCMC
	for (int i = 0; i < set->getNumVertices(); i++) {
		Content* content = set->getContent(i);
		if (content->getProbability(0) > 1 - EPSILON) {
			//This is our required content
			std::list<int> rules = set->getPredicates(content);
			for (std::list<int>::const_iterator it = rules.begin(); it != rules.end(); it++) {
				set->setProbability((*it), 0.5);
			}
		}
	}

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
		Vertex* v = vmap[i];
		//Here, we also determine if the content was a necessary item that is no longer one (and therefore we need INC)
		if (set->getContent(i)->getNumLeafs() > 0 && set->getContent(i)->getLeaf(0)->getName() == "Story" && set->getContent(i)->getProbability(0) < 1 - EPSILON) {
			int keeper = rand() % v->getNumEdges();
			set->addFrequency(set->getVertex(v->getEdge(0, true, keeper)->getName()), 1.0f);
		}
		/*for (std::list<Factor*>::iterator it = support.begin(); it != support.end(); it++) {
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
		} */
	}
	//End removing supports
	delete fg;
}