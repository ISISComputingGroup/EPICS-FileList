#include <iostream>
#include <list>
#include <string>
#include <cstdlib>
#include <cstring>

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <iocsh.h>
#include <efsw.hpp>

#include <sys/stat.h>

#include "FileList.h"

#include <macLib.h>
#include <epicsGuard.h>

#include <epicsExport.h>

#include <utilities.h>

#define OUT_CHAR_LIM 16384
#define EPICS_CHAR_LIM 40

static const char *driverName="FileList";

/// Processes a file action
class UpdateListener : public efsw::FileWatchListener
{
	private:
		FileList * parent;

	public:
		UpdateListener() : parent(nullptr) {}

		void setParent (FileList * par ) {parent = par;}

		void handleFileAction( efsw::WatchID watchid, const std::string& dir, const std::string& filename, efsw::Action action, std::string oldFilename = ""  )
		{
			parent->updateList();
		}
};


/// Constructor for the FileList class.
/// Calls constructor for the asynPortDriver base class.
FileList::FileList(const char *portName, const char *searchDir, const char *searchPat) 
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
	eventId_ = epicsEventCreate(epicsEventEmpty);

	//Allocate column data
	pJSONOut_ = (char *)calloc(OUT_CHAR_LIM, 1);

	//Init
	fileWatcher = new efsw::FileWatcher ();
	setStringParam(P_DirBase, searchDir);
	setStringParam(P_Search, searchPat);
	addFileWatcher(searchDir);
	updateList();

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

		if (function == P_DirBase)
			addFileWatcher(value);
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

asynStatus FileList::addFileWatcher(const char *dir)
{
	//Create listener
	UpdateListener * listener = new UpdateListener();
	listener->setParent(this);

	//Remove previous watch
	fileWatcher->removeWatch(0);

	// Adds a non-recursive watch.
	efsw::WatchID watchID = fileWatcher->addWatch( dir, listener, false);
	if (watchID < 0)
	{
		std::cerr << efsw::Errors::Log::getLastErrorLog().c_str() << std::endl;
		return asynError;
	}

	// Start watching asynchronously the directories
	fileWatcher->watch();

	return asynSuccess;
}

asynStatus FileList::updateList()
{
	char dirBase [EPICS_CHAR_LIM];
	char search [EPICS_CHAR_LIM];
	std::list<std::string> files;
	int status = asynSuccess;
	std::string out;

	lock();

	//get all files in directory
	status |= getStringParam(P_DirBase, EPICS_CHAR_LIM, dirBase);

	getFileList(dirBase, files);

	//search files

	status |= getStringParam(P_Search, EPICS_CHAR_LIM, search);

	status |= filterList(files, search);

	//add appropriate files to PV
	std::string tOut = json_list_to_array(files);
	status |= compressString(tOut, out);

	if (out.size() < OUT_CHAR_LIM)
		std::copy(out.begin(), out.end(), pJSONOut_);
	else
		std::cerr << "File list too long: " << out.size() << std::endl;

	status |= uncompressString(out, tOut);
	std::cerr << tOut << std::endl;

	status |= setStringParam(P_JSONOutArr, pJSONOut_);

	unlock();

	return (asynStatus)status;
}

extern "C" {

/// EPICS iocsh callable function to call constructor of FileList().
/// \param[in] portName The name of the asyn port driver to be created.
/// \param[in] searchDir The directory to be searched
/// \param[in] searchPattern The regex to search for
int FileListConfigure(const char *portName, const char *searchDir, const char *searchPat)
{
	try
	{
		new FileList(portName, searchDir, searchPat);
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
static const iocshArg initArg1 = { "searchDir",iocshArgString};
static const iocshArg initArg2 = { "searchPattern",iocshArgString};

static const iocshArg * const initArgs[] = { &initArg0, &initArg1, &initArg2 };

static const iocshFuncDef initFuncDef = {"FileListConfigure", sizeof(initArgs) / sizeof(iocshArg*), initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    FileListConfigure(args[0].sval, args[1].sval, args[2].sval);
}

static void FileListRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(FileListRegister);

}

