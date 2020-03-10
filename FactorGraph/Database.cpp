#include "Database.h"
#include <sstream> //For constructing our 
#include <map>
#include <list>
#include <set>
#include <iostream>

struct sql_row{
	std::map<std::string, std::string> data;
};

static int sql_callback(void *data, int argc, char **argv, char **col_name) { //Taken from the tutorial on sqlite. This is called for each row, and so adds that data
	std::list<sql_row> *res = reinterpret_cast<std::list<sql_row> *>(data);
	sql_row row;
	for (int i = 0; i < argc; i++) {
		row.data[col_name[i]] = argv[i] ? argv[i] : "";
	}
	res->push_back(row);
	return 0;
}
//This allows to get back a single piece of data
//still need to limit in queries, but it's good practice to do that anyways
static int sql_callback_single(void *data, int argc, char**argv, char **col_name) { 
	std::string &res = *static_cast<std::string*>(data);
	res = std::string(argv[0]? argv[0]: "");
	return 0;
}
//Creates a connection to our database
Database::Database(const std::string& file,bool populate):db(NULL), ErrMsg(NULL){
	int rc = sqlite3_open(file.c_str(), &this->db); //Creates a connection to the database
	if (rc != SQLITE_OK) {
		//Do error checking here
		//ErrMsg = sqlite3_errmsg(db);
		return;
	}
	if (populate)
		this->populateSharedData();
}

//Closes the database connection and sets the pointer to NULL
Database::~Database(){
	sqlite3_close(db);
	db = NULL;
	//And clean up our base data-structures
	for (std::map<int, Vertex*>::iterator it = this->objects.begin(); it != this->objects.end(); it++) {
		delete (*it).second;
	}
	this->objects.clear();
	for (std::map<int, Factor*>::iterator it = this->factors.begin(); it != this->factors.end(); it++) {
		delete (*it).second;
	}
	this->objects.clear();
	for (std::map<int, Operational*>::iterator it = this->actions.begin(); it != this->actions.end(); it++) {
		delete (*it).second;
	}
	this->objects.clear();
}
//Populates shared information between our data-structures
void Database::populateSharedData() {
	//First, we add in the objects
	std::list<sql_row> res;
	std::string query = "SELECT * from Objects WHERE 1 ORDER BY Parent";
	int rc = sqlite3_exec(this->db, query.c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return;
	}
	for (std::list<sql_row>::iterator it = res.begin(); it != res.end(); it++) {
		//For now we assume that parents are higher in the list!
		int id = atoi((*it).data["ObjectsID"].c_str());
		this->objects[id] = new Vertex((*it).data["Name"]);
		//Here we do parents
		int par_id = atoi((*it).data["Parent"].c_str());
		if (this->objects.find(par_id) != this->objects.end())
			this->objects[par_id]->addEdge(this->objects[id],true);
			//this->objects[id]->addEdge(this->objects[par_id], false);
	}
	res.clear();
	query = "SELECT * from Factor WHERE 1";
	rc = sqlite3_exec(this->db, query.c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return;
	}
	for (std::list<sql_row>::iterator it = res.begin(); it != res.end(); it++) {
		//For now we assume that parents are higher in the list!
		int id = atoi((*it).data["FactorID"].c_str());
		int params = atoi((*it).data["Params"].c_str());
		bool exist = false;
		if (atoi((*it).data["Existance"].c_str()) == 1)
			exist = true;
		this->factors[id] = new Factor(params,(*it).data["Name"],exist);
	}

}
std::string Database::getAllParentIds(Vertex* v) {
	std::stringstream res;
	res << "(";
	Vertex *traveler = v;
	int id = this->getObjects(traveler);
	while (id != -1) {
		res << id;
		traveler = this->getParentObject(id);
		id = this->getObjects(traveler);
		if (id != -1) {
			res << ",";
		}
	}
	res << ")";
	return res.str();
}
//There are times when data that exists in the database (i.e. the object tree or motifs) is changed. This rectifies that
void Database::cleanObjects() {
	std::set<Vertex*> all_verts;
	std::set<Vertex*> db_verts;
	//Go through each object, and add it and it's children to the set
	for (std::map<int, Vertex*>::const_iterator it = this->objects.begin(); it != this->objects.end(); it++) {
		all_verts.insert((*it).second);
		db_verts.insert((*it).second);
		for (int i = 0; i < (*it).second->getNumEdges(); i++) {
			Vertex * v = (*it).second->getEdge(0, true, i);
			if(v != NULL)
				all_verts.insert(v);
		}
	}
	//The mis-matches between the two sets are what need to be removed
	std::set<Vertex*> diff_set;
	for (std::set<Vertex*>::iterator it = all_verts.begin(); it != all_verts.end(); it++) {
		if (db_verts.find((*it)) == db_verts.end()) {
			diff_set.insert((*it));
		}
	}
	for (std::set<Vertex*>::iterator it = diff_set.begin(); it != diff_set.end(); it++) {
		//Remove all connections
		for (int i = 0; i < (*it)->getNumEdges(); i++) {
			Vertex * u = (*it)->getEdge(0, true, i);
			(*it)->removeEdge(u);
		}
		for (int i = 0; i < (*it)->getNumEdges(false); i++) {
			Vertex * u = (*it)->getEdge(i,false);
			u->removeEdge((*it));
		}
		delete (*it);
	}
}
//Adds a factor to the database
bool Database::addFactor(Factor* fact) {
	if (fact == NULL) { //Don't add if it's null
		return false;
	}
	//Also don't add if it's already there
	if (this->getFactor(fact) != -1)
		return false;
	std::list<sql_row> res;
	std::stringstream query;
	int existance = 0;
	if (fact->getExistance())
		existance = 1;
	query << "INSERT INTO Factor (Name,Params,Existance) VALUES('" << fact->getName() << "'," << fact->getNumComponents() << ","<<existance<<");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	return true;
}
//Adds a vertex (an object) to the database
bool Database::addObject(Vertex* obj, int pos) {
	if (obj == NULL) {
		return false;
	}
	if (this->getObjects(obj) != -1) //It exists or there is some problem that will prevent us from adding it
		return false;
	std::stringstream query;
	if (pos == -1) {
		//Here, we get the new max
		std::string max_string = "SELECT MAX(ObjectsID) FROM Objects LIMIT 1";
		std::string res;
		int rc = sqlite3_exec(this->db, max_string.c_str(), sql_callback_single, &res, &this->ErrMsg);
		if (rc != SQLITE_OK) {
			sqlite3_free(this->ErrMsg);
			return false;
		}
		pos = atoi(res.c_str()) + 1;
	}
	if (obj->getNumEdges(false) > 0) {
		int id = this->getObjects(obj->getEdge(0, false));
		query << "INSERT INTO Objects (ObjectsID, Name,Parent) VALUES("<<pos<<",'"<< obj->getName() << "',"<<id<<");";
	}
	else {
		query << "INSERT INTO Objects (ObjectsID,Name) VALUES("<<pos<<",'" << obj->getName() << "');";
	}
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	this->objects[pos] = obj; //If we have to add it, we should remember it
	return true;
} 
bool Database::addMotifObject(const std::string& name, Vertex* obj,int pos, float frequency) {
	std::stringstream query;
	int location_id = this->getLocationID(name);
	int vertex_id = this->getObjects(obj);
	if ( location_id == -1 || vertex_id == -1) //We didn't find our connections!
		return false; //Already there, so no need to add
	query << "INSERT INTO MotifObjects (LocationID, ObjectsID,ObjectPosition) VALUES(" 
		  << location_id << ","<<vertex_id<<","<<pos<<");";
	query <<"INSERT INTO MotifObjectsFrequency (LocationID,ObjectPosition,Frequency) VALUES ("
		<< location_id << "," << pos << "," << frequency << ");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	return true;
}
bool Database::addLocation(const std::string& name) {
	std::stringstream query;
	if (this->getLocationID(name) != -1)
		return false; //Already there, so no need to add
	query << "INSERT INTO Locations (Name) VALUES('" << name << "');";
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	return true;
}

bool Database::addMotifRule(const std::string& location, int position, Factor* fact, float prob) {
	int loc_id  = this->getLocationID(location);
	int fact_id = this->getFactor(fact);
	if (loc_id == -1 || fact_id == -1 || position == -1)
		return false;
	std::stringstream query;
	query << "INSERT INTO MotifRule (LocationID,RuleID,FactorID,Probability) "
		<<"VALUES("<<loc_id<<","<<position<<","<<fact_id<<","<<prob<<");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	return true;

}

bool Database::addMotifVariable(const std::string& location, int rule_pos, int var_pos, int vert_pos) {
	if (rule_pos < 0 || var_pos < 0) //Can also add in a pos check per factor
		return false;
	int loc_id = this->getLocationID(location);
	if (vert_pos == -1 || loc_id == -1)
		return false;

	std::list<sql_row> res;
	std::stringstream query;
	query << "INSERT INTO MotifVariable (LocationID,RuleID,VariablePos,ObjectsID) "
		<< " VALUES(" << loc_id << "," << rule_pos << "," << var_pos << "," << vert_pos << ");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	return true;

}

//Adds a constraint to the database
bool Database::addConstraintParam(int shape_id, double p1, double p2, double p3, double p4) {
	std::stringstream query;
	query << "INSERT INTO ConstraintParams (ShapeID,Param1,Param2,Param3,Param4) "
		<< "VALUES(" << shape_id << "," << p1 << "," << p2 << "," << p3 << "," << p4 << ");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	return true;
}

//Adds a complex operational constrant to the database
//loc_id may refer to either the operational action (for the case of the narrative) or the location (for the case of the motif)
bool Database::addOperationalConstruction(int loc_id, int prule_id, int prule_pos, int crule_id) {
	std::stringstream query;
	//For now, we only check the motif
	if (this->getLocation(loc_id) == "")
		return false;
	if (this->getMotifRuleFactor(loc_id, crule_id) == NULL)
		return false;
	query << "INSERT INTO OperationalConstruction (LocationID, ParentRuleID, ParentRulePos, ChildRuleID) "
		<< "VALUES(" << loc_id << "," << prule_id << "," << prule_pos << "," << crule_id << ");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	return true;
}
bool Database::addRuleConstraint(int obj1, int obj2, int constraint_id) {
	std::stringstream query;
	//For now, we only check the motif
	if (this->getObjectByID(obj1) == NULL || this->getObjectByID(obj2) == NULL)
		return false;
	if (this->getConstraintParameter(constraint_id, 0) == -9999999999)
		return false;
	query << "INSERT INTO RuleConstraint (ObjectsIDFirst, ObjectsIDSecond, ConstraintID) "
		<< "VALUES(" << obj1 << "," << obj2 << "," << constraint_id <<");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	return true;
}
bool Database::addMesh(Vertex* vert, const std::string& mesh_name, double x, double y, double z) {
	std::stringstream query;
	//For now, we only check the motif
	if (vert == NULL)
		return false;
	if (this->getMeshName(vert,false) != "")
		return false;
	query << "INSERT INTO Mesh (ObjectsID, MeshName, MeshLength, MeshWidth, MeshHeight) "
		<< "VALUES(" << this->getObjects(vert) << ",'" << mesh_name << "'," << x <<" , " << y << " , " << z << ");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), NULL, 0, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	return true;
}
/*This is the start of retreiving information from the database*/
//Retreives an action ID from the relational database if it exists
/*int Database::getAction(Operational* act) {
	if (act == NULL) {
		return -1;
	}
	std::list<sql_row> res;
	std::stringstream query;
	query << "SELECT ActionsID from Actions WHERE Name = '" << act->getName() << "' and NeededParams = " << act->getNumComponents()
		  << " AND AdditionalParams = "<<act->getOptionalComponents();
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res != "") {
		return atoi(res.front().data["ActionsID"].c_str()); //Already exists
	}
	return -1;
}*/
//Gets the index of the factor (so we can add in Motif Rules to our database)
int Database::getFactor(Factor* fact) {
	if (fact == NULL) {
		return -1;
	}
	//Otherwise, get the id if it exists
	std::string res;
	std::stringstream query;
	query << "SELECT FactorID from Factor WHERE Name = '" << fact->getName() << "' and Params = " << fact->getNumComponents() <<" LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return -1;
}
//Gets the index based on the name only and not the number of parameters
int Database::getFactor(const std::string& name) {
	//Otherwise, get the id if it exists
	std::string res;
	std::stringstream query;
	query << "SELECT FactorID from Factor WHERE Name = '" << name <<"' LIMIT 0,1";
	//std::cout << query.str() << std::endl;
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return -1;
}
//Gets the number of factor parameters
int Database::getFactorParameters(int id) {
	//Otherwise, get the id if it exists
	std::string res;
	std::stringstream query;
	query << "SELECT Params from Factor WHERE FactorID = " << id << " LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return -1;
}
//Returns true if the factor is an existance factor
bool Database::getFactorExistance(int id) {
	//Otherwise, get the id if it exists
	std::string res;
	std::stringstream query;
	query << "SELECT FactorID from Factor WHERE FactorID= " << id << " LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	if (res != "") {
		int exist = atoi(res.c_str()); //Already exists
		return exist != 0;
	}
	return false;
}

Factor* Database::getFactorByID(int id) {
	if (this->factors.find(id) != this->factors.end())
		return this->factors[id];
	std::string res;
	std::stringstream query;
	query << "SELECT Name from Factor where FactorID = " << id << " LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return NULL;
	}
	if (res != "") {
		//We have it in database but not in our lookup table
		Factor* factor = new Factor(this->getFactorParameters(id),res,this->getFactorExistance(id));
		this->factors[id] = factor;
		return factor;
	}
	return NULL;
}

int Database::getNumberObjects() {
	std::string res;
	std::stringstream query;
	query << "SELECT COUNT(*) as num_objects from Objects";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return 0;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return 0;
}
int Database::getMaxObjectID() {
	std::string res;
	std::stringstream query;
	query << "SELECT MAX(ObjectsID) as num_objects from Objects";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return 0;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return 0;
}
//Gets the index of our object
int Database::getObjects(Vertex* obj) {
	if (obj == NULL) {
		return -1;
	}
	return this->getObjects(obj->getName());
}

int Database::getObjects(const std::string& name) {
	std::string res;
	std::stringstream query;
	query << "SELECT ObjectsID from Objects where Name = '" << name << "' LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return -1;
}
//Returns the object based on the the id. Does two checks, one in the populated db and one in the written db
Vertex* Database::getObjectByID(int id) {
	if (this->objects.find(id) != this->objects.end())
		return this->objects[id];
	std::string res;
	std::stringstream query;
	query << "SELECT Name from Objects where ObjectsID = " << id << " LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return NULL;
	}
	if (res != "") {
		//We have it in database but not in our lookup table
		Vertex * vert = new Vertex(res);
		this->objects[id] = vert;
		return vert;
	}
	return NULL;
}
//Returns the parent vertex
Vertex* Database::getParentObject(Vertex* obj) {
	if (obj == NULL || obj->getEdge(0, false) == NULL) //If the parent doesn't exist, then we cannot get it
		return NULL;
	if (this->getObjects(obj->getEdge(0, false)) != -1)//if the parent does exist, then we just use that
		return obj->getEdge(0, false);
	return this->getParentObject(obj->getEdge(0, false));
}
//Returns the parent vertex when we know the id
Vertex* Database::getParentObject(int id) {
	Vertex *obj = this->getObjectByID(id);
	return this->getParentObject(obj);
	if (obj == NULL || obj->getEdge(0, false) == NULL)
		return NULL;
	if (this->getObjects(obj->getEdge(0, false)) != -1)
		return obj->getEdge(0, false);
	return this->getParentObject(obj->getEdge(0, false));
}
//Gets the name of the location based on the index
std::string Database::getLocation(int id) {
	std::string res;
	std::stringstream query;
	query << "SELECT Name from Locations WHERE LocationsID = " << id << " LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return "";
	}
	if (res != "") {
		return res; //Already exists
	}
	return "";
}
//Gets the location index based on the id
int Database::getLocationID(const std::string& name) {
	std::string res;
	std::stringstream query;
	query << "SELECT LocationsID from Locations WHERE Name = '" << name << "' LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return -1;
}
int Database::getMinLocationID() {
	std::string res;
	std::stringstream query;
	query << "SELECT MIN(LocationsID) as lid from Locations;";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return -1;
}
int Database::getMaxLocationID() {
	std::string res;
	std::stringstream query;
	query << "SELECT MAX(LocationsID) as lid from Locations;";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return -1;
}
//Gets the location's parent based on the location itself
int Database::getLocationParent(int loc_id) { 
	std::string res;
	std::stringstream query;
	query << "SELECT Parent from Locations WHERE LocationsID = " << loc_id << " LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res != "") {
		return atoi(res.c_str()); //Already exists
	}
	return -1;
}
//Gets the factor associated with the rule
Factor* Database::getMotifRuleFactor(int locationsID, int pos) {
	if (locationsID == -1 || pos == -1)
		return NULL;
	std::list<sql_row> res;
	std::stringstream query;

	query << "SELECT Factor.FactorID,Factor.Name,Factor.Params from Factor NATURAL JOIN MotifRule WHERE "
		  << "MotifRule.LocationID = " << locationsID << " AND MotifRule.RuleID =" << pos;
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return NULL;
	}
	if (res.size() == 0)
		return NULL;
	int fact_id = atoi(res.front().data["FactorID"].c_str());
	if (this->factors.find(fact_id) != this->factors.end())
		return this->factors[fact_id]; //Don't create a factor if we already have one
	Factor *fact = new Factor(atoi(res.front().data["Params"].c_str()), res.front().data["Name"]);
	this->factors[fact_id] = fact;
	return fact;
}
Vertex* Database::getMotifRuleVariable(int locationsID, int rule_pos, int var_pos) {
	if (locationsID == -1 || rule_pos == -1 || var_pos == -1)
		return NULL;
	std::list<sql_row> res;
	std::stringstream query;

	query<< " SELECT Objects.ObjectsID, Objects.Name FROM OBJECTS INNER JOIN "
		 << " (SELECT MotifVariable.LocationID, RuleID, VariablePos, MotifObjects.ObjectsID from MotifVariable INNER JOIN MotifObjects ON "
		 << " MotifVariable.LocationID = MotifObjects.LocationID  AND MotifVariable.ObjectsID = MotifObjects.ObjectPosition) AS MV ON Objects.ObjectsID = MV.ObjectsID WHERE "
		 << " MV.LocationID = " << locationsID << " AND MV.RuleID =" << rule_pos
		 << " AND MV.VariablePos = "<<var_pos;
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return NULL;
	}
	if (res.size() == 0)
		return NULL;
	int obj_id = atoi(res.front().data["ObjectsID"].c_str());
	if (this->objects.find(obj_id) != this->objects.end())
		return this->objects[obj_id]; //Don't create a factor if we already have one
	Vertex *obj = new Vertex(res.front().data["Name"]);
	this->objects[obj_id] = obj;
	return obj;
}
int Database::getMotifRuleVariablePos(int locationsID, int rule_pos, int var_pos) {
	if (locationsID == -1 || rule_pos == -1 || var_pos == -1)
		return -1;
	std::string res;
	std::stringstream query;

	query << "SELECT ObjectsID FROM MotifVariable"
		<< " WHERE LocationID = " << locationsID
		<< " AND RuleID = " << rule_pos
		<< " AND VariablePos = " << var_pos;
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res.size() == 0)
		return -1;
	int obj_id = atoi(res.c_str());
	return obj_id;
}
//Gets the number of motif objects per location
int Database::getNumMotifObjects(int locationsID) {
	if (locationsID == -1)
		return -1;
	std::string res;
	std::stringstream query;
	query << "SELECT COUNT(*) as num_objects from MotifObjects" 
		  << " WHERE LocationID = " << locationsID;
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	return atoi(res.c_str());

} 
//Gets the number of motif objects per location and fragment
int Database::getNumMotifObjects(int locationsID, int fragmentID) {
	if (locationsID == -1)
		return -1;
	std::string res;
	std::stringstream query;
	query << "SELECT COUNT(DISTINCT ObjectsID) AS num_objects FROM MotifVariable"
		<< " WHERE LocationID = " << locationsID
		<< " AND RuleID IN ( SELECT ChildRuleID from OperationalConstruction WHERE LocationID = " << locationsID
		<< " AND ParentRuleID = " << fragmentID << ");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	return atoi(res.c_str());

}
//Provides a motif object based on the location and position
Vertex* Database::getMotifObject(int locationsID, int pos) {
	if (locationsID == -1 || pos == -1)
		return NULL;
	std::string res;
	std::stringstream query;
	query << "SELECT ObjectsID from MotifObjects" 
		 << " WHERE LocationID = " << locationsID << " LIMIT " << pos<<",1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return NULL;
	}
	if (res.size() == 0)
		return NULL;
	int obj_id = atoi(res.c_str());
	if (this->objects.find(obj_id) == this->objects.end())
		return NULL;
	return this->objects[obj_id];
}
Vertex* Database::getMotifObject(int locationsID, int fragmentID, int pos) {
	if (locationsID < 0 || fragmentID < 0 || pos <0)
		return NULL;
	int obj_pos_id = this->getMotifObjectID(locationsID, fragmentID, pos);
	if (obj_pos_id < 0)
		return NULL;
	std::list<sql_row> res;
	std::stringstream query;
	query <<"SELECT ObjectsID,Name FROM Objects WHERE ObjectsID =" 
		  <<" (SELECT ObjectsID FROM MotifObjects WHERE LocationID = "<<locationsID<<" AND ObjectPosition = "<<obj_pos_id <<");";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return NULL;
	}
	if (res.size() == 0)
		return NULL;
	int obj_id = atoi(res.front().data["ObjectsID"].c_str());
	if (this->objects.find(obj_id) != this->objects.end())
		return this->objects[obj_id]; //Don't create a factor if we already have one
	Vertex *obj = new Vertex(res.front().data["Name"]);
	this->objects[obj_id] = obj;
	return obj;
}
int Database::getMotifObjectID(int locationsID, int fragmentID, int pos) {
	if (locationsID < 0 || fragmentID < 0 || pos <0)
		return -1;
	std::string res;
	std::stringstream query;
	query << "SELECT DISTINCT ObjectsID FROM MotifVariable"
		<< " WHERE LocationID = " << locationsID
		<< " AND RuleID IN(SELECT ChildRuleID from OperationalConstruction WHERE LocationID = " << locationsID
		<< " AND ParentRuleID = " << fragmentID << ") ORDER BY ObjectsID LIMIT " << pos << ", 1;";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res.size() == 0)
		return -1;
	int obj_id = atoi(res.c_str());
	return obj_id;
}

float Database::getMotifObjectProbability(int locationsID, int pos,int which) {
	if (locationsID < 0 || pos < 0)
		return 0.0f; //If it doesn't exist, then the probability of it should be nothing
	std::string res;
	std::stringstream query;
	query << "SELECT Frequency from MotifObjectsFrequency"
		<< " WHERE LocationID = " << locationsID << " AND ObjectPosition = " << pos << " and FrequencyPosition = " << which << " LIMIT 1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return 0.0f;
	}
	if (res.size() == 0)
		return 0.0f;
	return (float)atof(res.c_str());

}

float Database::getMotifRuleProbability(int locationsID, int pos) {
	if (locationsID == -1 || pos == -1)
		return 0.0f;
	std::string res;
	std::stringstream query;

	query << "SELECT Probability from MotifRule" << " WHERE LocationID = " << locationsID << " AND RuleID = " << pos <<" LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return NULL;
	}
	if (res.size() == 0)
		return 0.0f;
	return (float)atof(res.c_str());
}
//Returns the total number of rules in the RuleSet
int Database::getNumberMotifRules(int locationsID) {
	std::string res;
	std::stringstream query;

	query << "SELECT COUNT(*) FROM MotifRule WHERE MotifRule.LocationID = " << locationsID;
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	return atoi(res.c_str());
}

//Helper Checkers
bool Database::hasMotifRulePosition(const std::string& location, int pos) {
	std::string res;
	std::stringstream query;
	if (pos < 0)//We can quickly do this one without bugging the datbase
		return false; 
	int locationsID = this->getLocationID(location);
	if (locationsID == -1) //Also obvious
		return false; 
	query << "SELECT RuleID FROM MotifRule WHERE MotifRule.LocationID = " << locationsID <<" AND RuleID = "<<pos;
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	if (res.size() == 0)
		return false;
	return true;
}

//Gets the Constraint Shape ID for a given Parameter
int Database::getConstraintShape(int constraint_id) {
	std::string res;
	std::stringstream query;

	query << "SELECT ShapeID from ConstraintParams WHERE ConstraintID = " << constraint_id <<" LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res  == "")
		return -1;
	return atoi(res.c_str());
}

//Gets the number of valid parameters for a given constraint shape
int Database::getConstraintShapeNumParams(int shape_id) {
	std::string res;
	std::stringstream query;

	query << "SELECT ParamCount from ConstraintShape WHERE ShapeID = " << shape_id <<" LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res.size() == 0)
		return -1;
	return atoi(res.c_str());
}
//Gets a constraint parameter based on a given ID
double Database::getConstraintParameter(int constraint_id,int pos) {
	int shape = this->getConstraintShape(constraint_id);
	if(pos > this->getConstraintShapeNumParams(shape)){
		return -9999999999.0; //Should throw an error at this point
	}
	std::string res;
	std::stringstream query;
	if (pos == 0) {
		query << "SELECT Param1 as Param";
	}
	else if (pos == 1) {
		query << "SELECT Param2 as Param";
	}
	else if (pos == 2) {
		query << "SELECT Param3 as Param";
	}
	else if (pos == 3) {
		query << "SELECT Param4 as Param";
	}
	query << " from ConstraintParams WHERE ConstraintID = " << constraint_id <<" LIMIT 0,1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -9999999999.0;
	}
	if (res.size() == 0)
		return -999999999.0;
	return atof(res.c_str());
}
int Database::getConstraintParameterID(int shape_id, double p1, double p2, double p3, double p4) {
	std::string res;
	std::stringstream query;

	query << "SELECT ConstraintID FROM ConstraintParams WHERE"
		<< " ShapeID = " << shape_id << " AND Param1 = " << p1
		<< " AND Param2 = " << p2
		<< " AND Param3 = " << p3
		<< " AND Param4 = " << p4;
	query << " LIMIT 0,1";//Should be distinct, but it's good just to have

	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res == "")
		return -1;
	return atoi(res.c_str());
}
//Gets a constraint parameter ID based on two objects and the position
int Database::getConstraintParameterID(int obj1, int obj2, int pos) {
	std::string res;
	std::stringstream query;

	query << "SELECT ConstraintID FROM RuleConstraint WHERE ";
	query << " ObjectsIDFirst IN ";
	query << this->getAllParentIds(this->getObjectByID(obj1));
	query << " AND ObjectsIDSecond IN ";
	query << this->getAllParentIds(this->getObjectByID(obj2));
	query << " ORDER BY ObjectsIDFirst ASC, ObjectsIDSecond ASC LIMIT "<<pos<<",1";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	if (res == "")
		return -1;
	return atoi(res.c_str());
}

//Gets the number of constraints based on the object pair
int Database::getNumConstraints(int obj1, int obj2) {
	std::string res;
	std::stringstream query;
	query << "SELECT COUNT(ConstraintID) as NumConstraints FROM RuleConstraint WHERE";
	query << " ObjectsIDFirst IN ";
	query << this->getAllParentIds(this->getObjectByID(obj1));
	
	query << " AND ObjectsIDSecond IN ";
	query << this->getAllParentIds(this->getObjectByID(obj2));
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	return atoi(res.c_str());
}
//Gets the number of motif rules that exist for a given location and two objects
int Database::getNumberMotifRules(int loc_id, int obj1, int obj2) {
	std::string res;
	std::stringstream query;
	query << "SELECT COUNT(Rule1) as num_rules from ";
	query << "(SELECT RuleID as Rule1 from MotifVariable WHERE LocationID = " << loc_id << " AND ObjectsID = " << obj1 << ") ";
	query << "JOIN ";
	query << "(SELECT RuleID as Rule2 from MotifVariable WHERE LocationID = " << loc_id << " AND ObjectsID = " << obj2 << ") ";
	query << "WHERE Rule1 = Rule2;";

	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	return atoi(res.c_str());
}

//Gets the motif rule id based on the location, objects, and which one we want
int Database::getMotifRule(int loc_id, int obj1, int obj2, int pos) {
	std::string res;
	std::stringstream query;
	query << "SELECT Rule1 from ";
	query << "(SELECT RuleID as Rule1 from MotifVariable WHERE LocationID = " << loc_id << " AND ObjectsID = " << obj1 << ") ";
	query << "JOIN ";
	query << "(SELECT RuleID as Rule2 from MotifVariable WHERE LocationID = " << loc_id << " AND ObjectsID = " << obj2 << ") ";
	query << "WHERE Rule1 = Rule2 LIMIT "<<pos<<",1";

	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	return atoi(res.c_str());
}
int Database::getOperationalConstructionChildRuleID(int loc_id, int frag_id, int rule_pos) {
	std::string res;
	std::stringstream query;
	query << "SELECT ChildRuleID from OperationalConstruction WHERE ";
	query << "LocationID = " << loc_id << " AND ParentRuleID = " << frag_id << " AND ParentRulePos = " << rule_pos;
	query << " LIMIT 0,1";

	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	return atoi(res.c_str());
}


double Database::getMeshLength(Vertex* v) {
	std::string res;;
	std::stringstream query;
	int id = this->getObjects(v);
	query << "SELECT MeshLength from Mesh WHERE ObjectsID IN ";
	query << this->getAllParentIds(v);
	query << " ORDER BY ObjectsID DESC LIMIT 0,1;";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return 0.0;
	}
	if (res.size() == 0)
		return 0.0;
	return atof(res.c_str());
}

double Database::getMeshWidth(Vertex* v) {
	std::string res;
	std::stringstream query;
	int id = this->getObjects(v);
	query << "SELECT MeshWidth from Mesh WHERE ObjectsID IN ";
	query << this->getAllParentIds(v);
	query << " ORDER BY ObjectsID DESC LIMIT 0,1;";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return 0.0;
	}
	if (res.size() == 0)
		return 0.0;
	return atof(res.c_str());
}

double Database::getMeshHeight(Vertex* v) {
	std::string res;
	std::stringstream query;
	int id = this->getObjects(v);
	query << "SELECT MeshHeight from Mesh WHERE ObjectsID IN ";
	query << this->getAllParentIds(v);
	query << " ORDER BY ObjectsID DESC LIMIT 0,1;";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return 0.0;
	}
	if (res.size() == 0)
		return 0.0;
	return atof(res.c_str());
}

std::string Database::getMeshName(Vertex* v , bool recurse) {
	std::string res;
	std::stringstream query;
	query << "SELECT MeshName from Mesh WHERE ObjectsID IN ";
	if (recurse) {
		query << this->getAllParentIds(v);
	}
	else {
		query << "(" << this->getObjects(v) << ")";
	}
	
	query << " ORDER BY ObjectsID DESC LIMIT 0,1;";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback_single, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return "";
	}
	return res;
}

