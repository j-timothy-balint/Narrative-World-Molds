#include "RuleSetConverter.h"

//I'm going to need to clean all this up after the workshop paper
float tableExtern(const std::vector<int>& selection,const std::vector<float>& vals) {
	float prob = 1.0f;
	for (int i = 0; i < (int)vals.size(); i++) {
		if (selection[i] != 0) {
			prob = vals[selection[i]];
		}
	}
	return prob; //Otherwise, return the last good one
}

int *probabilitySelection(const std::vector<Content*>& items) {
	int *data = new int[items.size()];
	for (int i = 0; i < (int)items.size(); i++) {
		data[i] = 0;
		float prob = (float)rand() / (float)RAND_MAX;
		for (int j = items[i]->getNumContent() - 1; j < 0 && data[i] == 0; j++) {
			if (items[i]->getProbability(j) > prob) {
				data[i] = j;
			}
		}
	}
	return data;
}
float SingleExternalFunction(const std::vector<int>& selection, const std::vector<float>& items) { //Everything should be the first probability for our content stuff
	return items[0];
}
int* firstSelection(const std::vector<Content*>& items) {
	int *selection = new int[(int)items.size()];
	for (int i = 0; i < (int)items.size(); i++) {
		selection[i] = 0;
	}
	selection[0] = 1;
	return selection;
}



RuleSet* RuleSetConverter::ConvertRuleSet(RuleSet* in,bool kermani) {
	//We are copying the ruleset
	RuleSet* out = new RuleSet();
	std::map<int, int> vertex_map; //maps from in to out
	std::multimap<Vertex*, int> duplicate_map; //Maps our index to our vertices
	//Search through each content nodes and find duplicates
	for(int i=0; i < in->getNumVertices(); i++){
		Vertex* v = in->getVertex(i);
		//if (duplicate_map.find(v) == duplicate_map.end()) {
			duplicate_map.insert(std::make_pair(v, i));
		//}
	}
	for (std::multimap<Vertex*, int>::iterator it = duplicate_map.begin();  //This is a dirty way of deliniating a for loop
			it != duplicate_map.end();
			it = duplicate_map.upper_bound((*it).first)) {//but my laptop screen is small, so I can see it all
		if (duplicate_map.count((*it).first) == 1) {//Non-duplication
			Content *new_node = new Content(*in->getContent((*it).second));
			new_node->changeSelectorFunction(firstSelection);
			new_node->changeExternalFunction(SingleExternalFunction);
			int new_pos = out->addContent(new_node);
			vertex_map[(*it).second] = new_pos;
		}
		else {
			//Use a vertex for the base content node
			Content *new_node = new Content((*it).first,false);
			std::vector<float> values; //Recall that at this time we do not have a deliniation, so the values are 
									   //the same for all of it
			std::pair<std::multimap<Vertex*, int>::const_iterator, std::multimap<Vertex*, int>::const_iterator> range;
			int new_pos;
			if (kermani) {
				new_node->addLeaf(0, (*it).first);//Our set is the one. We need to re-write content
				range = duplicate_map.equal_range((*it).first);
				for (range.first = range.first; range.first != range.second; range.first++) {
					values.push_back(in->getFrequency((*range.first).second));
				}
				//Sort everything
				for (int i = 0; i < values.size(); i++) {
					for (int j = i; j < values.size(); j++) {
						if (values[i] < values[j]) {
							float temp = values[i];
							values[i] = values[j];
							values[j] = temp;
						}
					}
				}
				//Run the IPF and store all that information
				std::vector<float> ipf = this->IPF_Convert(values);
				for (unsigned int i = 0; i < ipf.size(); i++) {
					new_node->addLeafProbability(0,i,ipf[i]);
				}
				//Run the EPF and store all that information
				std::vector<float> epf = this->EPF_Convert(values);
				new_pos = out->addContent(new_node);
				for (unsigned int i = 0; i < epf.size(); i++) {
					out->addFrequency(new_pos, epf[i], i);
				}
			}
			else {
				new_node->addLeaf(0, (*it).first);//Our set is the one. We need to re-write content
				range = duplicate_map.equal_range((*it).first);
				for (range.first = range.first; range.first != range.second; range.first++) {
					values.push_back(in->getFrequency((*range.first).second));
				}
				//Sort everything
				for (int i = 0; i < values.size(); i++) {
					for (int j = i; j < values.size(); j++) {
						if (values[i] < values[j]) {
							float temp = values[i];
							values[i] = values[j];
							values[j] = temp;
						}
					}
				}
				//Run the IPF and store all that information
				std::vector<float> ipf = this->IPF_Convert(values);
				for (unsigned int i = 0; i < ipf.size(); i++) {
					new_node->addLeafProbability(0, i, ipf[i]);
				}
				//Don't have an EPF, just add
				new_pos = out->addContent(new_node);
				for (unsigned int i = 0; i < values.size(); i++) {
					out->addFrequency(new_pos, values[i], i);
				}
			}
			range = duplicate_map.equal_range((*it).first);
			for (range.first = range.first; range.first != range.second; range.first++) {
				vertex_map[(*range.first).second] = new_pos;
			}
		}
	}
	//Now that we have all the duplicates in our format, we need to reconnect all the links together
	for (int i = 0; i < in->getNumRules(); i++) {
		Factor* fact = in->getFactor(i);
		float prob = in->getRuleProbability(i);
		std::list<int> verts;
		std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> v_map = in->getPredicate(i);
		for (std::multimap<int, int>::const_iterator v_it = v_map.first;v_it != v_map.second; v_it++) {
			verts.push_back(vertex_map[(*v_it).second]);
		}
		bool found = false;
		for (unsigned int j = 0; j < out->getNumRules() && !found; j++) {
				if (out->getFactor(j) == fact) {
					//Now, we determine if any of the 
					bool same = true;
					std::list<int>::const_iterator it = verts.begin();
					v_map = out->getPredicate(j);
					for (std::multimap<int, int>::const_iterator v_it = v_map.first; v_it != v_map.second && same; v_it++) {
						if ((*v_it).second != (*it)) {
							same = false;
						}
						it++;
						//See if any of these are a match for it
					}
					if (same) { //If we have a match, then we are good and can do the right check
						found = true;
						if (prob > out->getRuleProbability(j)) {
							out->setProbability(j, prob);//Go with the higher one
						}
					}
				}
		}
		if (!found) {
			out->addRule(fact, verts, prob);
		}
		
	}
	//Finally, we return the new rule-set
	return out;
}

//Normalizes the values in the vector
std::vector<float> normalize(const std::vector<float>& in) {
	float summation = 0;
	std::vector<float> out (in);
	for (unsigned int i = 0; i < in.size(); i++) {
		summation += in.at(i);
	}
	for (unsigned int i = 0; i < out.size(); i++) {
		out[i] /= summation;
	}
	return out;
}
//Normalizes based on the multiplicative sigma 
std::vector<float> sigmaNormalize(const std::vector<float>& in) {
	std::vector<float> out (in.size(),1.0); //So we can just multipy things together
	for (unsigned int i = 0; i < out.size(); i++) {
		for (int j = 0; j <= i; j++) {
			out[i] *= in[j];
		}
	}
	return out;
}