#pragma once
#ifndef __Set_Transformation_H__
#define __Set_Transformation_H__
#include <map>
#include <string>
#include <list>
#include <vector>
#include "BaseDataStructures.h"
#include "GluNet.h"



//Helper struct to not need to pass so much
typedef struct {
	int index; //Our vertex index that connects to the annotations and connections
	bool visited; //Describes when a node has been visited
	std::multimap<unsigned int, int>::iterator beg_aff; //To save computation
	std::multimap<unsigned int, int>::iterator cur_aff; //The current affordance we are at
	std::multimap<unsigned int, int>::iterator end_aff; //To save computation
} node;

class FrameTransformation {
private:
	std::vector<Vertex*> vertices; //Our vector list of vertices
	std::multimap<int, Vertex*> all_paths; //
	std::multimap<unsigned int, int> annotations; //The annotations found from FrameNet
	std::vector<int> frameFEs;  //The positions of the FEs in GLUNET
	int frame_id; //The id of the frame that we are examining
	int num_req; //The number of required functional elements. We assume the first num_req in frameFEs are required
	int num_opt; //The number of optional functional elements
	bool **connections; //Our 2D array of visited nodes
	GluNetReader *reader;

	void resetVisited(bool*);
	void backtrack(int,node*); //Backtracks a vertex
	int  processVertex(int,node*, bool*,int);
	std::list<int> findPath(int, int); //Given a starting path vertex, find a path of length k (that is, has k checked off) 
	bool matched(const std::list<int>&); //Determines if we have a match already in our paths
	int findFE(int fe);//Gets the position of the FE in the list based on the ID of the FE
	

public:
	FrameTransformation(GluNetReader* r) :frame_id(-1),
		num_req(0),
		num_opt(0),
		connections(NULL),
		reader(r) {}//Our constructor

	int  setFrameID(int f_id) { this->frame_id = f_id; }
	void addVertex(Vertex*); //Adds a vertex to our list,
	void addAnnotation(Vertex*, int); //Adds an annotation to our vertex
	void addFE(int,bool); //Adds an functional element to our system

	void calcAllPaths(); //Path number, Vertex that belongs to the path
	std::pair<std::multimap<int, Vertex*>::iterator, std::multimap<int, Vertex*>::iterator> getAllPaths() { return std::make_pair(this->all_paths.begin(), this->all_paths.end()); }
	std::pair<std::multimap<int, Vertex*>::iterator, std::multimap<int, Vertex*>::iterator> getPaths(int i) { return this->all_paths.equal_range(i); }
	int getNumPaths();
	int getFE(bool required) { return (required) ? this->num_req : this->num_opt; }

	//Determines a list of actions that match a given set of vertex objects
	std::list<int> getActionsFromObjSet(const std::list<Vertex*>&, const std::multimap<std::string, std::string>&);

	//Figures out a jaccard comparision between different actions to determine a common language overlap
	std::map<std::string, float> totalJacComp(std::multimap<std::string, int> input);

};
#endif // !__Set_Transformation_H__
