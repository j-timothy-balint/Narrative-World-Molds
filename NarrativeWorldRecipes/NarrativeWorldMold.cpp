#include "NarrativeWorldMold.h"
#include <set>
#include <sstream>
#include "FactorGraph.h"

#include<iostream>
#include "rapidjson/document.h"

//This is a terrible way to ensure two things:
//1)RapidJson is only in the cpp file
//2)We can break up our json constructor
RuleSet * parseSet(const rapidjson::Value& spatial, Database* db, NarrativeWorldMold* recipe,bool time) {
	if (!(spatial.HasMember("vertices") && spatial.HasMember("rules") && spatial.HasMember("probability"))) {
		return NULL;
	}
	RuleSet *set = new RuleSet(false);
	//With our spatial parser, we really just need to build the data-set
	//The narrative objects can be determined with Time (as that is what is linked to the narrative)
	for (auto& v : spatial["vertices"].GetArray()) {
		if (time) {
			//Here, we also should connect our item
			Vertex* item = recipe->getStoryVertex(v["name"].GetString());
			if (item == NULL) {
				//Then we need to add it to the story set
				std::string name = v["name"].GetString();
				item = new Vertex(name);
				name = name.substr(0, name.find_last_of("_"));
				Vertex* shadow = db->getObjectByID(db->getObjects(name));
				recipe->addStoryItem(item, shadow);
			}
		}
		float freq = v["frequency"].GetFloat();
		//We search first in the story to make sure that we always connect story items
		//and don't get confused saying that the item is both
		Vertex* item = recipe->getStoryVertex(v["name"].GetString());
		if (item == NULL) {
			item = db->getObjectByID(db->getObjects(v["name"].GetString()));
		}
		if (item != NULL) {
			int id = set->addVertex(item);
			Content *content = set->getContent(id);//Now, we have to add in the leaves so we can properly ground
			int counter = 0;
			for (auto& l : v["Leaves"].GetArray()) {
				item = db->getObjectByID(db->getObjects(l["name"].GetString()));
				content->addLeaf(counter, item);
				content->addLeafProbability(counter, 0, l["frequency"].GetFloat());
				counter++;
			}
			set->addFrequency(id, freq);
		} //We should throw an error here, because it'll mess up the other links
	}
	std::string parse_string;
	for (auto& r : spatial["rules"].GetArray()) {
		Factor* f = db->getFactorByID(db->getFactor(r["factor"].GetString()));
		//So, we have alot of other places where source and target are strings
		//may go through and refactor all the other code as well, which will change this
		std::list<int> conn;
		parse_string = r["source"].GetString();
		conn.push_back(atoi(parse_string.c_str()));
		parse_string = r["target"].GetString();
		conn.push_back(atoi(parse_string.c_str()));
		set->addRule(f, conn, 0.00f);//Default probability for now
	}
	for (auto& p : spatial["probability"].GetArray()) {
		parse_string = p["id"].GetString();
		int rule_id = atoi(parse_string.c_str());
		float val = p["value"].GetFloat();
		set->setProbability(rule_id, val);
	}
	return set;
}

NarrativeWorldMold::NarrativeWorldMold(const std::string& json_file,Database *db) {
	rapidjson::Document reader;
	reader.Parse(json_file.c_str());
	if (reader.HasMember("Locations")) {
		//Here, we iterate through the world, grabbing all the location names 
		for (unsigned int i = 0; i < reader["Locations"].Size(); i++) {
			this->addLocation(reader["Locations"][i].GetString());
		}
		if (reader.HasMember("Connections")) {
			//And our Connections
			for (unsigned int i = 0; i < reader["Connections"].Size(); i++) {
				const rapidjson::Value& member = reader["Connections"][i].GetObject();
				this->addLocationConnection(member["source"].GetInt(), member["target"].GetInt());
			}
		}
		if (reader.HasMember("StoryItems")) {
			for (auto& member : reader["StoryItems"].GetArray()) {
					std::string name = member[0].GetString();
					Vertex* story = new Vertex(name);
					name = member[1].GetString();
					Vertex* shadow = db->getObjectByID(db->getObjects(name));
					this->addStoryItem(story, shadow);
			}
		}
		//Now, we add in our world
		if (reader.HasMember("World")) {
			for(auto& member: reader["World"].GetArray()){
				int loc_id = this->getLocationID(member["name"].GetString());
				//Get our time points and set up our narrative connections
				if (member.HasMember("Time")) {
					for (auto& time_point : member["Time"].GetArray()) {
						double tp = time_point["id"].GetDouble();
						this->addTemporialConstraint(loc_id, tp, parseSet(time_point["ruleset"].GetObject(), db, this, true));
					}
				}
				//We get the narrative set that is a set of the database
				if (member.HasMember("SuperSet")) {
					const rapidjson::Value& superset = member["SuperSet"].GetObject();
					this->addSuperset(loc_id, parseSet(superset, db, this,false));
				}
				//Spatial rule-set
				if (member.HasMember("Spatial")) {
					const rapidjson::Value& spatial = member["Spatial"].GetObject();
					this->addSpatialConstraint(loc_id, parseSet(spatial,db,this,false));
				}
				
				
			}
		}
	}
}

NarrativeWorldMold::~NarrativeWorldMold() {
	//Delete all the temporial constraints
	for (std::map<int, TimeStructure*>::iterator it = this->temporial_constraints.begin(); it != temporial_constraints.end(); it++) {
		
		for (std::map<double, RuleSet* >::iterator it2 = (*it).second->time_points.begin(); it2 != (*it).second->time_points.end(); it2++) {
			delete (*it2).second; //since it's a shallow made copy, no worry about the vertices and factors being destroyed
		}
		delete (*it).second;
	}
	this->temporial_constraints.clear();

	//Delete the location constraints
}

bool NarrativeWorldMold::addLocation(const std::string& loc_name) {
	if (this->getLocationID(loc_name) == -1) {
		this->locations.push_back(loc_name);
		return true;
	}
	return false;
}
//Function that allows us to use more human readible data
bool NarrativeWorldMold::addLocationConnection(const std::string& loc_name_1, const std::string& loc_name_2) {
	int loc_1 = this->getLocationID(loc_name_1);
	int loc_2 = this->getLocationID(loc_name_2);
	return this->addLocationConnection(loc_1, loc_2);
}
//This is our actual add function
bool NarrativeWorldMold::addLocationConnection(int loc_1, int loc_2) {
	//Needs to be a location, which we are assuming matches the string ids
	if (loc_1 == -1 || loc_2 == -1) {
		return false;
	}
	if (loc_1 >= this->locations.size() || loc_2 >= this->locations.size()) {
		return false;
	}
	if (!this->hasLocationConnection(loc_1, loc_2)) {
		this->location_graph.insert(std::make_pair(loc_1, loc_2));
		return true;
	}
	return false;
}
//This two functions add a spatial ruleset to a location
bool NarrativeWorldMold::addSpatialConstraint(const std::string& loc_name, RuleSet* set) {
	int loc_id = this->getLocationID(loc_name);
	return this->addSpatialConstraint(loc_id,set);
}
bool NarrativeWorldMold::addSpatialConstraint(int loc_id, RuleSet* set) {
	if (loc_id == -1)
		return false;
	this->spatial_constraints[loc_id] = set;
	return true;
}
//Adds the entire temporial constraint to the system
bool NarrativeWorldMold::addTemporialConstraint(const std::string& loc_name, double time, RuleSet* time_point) {
	int loc_id = this->getLocationID(loc_name);
	return this->addTemporialConstraint(loc_id, time, time_point);
}

//Adds the entire temporial constraint to the system
bool NarrativeWorldMold::addTemporialConstraint(int loc_id, double time, RuleSet* time_point) {
	if (loc_id == -1)
		return false;
	//Here, we put in a constraint that we will not add a time point if it doesn't contain anything
	if (time_point->getNumRules() == 0 && time_point->getNumVertices() == 0)
		return false;
	TimeStructure* time_struct = this->getTimeStruct(loc_id);
	if (time_struct == NULL) {
		time_struct = this->addTemporialStructure(loc_id);
	}
	RuleSet* exists = this->getTimePoint(loc_id, time);
	if (exists != NULL) {
		delete exists;
	}
	time_struct->time_points[time] = time_point;
	return true;
}

bool NarrativeWorldMold::addSuperset(int loc_id, RuleSet* superset) {
	if (loc_id < 0 || loc_id >= (int)this->locations.size())//Reasons we wouldn't want this
		return false;
	if (this->location_superset.find(loc_id) != this->location_superset.end()) {
		if (this->location_superset[loc_id] != NULL) {
			delete this->location_superset[loc_id];
		}
		if (superset == NULL) {
			//We are deleting if we are sending in a bad super-set
			this->location_superset.erase(loc_id);
		}
	}
	if (superset == NULL) {
		return false;
	}
	this->location_superset[loc_id] = superset;
	return true;
}
bool NarrativeWorldMold::addSuperset(const std::string& loc_name, RuleSet* superset) {
	return this->addSuperset(this->getLocationID(loc_name), superset);
}
//Adds a story item to our set of objects if the story vertex does not already
//exist
bool NarrativeWorldMold::addStoryItem(Vertex* item, Vertex* shadow) {
	if (this->hasStoryItem(item))
		return false;
	if (item == NULL)
		return false;
	this->story_items[item] = shadow;
	return true;
}

/*Begin Value Getting*/
int NarrativeWorldMold::getLocationID(const std::string& loc_name) const{
	for (int i = 0; i < (int)this->locations.size(); i++) {
		if (this->locations.at(i) == loc_name)
			return i;
	}
	return -1; //Our value for when something doesn't exist
}
std::string NarrativeWorldMold::getLocation(int loc_id) const {
	if (loc_id < 0 || loc_id > this->locations.size())
		return "";
	return this->locations.at(loc_id);
}
bool NarrativeWorldMold::hasLocation(int loc_id) {
	if (loc_id < 0 || loc_id >(int)this->locations.size())
		return false;
	return true;
}
int NarrativeWorldMold::getNumTimePoints(int loc_id) const { //Each location has a number of timed plot points
	if (this->temporial_constraints.find(loc_id) == this->temporial_constraints.end())
		return -1;
	return (int)this->temporial_constraints.at(loc_id)->time_points.size();
}
double NarrativeWorldMold::getTime(int loc_id, int which) const {
	int num_points = this->getNumTimePoints(loc_id);
	if (which < 0 || which >= num_points)
		return -2.0;//The lowest we go for now is -1 (init), so -2 means that it doesn't exist
	TimeStructure * ts = this->getTimeStruct(loc_id);
	if (ts == NULL) {
		return -2.0;
	}
	std::map<double, RuleSet*>::const_iterator it = ts->time_points.begin();
	std::advance(it, which);
	return (*it).first;
}
TimeStructure* NarrativeWorldMold::getTimeStruct(int loc_id) const{
	std::map<int,TimeStructure*>::const_iterator it = this->temporial_constraints.find(loc_id);
	if (it == this->temporial_constraints.end())
		return NULL;
	return (*it).second;
}
//Gets the time point
RuleSet* NarrativeWorldMold::getTimePoint(int loc_id, double time) const{
	TimeStructure* time_struct = this->getTimeStruct(loc_id);
	if (time_struct == NULL)
		return NULL;
	//We are going to over-write if we have the whole structure
	std::map<double, RuleSet*>::const_iterator exists = time_struct->time_points.find(time);
if (exists == time_struct->time_points.end())
return NULL;
return (*exists).second;
}

//Returns the whole rule-set spatial constraint
RuleSet* NarrativeWorldMold::getSpatialConstraint(const std::string& loc_name) {
	return this->getSpatialConstraint(this->getLocationID(loc_name));
}

//Returns the whole rule-set spatial constraint
RuleSet* NarrativeWorldMold::getSpatialConstraint(int loc_id) {
	if (loc_id < 0)
		return NULL;
	std::map<int, RuleSet*>::const_iterator it = this->spatial_constraints.find(loc_id);
	if (it != this->spatial_constraints.end())
		return (*it).second;
	return NULL;
}
RuleSet* NarrativeWorldMold::getSuperset(int loc_id) { //Returns the superset of items, which is still a rule-set
	std::map<int, RuleSet*>::const_iterator it = this->location_superset.find(loc_id);
	if (it == this->location_superset.end())
		return NULL;
	return (*it).second;
}
//Returns the shadow vertices for a given superset as a copy of the orignial superset
RuleSet* NarrativeWorldMold::getShadowSuperSet(int loc_id) {
	RuleSet* superset = this->getSuperset(loc_id);
	if (superset == NULL) { //motif only locations wont have a superset or a shadow set
		return NULL;
	}
	//Now, we go through and replace all super-set items with their shadow set content variables
	//Create a copy so that we don't get confused
	RuleSet *shadow = new RuleSet((*superset),false);//We don't want to DCS it
	for (int i = 0; i < shadow->getNumVertices(); i++) {
		shadow->replaceVertex(shadow->getVertex(i), this->getShadowVertex(shadow->getVertex(i))); //Replaces one vertex with another
	}
	return shadow;
}
//Changes our shadow vertices for a given location into story vertices
void NarrativeWorldMold::inputStoryItems(int loc_id) {
	RuleSet* set = this->getSpatialConstraint(loc_id);
	RuleSet* superset = this->getSuperset(loc_id);
	RuleSet* shadow_set = this->getShadowSuperSet(loc_id);
	if (set == NULL || superset == NULL || shadow_set == NULL) {
		return;
	}
	//Now, we change each shadow vertex back into a superset vertex
	for (int i = 0; i < superset->getNumVertices(); i++) {
		set->replaceVertex(shadow_set->getVertex(i),superset->getVertex(i));
	}
}
//From a shadow vertex, return the story one
Vertex* NarrativeWorldMold::getStoryVertex(Vertex* shadow) {
	if(shadow == NULL)
		return NULL;
	for (std::map<Vertex*, Vertex*>::const_iterator it = this->story_items.begin(); it != this->story_items.end(); it++) {
		if ((*it).second == shadow)
			return (*it).first;
	}
	return NULL;
}
//If we have the story name, we can get the vertex
Vertex* NarrativeWorldMold::getStoryVertex(const std::string& name) {
	for (std::map<Vertex*, Vertex*>::const_iterator it = this->story_items.begin(); it != this->story_items.end(); it++) {
		if ((*it).first->getName() == name)
			return (*it).first;
	}
	return NULL;
}

Vertex* NarrativeWorldMold::getShadowVertex(Vertex* story_item) {
	if(story_item == NULL)
		return NULL;
	std::map<Vertex*, Vertex*>::const_iterator it = this->story_items.find(story_item);
	if (it == this->story_items.end())
		return NULL;
	return (*it).second;
}


/*Begin Value Checking*/
bool NarrativeWorldMold::hasLocationConnection(int loc_1, int loc_2) {
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range;
	range = this->location_graph.equal_range(loc_1);
	if (range.first == range.second)
		return false;
	for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
		if ((*it).second == loc_2)
			return true;
	}
	return false;
}
bool NarrativeWorldMold::isNarrativeLocation(int location_id, double time_point, RuleSet* test_set) {
	bool is_narrative_world = true;//Assume innocence until we know that it fails
	RuleSet* set = this->getSuperset(location_id); //This tells us the objects that we need
	TimeStructure* ts = this->getTimeStruct(location_id);//And this lets us know when 
	FactorGraph* superset = new FactorGraph(this->getSpatialConstraint(location_id));
	if (ts == NULL && set == NULL) { //Denotes a motif-only location
		return is_narrative_world; 
	}
	bool* is_INC = new bool[set->getNumVertices()];
	for (unsigned int i = 0; i < set->getNumVertices(); i++) {
		is_INC[i] = true;
	}
	//Now, we get the closest points without going over for that location
	RuleSet* narrative_objects = NULL;
	for (std::map<double, RuleSet*>::const_iterator it = ts->time_points.begin(); it != ts->time_points.end(); it++) {
		if ((*it).first <= time_point) {
			narrative_objects = (*it).second;
		}
	}
	//Now, we have two choices, either the objects are in the narrative world (test_set), or the objects
	//have a connection to the narrative world. First, we'll see if there are the objects that are 
	//suppose to be there
	for (unsigned int i = 0; i < narrative_objects->getNumVertices() && is_narrative_world; i++) {
		if (!test_set->hasVertex(this->getShadowVertex(narrative_objects->getVertex(i)))) {
			is_narrative_world = false;
		}
		is_INC[set->getVertex(narrative_objects->getVertex(i))] = false;
	}
	for (unsigned int i = 0; i < set->getNumVertices() && is_narrative_world; i++) {
		if (is_INC[i]) {
			//Now, we search for the vertices that could possibly be connected to those shadow objects
			bool found = false;
			Vertex* vert = superset->getVertex(set->getVertex(i)->getName());
			for (unsigned int j = 0; j < vert->getNumEdges() && !found; j++) {
				if (test_set->hasVertex(vert->getEdge(j))) {
					found = true;
				}
			}
			for (unsigned int j = 0; j < vert->getNumEdges(false) && !found; j++) {
				if (test_set->hasVertex(vert->getEdge(j))) {
					found = true;
				}
			}
			if (!found) {
				is_narrative_world = false;
			}

		}
	}
	//Now, we see if there is a connection for the objects that are not yet in the scene
	delete is_INC;
	delete superset;
	return is_narrative_world;
}

bool NarrativeWorldMold::hasStoryItem(Vertex* item) {
	if (item == NULL)
		return false; //We don't store NULLs
	if (this->story_items.find(item) == this->story_items.end())
		return false;
	return true;
}
bool NarrativeWorldMold::hasStoryItem(const std::string& name) {
	for (std::map<Vertex*,Vertex*>::const_iterator it = this->story_items.begin(); it != this->story_items.end(); it++) {
		if ((*it).first->getName() == name)
			return true;
	}
	return false;
}
//This is our realization functions
RuleSet* NarrativeWorldMold::getLocationAtTime(const std::string& loc_name, double tp, RuleSet* complete_set,bool cleanup_toggle) {
	return this->getLocationAtTime(this->getLocationID(loc_name), tp, complete_set, cleanup_toggle);
}
RuleSet* NarrativeWorldMold::getLocationAtTime(int loc_id, double tp, RuleSet* complete_set,bool cleanup_toggle) {
	//std::cout << "Cleanup Toggle is " << cleanup_toggle << std::endl;
	TimeStructure * ts = this->getTimeStruct(loc_id);
	if(complete_set == NULL)
		complete_set = this->getSpatialConstraint(loc_id);
	if (ts == NULL) {
		if (complete_set != NULL) {
			if (cleanup_toggle == true) {
				//Let's make sure we are working with a clean set
				complete_set->cleanRuleSetProb();
			}
		}
		return complete_set;//If set is null, then we get null. otherwise we are a motif only location
	}
	Vertex *story = new Vertex("Story");
	//If we have time_points, we should zero out the objects frequency that are in all other time_points
	//then we set frequency to what it is in the tp
	complete_set = new RuleSet((*complete_set), false); //copy it because we are changing it
	complete_set->cleanRuleSetProb();//Same here, before we remove the things, let's make sure we are working with a clean set
	RuleSet* good_set = NULL;
	if (ts->time_points.find(tp) != ts->time_points.end()) {
		good_set = ts->time_points[tp];
	}
	else {
		//We go through the list until we find a time point greater. The time-point before is our good set!
		bool too_high = false;
		for (std::map<double, RuleSet*>::iterator time_point = ts->time_points.begin(); time_point != ts->time_points.end() && !too_high; time_point++) {
			if (tp > (*time_point).first) {
				good_set = (*time_point).second;
			}
			else {
				too_high = true;
			}
		}

	}
	for (std::map<double, RuleSet*>::iterator time_point = ts->time_points.begin(); time_point != ts->time_points.end(); time_point++) {
		RuleSet* bad_set = (*time_point).second;
		if (bad_set != good_set) {
			//then, we need to find the binary ons and turn them off
			//Also, we know that they are in here
			for (int i = 0; i < bad_set->getNumVertices(); i++) {
				if (complete_set->getFrequency(complete_set->getVertex(bad_set->getVertex(i))) > 0.9999) {
					complete_set->addFrequency(complete_set->getVertex(bad_set->getVertex(i)), 0.0f);
				}
			}
		}
	}
	//Turn on the good ones
	if (good_set != NULL) {
		for (int i = 0; i < good_set->getNumVertices(); i++) {
			if (complete_set->getFrequency(complete_set->getVertex(good_set->getVertex(i))) < 0.0001) {
				complete_set->addFrequency(complete_set->getVertex(good_set->getVertex(i)), good_set->getFrequency(i));
				Content *c = complete_set->getContent(complete_set->getVertex(good_set->getVertex(i)));
				//And we set it to the shadow object as we are grounding
				if (c != NULL) {
					c->changeVertex(this->getShadowVertex(good_set->getVertex(i)));
					//c->addLeaf(0, story);
					//c->addLeaf(1, (this->getShadowVertex(good_set->getVertex(i)));
				}
			}
		}
	}
	return complete_set;
}
//This goes through each location in the motif, making sure we aren't adding in the objects represented
//by the shadow vertices
bool NarrativeWorldMold::cleanShadowVertices() {
	for (int i = 0; i < (int)this->locations.size(); i++) {
		RuleSet* set = this->getSpatialConstraint(i);
		if (set != NULL) {
			RuleSet* superset = this->getSuperset(i);
			//Now, unwanted shadow vertices are items in the story set that are not in the superset
			//So we have to figure out if we have it in the set and in the superset
			for (std::map<Vertex*, Vertex*>::const_iterator it = this->story_items.begin(); it != this->story_items.end(); it++) {
				if (superset == NULL || !(superset->hasVertex((*it).first))) { //Smaller set, so it goes first
					if (set->hasVertex((*it).second)) {
						Content *c = set->getContent((*it).second);
						if (c->getVertex() == (*it).second) {//Pointer comparison because we should not be deep copied
						}
						else { //It's a child
							int leaf_pos = c->getLeaf((*it).second);
							c->addLeafProbability(leaf_pos, c->getNumLeafProbability(leaf_pos) - 1, 0.0f);
						}
					}
				}
			}
		}
	}
	return true;
}

bool NarrativeWorldMold::removeTimePoint(int loc_id, double time_point) {
	TimeStructure * ts = this->getTimeStruct(loc_id);
	if (ts == NULL)
		return false;
	if (ts->time_points.find(time_point) == ts->time_points.end())
		return false;
	ts->time_points.erase(time_point);
	return true;
}
bool NarrativeWorldMold::removeTimePoint(int loc_id, int which) {
	TimeStructure * ts = this->getTimeStruct(loc_id);
	if (ts == NULL)
		return false;
	if (which > ts->time_points.size() || which < 0)
		return false;
	std::map<double, RuleSet*>::iterator it = ts->time_points.begin();
	advance(it, which);
	//Clean up the rule-set
	delete (*it).second;
	ts->time_points.erase(it);
	if (ts->time_points.empty()) {
	//If it's empty, then it's a motif room and should be removed
		delete ts;
		this->temporial_constraints.erase(loc_id);
	}
	return true;
}

/*Private Member Functions*/
TimeStructure* NarrativeWorldMold::addTemporialStructure(int loc_id) {
	TimeStructure* ts = new TimeStructure();
	this->temporial_constraints[loc_id] = ts;
	return ts;
}


//We sometimes want to get the recipe out of the system, which we do as a json
std::string NarrativeWorldMold::createJson() {
	std::stringstream result;
	int counter = 0;
	result << "{\n"<<"\"World\":[\n";
	//Describes each individual location
	for (int i = 0; i < this->locations.size(); i++) {
		result << "{";
		result << "\"name\":\"" << this->locations.at(i) << "\",\n";
		//Get the spatial component
		if (this->getSpatialConstraint(i) != NULL)
			result << "\"Spatial\":" << convertToJson(this->getSpatialConstraint(i))<<",\n";
		else
			result << "\"Spatial\":{},\n";
		if (this->getSuperset(i) != NULL)
			result << "\"SuperSet\":" << convertToJson(this->getSuperset(i)) << ",\n";
		else
			result << "\"SuperSet\":{},\n";
		if (this->getTimeStruct(i) != NULL) {
			result << "\"Time\":[\n";
			counter = 0;
			for (std::map<double, RuleSet*>::const_iterator it = this->getTimeStruct(i)->time_points.begin(); it != this->getTimeStruct(i)->time_points.end(); it++) {
				result << "{\"id\":" << (*it).first << ",\"ruleset\":" << convertToJson((*it).second) << "}";
				if (counter < this->getTimeStruct(i)->time_points.size() - 1) {
					result << ",\n";
				}
				else {
					result << "\n";
				}
				counter++;
			}
			result << "]\n";
		}
		else {
			result << "\"Time\":[]\n";
		}
		result << "}";
		if (i < this->locations.size() - 1) {
			result << ",\n";
		}
		else {
			result << "\n";
		}
	}
	//Describes connections between actions
	result << "],\n";
	result << "\"StoryItems\":" << "[\n";
	std::map<Vertex*, Vertex*>::const_iterator s_it = this->story_items.begin();
	while (s_it != this->story_items.end()) {
		result << "\t[\"" << (*s_it).first->getName() << "\",\"" << (*s_it).second->getName() << "\"]";
		s_it++;
		if (s_it != this->story_items.end()) {
			result << ",\n";
		}
		else {
			result << "\n";
		}
	}
	result <<"],\n\"Locations\":[\n";
	for (int i = 0; i < this->locations.size(); i++) {
		result << "\"" << this->locations.at(i) << "\"";
		if (i < this->locations.size() - 1) {
			result << ",\n";
		}
		else {
			result << "\n";
		}
	}
	counter = 0;
	result << "],\n" << "\"Connections\":[\n";
	for (std::multimap<int, int>::const_iterator it = this->location_graph.begin(); it != this->location_graph.end(); it++) {
		result << "{\"id\":" << "\"" << counter << "\"," << "\"source\":" << (*it).first ;
		result << ",\"target\":" << (*it).second <<"}";
		if (counter < this->location_graph.size() - 1) {
			result << ",\n";
		}
		else {
			result << "\n";
		}
		counter++;
	}
	result << "]\n";
	result << "}";
	return result.str();
}