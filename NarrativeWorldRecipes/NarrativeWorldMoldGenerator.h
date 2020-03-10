#pragma once
#ifndef __NARRATIVE_WORLD_MOLD_GENERATOR__
#define __NARRATIVE_WORLD_MOLD_GENERATOR__
#include "RuleSet.h"
#include "NarrativeWorldMold.h"
#include "Database.h"



//This is actually part of the generator
class NarrativeWorldMoldGenerator {
private:
	Database *db;
	std::map<int, Vertex*> vertices;
	std::vector<RuleSet*> motifs; //The motifs we have in our data-set
	std::multimap<int, int> location_motifs;//Connects the locations to the motifs
	std::multimap<int, int> location_parents; //Connects the locations to the parents
	std::list<int> forbidden_objects; //Objects that should not be used in generation and grounding (such as story-specific items)
	std::multimap<std::string, RuleSet*> narrative; //The narrative information we have in our data-base
	bool directed; //Describes if the grounded factor graph is directed or undirected. 
	int search_budget; //The search budget for our seach techniques
					   //Allows us to add and select from a hierarchical perspective

	Vertex* searchSiblings(Vertex*, int);//Searches our vertices for one that matches the base of our vertex

	int  getFactorPosition(Factor*); //Goes through our factors to get the factor pos (id)

	Vertex* createNewObject(Vertex*,int);//There are grounding times where we create a new object, which this helper function assists with
	bool forbiddenObject(Vertex*); //Returns true if the object is a forbidden object and therefore shouldn't be used
	bool recursive(Vertex*, Vertex*); //Determines if one vertex is a child of another. We assume the whole tree is a part of our recipe
	bool recursive(int, int);//Determines if one vertex is a child of another based on the integers
	int getVertexNumber(Vertex*, bool deep_copy = false); //Determine the vector position of a vertex. While we shouldn't be dealing with deep copy, we track it just in case

	//We pass in intersection because it's kind of an expensive operation to perform  
	float cosineSim(RuleSet* intersect, RuleSet* a, RuleSet* b) { return (float)intersect->getNumVertices()*2.0f / (float)(a->getNumVertices() + b->getNumVertices()); } //Determines cosine similarity between the two
	float cosineSimRules(RuleSet* intersect,RuleSet* a, RuleSet* b) { return (float)intersect->getNumRules()*2.0f / (float)(a->getNumRules() + b->getNumRules()); }
	RuleSet* expand(RuleSet*, int,const std::map<int,int>&);//Expands our ruleset when self loops are present
	RuleSet* convertToCommonItems(RuleSet*, NarrativeWorldMold*);//Converts our ruleset to a common item set
	RuleSet* convertToStoryItems(RuleSet*,NarrativeWorldMold*);//The reverse of convert to Common Items
	//These three are for doing object additions
	RuleSet* buildConnections(int, RuleSet*,RuleSet*);
	RuleSet* Dijstra(RuleSet*,RuleSet*,int,Content*); //dijstra is how we expand the motif
	RuleSet* augmentRuleSet(RuleSet* superset, RuleSet* motif);//augments the motif with the superset

	//Our graph distance matching methods
	std::multimap<int, int>   vertexMatches(RuleSet*, RuleSet*);
	std::multimap<int, int> candidateMatches(RuleSet*, RuleSet*);
	int matchedEdges(RuleSet*, RuleSet*);



public:
	//always need a frequency
	NarrativeWorldMoldGenerator(Database* d, bool directed = true, int search_budget = 10000) :db(d),
		directed(directed), 
		search_budget(search_budget) {}
	~NarrativeWorldMoldGenerator();

	//Adds our data to the system
	void addVertex(int id, Vertex* v)      { this->vertices[id] = v; }
	void addLocationMotif(int loc_id, int motif_pos) { this->location_motifs.insert(std::make_pair(loc_id, motif_pos)); }
	void addMotif(RuleSet* r) { this->motifs.push_back(r); }
	void addnarrative(const std::string& name, RuleSet* r) { this->narrative.insert(std::make_pair(name, r)); }
	void addForbiddenObject(int obj_id) { this->forbidden_objects.push_back(obj_id); }
	Vertex* getVertex(int);//Gets a vertex based on the vertex number
	Vertex* getVertex(const std::string&);
	//Determines if our recipe has a factor or vertex (which our matrices are built off of)
	bool hasFactor(Factor*);
	bool hasVertex(const std::string&);
	bool hasLocationMotif(int loc_id);


	NarrativeWorldMold* readInNarrativeWorldMold(const std::vector<std::string>& location_graph, const std::vector<std::string>& narrative_table,bool motif_only=false);
	RuleSet *genSuperSetLocation(int, RuleSet*,NarrativeWorldMold*,bool with_addition = true); //Does our union stuff
	std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> getLocationMotif(int) const;
	//Add our factors and vertices to the set
};
#endif // !__NARRATIVE_WORLD_RECIPE_GENERATOR__

