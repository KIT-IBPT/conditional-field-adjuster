#!../../bin/linux-x86_64/cfaExample

#- SPDX-FileCopyrightText: 2003 Argonne National Laboratory
#-
#- SPDX-License-Identifier: EPICS

#- You may have to change cfaExample to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/cfaExample.dbd"
cfaExample_registerRecordDeviceDriver pdbbase

## Load record instances
#dbLoadRecords("db/cfaExample.db","user=hi0724")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=hi0724"
