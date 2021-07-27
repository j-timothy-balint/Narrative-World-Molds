#pragma once
#ifndef __Graph_Complete__
#define __Graph_Complete__
#include "RuleSet.h" //Always include the ruleset
#include "FactorGraph.h"//Because graph search is easiest with a graph
#include <list>

class GraphComplete {
private:
	FactorGraph* graph; //The graph we perform the searches on
	RuleSet*     set; //This makes our end copying much easier
	Vertex* start; //The starting node
	std::list<Vertex*> finish; //The ending nodes. Note that it will end when any one of the finishing nodes are found
	bool* nodes; //The list of all nodes that we work with
	int backwards; //States which way we should assume our rule-set goes. If it's backwards, then we search target -> source, which is much slower

	bool pathBFS(); //Performs a non-recursive BredthFirstSearch on the data-set so we can determine if there is a path
	bool pDFSGenerate(int,int*); //Performs a probabalistic DepthFirstSearch on the data-set to perform the actual generation
	bool selected(float);//Tells us that this is a selected link
	void initNodeList();//macro to initialize the bool list
public:
	GraphComplete():graph(NULL),set(NULL),start(NULL),finish(NULL),nodes(NULL),backwards(-1) {};
	GraphComplete(RuleSet *set,int back = -1,bool cleanup = true) :start(NULL), finish(NULL), nodes(NULL),backwards(back) { this->set = this->convertRuleSetNoExpand(new RuleSet(*set),cleanup);this->graph = new FactorGraph(this->set); }
	~GraphComplete() { delete graph; delete start; finish.clear(); }

	RuleSet* completeGraph(Vertex*, Vertex*); //This does our generation
	RuleSet* completeGraph(Vertex*, const std::list<Vertex*>&);//Generates where it reaches one of any of the vertices
	RuleSet* completeGraph(const std::list<Vertex*>&, Vertex*); //Generates a series of rule-sets and then concatenates them together
	RuleSet* completeGraph(const std::list<Vertex*>&, const std::list<Vertex*>&);//Generates a series of rule-sets where any of the vertices work, and concatinates them together.
	void ChooseXORProbabilities(RuleSet* set); //Kermani will keep XOR probabilities, so we must remove them from play so that it doesn't
	RuleSet* convertRuleSetNoExpand(RuleSet* set, bool cleanup = true);
	
	RuleSet* getSavedSet() { return set; }
};

#endif
