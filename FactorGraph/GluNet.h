#pragma once
#ifndef __GLUNET_READER__
#define __GLUNET_READER__

#include "sqlite3.h"
#include <string>
#include <list>

class GluNetReader {
private:
	sqlite3 * db;
	char *ErrMsg;

public:
	GluNetReader(const std::string&);
	~GluNetReader();
	std::string    getFrame(int);
	int            getFrame(const std::string&);
	std::list<int> framesFromFEs(const std::string&);
	std::list<int> framesFromFEs(const std::list<std::string>&);
	std::list<int> getFEpos(const std::string&, int);
	std::list<int> getFEpos(const std::list<std::string>&, int);
	std::list<int> getFEs(int);//Gets our FE number
	bool           getFECore(int, int); //Determines if the FE is a core type or not from
	int            getFECore(int); //Deterines the number of FE core types
	std::string    getSemType(int, int); //Gets the semantic type of a Functional Element

};

#endif // !__GLUNET_READER__
