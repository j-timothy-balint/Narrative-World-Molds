#pragma once
#ifndef __NARRATIVE_WORLD_RECIPE__
#define __NARRATIVE_WORLD_RECIPE__
#include <map>
#include <vector>
#include "RuleSet.h"
#include "Database.h"


struct TimeStructure {
	std::map<double, RuleSet* > time_points;//Note that this is a shallow copy ruleset. The only difference is the change in probabilites (and of course the smaller ruleset)
};

class NarrativeWorldMold {
private:
	std::vector<std::string> locations; //The name of our locations
	std::multimap<int, int> location_graph; //Location constraint from GameForge and Marijn
	std::map<int, RuleSet*> spatial_constraints; //The finalized spatial motif
	std::map<int, TimeStructure*> temporial_constraints; //Location-connected temporial constraints (the more general way, in reality we will probably only use the initial state)
	std::map<int, RuleSet*> location_superset;//We keep the location super-set of story items around to make some calculations easier. It's a ruleset because eventually it can have rule states from the text
	std::map<Vertex*, Vertex*> story_items; //Provides a reference to our story items, and the integer of the item that they map to
	TimeStructure* addTemporialStructure(int loc_id); //Creates a new time structure and adds it to the location
	//This should honestly be part of World Generation, as what constitutes a XOR or how the links are handled is a generation specific  
	//void ChooseXORProbabilities(RuleSet*);
public:
	NarrativeWorldMold() {}
	NarrativeWorldMold(const std::string& json_file, Database *db);
	~NarrativeWorldMold();
	
	//Our Add Functions
	bool addLocation(const std::string&);
	bool addLocationConnection(const std::string&, const std::string&);
	bool addLocationConnection(int, int);
	bool addSpatialConstraint(const std::string&, RuleSet*);
	bool addSpatialConstraint(int, RuleSet*);
	bool addTemporialConstraint(const std::string&, double, RuleSet*);
	bool addTemporialConstraint(int, double, RuleSet*);
	
	bool addSuperset(int, RuleSet*);
	bool addSuperset(const std::string&, RuleSet*);
	bool addStoryItem(Vertex*, Vertex*);//Adds a story item to our set
	

	//Our Get Functions
	int            getLocationID(const std::string&) const;
	std::string    getLocation(int) const;
	bool           hasLocation(int); //Returns true if our location already exists 
	int            getNumLocations() { return (int) this->locations.size(); }
	int            getNumTimePoints(int) const;
	double         getTime(int, int) const;
	TimeStructure* getTimeStruct(int) const;
	RuleSet*       getTimePoint(int, double)const;
	RuleSet*       getSpatialConstraint(const std::string&);
	RuleSet*       getSpatialConstraint(int);
	RuleSet*       getSuperset(int);
	RuleSet*       getShadowSuperSet(int);
	void           inputStoryItems(int);

	Vertex* getStoryVertex(Vertex*);
	Vertex* getStoryVertex(const std::string&);
	Vertex* getShadowVertex(Vertex*);

	//Our checking functions
	bool hasLocationConnection(int, int);  

	bool hasStoryItem(Vertex*);
	bool hasStoryItem(const std::string&);

	//Our realization function
	RuleSet* getLocationAtTime(const std::string&, double, RuleSet* complete_set = NULL,bool cleanup_toggle = true);
	RuleSet* getLocationAtTime(int, double, RuleSet* complete_set = NULL, bool cleanup_toggle = true);
	bool cleanShadowVertices(); //Cleans up the shadow vertices from motifs where they are not a part of the story
	bool removeTimePoint(int, double); //removes time points based on the time value they are
	bool removeTimePoint(int, int); //removes time points based on which one they are

	//Our json creation file
	std::string createJson();

	
	
};
#endif