TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += FileList

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install FileList.dbd into <top>/dbd
DBD += FileList.dbd

# specify all source files to be compiled and added to the library
FileList_SRCS += FileList.cpp

FileList_LIBS += asyn pcre libjson utilities zlib
FileList_LIBS += $(EPICS_BASE_IOC_LIBS)
FileList_SYS_LIBS_WIN32 += ws2_32

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

