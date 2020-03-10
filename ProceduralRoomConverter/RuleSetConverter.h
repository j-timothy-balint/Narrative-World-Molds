#pragma once
#ifndef __RULE_SET_CONVERTER__
#define __RULE_SET_CONVERTER__

#include "RuleSet.h"

typedef std::vector<float>(*Converter)(const std::vector<float>&);
class RuleSetConverter {
protected:
	Converter IPF_Convert;
	Converter EPF_Convert;
public:
	RuleSetConverter(Converter ipf, Converter epf) :IPF_Convert(ipf), EPF_Convert(epf) {}
	~RuleSetConverter();
	RuleSet *ConvertRuleSet(RuleSet*,bool);//copies the ruleset and changes the IPF and EPF, connected 
};

std::vector<float> normalize(const std::vector<float>&);
std::vector<float> sigmaNormalize(const std::vector<float>&);
#endif