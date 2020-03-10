#pragma once
#ifndef _FISHER_TWELVE_SCENE_SYNTH_
#define _FISHER_TWELVE_SCENE_SYNTH_
#include "BaseDataStructures.h"
#include "RuleSet.h"
#include "FactorGraph.h"
#include <queue>

class FisherSceneSynth {
private:
	RuleSet *ruleset;
	std::vector<bool> added;
	std::vector<bool> seen; //This keeps us from continually looking at the same nodes
	bool add(Content *c, FactorGraph* fg);
public:
	FisherSceneSynth(RuleSet* starting = NULL) :ruleset(starting) {}

	RuleSet* fowardSample(); //Creates a smaller factor graph based on the foward sampling from the root of the system
};
#endif // !_FISHER_TWELVE_SCENE_SYNTH_