TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

### NOTE: there should only be one build.mak for a given IOC family and this should be located in the ###-IOC-01 directory

#=============================
# Build the IOC application FileListTest
# We actually use $(APPNAME) below so this file can be included by multiple IOCs

PROD_IOC = $(APPNAME)
# FileListTest.dbd will be created and installed
DBD += $(APPNAME).dbd

# FileListTest.dbd will be made up from these files:
$(APPNAME)_DBD += base.dbd
## ISIS standard dbd ##
$(APPNAME)_DBD += icpconfig.dbd
$(APPNAME)_DBD += pvdump.dbd
$(APPNAME)_DBD += asSupport.dbd
$(APPNAME)_DBD += devIocStats.dbd
$(APPNAME)_DBD += caPutLog.dbd
$(APPNAME)_DBD += utilities.dbd
$(APPNAME)_DBD += calcSupport.dbd 
$(APPNAME)_DBD += asyn.dbd 
$(APPNAME)_DBD += stream.dbd
$(APPNAME)_DBD += libjson.dbd 

## add other dbd here ##
$(APPNAME)_DBD += FileList.dbd

# Add all the support libraries needed by this IOC
## ISIS standard libraries ##
$(APPNAME)_LIBS += FileList
$(APPNAME)_LIBS += seq pv
$(APPNAME)_LIBS += devIocStats 
$(APPNAME)_LIBS += pvdump $(MYSQLLIB) easySQLite sqlite 
$(APPNAME)_LIBS += caPutLog
$(APPNAME)_LIBS += icpconfig pugixml
$(APPNAME)_LIBS += autosave
$(APPNAME)_LIBS += utilities 
$(APPNAME)_LIBS += stream
$(APPNAME)_LIBS += asyn 
$(APPNAME)_LIBS += calc sscan pcrecpp pcre 
$(APPNAME)_LIBS += libjson zlib efsw
## Add other libraries here ##
#$(APPNAME)_LIBS += xxx

# FileListTest_registerRecordDeviceDriver.cpp derives from FileListTest.dbd
$(APPNAME)_SRCS += $(APPNAME)_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
$(APPNAME)_SRCS_DEFAULT += $(APPNAME)Main.cpp
$(APPNAME)_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#$(APPNAME)_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
$(APPNAME)_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

