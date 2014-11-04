#ifndef FileList_H
#define FileList_H

/// @file FileList.h Header for read ASCII driver

#include "asynPortDriver.h"
#include <time.h>
#include <list>
#include <string>

#define DIR_LENGTH 260

/// Driver for returning a list of files within a directory
class FileList : public asynPortDriver 
{
public:
    FileList(const char* portName, const char *searchDir, const char *searchPat);

	virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);
	asynStatus updateList();

protected:
	int P_DirBase; // string
	int P_Search; //string
	int P_Test; //float
	int P_JSONOutArr; //string Array

private:
	epicsEventId eventId_;
	char *pJSONOut_;
	efsw::FileWatcher * fileWatcher;
	asynStatus addFileWatcher(const char *dir);
#define FIRST_FileList_PARAM P_DirBase
#define LAST_FileList_PARAM P_JSONOutArr
	
};

#define NUM_FileList_PARAMS (&LAST_FileList_PARAM - &FIRST_FileList_PARAM + 1)

#define P_DirBaseString "DIRBASE"
#define P_SearchString "SEARCH"
#define P_TestString "TST"

#define P_JSONArrString "JARR"


#endif /* FileList_H */
