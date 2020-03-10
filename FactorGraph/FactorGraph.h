#pragma once
#ifndef __FACTOR_GRAPH__
#define __FACTOR_GRAPH__
#include <string>
#include <vector>
#include <list>
#include <map>
#include "BaseDataStructures.h"



class RuleSet;
/*The goal of the factor graph is to keep track of our vertices, as well as  the factors that are contained in them.
  It should provide us with searching abilities as well as methods to create metrics out of the graph. We use it when graph searching is easier as well*/
class FactorGraph {
private:
	//lists and multimaps because we don't want to have order to really any of the vertices. Rules of the same graph have factors
	std::vector<Content*> vertices; //Keeps a vector of vertices even though we don't really have a root. It just makes some of our other functions easier to use a numerical tag, but doesn't have a meaning
	std::list<Factor*> factors; //A list to keep track of the factors
	//Note, the multimap should respect order as long as we are c++ 11 complient
	std::multimap<Vertex*,Factor*> graph; //Gives an edge name to our factors from the lead vertex. Note, while our Vertices keep in and out maps, this is only the outmap
	std::multimap<Vertex*, int> rule_numbers; //The graph is really just a representation of our ruleset, and so we maintain a connection to it
	std::multimap<Vertex*, float> prob; //Gives a probability to each of our factors
	int num_edges; //Keep a running tally of the edges

	std::map<Vertex*, Vertex*> intersectionVertices(const FactorGraph&) const;//Sparse Graph Kernels Section 4.1
	std::map<Factor*, Factor*> FactorGraph::intersectionFactors(const FactorGraph& rhs) const; //Sparse Graph Kernels Section 4.2

public:
	FactorGraph();
	FactorGraph(const std::list<std::string>&); //For loading a FactorGraph csv
	FactorGraph(RuleSet*,bool reverse = false); //Loads a ruleset as a FactorGraph
	~FactorGraph();
	
	
	//This allows us to get factors based on the factor graph
	Vertex* getVertex(const std::string&) const;
	Vertex* getFactorVertices(Vertex*, Factor*,int) const;
	float   getProbability(Vertex*, Factor*);
	//Probability constraints
	float   getMaxProb(Vertex*);
	float   getTotalMaxProb();//Gets the total highest probability for the entire graph
	void    normByValue(float val); //If we are normalizing by the number of rooms, we would use this function

	//Gets counts of things
	int getNumVertices() { return this->vertices.size(); }
	int getNumFactors() { return this->factors.size(); }

	//We pass the iterator as our range iterators so we can transverse the map without inflicting a large memory penalty
	std::pair<std::vector<Content*>::const_iterator, std::vector<Content*>::const_iterator> getVertices() const { return std::make_pair(this->vertices.begin(), this->vertices.end()); }
	std::pair<std::list<Factor*>::const_iterator, std::list<Factor*>::const_iterator> getFactors()  const { return std::make_pair(this->factors.begin(), this->factors.end()); }
	std::pair<std::multimap<Vertex*, int>::const_iterator, std::multimap<Vertex*, int>::const_iterator> getRuleNumbers(Vertex*) const;
	std::pair<std::multimap<Vertex*, Factor*>::const_iterator, std::multimap<Vertex*, Factor*>::const_iterator> getFactorEdges(Vertex*) const ;
	std::pair<std::multimap<Vertex*, float>::iterator, std::multimap<Vertex*, float>::iterator> getFactorProb( Vertex*); //Because we are working with a floating point here, we cannot make this const

	//We are going to assume that the vertex came from the fg, and therefore, we will do a pointer comparison. So they will fail on a deep copy, which just requires one more step
	int getVertexPos(Vertex*);
	Content* getContent(Vertex*);
	Content* getContent(int pos) { if (pos < 0 || pos > this->vertices.size()) { return false; }return this->vertices.at(pos); }

	std::string graphToJson();//Writes the factor graph as a json string that can be put into a force layout diagram

	float graphKernel(const FactorGraph& rhs) const; //Graph Kernels Section 4.3

};

std::map<std::string, FactorGraph*> parseCSVFactorGraph(const std::string &);


#endif
