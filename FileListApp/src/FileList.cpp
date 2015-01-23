#include <iostream>
#include <list>
#include <string>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <iocsh.h>
#include <efsw.hpp>
#include "epicsMessageQueue.h"

#include <sys/stat.h>

#include "FileList.h"

#include <macLib.h>
#include <epicsGuard.h>

#include <epicsExport.h>

#include <utilities.h>

#define OUT_CHAR_LIM 16384
#define EPICS_CHAR_LIM 40

static const char *driverName="FileList";

static void fileWatcherThreadC(void *pPvt);

/// Constructor for the FileList class.
/// Calls constructor for the asynPortDriver base class.
FileList::FileList(const char *portName, const char *searchDir, const char *searchPat, int fullPath) 
   : asynPortDriver(portName, 
                    4, /* maxAddr */ 
                    NUM_FileList_PARAMS,
					asynInt32Mask | asynOctetMask | asynDrvUserMask | asynFloat64Mask, /* Interface mask */
                    asynInt32Mask | asynOctetMask | asynFloat64Mask,  /* Interrupt mask */
                    0, /* asynFlags.  This driver can block but it is not multi-device */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0)	/* Default stack size*/,
	watchQueue_(10, sizeof(char *)), m_fullPath(fullPath != 0 ? true : false)
{
	int status = asynSuccess;
    const char *functionName = "FileList";

	createParam(P_DirBaseString, asynParamOctet, &P_DirBase);
	createParam(P_SearchString, asynParamOctet, &P_Search);
	createParam(P_CaseString, asynParamInt32, &P_CaseSensitive);
	createParam(P_JSONArrString, asynParamOctet, &P_JSONOutArr);

	//Allocate column data
	pJSONOut_ = (char *)calloc(OUT_CHAR_LIM, 1);

	setStringParam(P_DirBase, searchDir);
	setStringParam(P_Search, searchPat);
	setIntegerParam(P_CaseSensitive, 0);
	updateList();

	// Start filewatcher
	char * str = strdup(searchDir);
	watchQueue_.send((void*)&str, sizeof(char*));

	/* Do callbacks so higher layers see any changes */
	status |= (asynStatus)callParamCallbacks();

	// Create the thread that will service the file watcher
	// To write to the controller
	epicsThreadCreate("fileWatcher", 
                    epicsThreadPriorityMax,
                    epicsThreadGetStackSize(epicsThreadStackMedium),
                    (EPICSTHREADFUNC)fileWatcherThreadC, (void *)this);
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

		status = updateList();

		if ( (status == asynSuccess) && (function == P_DirBase) )
		{
			//change watching directory
			char * str = strdup(value);
			watchQueue_.send((void*)&str, sizeof(char*));
		}
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

asynStatus FileList::updateList()
{
	char dirBase [EPICS_CHAR_LIM];
	char search [EPICS_CHAR_LIM];
	int caseSense;
	std::list<std::string> files;
	int status = asynSuccess;
	std::string out;

	lock();

	//get all files in directory
	status |= getStringParam(P_DirBase, EPICS_CHAR_LIM, dirBase);

	if (getFileList(dirBase, files) == -1)
	{
		std::cerr << "Directory not found: " << dirBase << std::endl;
		unlock();
		return asynError;
	}

	//search files
	status |= getStringParam(P_Search, EPICS_CHAR_LIM, search);

	status |= getIntegerParam(P_CaseSensitive, &caseSense);

	if (caseSense == 0)
	{
		status |= filterList(files, std::string("(?i)").append(search));
	} else {
		status |= filterList(files, search);
	}
	
	if (m_fullPath)
	{
	    std::string dir_prefix = std::string(dirBase) + "/";
	    std::replace(dir_prefix.begin(), dir_prefix.end(), '\\', '/');
		for(std::list<std::string>::iterator it = files.begin(); it != files.end(); ++it)
		{
		    it->insert(0, dir_prefix);
		}
	}
	
	//add appropriate files to PV
	std::string tOut = json_list_to_array(files);
	status |= compressString(tOut, out);

	if (out.size() < OUT_CHAR_LIM)
		std::copy(out.begin(), out.end(), pJSONOut_);
	else
		std::cerr << "File list too long: " << out.size() << std::endl;

	status |= setStringParam(P_JSONOutArr, pJSONOut_);

	/* Do callbacks so higher layers see any changes */
	status |= (asynStatus)callParamCallbacks();

	unlock();

	return (asynStatus)status;
}

void FileList::watchFiles(void)
{
	/// Processes a file action
	class UpdateListener : public efsw::FileWatchListener
	{
		private:
			FileList * parent;

		public:
			UpdateListener() : parent(NULL) {}

			void setParent (FileList * par ) {parent = par;}

			void handleFileAction( efsw::WatchID watchid, const std::string& dir, const std::string& filename, efsw::Action action, std::string oldFilename = ""  )
			{
				parent->updateList();
			}
	};

	efsw::FileWatcher * watcher = new efsw::FileWatcher ();
	
	//Create listener
	UpdateListener * listener = new UpdateListener ();
	listener->setParent(this);

	//Keep track of watches
	efsw::WatchID watchID = -1;

	char * directory; 

	while (true)
	{
		watchQueue_.receive(&directory, sizeof(char*));

		//Strip final slash (fudge due to poor programming in efsw)
		char final = directory[strlen(directory)-1];
		if ( (final == '/') || (final == '\\') )
		{
			if (directory[strlen(directory)-2] == ':')
			{
				//root directory
				directory[strlen(directory)-1] = '/';
			} else {
				directory[strlen(directory)-1] = 0;
			}
		}

		//Remove previous watch
		if (watchID > 0)
		{
			watcher->removeWatch(watchID);
		}
	
		// Adds a non-recursive watch.
		watchID = watcher->addWatch( directory, listener);
	
		//Check for error
		if (watchID < 0)
		{
			std::cerr << "FileWatcher, " << efsw::Errors::Log::getLastErrorLog().c_str() << std::endl;
		}

		// Start watching the directories asynchronously
		watcher->watch();

		delete directory;
	}
}

/* C Function which runs the fileWatcher thread */ 
static void fileWatcherThreadC(void *pPvt)
{
	FileList *list = (FileList*)pPvt;
	list->watchFiles();
}

extern "C" {

/// EPICS iocsh callable function to call constructor of FileList().
/// \param[in] portName The name of the asyn port driver to be created.
/// \param[in] searchDir The directory to be searched
/// \param[in] searchPattern The regex to search for
/// \param[in] fullPath Set to 1 to return full path name of files (otherwise just names are returned)
int FileListConfigure(const char *portName, const char *searchDir, const char *searchPat, int fullPath)
{
	try
	{
		new FileList(portName, searchDir, searchPat, fullPath);
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
static const iocshArg initArg3 = { "fullPath",iocshArgInt};

static const iocshArg * const initArgs[] = { &initArg0, &initArg1, &initArg2, &initArg3 };

static const iocshFuncDef initFuncDef = {"FileListConfigure", sizeof(initArgs) / sizeof(iocshArg*), initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    FileListConfigure(args[0].sval, args[1].sval, args[2].sval, args[3].ival);
}

static void FileListRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(FileListRegister);

}

