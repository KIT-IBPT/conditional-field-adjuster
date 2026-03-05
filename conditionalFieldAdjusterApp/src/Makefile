# SPDX-FileCopyrightText: 2003 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += conditionalFieldAdjuster

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install conditionalFieldAdjuster.dbd into <top>/dbd
DBD += conditionalFieldAdjuster.dbd

# specify all source files to be compiled and added to the library
#conditionalFieldAdjuster_SRCS += xxx

conditionalFieldAdjuster_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

