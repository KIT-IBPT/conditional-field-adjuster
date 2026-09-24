/*************************************************************************
 * Copyright (c) 2026 Karlsruhe Institute of Technology.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE.txt that is included with this distribution.
 *************************************************************************/

#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <map>

#include <dbAccess.h>
#include <dbStaticLib.h>
#include <epicsExport.h>
#include <epicsVersion.h>
#include <errlog.h>
#include <initHooks.h>
#include <iocsh.h>

#include "RecordGenerator.h"

// The error handling code needs the macros for ANSI escape sequences that were
// introduced with EPICS Base version 7.0.7.
#if EPICS_VERSION_INT < VERSION_INT(7,0,7,0)
#  error "This module needs EPICS Base version 7.0.7 or newer."
#endif // EPICS_VERSION_INT < VERSION_INT(7,0,7,0)

using namespace epics::cfa;

namespace {

/**
 * Record name template that is used when processing the info fields.
 *
 * This can be set from the IOC shell by calling the
 * cfaSetRecordNameTemplate function. If it is not set, a default template is
 * used.
 */
std::optional<RecordNameTemplate> globalRecordNameTemplate;

/**
 * Print an error message.
 */
void printError(
  std::string const &recordName, std::string const &exceptionMessage
) {
  if (recordName.empty()) {
    ::errlogPrintf(
      ANSI_RED(
        "Error: Initialization of conditional field adjuster failed%s%s\n"
      ),
      (exceptionMessage.empty()) ? "." : ": ",
      exceptionMessage.c_str()
    );
  } else {
    ::errlogPrintf(
      ANSI_RED("%s: Error configuring conditional field adjuster%s%s\n"),
      recordName.c_str(),
      (exceptionMessage.empty()) ? "." : ": ",
      exceptionMessage.c_str()
    );
  }
}

/**
 * Iterate over all records and process the CFA info fields.
 *
 * Records that do not contain info entries for the conditional field adjuster
 * are simply ignored.
 */
void processCfaInfoFields() {
  std::map<std::string, std::string> arguments;
  ::DBENTRY entry;
  void *lastRecord = nullptr;
  std::string lastRecordName;
  RecordGenerator recordGenerator(
    globalRecordNameTemplate.value_or(RecordNameTemplate(("cfa:{random(30)}")))
  );
  ::dbInitEntry(pdbbase, &entry);

  // dbNextMatchingInfo iterates over record types, then records, then info
  // fields. This means that all fields for the same record should be returned
  // contiguously, which means that we can assume that we have seen all fields
  // for a record when the iteration reaches the next record.
  try {
    while (!::dbNextMatchingInfo(&entry, nullptr)) {
      void *record = entry.precnode->precord;
      if (record != lastRecord) {
        if (lastRecord && arguments.size()) {
          try {
            recordGenerator.processInfoFields(
              lastRecordName, arguments
            );
          } catch (std::exception const &err) {
            printError(lastRecordName, err.what());
          } catch (...) {
            printError(lastRecordName, "");
          }
          arguments.clear();
        }
        lastRecord = record;
        lastRecordName = ::dbGetRecordName(&entry);
      }
      auto infoName = std::string(::dbGetInfoName(&entry));
      if (
        (infoName.length() >= RecordGenerator::infoNamePrefix.length())
        && (
          infoName.substr(0, RecordGenerator::infoNamePrefix.length())
          == RecordGenerator::infoNamePrefix
        )
      ) {
        arguments.emplace(
          std::move(infoName), std::string(::dbGetInfoString(&entry))
        );
      }
    }
    if (lastRecord && arguments.size()) {
      try {
        recordGenerator.processInfoFields(
          lastRecordName, arguments
        );
      } catch (std::exception const &err) {
        printError(lastRecordName, err.what());
      } catch (...) {
        printError(lastRecordName, "");
      }
    }
  } catch (...) {
    ::dbFinishEntry(&entry);
    throw;
  }
  ::dbFinishEntry(&entry);
}

} // anonymous namespace

extern "C" {

/**
 * Init hook that creates aux. records for the conditional field adjuster.
 *
 * This hook does its work during the initHookAfterInitDevSup phase of the IOC
 * startup process.
 */
static void cfaInitHook(::initHookState state) noexcept {
  if (state != initHookAfterInitDevSup) {
    return;
  }
  try {
    processCfaInfoFields();
  } catch (std::exception const &err) {
    printError("", err.what());
  } catch (...) {
    printError("", "");
  }
}

static const iocshArg iocshCfaSetRecordNameTemplateArg0 {
  "template string", iocshArgString
};
static const iocshArg * const iocshCfaSetRecordNameTemplateArgs[] = {
  &iocshCfaSetRecordNameTemplateArg0
};
static const iocshFuncDef iocshCfaSetRecordNameTemplateFuncDef = {
  "cfaSetRecordNameTemplate", 1, iocshCfaSetRecordNameTemplateArgs
};

static void iocshCfaSetRecordNameTemplateFunc(
  const iocshArgBuf *args
) noexcept {
  char *templateString = args[0].sval;
  // Verify and convert the parameters.
  if (!templateString || !std::strlen(templateString)) {
    ::errlogPrintf(
      ANSI_RED(
        "Error: Template string must be specified.\n"
      )
    );
    ::iocshSetError(1);
    return;
  }
  try {
    globalRecordNameTemplate = RecordNameTemplate(templateString);
  } catch (std::exception &e) {
    ::errlogPrintf(ANSI_RED("%s"), e.what());
    ::iocshSetError(1);
    return;
  } catch (...) {
    ::errlogPrintf(ANSI_RED("Error: Record name template could not be set."));
    ::iocshSetError(1);
    return;
  }
}

/**
 * Registrar that registers the hooks needed by the conditional field adjuster.
 */
static void conditionalFieldAdjusterRegistrar() noexcept {
  ::initHookRegister(cfaInitHook);
  ::iocshRegister(
    &iocshCfaSetRecordNameTemplateFuncDef,
    iocshCfaSetRecordNameTemplateFunc
  );
}

epicsExportRegistrar(conditionalFieldAdjusterRegistrar);

} // extern "C"
