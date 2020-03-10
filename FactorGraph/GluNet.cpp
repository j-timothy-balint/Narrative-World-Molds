#include "GluNet.h"
#include <sstream> //For constructing our 
#include <map>
#include <list>
#include <vector>

struct sql_row {
	std::map<std::string, std::string> data;
};

static int sql_callback(void *data, int argc, char **argv, char **col_name) { //Taken from the tutorial on sqlite. This is called for each row
	std::list<sql_row> *res = reinterpret_cast<std::list<sql_row> *>(data);
	sql_row row;
	for (int i = 0; i < argc; i++) {
		if (argv[i] != NULL)
			row.data[col_name[i]] = argv[i];
		else
			row.data[col_name[i]] = "NULL";
	}
	res->push_back(row);
	return 0;
}
//Creates a connection to our database
GluNetReader::GluNetReader(const std::string& file) :db(NULL), ErrMsg(NULL) {
	int rc = sqlite3_open(file.c_str(), &this->db); //Creates a connection to the database
	if (rc != SQLITE_OK) {
		//Do error checking here
		//ErrMsg = sqlite3_errmsg(db);
		return;
	}
}

//Closes the database connection and sets the pointer to NULL
GluNetReader::~GluNetReader() {
	sqlite3_close(db);
	db = NULL;
}

//Gets a frame based on its ID
std::string GluNetReader::getFrame(int gid) {
	std::list<sql_row> res;
	std::stringstream query;

	query << "SELECT FRAME_NAME FROM FRAMENET_INDEX WHERE ID = " << gid;
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return "";
	}
	return res.front().data["FRAME_NAME"];
}
int GluNetReader::getFrame(const std::string& name) {
	std::list<sql_row> res;
	std::stringstream query;

	query << "SELECT ID FROM FRAMENET_INDEX WHERE FRAME_NET = '" << name <<"';";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return -1;
	}
	return atoi(res.front().data["ID"].c_str());
}
//From the name of a funtional element, we get a list of all the frames that the functional element belongs to
std::list<int> GluNetReader::framesFromFEs(const std::string& name) {
	std::list<sql_row> res;
	std::stringstream query;
	std::list<int> frames;
	std::string clean_name;
	//The name might be a wordnet synset or a non-synset. If it is a synset, we really just want the base of it
	std::size_t dot = name.find('.');
	if (dot != std::string::npos) {
		clean_name = name.substr(0, dot);
	}
	else {
		clean_name = name;
	}
	query << "SELECT DISTINCT FRAME_ID from FRAME_ELEMENT NATURAL JOIN VERBNET_FRAMENET_FRAME_MAPPING WHERE NAME LIKE '" << clean_name << "';";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return frames;
	}
	for (std::list<sql_row>::iterator it = res.begin(); it != res.end(); it++) {
		frames.push_back(atoi((*it).data["FRAME_ID"].c_str()));
	}
	return frames;
}
//This will be a faster and more accurate per object FE detector as it looks at in instead of a single one
std::list<int> GluNetReader::framesFromFEs(const std::list<std::string>& names) {
	std::list<sql_row> res;
	std::stringstream query;
	std::list<int> frames;
	std::vector<std::string> clean_names;
	//The name might be a wordnet synset or a non-synset. If it is a synset, we really just want the base of it
	for (std::list<std::string>::const_iterator name = names.begin(); name != names.end(); name++) {
		std::size_t dot = (*name).find('.');
		if (dot != std::string::npos) {
			clean_names.push_back((*name).substr(0, dot));
		}
		else {
			clean_names.push_back((*name));
		}
	}
	query << "SELECT DISTINCT FRAME_ID from FRAME_ELEMENT NATURAL JOIN VERBNET_FRAMENET_FRAME_MAPPING WHERE ";
	for (int i = 0; i < clean_names.size(); i++) {
		query << " NAME LIKE '" << clean_names[i] << "' ";
		if (i + 1 < clean_names.size())
			query << " OR ";
	}
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return frames;
	}
	for (std::list<sql_row>::iterator it = res.begin(); it != res.end(); it++) {
		frames.push_back(atoi((*it).data["FRAME_ID"].c_str()));
	}
	return frames;
}

//Returns the FE glu-net position based on the name and frame
std::list<int> GluNetReader::getFEpos(const std::string& name, int frame_id) {
	std::list<sql_row> res;
	std::stringstream query;
	std::list<int> fe;
	std::string clean_name;
	//The name might be a wordnet synset or a non-synset. If it is a synset, we really just want the base of it
	std::size_t dot = name.find('.');
	if (dot != std::string::npos) {
		clean_name = name.substr(0, dot);
	}
	else {
		clean_name = name;
	}
	query << "SELECT ID from FRAME_ELEMENT WHERE NAME LIKE '" << clean_name << "' AND FRAME_ID = "<<frame_id<<";";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return fe;
	}
	for (std::list<sql_row>::iterator it = res.begin(); it != res.end(); it++) {
		fe.push_back(atoi((*it).data["ID"].c_str()));
	}
	return fe;
}

std::list<int> GluNetReader::getFEpos(const std::list<std::string>& names, int frame_id) {
	std::list<sql_row> res;
	std::stringstream query;
	std::list<int> frame;
	std::vector<std::string> clean_names;
	//The name might be a wordnet synset or a non-synset. If it is a synset, we really just want the base of it
	for (std::list<std::string>::const_iterator name = names.begin(); name != names.end(); name++) {
		std::size_t dot = (*name).find('.');
		if (dot != std::string::npos) {
			clean_names.push_back((*name).substr(0, dot));
		}
		else {
			clean_names.push_back((*name));
		}
	}
	query << "SELECT ID from FRAME_ELEMENT WHERE FRAME_ID = "<<frame_id<<" AND (";
	for (int i = 0; i < clean_names.size(); i++) {
		query << " NAME LIKE '" << clean_names[i] << "' ";
		if (i + 1 < clean_names.size())
			query << " OR ";
	}
	query << " );";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return frame;
	}
	if (res.size() > 1)
		query.clear();
	for (std::list<sql_row>::iterator it = res.begin(); it != res.end(); it++) {
		frame.push_back(atoi((*it).data["ID"].c_str()));
	}
	return frame;
}
std::list<int> GluNetReader::getFEs(int frame_id) {
	std::list<sql_row> res;
	std::stringstream query;
	std::list<int> frames;
	query << "SELECT ID from FRAME_ELEMENT WHERE FRAME_ID = " << frame_id << ";";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return frames;
	}
	for (std::list<sql_row>::iterator it = res.begin(); it != res.end(); it++) {
		frames.push_back(atoi((*it).data["ID"].c_str()));
	}
	return frames;
}
bool GluNetReader::getFECore(int frame_id, int fe) {
	std::list<sql_row> res;
	std::stringstream query;

	query << "SELECT CORE_TYPE FROM FRAME_ELEMENT WHERE FRAME_ID = " << frame_id << " AND ID = "<< fe <<";";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return false;
	}
	if(res.front().data["CORE_TYPE"] == "Core")
		return true;
	return false;
}

int GluNetReader::getFECore(int frame_id) {
	std::list<sql_row> res;
	std::stringstream query;

	query << "SELECT CORE_TYPE FROM FRAME_ELEMENT WHERE FRAME_ID = " << frame_id << " AND CORE_TYPE = 'Core';";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return 0;
	}
	return (int)res.size();
}

std::string GluNetReader::getSemType(int frame_id, int fe) {
	std::list<sql_row> res;
	std::stringstream query;

	query << "SELECT SEMANTIC_TYPE FROM FRAME_ELEMENT WHERE FRAME_ID = " << frame_id << " AND ID = " << fe << ";";
	int rc = sqlite3_exec(this->db, query.str().c_str(), sql_callback, &res, &this->ErrMsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(this->ErrMsg);
		return "";
	}
	return res.front().data["SEMANTIC_TYPE"];
}