TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += conditionalFieldAdjuster

# install conditionalFieldAdjuster.dbd into <top>/dbd
DBD += conditionalFieldAdjuster.dbd

# specify all source files to be compiled and added to the library
conditionalFieldAdjuster_SRCS += cfaRegistrar.cpp
conditionalFieldAdjuster_SRCS += RecordGenerator.cpp
conditionalFieldAdjuster_SRCS += RecordNameGenerator.cpp
conditionalFieldAdjuster_SRCS += RecordNameTemplate.cpp

conditionalFieldAdjuster_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

