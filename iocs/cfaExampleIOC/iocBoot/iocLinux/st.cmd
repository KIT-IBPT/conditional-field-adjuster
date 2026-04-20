#!../../bin/linux-x86_64/cfaExample

< envPaths

cd "${TOP}"

dbLoadDatabase "dbd/cfaExample.dbd"
cfaExample_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords("db/cfaExampleSimple.db","P=Example:,R=")

cd "${TOP}/iocBoot/${IOC}"
iocInit
