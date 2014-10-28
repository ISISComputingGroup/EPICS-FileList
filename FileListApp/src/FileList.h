#ifndef FileList_H
#define FileList_H

/// @file FileList.h Header for read ASCII driver

#include "asynPortDriver.h"
#include <time.h>
#include <vector>
#include <string>

#define DIR_LENGTH 260

/// Driver for Ramping SPs and Reading PIDs
class FileList : public asynPortDriver 
{
public:
    FileList(const char* portName);

	virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);

	virtual asynStatus readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason);

protected:
	int P_DirBase; // string
	int P_Search; //string
	int P_Test; //float

	int P_JSONOutArr; //string Array

private:
	char *pJSONOut_;
	asynStatus parseList(char* regex, std::vector<std::string> *files);
	asynStatus updateList();
	asynStatus toJSON(std::vector<std::string> *files, JSONNODE *n);
	asynStatus getFullList(char* dirBase, std::vector<std::string> * dirs);
	asynStatus compress(JSONNODE *n, char *pOut_);
	
#define FIRST_FileList_PARAM P_DirBase
#define LAST_FileList_PARAM P_JSONOutArr
	
};

#define NUM_FileList_PARAMS (&LAST_FileList_PARAM - &FIRST_FileList_PARAM + 1)

#define P_DirBaseString "DIRBASE"
#define P_SearchString "SEARCH"
#define P_TestString "TST"

#define P_JSONArrString "JARR"


#endif /* FileList_H */
