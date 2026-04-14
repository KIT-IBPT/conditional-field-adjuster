# SPDX-FileCopyrightText: 2003 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = cfaExample
DBD += cfaExample.dbd

cfaExample_DBD += base.dbd

cfaExample_SRCS += cfaExample_registerRecordDeviceDriver.cpp
cfaExample_SRCS += cfaExampleMain.cpp

cfaExample_DBD += conditionalFieldAdjuster.dbd
cfaExample_LIBS += conditionalFieldAdjuster

cfaExample_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
