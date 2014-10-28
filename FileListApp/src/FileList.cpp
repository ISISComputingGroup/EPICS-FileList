#define PCRE_STATIC

#include <iostream>
#include <pcrecpp.h>
#include <libjson.h>

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <iocsh.h>

#include <sys/stat.h>

#include "FileList.h"

#include <macLib.h>
#include <epicsGuard.h>

#include <epicsExport.h>

#include <utilities.h>

#define INIT_CHAR_LIM 255
#define EPICS_CHAR_LIM 40

static const char *driverName="FileList";

/// Constructor for the FileList class.
/// Calls constructor for the asynPortDriver base class.
FileList::FileList(const char *portName) 
   : asynPortDriver(portName, 
                    4, /* maxAddr */ 
                    NUM_FileList_PARAMS,
					asynOctetMask | asynDrvUserMask | asynFloat64Mask, /* Interface mask */
                    asynOctetMask | asynFloat64Mask,  /* Interrupt mask */
                    0, /* asynFlags.  This driver can block but it is not multi-device */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0)	/* Default stack size*/
{
	int status = asynSuccess;
    const char *functionName = "FileList";

	createParam(P_DirBaseString, asynParamOctet, &P_DirBase);
	createParam(P_SearchString, asynParamOctet, &P_Search);
	createParam(P_TestString, asynParamFloat64, &P_Test);
	createParam(P_JSONArrString, asynParamOctet, &P_JSONOutArr);
	
	//Allocate column data
	pJSONOut_ = (char *)calloc(INIT_CHAR_LIM, 1);

	//Init
	pJSONOut_[0] = 'D';
	pJSONOut_[1] = 'O';
	pJSONOut_[2] = 'M';

	status |= setDoubleParam(P_Test, 10.01);

	/* Do callbacks so higher layers see any changes */
	status |= (asynStatus)callParamCallbacks();

	if (status) {
		std::cerr << status << "epicsThreadCreate failure\n" << std::endl;
		return;
	}

}

asynStatus FileList::writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual)
{
	//Checks if directory has changed, reads file again if it has
	int function = pasynUser->reason;
	int status = asynSuccess;
	const char *paramName;
	const char* functionName = "writeOctet";

	/* Set the parameter in the parameter library. */
	status = (asynStatus)setStringParam(function, value);

	/* Fetch the parameter string name for possible use in debugging */
	getParamName(function, &paramName);

	if (function == P_Search || function == P_DirBase) {
		updateList();
	}

	/* Do callbacks so higher layers see any changes */
	status |= (asynStatus)callParamCallbacks();

	if (status)
		epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
		"%s:%s: status=%d, function=%d, name=%s, value=%d",
		driverName, functionName, status, function, paramName, value);

	*nActual = maxChars;
	return (asynStatus)status;
}

asynStatus FileList::readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason)
{
	//Return arrays when waveform is scanned
	int ncopy = 0;
	int function = pasynUser->reason;
	asynStatus status = asynSuccess;
	const char *functionName = "readOctetArray";

	if (maxChars < ncopy) ncopy = maxChars;
	else ncopy = INIT_CHAR_LIM;
	if (function == P_JSONOutArr) {
		memcpy(value, pJSONOut_, ncopy);
	}
	*nActual = ncopy;
	
	if (status)
		epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
		"%s:%s: status=%d, function=%d",
		driverName, functionName, status, function);
	else
		asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
		"%s:%s: function=%d\n",
		driverName, functionName, function);
	return status;

	return asynSuccess;
}

asynStatus FileList::updateList()
{
	char dirBase [EPICS_CHAR_LIM];
	char search [EPICS_CHAR_LIM];
	std::vector<std::string> files;
	int status = asynSuccess;

	//get all files in directory
	status |= getStringParam(P_DirBase, EPICS_CHAR_LIM, dirBase);

	status |= getFullList(dirBase, &files);

	//search files

	status |= getStringParam(P_Search, EPICS_CHAR_LIM, search);

	status |= parseList(search, &files);

	for( std::vector<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
		std::cerr << *i << std::endl;

	//add appropriate files to PV
	//status |= toJSON(&files);


	return (asynStatus)status;
}

asynStatus FileList::toJSON(std::vector<std::string> *files)
{

	JSONNODE *n = json_new(JSON_ARRAY);
	json_push_back(n, json_new_a("String Test", "Test"));

	json_char *jc = json_write_formatted(n);

	std::cerr << jc << std::endl;

	json_free(jc);
	json_delete(n);

	return asynSuccess;
}

asynStatus FileList::getFullList(char* dirBase, std::vector<std::string> *files)
{
	struct dirent *pDirent;
	DIR *pDir;

	pDir = opendir(dirBase);

    if (pDir == NULL) {
        std::cerr << "Cannot open directory" << std::endl;
        return asynError;
    }

	//First two files are '.' and '..' so skip
	readdir(pDir);
	readdir(pDir);

    while ((pDirent = readdir(pDir)) != NULL) {
       files->push_back(pDirent->d_name);
    }
    closedir (pDir);

	return asynSuccess;

}
	
asynStatus FileList::parseList(char* regex, std::vector<std::string> *files)
{
	pcre *re;
	const char *error;
	int erroffset;

	re = pcre_compile(
	  regex,              /* the pattern */
	  0,                    /* default options */
	  &error,               /* for error message */
	  &erroffset,           /* for error offset */
	  NULL);                /* use default character tables */

	if (re == NULL)
	{
		std::cerr << "PCRE compilation failed" << std::endl;
		return asynError;
	}

	for (std::vector<std::string>::iterator it = files->begin(); it != files->end();)
	{
		std::string file = *it; 
		std::cerr << file << std::endl;
		if(0 > pcre_exec(re, NULL, file.c_str(), file.length(), 0, 0, NULL, NULL))
			it = files->erase(it);
		else
			 ++it;
	}

	for (std::vector<std::string>::iterator it = files->begin(); it != files->end(); ++it)
		std::cerr << *it << std::endl;

	return asynSuccess;
}

extern "C" {

/// EPICS iocsh callable function to call constructor of FileList().
/// \param[in] portName @copydoc initArg0
int FileListConfigure(const char *portName)
{
	try
	{
		new FileList(portName);
		return(asynSuccess);
	}
	catch(const std::exception& ex)
	{
		std::cerr << "FileListDriver failed: " << ex.what() << std::endl;
		return(asynError);
	}
}

// EPICS iocsh shell commands 

static const iocshArg initArg0 = { "portName", iocshArgString};			///< The name of the asyn driver port we will create

static const iocshArg * const initArgs[] = { &initArg0 };

static const iocshFuncDef initFuncDef = {"FileListConfigure", sizeof(initArgs) / sizeof(iocshArg*), initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    FileListConfigure(args[0].sval);
}

static void FileListRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(FileListRegister);

}

