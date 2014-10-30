#include <iostream>
#include <list>
#include <string>

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

#define OUT_CHAR_LIM 16384
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
	pJSONOut_ = (char *)calloc(OUT_CHAR_LIM, 1);

	/* Do callbacks so higher layers see any changes */
	status |= (asynStatus)callParamCallbacks();

	if (status) {
		std::cerr << status << "epicsThreadCreate failure" << std::endl;
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
	int status = asynSuccess;
	const char *functionName = "readOctet";

	if (maxChars < ncopy) ncopy = maxChars;
	else ncopy = OUT_CHAR_LIM;
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
	
	return (asynStatus)status;
}

asynStatus FileList::updateList()
{
	char dirBase [EPICS_CHAR_LIM];
	char search [EPICS_CHAR_LIM];
	std::list<std::string> files;
	int status = asynSuccess;
	std::string out;

	//get all files in directory
	status |= getStringParam(P_DirBase, EPICS_CHAR_LIM, dirBase);

	getFileList(dirBase, files);

	//search files

	status |= getStringParam(P_Search, EPICS_CHAR_LIM, search);

	status |= filterList(files, search);

	//add appropriate files to PV
	std::string tOut = json_list_to_array(files);
	status |= compressString(tOut, out);
	
	std::cerr << "Before: " << tOut.size() << "    After: " << out.size() << std::endl;

	if (out.size() < OUT_CHAR_LIM)
		std::copy(out.begin(), out.end(), pJSONOut_);
	else
		std::cerr << "File list too long: " << out.size() << std::endl;

	status |= uncompressString(out, tOut);
	std::cerr << tOut << std::endl;

	return (asynStatus)status;
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

