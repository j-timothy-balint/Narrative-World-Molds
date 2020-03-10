#pragma once
#ifndef __KERMANI_SCENE_SYNTH__
#define __KERMANI_SCENE_SYNTH__
#include "RuleSet.h"

//This is actually a simplified version of Kermani et al, because we should also be using the layout as part of the accept/reject. 
//Merrel et al. could be used in this manner, but in reality it doesn't make sense to with simulated annealing unless we incorperate
//burn-in. In reality all the layout needs to be used for is to make sure selection isn't too crowded, which would lead to a Multi-jump
//MCMC being the best. I'll write it up eventually.
class SamplingConstraint {
private:
	std::vector<bool> vertices;
	std::vector<bool> rules;
	RuleSet *set;
	int iterations;
	void propose(int desired_objects); //this function proposes a new move, if possible
	float cost_function(int);//This function determines the total cost of the system
	bool accept(float, float); //Determines if we accept it based on the met hastings ratio
	int calcVerts(); //Calculates the number of vertices
	void addProposal(int);//Adds a factor from one to the other
	void removeProposal(int); //removes the factor from the list
	float beta;
	RuleSet* expand(RuleSet*, int); //This is the old way of doing it inside the mold generator. Kept for posterity
	RuleSet* expand(RuleSet*, Content*); //In this case, we know the content
	void ChooseXORProbabilities(RuleSet* set); //Kermani will keep XOR probabilities, so we must remove them from play so that it doesn't
	
	void ground(Content*);//Replaces our content with grounded content

public:
	SamplingConstraint(int it,bool clean) :iterations(it),set(NULL),beta(1.0f){}
	SamplingConstraint(int it, RuleSet* set,bool clean = true) :iterations(it), set(set), beta(1.0f) { this->set = this->convertRuleSetNoExpand(clean); } //If we have a ruleset, then we use it
	~SamplingConstraint() { delete set; }
	RuleSet* convertRuleSet(bool cleanup = true); //This converts the ruleset we are given into a grounded formation, cleaned up and everything
	RuleSet* convertRuleSetNoExpand(bool cleanup = true);
	RuleSet* sampleConstraints(int);//Gives us a new RuleSet that is the sampled evolution of our old ruleset
	RuleSet* getRuleSet() { return this->set; }//Returns back the ruleset we put in
};
#endif // !__KERMANI_SCENE_SYNTH__
