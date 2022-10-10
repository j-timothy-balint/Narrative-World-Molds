#include "FactorGraph.h"
#include "RuleSet.h"
#include <iostream>
#include <fstream>
#include <cstdlib> //For srand
#include <ctime> //for time
#include<cctype> //For isdigit
#include <sstream> //for stringstream


#include "Database.h"
#include "SetTransformation.h"

#include "NarrativeWorldMold.h"
#include "NarrativeWorldMoldGenerator.h"

#include "KermaniSceneSynth.h"
#include "FisherTwelveSceneSynth.h"
#include "GraphComplete.h"
#include "RuleSetConverter.h"
#include "Support_Functions.h"
#include "NarrativeWorldGeneration.h"

#include "rapidjson/document.h"




void initializeVertexSetFromJson(const std::string& json_file, Database *db) { //I deleted my object database, and need to restore it
	std::ifstream myfile;
	std::stringstream lines;
	std::string line;
	myfile.open(json_file);
	if (myfile.is_open()) {
		while (getline(myfile, line)) {
			lines << line;
		}
	}
	rapidjson::Document reader;
	reader.Parse(lines.str().c_str());
	std::map<std::string, Vertex*> name_map;
	std::vector<Vertex*> counter_map;
	if (reader.HasMember("object_tree")) {
		const rapidjson::Value& object_tree = reader["object_tree"].GetObject();
		if (object_tree.HasMember("items")) {
			for (auto& member : object_tree["items"].GetArray()) {
				Vertex* vex = new Vertex(member.GetString());
				name_map[member.GetString()] = vex;
				counter_map.push_back(vex);
			}
		}
		if (object_tree.HasMember("connections")) {//Here, the item is a map
			for (auto& member : object_tree["connections"].GetObject()) {
				std::string key = member.name.GetString();
				std::string value = member.value.GetString();
				if (name_map.find(key) != name_map.end() && name_map.find(value) != name_map.end()) {
					name_map[key]->addEdge(name_map[value], false);
				}
			}
		}
	}
	for (int i = 0; i < counter_map.size(); i++) {
		db->addObject(counter_map[i], i);
	}
}

//Initializes the Generator 
void initializeGenerator(Database *db, NarrativeWorldMoldGenerator *gen,const std::list<std::string>& must_have_items_any) {
	//Add in our locational information
	RuleSet* set;
	int counter = 0;
	for (int i = db->getMinLocationID(); i < db->getMaxLocationID(); i++) {
		std::string name = db->getLocation(i);
		//std::cout << name << std::endl;
		set = createRuleSet(name, db);
		if (set != NULL) {
			bool has_items = false; //Checks to make sure we have at least one of the items
			for (std::list<std::string>::const_iterator it = must_have_items_any.begin(); it != must_have_items_any.end() && !has_items; it++) {
				if (set->hasVertex(db->getObjectByID(db->getObjects((*it))))) {
					has_items = true;
				}
			}
			if (!has_items) {
				set = combineRuleSetParents(name, db); //Usually, one of them will have the item
			}
			gen->addMotif(set);
			gen->addLocationMotif(i, counter);
			counter++;
		}
	}
}

//This generates the mold (recipe), saving the mold as a json string
__declspec(dllexport) std::string NarrativeWorldGeneration::generateNarrativeWorldMold(const std::vector<std::string>& location_graph, const std::vector<std::string>& time_file) {
	Database *db = new Database("NarrativeWorldMolds.db", true);
	//Set up the Generator
	NarrativeWorldMoldGenerator *gen = new NarrativeWorldMoldGenerator(db);
	//Add in our locational information
	RuleSet* set;
	int counter = 0;
	for (int i = db->getMinLocationID(); i < db->getMaxLocationID(); i++) {
		std::string name = db->getLocation(i);
		//std::cout << name << std::endl;
		set = createRuleSet(name, db);
		if (set != NULL) {
			gen->addMotif(set);
			gen->addLocationMotif(i, counter);
			counter++;
		}
	}
	//Build the world from the plot-points
	NarrativeWorldMold *recipe = gen->readInNarrativeWorldMold(location_graph, time_file);
	//Now, we want to make sure that most (if not all) locations from our narrative will have at least one location motif
	for (int i = 0; i < recipe->getNumLocations(); i++) {
		int loc_id = db->getLocationID(recipe->getLocation(i));
		int travel = db->getLocationID(recipe->getLocation(i));
		bool found = gen->hasLocationMotif(travel); //Not doing multiple ones :)
													//We add up all the motif trees as well
		while (travel != -1 && !found) {
			travel = db->getLocationParent(travel);
			//If this has a motif, add it to our travel as well
			if (gen->hasLocationMotif(travel)) {
				std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = gen->getLocationMotif(travel);
				for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
					gen->addLocationMotif(loc_id, (*it).second);
					found = true;
				}
			}
		}
	}
	for (int i = 0; i < recipe->getNumLocations(); i++) {
		//Search for the connection we need
		int loc_id = db->getLocationID(recipe->getLocation(i));
		std::cout << recipe->getLocation(i) << std::endl;
		RuleSet *tester = gen->genSuperSetLocation(loc_id, recipe->getShadowSuperSet(i), recipe);

		recipe->addSpatialConstraint(i, tester);
		if (tester != NULL) {
			recipe->inputStoryItems(i);
		}
	}
	recipe->cleanShadowVertices();
	std::string result = recipe->createJson();
	delete recipe;
	delete gen;
	delete db;
	return result;
}
//From our mold, we can create a narrative world
__declspec(dllexport) std::string NarrativeWorldGeneration::generateNarrativeWorld(const std::string& mold_json,double time_point,int num_objects) {
	Database *db = new Database("NarrativeWorldMolds.db", true);
	NarrativeWorldMoldGenerator *gen = new NarrativeWorldMoldGenerator(db);
	NarrativeWorldMold *mold = new NarrativeWorldMold(mold_json, db);
	//initializeGenerator(db, gen); //FIX
	
	std::stringstream output;
	SamplingConstraint *constraint;
	output << "[";
	for (int j = 0; j < mold->getNumLocations(); j++) {
		RuleSet *ground = mold->getSpatialConstraint(j);// TODO GROUND IN Recipe gen->GroundRuleSet(recipe->getSpatialConstraint(j), recipe, j);
		RuleSet *fin = mold->getLocationAtTime(mold->getLocation(j), time_point, ground);
		if (fin != NULL) {
			constraint = new SamplingConstraint(10000, fin);
			//constraint->convertRuleSet();
			int fail_counter = 0;
			fin = NULL;
			while (fin == NULL && fail_counter < 100) {
				fin = constraint->sampleConstraints(num_objects);
				fail_counter += 1;
				if (fail_counter % 10 == 0) { //I've written Kermani so that it can find nothing, which we want to try again for
					std::cout << "Failed on " << mold->getLocation(j) << " for " << fail_counter << " iterations" << std::endl;
				}
			}
			if (fin == NULL) {
				fin = new RuleSet(constraint->getRuleSet(),false);
			}
			std::string positions = createPositionFile(db, fin);
			output << "{\"location\":\"" << mold->getLocation(j) << "\",";
			output << " \"setup\":" << positions <<"}"; //positions is an object
			if (j != mold->getNumLocations() - 1) {
				output << ",\n";
			}
			delete constraint;
			if (fin != NULL) {
				delete fin;
			}
		}
	}
	output << "]";
	delete gen;
	delete mold;
	delete db;
	return output.str();
}

void convertAndGenerate(Database* db, std::string name) {
	RuleSet* set= createRuleSet(name,db);
	std::map<std::string, RuleSet*> maps;
	//KermaniTest(db,set, name, "C:\\Users\\timmy\\Documents\\Computer Science\\Narrative World\\MotifCreation\\FactorGraph\\MainProject\\Kermani\\");
	//GraphCompleteTest(db,set, name, "C:\\Users\\timmy\\Documents\\Computer Science\\Narrative World\\MotifCreation\\FactorGraph\\MainProject\\SceneSeer\\");
	maps[name] =  convertRuleSet(set);
	//db->deleteMotif(name);
	std::string newname = name + "_general";
	KermaniTest(db,maps[name], newname, "C:\\Users\\jbalint\\Documents\\Workspace\\Narrative World\\MotifCreation\\FactorGraph\\MainProject\\Kermani\\");
	GraphCompleteTest(db,maps[name], newname, "C:\\Users\\jbalint\\Documents\\Workspace\\Narrative World\\MotifCreation\\FactorGraph\\MainProject\\SceneSeer\\");
	delete maps[name];
}

/*************************************************************************
 *Trans Games Narrative World Mold Test, which tests out each component of
 *the narrative world mold model (STIF creation, motif expansion, Implied
 *necessary content, and XOR removal). The input to this function is as 
 *follows:
 *db (Database *): Which knowledge base should be used for testing
 *with_addition (bool): Testing for motif-expansion in the graph
 *with_temporial (bool): Testing if spatial-temporal inventory changes are tracked in the graph
 *with_cleanup (bool): Determines when to perform XOR removal
 *motif_only (bool): In this case, the narrative will simply be ignored
 *
 *Outputs several created rooms to specified (hardcoded) folders for 
 *whichever graph connection and narrative table is used. 
 *************************************************************************/
void NarrativeWorldMoldTest(Database *db, bool with_addition = true, bool with_temporial = true, bool with_cleanup = true,bool motif_only = false) {
	NarrativeWorldMoldGenerator * gen = new NarrativeWorldMoldGenerator(db);
	std::string saveout;
	std::ofstream myfile;

	std::list<std::string> anchors;
	anchors.push_back("wall");
	anchors.push_back("room");
	anchors.push_back("floor");
	initializeGenerator(db, gen,anchors);
	//Read in location file
	std::ifstream reader;
	std::string line;
	reader.open("../graph_connections.txt"); //At some point, the hardcoding should be removed
	std::vector<std::string> location_lines;
	if (reader.is_open()) {
		while (getline(reader, line)) {
			location_lines.push_back(line);
		}
		reader.close();
	}

	reader.open("../narrative_table.txt");
	std::vector<std::string> narrative_lines;

	if (reader.is_open()) {
		while (getline(reader, line)) {
			narrative_lines.push_back(line);
		}
		reader.close();
	}

	//we also need their parent information
	//Build the world from the plot-points
	NarrativeWorldMold *recipe = gen->readInNarrativeWorldMold(location_lines,narrative_lines,motif_only);
	//Now, we want to make sure that most (if not all) locations from our narrative will have at least one location motif
	for (int i = 0; i < recipe->getNumLocations(); i++) {
		int loc_id = db->getLocationID(recipe->getLocation(i));
		int travel = db->getLocationID(recipe->getLocation(i));
		bool found = gen->hasLocationMotif(travel); //Not doing multiple ones :)
		//We add up all the motif trees as well
		while (travel != -1 && ! found) {
			travel = db->getLocationParent(travel);
			//If this has a motif, add it to our travel as well
			if (gen->hasLocationMotif(travel)) {
				std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> range = gen->getLocationMotif(travel);
				for (std::multimap<int, int>::const_iterator it = range.first; it != range.second; it++) {
					gen->addLocationMotif(loc_id, (*it).second);
					found = true;
				}
			}
		}
	}
	if (!with_temporial) {
		for (int i = 0; i < recipe->getNumLocations(); i++) {
			for (int j = recipe->getNumTimePoints(i) -1; j >=0; j--) {
				if (recipe->getTime(i, j) != -1) {
					recipe->removeTimePoint(i, j);
				
				}
			}
			//Also, remove all superset items not in our time points
			recipe->addSuperset(i, recipe->getTimePoint(i, -1));
		}
	}
	if (motif_only) {
		for (int i = 0; i < recipe->getNumLocations(); i++) {
			for (int j = recipe->getNumTimePoints(i) - 1; j >= 0; j--) {
				recipe->removeTimePoint(i, j);
			}
			recipe->addSuperset(i, NULL);
		}
	}
	for (int i = 0; i < recipe->getNumLocations(); i++) {
		//Search for the connection we need
		int loc_id = db->getLocationID(recipe->getLocation(i));
		std::cout << recipe->getLocation(i) << std::endl;
		RuleSet *tester = gen->genSuperSetLocation(loc_id, recipe->getShadowSuperSet(i), recipe, with_addition);

		recipe->addSpatialConstraint(i,tester);
		if (tester != NULL) {
			recipe->inputStoryItems(i);
		}
	}

	saveout.clear();
	recipe->cleanShadowVertices();
	saveout = recipe->createJson();
	//myfile.open("heist_narrative.json");
	//myfile.open("princess_narrative.json");
	//myfile << saveout;
	//myfile.close(); 
	if (with_addition && with_temporial && with_cleanup) {
		narrativeWorldDifferenceTest("E:\\Generated Data\\Narrative Worlds Mold Test\\Heist\\Full\\Kermani_first\\", recipe, gen,db,with_cleanup);
		narrativeWorldDifferenceTestGraph("E:\\Generated Data\\Narrative Worlds Mold Test\\Heist\\Full\\SceneSeer_first\\", recipe, gen, db, with_cleanup);
	}
	else if (!with_addition) {
		narrativeWorldDifferenceTest(     "E:\\Generated Data\\Narrative Worlds Mold Test\\Heist\\No Add\\Kermani_first\\", recipe, gen, db, with_cleanup);
		narrativeWorldDifferenceTestGraph("E:\\Generated Data\\Narrative Worlds Mold Test\\Heist\\No Add\\SceneSeer_first\\", recipe, gen, db, with_cleanup);
	}
	/*else if (!with_temporial){
		narrativeWorldDifferenceTest("E:\\Generated Data\\NarrativeWorldMoldTest\\heist\\No Time\\Kermani_first\\", recipe, gen, db, with_cleanup);
		narrativeWorldDifferenceTestGraph("E:\\Generated Data\\Narrative Worlds Mold Test\\Heist\\No Time\\SceneSeer_first\\", recipe, gen, db, with_cleanup);
	}
	else if (motif_only) {
		narrativeWorldDifferenceTest("C:\\Users\\timmy\\Documents\\Computer Science\\Narrative World\\MotifCreation\\FactorGraph\\MainProject\\Kermani_first\\", recipe, gen, db, with_cleanup);
		narrativeWorldDifferenceTestGraph("C:\\Users\\timmy\\Documents\\Computer Science\\Narrative World\\MotifCreation\\FactorGraph\\MainProject\\SceneSeer_first\\", recipe, gen, db, with_cleanup);
	}*/
	else {
		//std::cout << "Cleanup toggle is " << with_cleanup << std::endl;
		narrativeWorldDifferenceTest(     "E:\\Generated Data\\Narrative Worlds Mold Test\\Heist\\No Remove\\Kermani_first\\", recipe, gen, db, with_cleanup);
		narrativeWorldDifferenceTestGraph("E:\\Generated Data\\Narrative Worlds Mold Test\\Heist\\No Remove\\SceneSeer_first\\", recipe, gen, db, with_cleanup);
	}
	delete gen;
	db->cleanObjects();
}


int main() {
	srand((unsigned int)time(NULL));
	//Next, we test out the whole factor graph
	Database *db = new Database("NarrativeWorldMolds.db", true);
	//for (int i = db->getMinLocationID(); i < db->getMaxLocationID(); i++) {
	//	std::string name = db->getLocation(i);
	//	convertAndGenerate(db, name);
	//}
	//GraphCompleteTest(db);
	//initializeVertexSetFromJson("../saava.json",db);
	//addMeshSizeSet(db, "../../sizes.csv");
	/*std::map<std::string, RuleSet*> maps = parseCSVRuleSet("../../SUN RGBD/mining.csv",false);
	std::map<std::string, RuleSet*> support_maps = parseCSVRuleSet("../../SUN RGBD/support_mining.csv",false);
	for (std::map<std::string, RuleSet*>::iterator it = support_maps.begin(); it != support_maps.end(); it++) {
		std::map<std::string, RuleSet*>::iterator it2 = maps.find((*it).first);
		if (it2 == maps.end())
			maps[(*it).first] = (*it).second;
		else {
			RuleSet test = (*(*it).second) + (*(*it2).second);
			maps[(*it).first] = new RuleSet(test);
		}
	}*/
	//std::map<std::string, RuleSet*> maps = parseCSVRuleSet("../../SUN RGBD/SUNCG_scene_suggest.csv",true);
	//populateDatabase(db, maps);
	//std::string return_file = createPositionFile(db, maps["bedroom"]);
	//std::ofstream myfile;
	//myfile.open("test.txt");
	//myfile << return_file;
	//myfile.close();
	//std::map<std::string, RuleSet*> maps;
	//calculateAverageDegree(db,maps);
	//std::multimap<std::string, std::string> adjectives = getAdjectives("D:\\data\\parsed ALET data\\wikipedia_adjectives.csv", 800);
	/*Vertex *vec = db->getObjectByID(990);
	while (vec != NULL) {
		std::pair<std::multimap<std::string, std::string>::iterator, std::multimap<std::string, std::string>::iterator> range = adjectives.equal_range(vec->getName().substr(0,vec->getName().find(".")));
		for (std::multimap<std::string, std::string>::iterator it = range.first; it != range.second; it++)
			std::cout << (*it).second << " ";
		vec = vec->getEdge(0, false);
	}*/
	/*std::ifstream ifs("heist_narrative.json");
	std::stringstream content;
	std::string some_string;
	while (getline(ifs, some_string)) {
		content << some_string;
	}
	std::string test = NarrativeWorldGeneration::generateNarrativeWorld(content.str(), -1, 5);
	std::ofstream ofs("output.json");
	ofs << test;
	ofs.close();*/
	//similarityData data = calculateLocationSimilarity(db);
	//std::string result = motifsAsJson(db);
	////std::ofstream results;
	//results.open("test.json");
	//results << result;
	//results.close();
	
	/*results << ",";
	for (int i = 0; i < data.location_names.size(); i++) {
		results << data.location_names.at(i);
		if (i < data.location_names.size() - 1) {
			results<< ",";
		}
		else {
			results << "\n";
		}
	}
	for (int i = 0; i < data.location_names.size(); i++) {
		results << data.location_names.at(i) <<",";
		for (int j = 0; j < data.location_names.size(); j++) {
			results << data.data[i][j];
			if (j < data.location_names.size() - 1) {
				results << ",";
			}
			else {
				results << "\n";
			}
		}
	}
	results.close();*/
	std::cout << "Full" << std::endl;
	NarrativeWorldMoldTest(db);
	std::cout << "No Additional Structure" << std::endl;
	NarrativeWorldMoldTest(db,false);
	//std::cout << "No Additional Time" << std::endl;
	//NarrativeWorldMoldTest(db, true, false);
	std::cout << "No Removal of items" << std::endl;
	NarrativeWorldMoldTest(db, true, true,false);
	//std::cout << "Motif Only" << std::endl;
	//NarrativeWorldMoldTest(db, true, true, false,true);
	//std::cout << "Calculating Kermani Motifs" << std::endl;
	//WorldTest(db);
	/*std::list<ConstraintStruct> constraints = getConstraints(db, "../../SUN RGBD/Kermani_Bounds.csv");
	populateConstraints(db, constraints,1);*/
	
	//addVertexSet("../../parents.txt", db);
	//Comparision between what we had and what we saved
	//std::list<Vertex*> objs;
	//std::multimap<std::string, int> converted_data;
	//std::vector<std::string> rooms;
	//std::vector<std::map<int, int> > hists;
	//int i = 45; //Bedroom
	//for (int i = 43; i < 92; i++) {
	//	std::string name = db->getLocation(i);
	//	std::cout << name << std::endl;
	//	RuleSet *set = createRuleSet(name, db);
	//	maps[name] = set;
		/*FactorGraph* graph = new FactorGraph(set);
		objs.clear();
		for (int j = 0; j < set->getNumVertices(); j++) {
			Vertex *v = set->getVertex(j);
			if(v != NULL)
				objs.push_back(v);
		}
		std::list<int> actions = getActionsFromObjSet(objs, adjectives);
		for (std::list<int>::iterator it = actions.begin(); it != actions.end(); it++) {
			if (hasTranformationPath((*it), graph, objs, db, db->getLocation(i),adjectives) > 0)
				std::cout << reader->getFrame((*it)) << " has a transformation path\n";
			//else
				//std::cout << reader->getFrame((*it).first) << " None\n";
		}*/
	//}
	
	
	delete db;
	system("PAUSE");
	return 0;
}