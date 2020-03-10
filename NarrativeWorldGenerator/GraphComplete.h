#pragma once
#ifndef __Graph_Complete__
#define __Graph_Complete__
#include "RuleSet.h" //Always include the ruleset
#include "FactorGraph.h"//Because graph search is easiest with a graph

class GraphComplete {
private:
	FactorGraph* graph; //The graph we perform the searches on
	RuleSet*     set; //This makes our end copying much easier
	Vertex* start; //The starting node
	Vertex* finish; //The ending node
	bool* nodes; //The list of all nodes that we work with

	bool pathBFS(); //Performs a non-recursive BredthFirstSearch on the data-set so we can determine if there is a path
	bool pDFSGenerate(int); //Performs a probabalistic DepthFirstSearch on the data-set to perform the actual generation
	bool selected(float);//Tells us that this is a selected link
	void initNodeList();//macro to initialize the bool list
public:
	GraphComplete():graph(NULL),set(NULL),start(NULL),finish(NULL),nodes(NULL) {};
	GraphComplete(RuleSet *set) :start(NULL), finish(NULL), nodes(NULL) { this->set = this->convertRuleSetNoExpand(new RuleSet(*set));this->graph = new FactorGraph(this->set); }
	~GraphComplete() { delete graph; delete start; delete finish; }

	RuleSet* completeGraph(Vertex*, Vertex*); //This does our generation
	void ChooseXORProbabilities(RuleSet* set); //Kermani will keep XOR probabilities, so we must remove them from play so that it doesn't
	RuleSet* convertRuleSetNoExpand(RuleSet* set, bool cleanup = true);
};

#endif
