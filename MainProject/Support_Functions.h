#pragma once
#include <algorithm>
Vertex* findVert(std::list<Vertex*> objs, Vertex* v) {
	for (std::list<Vertex*>::iterator it = objs.begin(); it != objs.end(); it++) {
		if (v->getName() == (*it)->getName())
			return (*it);
	}
	return NULL;
}
//Performs a Depth first search on the ruleset in order to determine 
//all the child actions. We give each leaf node a probability of one 
//for now
void addChildren(Database* db, Vertex* travel, Content* content) {
	if (travel->getNumEdges() == 0) { //we have a leaf node
		content->addLeaf(content->getNumLeafs(), travel);
		content->addLeafProbability(content->getNumLeafs() - 1, 0, 1.0);
		return; //We are done here
	}
	//Now, we go to all the children to find those leaves
	for (int i = 0; i < travel->getNumEdges(); i++) {
		addChildren(db, travel->getEdge(0,true,i), content);
	}
	return; //symmetry in coding 
}


//Creates a ruleset from the database, so we can populate rulesets from the database
RuleSet* createRuleSet(const std::string &name, Database* db) {
	int loc_id = db->getLocationID(name);
	int vert_num = db->getNumMotifObjects(loc_id);
	if (vert_num <= 0)
		return NULL; //Return NULL when we have nothing
	RuleSet *res = new RuleSet(false); //for these, we rely on knowing the children

	std::map<int, int> obj_map;
	for (int i = 0; i < vert_num; i++) {
		Vertex* vex = db->getMotifObject(loc_id, i);

		res->addVertex(vex);
		res->addFrequency(i, db->getMotifObjectProbability(loc_id, i));
		//Here, we determine children as well
		if (vex->getNumEdges() > 0) {
			addChildren(db, vex, res->getContent(i));
			res->getContent(i)->normalizeLeafs(false);//When we have symmetric sets, they should be normalized from the database
		}
	}
	int motif_rule_num = db->getNumberMotifRules(loc_id);
	//Everything that we need per rule
	Factor *fact;
	std::list<int> variables;
	float prob;
	for (int i = 0; i < motif_rule_num; i++) {
		variables.clear();
		//Let's get the rule
		fact = db->getMotifRuleFactor(loc_id, i);
		if (fact != NULL) {
			for (int j = 0; j < fact->getNumComponents(); j++) {
				int vex = db->getMotifRuleVariablePos(loc_id, i, j);
				if (vex != -1)
					variables.push_back(vex);
			}
			prob = db->getMotifRuleProbability(loc_id, i);
			if (fact != NULL && variables.size() > 0) { //Only add if we have something to add
				res->addRule(fact, variables, prob);
			}
		}
	}

	return res;
}

//This creates the ruleset, but also combines the created ruleset with all of their parents
RuleSet* combineRuleSetParents(const std::string& name, Database* db) {
	RuleSet* set = createRuleSet(name, db);
	int location_id = db->getLocationID(name);
	//-1 in the db signifies no parent
	while (db->getLocationParent(location_id) > 0) {
		location_id = db->getLocationParent(location_id);
		RuleSet* result = createRuleSet(db->getLocation(location_id), db);
		if (result != NULL) {
			RuleSet new_set = (*result) + set;
			delete result;
			delete set;
			set = new RuleSet(new_set);
		}
	}
	return set;
}


std::multimap<std::string, std::string> getAdjectives(const std::string& file_name, int cutoff_value) {
	std::multimap<std::string, std::string> results;
	std::ifstream myfile;
	std::string line;
	myfile.open(file_name);
	if (myfile.is_open()) {
		while (getline(myfile, line)) {
			//Here, we get the name, and then keep adding lines to our thing until we hit another one
			int num_elements = (int)std::count(line.begin(), line.end(), ',');
			if (num_elements == 2) { //In this case, we have a room
				std::size_t first, second;
				first = line.find(",");
				second = line.find(",", first + 1);
				std::string adj = line.substr(0, first);
				std::string name = line.substr(first + 1, second - first);
				int support = atoi(line.substr(second + 1).c_str());
				if (support > cutoff_value) {
					name = name.substr(0, name.find("."));
					adj = adj.substr(0, adj.find("."));
					results.insert(std::make_pair(name, adj));
				}
			}
		}
		myfile.close();
	}

	return results;
}

void addVertexSet(const std::string& file_name, Database *db) {
	std::ifstream myfile;
	std::string line;
	std::map<std::string, Vertex*> obj_map; //Easy lookup of our vertices
	myfile.open(file_name);
	Vertex* vert = NULL;
	std::size_t space = -1;
	int counter = 1;
	if (myfile.is_open()) {
		while (getline(myfile, line)) {
			space = line.find(' ');
			//std::cout << space << std::endl;
			if (space == std::string::npos) {
				vert = new Vertex(line);
				obj_map[line] = vert;
			}
			else {
				std::string first = line.substr(0, space);
				std::string second = line.substr(space + 1);
				vert = new Vertex(first);
				if (obj_map.find(second) != obj_map.end()) {
					vert->addEdge(obj_map[second], false);
				}
				else {
					obj_map[second] = new Vertex(second);
					db->addObject(obj_map[second], counter);
					counter++;
					vert->addEdge(obj_map[second], false);
				}
			}
			db->addObject(vert, counter);
			//std::cout << "Adding " << vert->getName() << std::endl;
			counter++;
		}
	}
}

void populateDatabase(Database* db, const std::map<std::string, RuleSet*>& maps) { //Populates our database with our motifs
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> frange;
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> prange;
	//char file_name[100];
	for (std::map<std::string, RuleSet*>::const_iterator it = maps.begin(); it != maps.end(); it++) {
		std::cout << (*it).first << std::endl;
		(*it).second->cleanRuleSetProb();
		db->addLocation((*it).first);
		//Next, add all factors and objects 
		frange = (*it).second->getAllFactors();
		//Add all factors to db
		for (std::list<Factor*>::const_iterator it2 = frange.first; it2 != frange.second; it2++) {
			db->addFactor((*it2));
		}
		//Next, we add in the vertices, including the frequency probabilities
		std::map<int, int> vert_map;
		int pos = 0; //Our motif position variable
		for (int i = 0; i < (*it).second->getNumVertices(); i++) {
			Vertex* vert = (*it).second->getVertex(i);
			if (vert->getNumEdges(false) == 0) { //This means it is not in the hierarchy
				Vertex *v = NULL;
				std::size_t found = vert->getName().find_last_of("_");
				if (found != std::string::npos) {
					//Here, we determine if the substring is a number. If it is and it2 has no parent, lets attach one
					if (isdigit(vert->getName().substr(found + 1).c_str()[0])) {
						v = db->getObjectByID(db->getObjects(vert->getName().substr(0, found)));
					}
					else {
						//Otherwise, lets see if we can connect one
						v = db->getObjectByID(db->getObjects(vert->getName()));
					}
				}
				else {
					v = db->getObjectByID(db->getObjects(vert->getName()));
				}
				if (v != NULL) { //In this case, we found one
					vert = v;
				}
				else {
					vert = NULL; //We don't want to add it here 
				}
			}
			if (vert != NULL) {
				if (db->addMotifObject((*it).first, vert, pos, (*it).second->getFrequency(i)) == false) { //Then, we can probably find it in the database
					std::cout << vert->getName() << "Could not be added to the database" << std::endl;
					//Next, lets see if we can get the position from the database
				}
				else {
					//Find the next good position
					if (vert_map.find(i) == vert_map.end()) {
						vert_map[i] = pos;
						pos++;
					}
				}
			}
		}
		//Next, we do the Actual Rules
		int num_rules = (*it).second->getNumRules();
		int rule_pos = 0;
		for (int i = 0; i < num_rules; i++) {
			prange = (*it).second->getPredicate(i);
			bool not_found = false;//Existance of connecting object check
			for (std::multimap<int, int>::const_iterator v = prange.first; v != prange.second; v++) {
				if (vert_map.find((*v).second) == vert_map.end())
					not_found = true;

			}

			if (!not_found) {
				if (db->addMotifRule((*it).first, rule_pos, (*it).second->getFactor(i), (*it).second->getRuleProbability(i))) {
					//prange = (*it).second->getPredicate(i);
					int j = 0;
					for (std::multimap<int, int>::const_iterator v = prange.first; v != prange.second; v++) {
						if (vert_map.find((*v).second) != vert_map.end()) {
							db->addMotifVariable((*it).first, rule_pos, j, vert_map[(*v).second]);
						}
						else {
							std::cout << (*it).second->getVertex((*v).second)->getName() << " not added to database" << std::endl;
						}
						j++;
					}
					rule_pos++;
				}
			}
		}
	}
}
//Writes our Constraints to a database file
std::list<ConstraintStruct> getConstraints(Database* db, const std::string & file_name) {
	std::ifstream myfile;
	std::string line;
	std::list<std::string> lines;
	std::list<ConstraintStruct> constraints;
	myfile.open(file_name);
	if (myfile.is_open()) {
		getline(myfile, line); //Clear out the header line
		while (getline(myfile, line)) {
			//Here, we get the name, and then keep adding lines to our thing until we hit another one
			int num_elements = (int)std::count(line.begin(), line.end(), ',');
			size_t beg = 0;
			size_t end = 0;
			ConstraintStruct constraint;
			constraint.param[2] = -1;
			constraint.param[3] = -1;
			for (int i = 0; i < 8; i++) {
				end = line.find(',', beg);
				if (i > 1) {
					//Round here
					if (line.find('.', beg) + 3 < end)
						end = line.find('.', beg) + 3;
				}
				//Kermani's bounds are now a minimum and maximum line distance
				switch (i) {
				case 0: {
					constraint.object_1 = db->getObjectByID(db->getObjects(line.substr(beg, end - beg))); 
					//Move up one level if possible
					if (constraint.object_1 != NULL && constraint.object_1->getEdge(0, false) != NULL)
						constraint.object_1 = constraint.object_1->getEdge(0, false);
					break; 
				}
				case 1:{ 
					constraint.object_2 = db->getObjectByID(db->getObjects(line.substr(beg, end - beg))); 
					if (constraint.object_2 != NULL && constraint.object_2->getEdge(0, false) != NULL)
						constraint.object_2 = constraint.object_2->getEdge(0, false);
					break;
				}
				case 2: constraint.param[0] = atof(line.substr(beg, end - beg).c_str()); break;
				case 3: constraint.param[1] = atof(line.substr(beg, end - beg).c_str()); break;
				case 4: constraint.param[2] = atof(line.substr(beg, end - beg).c_str()); break;
				case 5: constraint.param[3] = atof(line.substr(beg, end - beg).c_str()); break;

				}
				beg = line.find(',', end) + 1;
			}
			if (constraint.object_1 != NULL && constraint.object_2 != NULL)
				constraints.push_back(constraint);
		}
		myfile.close();
	}
	return constraints;
}

void populateConstraints(Database* db, const std::list<ConstraintStruct>& constraints, int type = 0) {
	int shape_id;
	for (std::list<ConstraintStruct>::const_iterator constraint = constraints.begin(); constraint != constraints.end(); constraint++) {
		//3 is box, 2 is line, 1 is cone (r and theta), 4 is guassian, and 5 is a 1D gaussian
		//So basically, we have to know who we are playing with
		//Fisher is 1 or 2 D gaussian (type  = 0)
		//Kermani is box or Line (type = 1)
		int obj_1 = db->getObjects((*constraint).object_1);
		int obj_2 = db->getObjects((*constraint).object_2);
		if (obj_1 != -1 && obj_2 != -1) {
			if (type == 0) {
				if ((*constraint).param[2] == -1) {
					shape_id = 5;
				}
				else {
					shape_id = 4;
				}
			}
			else if (type == 1) {
				if ((*constraint).param[2] == -1) {
					shape_id = 2;
				}
				else {
					shape_id = 3;
				}
			}
			//Need to add it to the database
			int constraint_param = db->getConstraintParameterID(shape_id, (*constraint).param[0], (*constraint).param[1], (*constraint).param[2], (*constraint).param[3]);
			if (constraint_param == -1) {
				if (shape_id == 5 || shape_id == 2) {
					db->addConstraintParam(shape_id, (*constraint).param[0], (*constraint).param[1]);
				}
				else {
					db->addConstraintParam(shape_id, (*constraint).param[0], (*constraint).param[1], (*constraint).param[2], (*constraint).param[3]);
				}
				constraint_param = db->getConstraintParameterID(shape_id, (*constraint).param[0], (*constraint).param[1], (*constraint).param[2], (*constraint).param[3]);
			}
			db->addRuleConstraint(obj_1, obj_2, constraint_param);
		}
	}
}
int getObjectNumber(Database* db, Content* c) {
	//First, figure out which content it is.
	//For starters, lets assume that if we have a selection, then it is unique 
	Vertex *v = c->getVertex();
	//First case, we have a shadow copied narrative item. The shadow vertex is passed as an attribute value to the system (with a story vertex)
	/*if (c->getLeaf(0)->getName() == "Story") {
		v = c->getLeaf(1); //Our shadow vertex
	}
	else {
		for (int i = 0; i < c->getNumLeafs(); i++) {
			if (c->getSelection(i) != -1) {
				v = c->getLeaf(i);
			}
		}
	}*/
	int val = db->getObjects(v);
	if (val == -1) {
		while (v != NULL && val == -1) {
			val = db->getObjects(v);
			if (val == -1)
				v = v->getEdge(0, false);
		}
	}
	return val;
}

//A function to write out the tree structure of the database as the 
std::string treeAsJson(Database* db) {
	std::stringstream json;
	std::pair<std::map<int, Vertex*>::const_iterator, std::map<int, Vertex*>::const_iterator> range = db->getObjectIterator();
	json << "{\n\t\"items\":[\n";
	int counter = 0;
	int num_parent_links = 0;
	for (std::map<int, Vertex*>::const_iterator it = range.first; it != range.second; it++) {
		Vertex* v = (*it).second;
		if (v->getEdge(0, false) != NULL) {
			num_parent_links++;
		}
		json << "\t\t" <<"\""<< v->getName() << "\"";
		
		if (counter < db->getNumberObjects() - 1) {
			counter++;
			json << ",\n";
		}
		else {
			json << "\n";
		}
	}
	json << "\t],\n\t\"connections\":{\n";
	counter = 0;
	for (std::map<int, Vertex*>::const_iterator it = range.first; it != range.second; it++) {
		Vertex* v = (*it).second;
		Vertex* p = NULL;
		if(v != NULL)
			p = v->getEdge(0, false);
		if (v == NULL || p == NULL) {
			//json << "\t\tnull";
		}
		else {
			json << "\t\t" << "\"" << v->getName() << "\":"<<"\""<<p->getName()<<"\"";
		
		if (counter < num_parent_links - 1) {
			json << ",\n";
			counter++;
		}
		else {
			json << "\n";
		}
		}
	}
	json << "\t}\n}\n";
	return json.str();
}
//This function returns back all of our motifs in a single json file
std::string motifsAsJson(Database* db) {
	std::stringstream json;
	json << "{\n\"motifs\":{\n";
	int good_motifs = 0;
	//First, we find out how many good motifs we have
	for (int i = db->getMinLocationID(); i < db->getMaxLocationID(); i++) {
		std::string name = db->getLocation(i);
		RuleSet* set = createRuleSet(name, db);
		if (set != NULL) {
			good_motifs++;
		}
		delete set;
	}
	good_motifs--; //We don't want to use the last one
	//Now, we act upon that data
	int found_good = 0;
	for (int i = db->getMinLocationID(); i < db->getMaxLocationID(); i++) {
		std::string name = db->getLocation(i);
		RuleSet* set = createRuleSet(name, db);

		if (set != NULL) {//There are times when the last set is null, and so we get an extra , in our json
			json << "\"" << name << "\":" << convertToJson(set);
			if (found_good < good_motifs) {
				json << ",\n";
				found_good++;
			}
			else {
				json << "\n";
			}
		}
		delete set;
	}
	json << "},\n";
	json << "\"object_tree\":" << treeAsJson(db);
	json << "}";
	return json.str();
}

//Should break up this function. This is used to pass the information into Merrell et al.
std::string createPositionFile(Database* db, RuleSet* set) {
	std::stringstream result;
	std::vector<std::string> objects;
	std::vector<std::string> constraints;
	std::vector<std::string> support;
	std::map<int, int> object_numbers;
	//This figures out all of the objects that we have meshes and bounding boxes for
	int counter = 0;
	std::map<int, int> found_objects;
	for (int i = 0; i < set->getNumVertices(); i++) {
		int val = getObjectNumber(db, set->getContent(i));
		object_numbers[i] = val;//Now we only have to look it up once							
		Vertex *v = db->getObjectByID(val);
		if (val != -1) {
			if (db->getMeshName(v) != "") {
				result.clear();
				result.str("");
				result << "{\"id\":" << counter << ","
					<< "\"name\": \"" << set->getVertex(i)->getName() << "\","
					<< "\"mesh\": \"" << db->getMeshName(v) << "\","
					<< "\"pos\":[0,0,0]," << "\"rot\":[0,0,0],"
					<< "\"bounds\":[" << db->getMeshLength(v) << "," << db->getMeshWidth(v) << "," << db->getMeshHeight(v) << "],"
					<<"\"scale\":["<<"1,1,1"<<"]"
					<< "}";
				found_objects[i] = counter;
				counter++;
				objects.push_back(result.str());
			}
		}
	}
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range;
	int obj_1, obj_2, fobj_1,fobj_2, constraint;
	for (int i = 0; i < set->getNumRules(); i++) { //For now, only doing ones that are structural. this may change
		range = set->getPredicate(i);
		if (set->getFactor(i)->getNumComponents() > 1) {
			obj_1 = -1;
			fobj_1 = -1;
			obj_2 = -1;
			fobj_2 = -1;
			//This section makes sure we are getting things that we can use in the database
			//and that we have meshes that we could use from the database as well
			for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
				if (found_objects.find((*it).second) != found_objects.end()) {
					int val = object_numbers[(*it).second];
					if (obj_1 == -1) {
						//See if we can traverse to find a parent in the dataset
						obj_1 = val;
						if (found_objects.find((*it).second) != found_objects.end())
							fobj_1 = found_objects[(*it).second];
					}
					else {
						obj_2 = val;// db->getObjects(v);
						if (found_objects.find((*it).second) != found_objects.end())
							fobj_2 = found_objects[(*it).second];
					}
				}
			}
			if (obj_1 != -1 && obj_2 != -1 && fobj_1 != -1 && fobj_2 != -1) {
				if (!set->getFactor(i)->getExistance()) { //This is a constraint building code
					if (set->getFactor(i)->getName().find("orientation") != std::string::npos) { //Orientation is a special case and it is a guassian
						result.clear();
						result.str("");
						result << "{\"source\":" << fobj_1 << ",\"target\":" << fobj_2 << ",\"type\":\"Guassian-one\",\"degree\":4,";
						if (!set->getFactor(i)->getName().find("parallel") != std::string::npos) {
							result << "\"params\":[0,0.001]}";
						}
						else {
							result << "\"params\":[1.57,0.001]}";
						}
						constraints.push_back(result.str());
					}else{//proximity is a distance function
						for (int k = 0; k < db->getNumConstraints(obj_1, obj_2) && k < 1; k++) {
							constraint = db->getConstraintParameterID(obj_1, obj_2, k);
							result.clear();
							result.str("");
							result << "{\"source\":" << fobj_1 << ",\"target\":" << fobj_2 << ",\"type\":\"Distance-all\",";
							result << "\"params\":[" << db->getConstraintParameter(constraint, 0) << "," << db->getConstraintParameter(constraint, 1) << ",2]}";
							constraints.push_back(result.str());
							}
							if (set->getFactor(i)->getName().find("symmetry") != std::string::npos) { //If there is symmetry, we add an extra same facing constraint
								result.clear();
								result.str("");
								result << "{\"source\":" << fobj_1 << ",\"target\":" << fobj_2 << ",\"type\":\"Guassian-one\",\"degree\":4,";
								result << "\"params\":[0,0.001]}";
								constraints.push_back(result.str());
							}
					}
				}
				else { //Here is our support building code
					//For now, support is source, target, type
					result.clear();
					result.str("");
					result << "{\"source\":" << fobj_1 << ",\"target\":" << fobj_2 << ",\"type\":\""<< set->getFactor(i)->getName() <<"\"}";
					support.push_back(result.str());
					//We should also add in a 2-D distance constraint
				}
			}
		}
	}
	//Here, we combine the three vectors together
	result.clear();
	result.str("");
	result << "{\"Objects\":[\n";
	//Add in all our found objects
	for (unsigned int i = 0; i < objects.size(); i++) {
		result << objects[i];
		if (i < objects.size() - 1) {
			result << ",\n";
		}
		else {
			result << "\n";
		}
	}
	result << "],\"Constraints\":[\n";
	//Add in all our found constraints
	for (unsigned int i = 0; i < constraints.size(); i++) {
		result << constraints[i];
		if (i < constraints.size() - 1) {
			result << ",\n";
		}
		else {
			result << "\n";
		}
	}
	result << "],\"Support\":[\n";
	//All in all our found support constraints
	for (unsigned int i = 0; i < support.size(); i++) {
		result << support[i];
		if (i < support.size() - 1) {
			result << ",\n";
		}
		else {
			result << "\n";
		}
	}
	result << "]}";
	return result.str();
}

//Feeds the database with a mesh string size function
void addMeshSizeSet(Database* db, const std::string & file_name) {
	std::ifstream myfile;
	std::string line;
	std::list<std::string> lines;
	myfile.open(file_name);
	if (myfile.is_open()) {
		//getline(myfile, line); //Clear out the header line
		while (getline(myfile, line)) {
			//Here, we get the name, and then keep adding lines to our thing until we hit another one
			int num_elements = (int)std::count(line.begin(), line.end(), ',');
			Vertex* a = NULL;
			Vertex* b = NULL;
			double x, y, z, dx, dy, dz;
			size_t beg = 0;
			size_t end = 0;
			for (int i = 0; i < 4; i++) {
				end = line.find(',', beg);
				if (i > 1) {
					//Round here
					if (line.find('.', beg) + 3 < end)
						end = line.find('.', beg) + 3;
				}
				switch (i) {
				case -1: a = db->getObjectByID(db->getObjects(line.substr(beg, end - beg))); break;
				case 0: b = db->getObjectByID(db->getObjects(line.substr(beg, end - beg))); break;
				case 1: x = atof(line.substr(beg, end - beg).c_str()); break;
				case 2: y = atof(line.substr(beg, end - beg).c_str()); break;
				case 3: z = atof(line.substr(beg, end - beg).c_str()); break;
				case 4: dx = atof(line.substr(beg, end - beg).c_str()); break;
				case 5: dy = atof(line.substr(beg, end - beg).c_str()); break;
				case 6: dz = atof(line.substr(beg, end - beg).c_str()); break;

				}
				beg = line.find(',', end) + 1;
			}
			if (b != NULL && x > 0.0 && y > 0.0 && z > 0.0)
				db->addMesh(b, b->getName(), x, y, z);
		}
		myfile.close();
	}
}
//The other helper function for testing that I should probably remove for release
void narrativeWorldDifferenceTest(const std::string& path, NarrativeWorldMold *recipe, NarrativeWorldMoldGenerator * gen, Database *db, bool with_cleanup) {
	//This is our generation phase
	std::string saveout;
	std::ofstream myfile;
	SamplingConstraint *constraint;
	char path_files[500];//Gives us the actual name of it
	RuleSet* time_con;
	for (int j = 0; j < recipe->getNumLocations(); j++) {
		time_con = recipe->getLocationAtTime(j, -1, NULL, with_cleanup);
		int total_failures = 0;
		int total_counts = 0;
		for (int i = 0; i <200; i++) {
			if (time_con != NULL) {
				constraint = new SamplingConstraint(10000, time_con,with_cleanup);
				for (int k = 0; k < 5; k++) {
					RuleSet *fin = NULL;
					int fail_counter = 0;
					if (time_con->getNumRules() > 0) {
						while (fin == NULL && fail_counter < 1) {
							fin = constraint->sampleConstraints(5 + k);
							fail_counter += 1;
							total_counts++;
							if (fin != NULL) {
								if (!recipe->isNarrativeLocation(j, -1, fin)) {
									fin = NULL; //No narrative World
								}
							}
							/*if (fail_counter % 10 == 0) { //I've written Kermani so that it can find nothing, which we want to try again for
								std::cout << "Failed on " << recipe->getLocation(j) << " for " << fail_counter << " iterations" << std::endl;
							}*/
						}
						total_failures += (fail_counter - 1); //We show minus 1 because the last one didn't fail
					}
					else {
						fin = new RuleSet(*time_con); //Yeah, if we have no rules Kermani breaks down
					}
					if (fin != NULL) {
						saveout = convertToJson(fin);
						sprintf_s(path_files, 500, "%s\\%s_%d.json", path.c_str(), recipe->getLocation(j).c_str(), i * 5 + k);
						//std::cout << path_files << std::endl;
						myfile.open(path_files);
						myfile << saveout;
						myfile.close();
						saveout.clear();
						delete fin;
					}
					else {
						//std::cout << "Failed on " << i << " iteration of " << recipe->getLocation(j) << std::endl;
						total_failures++; //Turns out the last one did fail
					}
				}
				delete constraint;
			}
		}
		std::cout << recipe->getLocation(j) << ":Failed on " << total_failures << " out of " << total_counts << " attempts to make 50 locations" << std::endl;
	}
}

void narrativeWorldDifferenceTestGraph(const std::string& path, NarrativeWorldMold *recipe, NarrativeWorldMoldGenerator * gen, Database *db, bool with_cleanup) {
	//This is our generation phase
	std::string saveout;
	std::ofstream myfile;
	GraphComplete *graph;
	char path_files[500];//Gives us the actual name of it
	RuleSet* time_con;
	for (int j = 0; j < recipe->getNumLocations(); j++) {
			time_con = recipe->getLocationAtTime(j, -1, NULL, with_cleanup);
			std::list<Vertex*> narrative_objects;
			std::list<Vertex*> end_objects;
 			graph = new GraphComplete(time_con, 0, with_cleanup);
			//Next, we get all objects that are deemed necessary (i.e, that have 100% requirement (either narrative objects or INC)
			if (graph->getSavedSet()->getMaxFrequency() < 0.999999f) { //There are no narrative objects
				narrative_objects.push_back(time_con->getVertex(rand() % time_con->getNumVertices()));
			}
			else {

				for (unsigned int i = 0; i < graph->getSavedSet()->getNumVertices(); i++) {
					if (graph->getSavedSet()->getFrequency(i) > 0.999999f) {
						narrative_objects.push_back(graph->getSavedSet()->getVertex(i));
					}
				}
			}
			if (time_con->getVertex("wall") != -1) {
				end_objects.push_back(time_con->getVertex(time_con->getVertex("wall")));
			}
			if (time_con->getVertex("floor") != -1) {
				end_objects.push_back(time_con->getVertex(time_con->getVertex("floor")));
			}
			if (time_con->getVertex("room") != -1) {
				end_objects.push_back(time_con->getVertex(time_con->getVertex("room")));
			}
			if (end_objects.size() != 0) {
				int total_failures = 0;
				int total_counts = 0;
				for (int i = 0; i < 200; i++) {
					if (time_con != NULL) {
						
						//constraint = new SamplingConstraint(10000, time_con, with_cleanup);
						for (int k = 0; k < 5; k++) {
							RuleSet* fin = NULL;
							int fail_counter = 0;
							if (time_con->getNumRules() > 0) {
								while (fin == NULL && fail_counter < 1) {
									fin = graph->completeGraph(narrative_objects, end_objects);
									fail_counter += 1;
									total_counts++;
									if (fin != NULL) {
										if (!recipe->isNarrativeLocation(j, -1, fin)) {
											fin = NULL; //No narrative World
										}
									}
								}
							}
							else {
								fin = new RuleSet(*time_con); //Yeah, if we have no rules Kermani breaks down
							}
							if (fin != NULL) {
								saveout = convertToJson(fin);
								sprintf_s(path_files, 500, "%s\\%s_%d.json", path.c_str(), recipe->getLocation(j).c_str(), i * 5 + k);
								//std::cout << path_files << std::endl;
								myfile.open(path_files);
								myfile << saveout;
								myfile.close();
								saveout.clear();
								delete fin;
							}
							else {
								//std::cout << "Failed on " << i << " iteration of " << recipe->getLocation(j) << std::endl;
								total_failures++; //Turns out the last one did fail
							}
							total_failures += fail_counter - 1;
						}
						
					}
					
				}
				delete graph;
				std::cout << recipe->getLocation(j)<<": Failed on " << total_failures << " out of " << total_counts << " attempts to make 10 locations" << std::endl;
			}
	}
}

//We get back a new ruleset that has only the expansion, and can add it later
RuleSet* expand(RuleSet* set, Content *item) {
	int item_pos = set->getContent(item);
	Content *converter = item;
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
	//Finally, we add in those to our new set so we correctly combine them
	
	int num_expand = item->getSelection(0); //Gonna hard code
	std::list<int> adds;
	std::list<int> added_vars;
	added_vars.push_back(0);
	Vertex* new_item = NULL;

	//Now, we add all of those links that were part of those rules
	for (std::list<int>::const_iterator it = connection_rules.begin(); it != connection_rules.end(); it++) {
		adds.clear();
		var_range = set->getPredicate((*it));
		for (std::multimap<int, int>::const_iterator it2 = var_range.first; it2 != var_range.second; it2++) {
			adds.push_back(variables[(*it2).second]);//This is the pointer between objects
		}
		//Don't need frequency of the other items, those are used when we combine!
		new_set->addRule(set->getFactor((*it)), adds, set->getRuleProbability((*it)));
	}

	for (int i = 1; i < num_expand; i++) {//since we already have one, we actually start at one
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
	}
	//Finally, we are done with it, so we flatten out the content set
	item->clearSetProbability();
	return new_set;
}

RuleSet *cleanupStuff(RuleSet *set) {
	RuleSet *result = new RuleSet((*set), false);
	for (int i = 0; i < set->getNumVertices(); i++) {
		Content *c = set->getContent(i);
		if (c->getSelection(0) > 1) {
			RuleSet *addition = expand(set, set->getContent(i));
			if (addition != NULL) {
				RuleSet new_set = (*result) + (*addition);
				delete result;
				result = new RuleSet(new_set, false);
			}
			delete addition;
		}
	}
	return new RuleSet((*result));
}

void KermaniTest(Database *db, RuleSet* test_set, std::string name, std::string path) {
	//This is our generation phase
	std::string saveout;
	std::ofstream myfile;
	SamplingConstraint *constraint;
	char path_files[500];//Gives us the actual name of it
		for (int i = 0; i <10; i++) {
				constraint = new SamplingConstraint(10000, test_set, true);
				for (int k = 0; k < 5; k++) {
					RuleSet *fin = NULL;
					int fail_counter = 0;
						while (fin == NULL && fail_counter < 100) {
							fin = constraint->sampleConstraints(10 + k);
							fail_counter += 1;
							if (fail_counter % 10 == 0) { //I've written Kermani so that it can find nothing, which we want to try again for
								std::cout << "Failed on " <<name << " for " << fail_counter << " iterations" << std::endl;
							}
						}
					if (fin != NULL) {
						RuleSet* test = cleanupStuff(fin);
						saveout = convertToJson(test);
						//saveout = createPositionFile(db,test);
						sprintf_s(path_files, 500, "%s\\%s_%d.json", path.c_str(), name.c_str(), i * 5 + k);
						std::cout << path_files << std::endl;
						myfile.open(path_files);
						myfile << saveout;
						myfile.close();
						saveout.clear();
						delete fin;
					}
					else {
						std::cout << "Failed on " << i << " iteration of " <<name << std::endl;
					}
				}
				delete constraint;
		}
}
void GraphCompleteTest(Database *db,RuleSet* test_set,std::string name, std::string path) {
	std::string saveout;
	std::ofstream myfile;
	GraphComplete *graph;
	char path_files[500];//Gives us the actual name of it
	int total_failures = 0;
	int total_counts = 0;
		for (int i = 0; i <10; i++) {
				graph = new GraphComplete(test_set);
				//constraint = new SamplingConstraint(10000, time_con, with_cleanup);
				for (int k = 0; k < 5; k++) {
					RuleSet *fin = NULL;
					int fail_counter = 0;
					while (fin == NULL && fail_counter < 100) {
						fin = graph->completeGraph(test_set->getVertex(rand() % test_set->getNumVertices()), test_set->getVertex(rand() % test_set->getNumVertices()));
						fail_counter += 1;
						total_counts++;
						if (fail_counter % 10 == 0) { //I've written Kermani so that it can find nothing, which we want to try again for
							std::cout << "Failed on " << name << " for " << fail_counter << " iterations" << std::endl;
						}
					}
					total_failures += (fail_counter - 1); //We show minus 1 because the last one didn't fail
					if (fin != NULL) {
						saveout = convertToJson(cleanupStuff(fin));
						sprintf_s(path_files, 500, "%s\\%s_%d.json", path.c_str(), name.c_str(), i * 5 + k);
						std::cout << path_files << std::endl;
						myfile.open(path_files);
						myfile << saveout;
						myfile.close();
						saveout.clear();
						delete fin;
					}
					else {
						std::cout << "Failed on " << i << " iteration of " << name << std::endl;
						total_failures++; //Turns out the last one did fail
					}
				}
				delete graph;
		}
		std::cout << "Failed on " << total_failures << " out of " << total_counts << " attempts to make 50 locations"<<std::endl;
}



RuleSet* convertRuleSet(RuleSet* in) {
	RuleSetConverter* converter = new RuleSetConverter(normalize, sigmaNormalize);
	return converter->ConvertRuleSet(in,false);
}