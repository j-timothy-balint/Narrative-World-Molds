#pragma once
#ifndef __NWM_DATABASE__
#define __NWM_DATABASE__



#include <string>
#include "BaseDataStructures.h"
#include "sqlite3.h"
//This is the database to hold all of the generated information
class Database {
private:
	sqlite3 *db;
	char *ErrMsg;
	/*We can have a unified factors and vertices here. It will ease some other calculations*/
	std::map<int, Vertex*> objects;
	std::map<int, Factor*> factors;
	std::map<int, Operational*> actions;

	//These are some helping checkers
	bool hasMotifRulePosition(const std::string&, int); //Makes sure we have the motif rule position
	////Setup of the base data-structures into working memory. This gives a common background for the common data-structures,
	//Removing the need for a deep copy and equality
	void populateSharedData();

	std::string getAllParentIds(Vertex* v);//Provides all Parent Ids as (ID,ID). It is the vertex inclusive
public:
	Database(const std::string&,bool); //Connects to the database
	~Database(); //Deletes the connection

	//Cleans up our database in-case there is un-database stuff in it
	void cleanObjects();

	//Getter for object map
	std::pair<std::map<int, Vertex*>::const_iterator, std::map<int, Vertex*>::const_iterator> getObjectIterator() { return std::make_pair(this->objects.begin(), this->objects.end()); }

	//Our Setters for the database
	bool addFactor(Factor*); //Adds a factor to the database
	bool addLocation(const std::string&); //Adds a location based on the name
	bool addMotifRule(const std::string&, int, Factor*, float); //Adds a motif rule to the database
	bool addMotifVariable(const std::string&, int, int, int);//Adds a variable rule to the database
	bool addObject(Vertex*,int id = -1); //Adds a vertex (an object) to the database. We assume here that the first in-node is used as the parent object
	bool addMotifObject(const std::string&, Vertex*,int,float);//Adds an object to the motif (connecting it to the taxonomy).
	bool addConstraintParam(int, double, double, double p3=0.0, double p4 = 0.0); //Adds a constraint parameter to the database
	bool addOperationalConstruction(int, int, int, int);//Adds our more complex rules as a construction of smaller factors
	bool addRuleConstraint(int, int, int);//Adds a realization of our rule as a constraint to the system
	bool addMesh(Vertex*, const std::string&, double, double, double);

	//Our Information getters
	//int         getAction(Operational*); //Gets the action id based on the name and parameters
	int         getFactor(Factor*); //Gets a factor index based on the name
	int         getFactor(const std::string&);//Gets a factor index when we don't have a factor pointer
	int         getFactorParameters(int);//Gets the number of factor parameters
	bool        getFactorExistance(int);//Returns true if the factor is an existance factor
	Factor*     getFactorByID(int);//Gives us the factor pointer based on the ID
	int         getNumberObjects();//Gets the number of objects from the object table in the database
	int         getMaxObjectID();//Gets the maximum id for an object
	int         getObjects(Vertex*); //Gets a vertex index based on the name
	Vertex*     getObjectByID(int); //Gets a vertex from the database based on the index number
	int         getObjects(const std::string&);//Gets a vertex index based on the string name
	Vertex*     getParentObject(Vertex*);//Gets the closest parent vertex match that is already in the database
	Vertex*     getParentObject(int);//Gets the closest parent vertex match that is already in the database
	std::string getLocation(int);//Gets the location name from an index
	int         getLocationID(const std::string&); //Provides the location ID based on the location
	int         getMinLocationID();//Gets the minimum location ID in the database
	int         getMaxLocationID();//Gets the maximum location ID in the database
	int         getLocationParent(int); //Provides the location's parent based on the given location
	Factor*     getMotifRuleFactor(int, int); //Provides the Factor based on the Location and Rule Number
	Vertex*     getMotifRuleVariable(int, int, int);
	int         getMotifRuleVariablePos(int, int, int);
	int         getNumMotifObjects(int); //Gets the number of motif objects per location
	int         getNumMotifObjects(int,int); //Gets the number of motif objects per location and the fragment
	Vertex*     getMotifObject(int, int);//Provides a motif object based on the location and position
	Vertex*     getMotifObject(int, int, int);//Gets a motif object id based on the location and fragment and position
	int         getMotifObjectID(int, int, int);//Gets a motif object id based on the location and fragment and position
	float       getMotifObjectProbability(int, int,int which = 0);//Gets the frequency of the motif object from the database based on the location and position
	float       getMotifRuleProbability(int, int); //Returns the probability of a nth rule of a location
	int         getNumberMotifRules(int);//Provides the number of known motif rules for a given location
	int         getConstraintShape(int);//Gets the Constraint Shape ID for a given Parameter
	int         getConstraintShapeNumParams(int); //Gets the number of valid parameters for a given constraint shape
	double      getConstraintParameter(int,int);//Gets a constraint parameter based on a given ID
	int         getConstraintParameterID(int, double, double, double p3 = 0.0, double p4 = 0.0);
	int         getConstraintParameterID(int, int,int pos = 0);//Gets the constraint ID based on the two objects they are connected to and a position
	int         getNumConstraints(int, int);//Gets the number of constraints for a given object pair (and their parents)
	int         getNumberMotifRules(int, int, int);//Gets the number of motif rules that exist for a given location and two objects
	int         getMotifRule(int, int, int, int);//Gets the motif rule id based on the location, objects, and which one we want
	int         getOperationalConstructionChildRuleID(int, int, int); //Gets the child rule based on the Location, Rule, and parent position

	double      getMeshLength(Vertex*); //Gets the length of the mesh based on the id of the vertex (or one of it's parents)
	double      getMeshWidth(Vertex*); //Gets the width of the mesh based on the id of the vertex (or one of it's parents)
	double      getMeshHeight(Vertex*); //Gets the height of the mesh based on the id of the vertex (or one of it's parents)
	std::string getMeshName(Vertex*,bool recurse = true);  //Gets the name of a mesh based on the id of the vertex (or one of it's parents)

	//Add in Deletion to database

};
#endif // !__DATABASE__