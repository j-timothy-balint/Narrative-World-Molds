#pragma once

#ifndef __BASE_DATA_STRUCTURES_H__
#define __BASE_DATA_STRUCTURES_H__
#include <string>
#include <vector>
#include <map>
//checked 18-9-2018
class Content; //This helps with pointer declarations

/// <summary>
/// <c>Vertex</c> is the base class for content. 
///</summary>
class Vertex {
private:
	std::string name; 
	std::multimap<int, Vertex*> out_edges; //For each vertex out node, we may have several variables that are connected to the factor. The out-edge is considered the head of that factor. Think of it as F(this->node,node)
	std::vector<Vertex*> in_edges; //If the edge is not the head, then it should point back to the head. 

	int nextKey(); //gets the next key for the multimap vertex iterator
	std::map<std::string, int> countNodes(bool outbound = true) const;
public:
	Vertex(std::string);
	const std::string& getName() const { return name; }

	//Getters and setters for edges 
	void    addEdge(Vertex *edge, bool outbound = true, int key = -1);
	Vertex* getEdge(const std::string& name, bool outbound = true, int key = -1);
	bool    getEdge(Vertex* edge, bool outbound = true, int key = -1); //Overloaded for when we have a vertex so we don't have to do as many string compares
	Vertex* getEdge(int offset, bool outbound = true, int key = -1); //When we are matching the factor to the edges, we do this.
	int     getEdgePos(const std::string& name, bool outbound = true, int num = 0); //Provides the position in the list that a vertex is
	int     getEdgePos(Vertex* edge, bool outbound = true, int num = 0); //Overloaded for when we have a vertex so we don't have to do as many string compares
	int     getNumEdges(Vertex* edge, bool outbound = true) const; //Gets the number of edges that a given vertex exists as
	int     getNumEdges(bool outbound = true) const;//Returns the number of edges in either of the edge lists. This gives us the degree 
	bool    removeEdge(Vertex *edge, bool outbound = true, int key = -1);

	const std::multimap<int, Vertex*>& getEdges() {return this->out_edges;} //Returns the out edges, which are a multi-map (so that we can have different factors connected to our multi-map)

	//Complete Vertex Comparison
	bool operator ==(const Vertex &rhs) const;
	bool operator!= (const Vertex &rhs) const { return !(*this == rhs); }

};

/// <summary>
/// <c>Factor</c> is the base class for connections 
///</summary>
class Factor {
protected:
	int num_components; //The number of components we expect to see in a factor
	std::string name;//The name of the factor
	bool existance; //Existance factors between vertices matter when we have a change in probability. Structural factors do not
public:
	Factor(int num_vars, const std::string& name,bool exists = false) :num_components(num_vars),
		name(name),existance(exists) {}
	int          getNumComponents() const { return num_components; }
	bool         getExistance() const { return existance; }
	const std::string& getName() const { return name; }

	bool operator ==(const Factor &rhs) const { return ((this->num_components == rhs.getNumComponents()) && (this->name == rhs.getName()) && (this->existance == rhs.getExistance())); }
	bool operator!= (const Factor &rhs) const { return !(*this == rhs); }
};

/// <summary>
/// <c>Operational</c>: An action is a special kind of factor that has optional parameters and restrictions on the objects that can be attached to the factor in our ruleset (predicate restrictions) 
///</summary>
class Operational : public Factor {
private:
	int optional_components;
	std::multimap<int, Vertex*> roles;
public:
	Operational(int num_vars, const std::string& name, int optional):Factor(num_vars,name), optional_components(optional) {}

	int getOptionalComponents() const { return optional_components; }
	
	//Special getter for the roles
	std::pair<std::multimap<int,Vertex*>::const_iterator, std::multimap<int, Vertex*>::const_iterator> getRoles() const{ return std::make_pair(roles.begin(), roles.end()); }
	std::pair<std::multimap<int, Vertex*>::const_iterator, std::multimap<int, Vertex*>::const_iterator> getRoles(int pos) const { return roles.equal_range(pos); }
	int getNumRoles(int pos) const;
};

typedef float(*external_func)(const std::vector<int>&,const std::vector<float>&);
typedef float(*internal_func)(const std::vector<Content*>&,const std::vector<int>&);
typedef int*(*selector_func)(const std::vector<Content*>&);

float uniformExtern(const std::vector<int>&, const std::vector<float>&);//Our base select that returns the multiplication of all probabilities in the float
float uniformIntern(const std::vector<Content*>&,const std::vector<int>&);//Our base select that returns the multiplication for all selected probabilities
int* randomSelection(const std::vector<Content*>&); //Does a random selection for all f values in our a set
/// <summary>
/// <c>Content</c>: Content is a base data structure as it describes how the content varies over itself and its children 
///</summary>
class Content {
private:
	Vertex* vertex; //The vertex that represents the content
	std::vector<float> probability; //The distributional probability of the f set
	std::vector<Content*> leaf_nodes;//Content may also contain leaf nodes, which can have varying probability. This is our A-set.
	std::vector<int> selection;//This is our determined selection. It starts filled at -1

	//This describe how we compare and contrast the vertex
	bool deep_copy; //
	bool unique;

	external_func external;//This function describes how likely it is that the given Content is inside the motif
	internal_func internal;//This function describes how likely it is that the given configuration represents the content
	selector_func selector;//This function provides us with a selector
public:
	Content(Vertex*,bool deep_copy,bool unique = false); //Our basic creation
	Content(const std::string& name) :deep_copy(true),unique(false),vertex(new Vertex(name)),external(uniformExtern),internal(uniformIntern),selector(randomSelection){}//If we are receiving just a content string, it's obvs a deep copy
	~Content(); //Our destructor. Cleans based on deep_copy (very important variable)
	Content(const Content&); //Copy constructor that is a straight copy
	Content(const Content&, bool deep_copy,bool unique = false);//When we need to switch our DCS

	//Our addition functions perform our constraints on the distributions
	//These are also our update functions
	void addProbability(float,int which = 0);//We swap these two from the rest to allow for the default assignment
	void addLeaf(int, Vertex*);
	void addLeafProbability(int, int, float);

	//Gives us the distribution numbers for everything
	int getNumLeafs()           const { return (int)this->leaf_nodes.size(); } //Canada over leaves
	int getNumContent()         const { return (int)this->probability.size(); }
	int getNumLeafProbability(int) const;

	//Getters for all of our information
	Vertex*  getVertex() const { return this->vertex; }
	float    getProbability(int) const;
	int      getSelection(int) const; //This tells us which selection we made
	Vertex*  getLeaf(int) const;//Helecopter parenting 
	int      getLeaf(Vertex*) const; //Still helecopter parenting because we never give up the child
	float    getLeafProbability(int, int) const;
	//These two functions are how we should access our probabilitis when we are generating the world
	float	calculateExternal() { return this->external(this->selection,this->probability); }
	float	calculateInternal() { return this->internal(this->leaf_nodes, this->selection); }
	//And this seeds our selection
	void    select();

	//Functions to allow changing the values in our system
	void normalizeBase(float val) { this->probability[0] /= val; }
	void normalizeSet();//Normalizes the set to the base probability
	void normalizeLeafs(bool norm_sets_first = true); //Normalizes the children to their base probability (which also includes normalizing their sets to the base probability)
	void clearSetProbability() { this->probability.resize(1); }//We are basically getting rid of the set and becoming a single item
	void changeVertex(Vertex*);//Changes the vertex to a different one

	//These allow us to change our external functions to suit our needs
	void changeExternalFunction(float (f1)(const std::vector<int>&, const std::vector<float>&)) { this->external = f1; }
	void changeInternalFunction(float (f1)(const std::vector<Content*>&, const std::vector<int>&)) { this->internal = f1; }
	void changeSelectorFunction(int*(*f1)(const std::vector<Content*>&)) { this->selector = f1; }
	//Subtraction of probabilities
	friend Content operator -(const Content&, const Content&);//subtracts probability from the the intersection of left and right. Keeps the left hand side that isn't in right 
	//Comparison operators
	bool hasVertex(Vertex*) const;//Determines if we have the vertex, which may be either the vertex or a child
	//Finally, we perform our equality comparison operators
	bool operator ==(const Content &rhs) const; //Equivalance based on vertex name (exact), 
	bool operator!= (const Content &rhs) const { return !(*this == rhs); }//Oppisite of equals

	//Getters and setters for uniqueness
	void setUnique(bool value) { this->unique = value; }
	bool getUnique() { return this->unique; }
	
};

struct ConstraintStruct {//Our constraint structure between two objects
	Vertex* object_1;
	Vertex* object_2;
	double param[4]; //A maximum of four parameters in our database means we have a maximum of four parameters
};
//A 
#endif // !__BASE_DATA_STRUCTURES_H__
