#pragma once

#ifndef __RULE_SET_H__
#define __RULE_SET_H__
#include "BaseDataStructures.h"
#include <list>
/*The purpose of this class is to have an easy computation to Merrell's IFL with regards to the factors (instead of the vertices)*/
/*We consider an action and a object state to be a rule-set, and a NWR to be comprised of several rulesets*/
class RuleSet {
protected:
	std::list<Factor*> factors; //The different types of factors we can have
	std::vector<Content*> vertices; //The different vertices that we have in our graph
	std::vector<Factor*> ruleset; //The ruleset we have, may have the same factor multiple times
	std::multimap<int, int> variables;  //The variables that go along with the ruleset
	std::map<int, float>   probability; //The probability of each ruleset. These are all conditional probabilities. 
	bool deep_copied_set; //This lets us know if we need to deep copy the ruleset 

	//These are only dealing with looking in the rule-set
	
	void updateConditionalFactor(int, int,float);//Helper function to do bayes rule on a conditional factor 
	void updateConditionalFactor(int,int, int, int, float, float);//This is for the bi-directional use case
	
	std::map<int, int>         intersectionRules(const RuleSet&,const std::map<Factor*,Factor*>&, const std::map<int,int>&)const ;//provides the intersectin ids between structure in two rulesets
	std::map<Factor*, Factor*> intersectionFactors(const RuleSet&) const;//Provides the intersection between the semantics of the structure of two rulesets

public:
	RuleSet(bool dcs = false):deep_copied_set(dcs) {}; //Base constructor doesn't have anything to add
	RuleSet(const std::list<std::string>&,bool dcs = true); //Same constructor as factor graph because these are equivalent
	RuleSet(const RuleSet&,bool dcs = false); //Copy Constructor.
	~RuleSet(); //Destroy the factors and vertices

	//Normalization code
	void normByValue(float);
	void normVertsByValue(float);
	void normRulesByValue(float);

	bool matchedFactor(int, const std::vector<int>&); //Helper function to see if we match the vertex or not
	//This is public because it is useful to have it exposed in the Narrative world mold generator for augmenting the motifs
	std::map<int, int>         intersectionVertices(const RuleSet&) const;//provides the intersection ids between content in two rulesets

	//These provide information about the size of different properties 
	int getNumFactors() const { return (int) this->factors.size(); }
	int getNumRules() const   { return (int)this->ruleset.size(); }
	int getNumRules(Factor*) const; //Gets the number of rules that start with a given factor
	int getNumRules(const std::list<Vertex*>&) const; //Returns the number of ruls that match a given vertex pair
	int getNumLoops() const; //This is for self-loops
	int getNumVertices() const { return (int)this->vertices.size(); }

	//Getters for the whole list of items
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> getAllFactors()  const { return std::make_pair(factors.begin(), factors.end()); }
	//Getters for a single Vertex. We consider vertices in two ways, either the vertex itself or the position of the vertex in the ruleset
	Vertex*  getVertex(int) const;
	int      getVertex(Vertex*) const;
	int      getVertex(const std::string&) const;//When we only have the name
	Content* getContent(int) const; //Compared when we want to look at the content instead
	Content* getContent(Vertex*,int skips = 0) const;
	int      getContent(Content*,int skips = 0) const;
	

	Factor* getFactor(int) const; //Returns the factor at the ruleset position
	bool hasRule(Factor*, Vertex*, Vertex*) const; //Returns whether a rule exist in the current RuleSet
	bool hasVertex(int, int) const; //Really quick helper function to determine if the vertex is in the ruleset
	bool hasVertex(Vertex* vert) const { return this->getContent(vert) != NULL; } //This allows us to determine if the vertex exists in the ruleset
	//When we know a specific rule (factor), we can then get the predicates for that. There are several different cases for that
	std::list<int>                                                                               getPredicates(Factor*) const;
	std::list<int>																				 getPredicates(Content*,int which = -1) const;
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator>  getPredicate(Factor*,int) const;
	std::multimap<int, int>                                                                      getAllPredicates(Factor*, Vertex*,int) const;
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator>  getPredicate(int) const;
	int                                                                                          getPredicate(Factor*, const std::list<Vertex*>&) const;
	int                                                                                          getPredicate(const std::list<Vertex*>&, int) const;//Gets our rule number based on the vertices and an offset
	//Similarlly, we can do the same for our probabilities
	float getRuleProbability(Factor*, int) const;
	float getRuleProbability(int) const;
	float getFrequency(int,int which = 0) const;

	//These two functions go through our frequency and rule probability in order to figure out
	//what the maximum is (useful for normalization)
	float getMaxProbability() const;
	float getMaxFrequency() const;

	RuleSet* intersection(const RuleSet&, const RuleSet&) const;//Performs an intersection between the two RuleSets.
	//Instead of performing graph similarity, we can perform the data on the ruleset and probably get the results faster (plus that's what IFL has)
	friend RuleSet operator -(const RuleSet&, const RuleSet&);//subtracts probability from the the intersection of left and right. Keeps the left hand side that isn't in right 
	RuleSet operator +(const RuleSet&);//The union of the two. The intersection is then the maximum probability
	RuleSet operator +=(const RuleSet& rhs) { return (*this + rhs); }
	bool operator ==(const RuleSet &rhs) const; //Equivalance based on vertex name (exact), ruleset's being exact, and probability being exact
	bool operator!= (const RuleSet &rhs) const { return !(*this == rhs); }//Oppisite of equals

	//Other important functions that change the Dataset
	bool addRule(Factor*, const std::list<Vertex*>&,float); //This is when we do not know the position of the integers
	bool addRule(Factor*, const std::list<int>&, float); //This is when we do know the positions of the integers
	bool updateFrequency(int, float); //Updates the conditional probability on all of the frequencies where the vertex is the observation
	bool removeRule(int,bool); //Removes the rule from the ruleset, and does other clean-up if necessary
	void cleanRuleSet(bool freq_based = false); //Removes any vertices and rules that are not already present in the ruleset
	void cleanRuleSetProb(bool freq_based = false); //Removes any vertices and rules that are in conflict probabalistically in the ruleset

	//These are our setters. Quick for now, checks should be added later
	int addVertex(Vertex* vert);
	int addContent(Content* cont);

	Factor* addFactor(Factor* fact);
	void addFrequency(int vert, float prob, int which = 0);
	void setProbability(int ruleset, float prob) { probability[ruleset] = prob; }
	//These two replace a factor or vertex with another, for when we don't need story factors/vertices but motif factors/vertices
	Factor* replaceFactor(Factor*, Factor*);//Replaces it with the new factor and returns the value
	int replaceVertex(Vertex* ov, Vertex* nv);//Replaces it with the new vertex, returns the value
	int replaceContent(int which, Content* nv);//Replaces the content 
};

std::map<std::string, RuleSet*> parseCSVRuleSet(const std::string &,bool);// For easy changing from factor graphs to rulesets
RuleSet* getMaxSupriseRuleSet(const std::list<RuleSet*>&, RuleSet*);
std::string convertToJson(RuleSet*);//Converts our ruleset to Json, which is actually more natural

#endif // !__RULE_SET_H__

