/*************************************************************************
 * Copyright (c) 2026 Karlsruhe Institute of Technology.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE.txt that is included with this distribution.
 *************************************************************************/

#include <algorithm>
#include <array>
#include <cctype>
#include <forward_list>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

#include <dbAccess.h>
#include <dbStaticLib.h>
#include <epicsString.h>

#include "RecordGenerator.h"
#include "RecordNameGenerator.h"

namespace epics {
namespace cfa {

namespace {

/**
 * Type of the link to a target field.
 */
enum class TargetLinkType {
  /**
   * Channel Access link to a field representing a (long) string.
   *
   * This type must be used for fields that cannot be written directly through
   * a DB link (in particular fields of type DBF_FWDLINK, DBF_INLINK, and
   * DBF_OUTLINK).
   */
  CA_LONG_STRING,

  /**
   * (Internal) DB link to a field representing a (long) string.
   *
   * This type should be used for fields of type DBF_STRING, in order to ensure
   * that strings that are longer than 39 characters are not truncated unless
   * the maximum string length supported by the target field is actually
   * exceeded.
   */
  DB_LONG_STRING,

  /**
   * (Internal) DB link to a regular field.
   *
   * This can be used for most field types.
   */
  DB_REGULAR
};

/**
 * Link field names for the fanout record.
 *
 * This array can be used to convert from an index to the name of the
 * respective link field.
 */
std::array<std::string, 16> fanoutLinkIndexToFieldName = {
  "LNK0",
  "LNK1",
  "LNK2",
  "LNK3",
  "LNK4",
  "LNK5",
  "LNK6",
  "LNK7",
  "LNK8",
  "LNK9",
  "LNKA",
  "LNKB",
  "LNKC",
  "LNKD",
  "LNKE",
  "LNKF"
};

/**
 * Create a chain of fanout records.
 *
 * The first record in the chain uses the specified name, the names for
 * additional records (if needed) are generated using the supplied
 * RecordNameGenerator.
 *
 * Links to the targets are added in the order in which they are returned by
 * the supplied iterator.
 */
template<typename Iterator>
void createFanoutChain(
  std::string const &recordName,
  Iterator targetsBegin,
  Iterator targetsEnd,
  RecordNameGenerator &recordNameGenerator
);

/**
 * Create a lso record.
 *
 * The newly created record uses the supplied name and OUT link.
 *
 * The supplied value is used to initialize the DOL field with a constant JSON
 * link, so that the record initially has the specified value.
 */
void createLsoRecord(
  std::string const &recordName,
  std::string const &outputLink,
  std::string const &value
);

/**
 * Create a record.
 *
 * The record uses the specified name and type. The record’s fiels are
 * initialized with the supplied values.
 */
void createRecord(
  std::string const &recordType,
  std::string const &recordName,
  std::map<std::string, std::string> const &fields
);

/**
 * Escape a character sequeunce for use in a JSON string.
 */
std::string escapeString(std::string const &str);

/**
 * Generate and return a link to the specified field of the specified record.
 *
 * Depending on the specified link type, the link might be an internal DB link
 * or a Channel Access link, and it might include a dollar-sign after the field
 * name in order to support long strings.
 */
std::string generateTargetLink(
  std::string const &recordName,
  std::string const &targetFieldName,
  TargetLinkType targetLinkType
);

/**
 * Retrive the type and value of a field for an existing record.
 */
std::pair<dbfType, std::string> getRecordFieldTypeAndValue(
  std::string const &recordName,
  std::string const &fieldName
);

/**
 * Throw an exception indicating that the name of an info entry is malformed.
 *
 * The exception is of type std::invalid_argument.
 */
[[noreturn]] void invalidInfoName(
  std::string const &recordName,
  std::string const &infoName,
  std::string const &description = ""
);

template<typename Iterator>
void createFanoutChain(
  std::string const &recordName,
  Iterator targetsBegin,
  Iterator targetsEnd,
  RecordNameGenerator &recordNameGenerator
) {
  // We use 15 link fields for actual targets and reserve the 16th field for a
  // link to the next fanout.
  std::map<std::string, std::string> fields;
  auto targetsIterator = targetsBegin;
  for (int linkIndex = 0; linkIndex < 15; ++linkIndex) {
    if (targetsIterator == targetsEnd) {
      break;
    }
    fields.emplace(fanoutLinkIndexToFieldName.at(linkIndex), *targetsIterator);
    ++targetsIterator;
  }
  // If there are more than 15 targets, we create another fanout record and
  // link to it.
  if (targetsIterator != targetsEnd) {
    auto nextFanoutRecordName = recordNameGenerator.generateRecordName();
    createFanoutChain(
      nextFanoutRecordName, targetsIterator, targetsEnd, recordNameGenerator
    );
    fields.emplace("LNKF", nextFanoutRecordName);
  }
  createRecord("fanout", recordName, fields);
}

void createLsoRecord(
  std::string const &recordName,
  std::string const &outputLink,
  std::string const &value
) {
  createRecord(
    "lso",
    recordName,
    {
      // We intentionally set DOL but not OMSL. For an lso record, a constant
      // JSON link in DOL is correctly used to initialize the VAL field, but it
      // is not correctly used when the record is processed subsequently. The
      // reason is that as of EPICS Base 7.0.9, the process function for the
      // lsoRecord used dbGetLinkLS, which calls getDBFType. For constant JSON
      // links, getDBFType is not implemented, so that dbGetLinkLS does not
      // work. On initialization in constrast, dbLoadLinkLS is used, which uses
      // different functions and thus works correctly.
      std::make_pair(
        "DOL",
        std::string("{const:\"") + escapeString(value) + "\"}"
      ),
      std::make_pair("OUT", outputLink),
      std::make_pair("SIZV", std::to_string(value.length() + 1))
    }
  );
}

void createRecord(
  std::string const &recordType,
  std::string const &recordName,
  std::map<std::string, std::string> const &fields
) {
  ::DBENTRY entry;
  ::dbInitEntry(pdbbase, &entry);
  try {
    if (::dbFindRecordType(&entry, recordType.c_str())) {
      throw std::runtime_error("Record type " + recordType + " not found.");
    }
    if (::dbCreateRecord(&entry, recordName.c_str())) {
      throw std::runtime_error(
        "Could not create record "
        + recordName
        + " of type "
        + recordType
        + "."
      );
    }
    for (auto const &fieldNameAndValue : fields) {
      auto const &fieldName = fieldNameAndValue.first;
      auto const &fieldValue = fieldNameAndValue.second;
      if (::dbFindField(&entry, fieldName.c_str())) {
        throw std::runtime_error(
          "Cannot set field "
          + fieldName
          + " of record "
          + recordName
          + " because this field does not exist for record type "
          + recordType
          + "."
        );
      }
      if (::dbPutString(&entry, fieldValue.c_str())) {
        throw std::runtime_error(
          "Cannot set field "
          + fieldName
          + " of record "
          + recordName
          + " of type "
          + recordType
          + " to value "
          + fieldValue
          + "."
        );
      }
    }
  } catch (...) {
    ::dbFinishEntry(&entry);
    throw;
  }
  ::dbFinishEntry(&entry);
}

std::string escapeString(std::string const &str) {
  // We first try with a buffer that can accommodate a few escaped characters.
  // When no escapes are needed, a buffer that is just one byte greater than
  // the length of the string would be sufficient, but using a buffer that is
  // slightly larger allows us to handle the common case where only few
  // characters must be escaped in the first attempt.
  std::vector<char> buffer(str.size() + 32);
  auto resultSize = ::epicsStrnEscapedFromRaw(
    buffer.data(), buffer.size(), str.data(), str.size()
  );
  // The length returns by epicsStrnEscapedFromRaw does not include the
  // terminating null-byte, so if the returned length is equal to the buffer
  // size, the last character has been lost.
  if (resultSize >= buffer.size()) {
    buffer.resize(resultSize + 1);
  }
  resultSize = ::epicsStrnEscapedFromRaw(
    buffer.data(), buffer.size(), str.data(), str.size()
  );
  assert(resultSize < buffer.size());
  return std::string(buffer.data(), resultSize);
}

std::string generateTargetLink(
  std::string const &recordName,
  std::string const &targetFieldName,
  TargetLinkType targetLinkType
) {
  auto link = recordName + "." + targetFieldName;
  switch (targetLinkType) {
  case TargetLinkType::CA_LONG_STRING:
    link += "$ CA";
    break;
  case TargetLinkType::DB_LONG_STRING:
    link += "$";
    break;
  case TargetLinkType::DB_REGULAR:
    break;
  }
  return link;
}

std::pair<dbfType, std::string> getRecordFieldTypeAndValue(
  std::string const &recordName,
  std::string const &fieldName
) {
  ::DBENTRY entry;
  int fieldType;
  std::string fieldValue;
  ::dbInitEntry(pdbbase, &entry);
  try {
    if (::dbFindRecord(&entry, recordName.c_str())) {
      throw std::runtime_error("Record " + recordName + " not found.");
    }
    if (::dbFindField(&entry, fieldName.c_str())) {
      throw std::invalid_argument(
        "Record "
        + recordName
        + " does not have a "
        + fieldName
        + " field."
      );
    }
    fieldType = ::dbGetFieldDbfType(&entry);
    if (fieldType < 0) {
      throw std::invalid_argument(
        "Could not determine field type of  "
        + recordName
        + "."
        + fieldName
        + "."
      );
    }
    auto rawValue = ::dbGetString(&entry);
    if (!rawValue) {
      throw std::invalid_argument(
        "Canot read field "
        + fieldName
        + " of record "
        + recordName
        + ". Most likely, the field is not readable for this record type."
      );
    }
    fieldValue = rawValue;
  } catch (...) {
    ::dbFinishEntry(&entry);
    throw;
  }
  ::dbFinishEntry(&entry);
  return std::make_pair(static_cast<dbfType>(fieldType), fieldValue);
}

[[noreturn]] void invalidInfoName(
  std::string const &recordName,
  std::string const &infoName,
  std::string const &description
) {
  std::string addendum(".");
  if (description.length()) {
    addendum = ": " + description;
  }
  throw std::invalid_argument(
    "Invalid info field " + infoName + " for record " + recordName + addendum
  );
};

} // anonymous namespace

std::string const RecordGenerator::infoNamePrefix = "cfa:";

RecordGenerator::RecordGenerator(
  RecordNameTemplate const &nameTemplate
) :
  nameTemplate(nameTemplate), randomSeed(2)
{
  std::random_device randomDevice;
  randomSeed.push_back(randomDevice());
  randomSeed.push_back(randomDevice());
}

void RecordGenerator::processInfoFields(
  std::string const& recordName,
  std::map<std::string, std::string> const &infoFields
) {
  std::map<int, std::map<std::string, std::string>> fieldValues;
  RecordNameGenerator recordNameGenerator(
    nameTemplate, recordName, randomSeed
  );
  std::string scanString = "Passive";
  std::string selectString;
  std::set<std::string> targetFieldNames;
  for (auto const &infoNameAndString : infoFields) {
    auto const &infoName = infoNameAndString.first;
    auto const &infoString = infoNameAndString.second;
    // Info fields that do not start with the appropriate prefix should not be
    // passed to this function.
    if (
      (infoName.length() < infoNamePrefix.length())
      || (infoName.substr(0, infoNamePrefix.length()) != infoNamePrefix)
    ) {
      invalidInfoName(
        recordName,
        infoName,
        "CFA info names must start with " + infoNamePrefix + "."
      );
    }
    auto parameterName = infoName.substr(infoNamePrefix.length());
    if (parameterName == "scan") {
      scanString = infoString;
      continue;
    }
    if (parameterName == "select") {
      selectString = infoString;
      continue;
    }
    // Process remaining fields of form cfa:<integer>:<field name>.
    auto firstColonPos = parameterName.find_first_of(':');
    if (firstColonPos == std::string::npos || !firstColonPos) {
      invalidInfoName(
        recordName,
        infoName,
        (
          "CFA info names must be of form "
          + infoNamePrefix
          + ":<numeric index>:<field name>"
        )
      );
    }
    auto indexString = parameterName.substr(0, firstColonPos);
    auto targetFieldName = parameterName.substr(firstColonPos + 1);
    int index;
    try {
      index = std::stoi(indexString);
    } catch (std::invalid_argument const &) {
      invalidInfoName(
        recordName,
        infoName,
        "Index string is not an integer number: " + indexString
      );
    } catch (std::out_of_range const &) {
      invalidInfoName(
        recordName, infoName, "Index must be between 0 and 32767."
      );
    }
    // We allow indices between 0 and 32767. We cannot allow greater indices
    // because the OFFS field of fanout records is a signed 16-bit integer.
    if (index < 0 || index > 32767) {
      invalidInfoName(
        recordName, infoName, "Index must be between 0 and 32767."
      );
    }
    if (
      !targetFieldName.length()
      || !std::all_of(
        targetFieldName.begin(),
        targetFieldName.end(),
        [](char c){return std::isalnum(c);}
      )
    ) {
      invalidInfoName(recordName, infoName, "Target field name must be alphanumeric.");
    }
    auto &fieldValueMap = (
      fieldValues.emplace(index, std::map<std::string, std::string>()).first->second
    );
    fieldValueMap.emplace(targetFieldName, infoString);
    targetFieldNames.insert(targetFieldName);
  }
  // The cfa:select option must be specified.
  if (selectString.empty()) {
    throw std::invalid_argument("cfa:select must be specified.");
  }
  // For each combination of target field and value, we only have to create a
  // single record, to which we can then link for the various cases where this
  // value is used.
  std::map<
    std::pair<std::string, std::string>, std::string
  > recordNameForTargetFieldValue;
  // Get “default” values for all target fields and generate associated
  // records.
  std::map<std::string, std::string> defaultValues;
  std::forward_list<std::string> defaultValueRecordNames;
  std::map<std::string, TargetLinkType> linkTypeForTargetField;
  // The defaultValueRecordNames are processed in the reverse order in which
  // they are added, so we add the target record first, so that it is processed
  // after all the records writing to fields of the target record have been
  // processed. This ensures that the changes made to fields take effect.
  defaultValueRecordNames.push_front(recordName);
  for (auto const &targetFieldName : targetFieldNames) {
    auto fieldTypeAndDefaultValue = getRecordFieldTypeAndValue(
      recordName, targetFieldName
    );
    auto fieldType = fieldTypeAndDefaultValue.first;
    auto const &defaultValue = fieldTypeAndDefaultValue.second;
    TargetLinkType linkType;
    switch (fieldType) {
      case DBF_STRING:
        // For strings, we can use a local DB link, but we have to append a
        // dollar sign to the field name in order to avoid truncating long
        // strings.
        linkType = TargetLinkType::DB_LONG_STRING;
        break;
      case DBF_CHAR:
      case DBF_UCHAR:
      case DBF_SHORT:
      case DBF_USHORT:
      case DBF_LONG:
      case DBF_ULONG:
      case DBF_INT64:
      case DBF_UINT64:
      case DBF_FLOAT:
      case DBF_DOUBLE:
      case DBF_ENUM:
      case DBF_MENU:
      case DBF_DEVICE:
        // This is the regular case where we can write directly to the field.
        linkType = TargetLinkType::DB_REGULAR;
        break;
      case DBF_INLINK:
      case DBF_OUTLINK:
      case DBF_FWDLINK:
        // This case needs special handling by using a CA link and writing to
        // to <record name>.<field name>$ instead of
        // <record name>.<field name>.
        linkType = TargetLinkType::CA_LONG_STRING;
        break;
      case DBF_NOACCESS:
        // Such a field can never be updated.
        throw std::invalid_argument(
          "Cannot handle field " + targetFieldName + " of type DBF_NOACCESS."
        );
        break;
      default:
        // We assume that we can handle every newly introduced type as a
        // regular (non-CA) link.
        linkType = TargetLinkType::DB_REGULAR;
        break;
    }
    linkTypeForTargetField.emplace(targetFieldName, linkType);
    defaultValues.emplace(
      targetFieldName, defaultValue
    );
  }
  // After we have processed all fields, we generate the lso records for the
  // default values. We do not do this in the preceding loop, because an
  // exception might occur if an invalid field is specified, and in this case
  // we do not want to create the records for any other fields either.
  for (auto const &defaultValueEntry : defaultValues) {
    auto const &targetFieldName = defaultValueEntry.first;
    auto const &defaultValue = defaultValueEntry.second;
    // For link fields, writing to the field directly from the generated lso
    // records does not work. EPICS reports a recGblDbaddrError with an
    // “Illegal Database Request Type”. This can be avoided by using a CA link.
    // In this case, we also append a $ to the field name, so that we can
    // successfully write strings that are longer than 39 characters, which
    // might be quite common when dealing with links.
    auto outputLink = generateTargetLink(
      recordName, targetFieldName, linkTypeForTargetField.at(targetFieldName)
    );
    auto lsoRecordName = recordNameGenerator.generateRecordName();
    createLsoRecord(lsoRecordName, outputLink, defaultValue);
    recordNameForTargetFieldValue.emplace(
      std::make_pair(targetFieldName, defaultValue), lsoRecordName
    );
    defaultValueRecordNames.emplace_front(std::move(lsoRecordName));
  }
  // Generate a fanout record that links to all the lso records for the default
  // case.
  auto defaultFanoutRecordName = recordNameGenerator.generateRecordName();
  createFanoutChain(
    defaultFanoutRecordName,
    defaultValueRecordNames.begin(),
    defaultValueRecordNames.end(),
    recordNameGenerator
  );
  // If there are no target fields, we do not have to generate any records, so
  // we can simply return.
  if (targetFieldNames.empty()) {
    return;
  }
  // For each supported index, we create a fanout record (or a chain of such
  // records) that links to lso records that set all handled fields to their
  // respective values for that case.
  std::map<int, std::string> fanoutRecordNameForIndex;
  for (auto const &entry : fieldValues) {
    auto const &index = entry.first;
    auto const &fieldValuesMap = entry.second;
    std::forward_list<std::string> fanoutTargets;
    // The fanoutTargets are processed in the reverse order in which they are
    // added, so we add the target record first, so that it is processed after
    // all the records writing to fields of the target record have been
    // processed.
    fanoutTargets.push_front(recordName);
    for (auto const &defaultValuesEntry : defaultValues) {
      auto const &targetFieldName = defaultValuesEntry.first;
      auto const &defaultValue = defaultValuesEntry.second;
      auto fieldValuesIterator = fieldValuesMap.find(targetFieldName);
      auto const &fieldValue = (
        (fieldValuesIterator != fieldValuesMap.end())
        ? fieldValuesIterator->second
        : defaultValue
      );
      auto recordNameIterator = recordNameForTargetFieldValue.find(
        std::make_pair(targetFieldName, fieldValue)
      );
      if (recordNameIterator == recordNameForTargetFieldValue.end()) {
        // There is no record yet for this specific target field and value, so
        // we create one.
        auto lsoRecordName = recordNameGenerator.generateRecordName();
        // For link fields, writing to the field directly from the generated
        // lso records does not work. EPICS reports a recGblDbaddrError with an
        // “Illegal Database Request Type”. This can be avoided by using a CA
        // link. In this case, we also append a $ to the field name, so that we
        // can successfully write strings that are longer than 39 characters,
        // which might be quite common when dealing with links.
        auto outputLink = generateTargetLink(
          recordName,
          targetFieldName,
          linkTypeForTargetField.at(targetFieldName)
        );
        createLsoRecord(lsoRecordName, outputLink, fieldValue);
        recordNameIterator = recordNameForTargetFieldValue.emplace(
          std::make_pair(targetFieldName, fieldValue), std::move(lsoRecordName)
        ).first;
      }
      // Add the name of the lso record to the list of fanout targets.
      fanoutTargets.push_front(recordNameIterator->second);
    }
    // Create a fanout record that triggers processing of all the records that
    // write to the various fields and finally triggers processing of the
    // target record itself, so that the field changes take effect.
    auto fanoutRecordName = recordNameGenerator.generateRecordName();
    createFanoutChain(
      fanoutRecordName,
      fanoutTargets.begin(),
      fanoutTargets.end(),
      recordNameGenerator
    );
    fanoutRecordNameForIndex.emplace(index, std::move(fanoutRecordName));
  }
  // We can use fanoutRecordNameForIndex.begin() and .rbegin() without checking
  // whether it is valid, because the preceding code ensures that there always
  // is at least one entry.
  auto minIndex = fanoutRecordNameForIndex.begin()->first;
  auto maxIndex = fanoutRecordNameForIndex.rbegin()->first;
  // We apply a global offset to all indices, so that minIndex is always mapped
  // to one. This way, we can be sure that index is available for the default
  // case and that we avoid a large gap, just because minIndex is much greater
  // than one.
  auto globalIndexOffset = 1 - minIndex;
  // Generate a calc record, that adds the global offset to the value from the
  // user-supplied record and ensures that all out-of-range values are mapped
  // to zero.
  std::string calcRecordName = recordNameGenerator.generateRecordName();
  // We need to determine the name of the first fanout record now, because the
  // calc record has a FLNK to this record.
  std::string nextFanoutRecordName = recordNameGenerator.generateRecordName();
  createRecord(
    "calc",
    calcRecordName,
    {
      std::make_pair(
        "CALC",
        (
          "(A >= "
          + std::to_string(minIndex)
          + " && A <= "
          + std::to_string(maxIndex)
          + ") ? (A + "
          + std::to_string(globalIndexOffset)
          + ") : 0"
        )
      ),
      std::make_pair("FLNK", nextFanoutRecordName),
      std::make_pair("INPA", selectString),
      std::make_pair("SCAN", scanString)
    }
  );
  // Generate fanout record that has the SELM field set to Specified and links
  // to the fanout records from fanoutRecordNameForIndex.
  std::vector<std::string> fanoutLinkTargets;
  // We need at least one link for the default case, so we use an index of zero
  // for this case. We shift all the other indices by one, so that zero can
  // also be used for a user-defined case.
  fanoutLinkTargets.push_back(defaultFanoutRecordName);
  int fanoutOffset = 0;
  for (
    auto fanoutRecordNameForIndexIterator = fanoutRecordNameForIndex.begin();
    fanoutRecordNameForIndexIterator != fanoutRecordNameForIndex.end();
  ) {
    // All indices are shifted by the global offset.
    auto index = fanoutRecordNameForIndexIterator->first + globalIndexOffset;
    auto const &targetLinkName = fanoutRecordNameForIndexIterator->second;
    auto linkIndex = index + fanoutOffset;
    // We have to fill gaps in the indices with links to the fanout for the
    // “default” case.
    auto expectedFanoutLinkTargetsSize = (linkIndex > 15) ? 16 : linkIndex;
    while (fanoutLinkTargets.size() < expectedFanoutLinkTargetsSize) {
      fanoutLinkTargets.push_back(defaultFanoutRecordName);
    }
    // The highest allowed link index is 15 (a fanout record has 16 links), so
    // we have to create a new record when we exceed this index.
    if (linkIndex > 15) {
      // Generate the fanout record and link to the next one.
      std::map<std::string, std::string> fanoutFields;
      fanoutFields.emplace("OFFS", std::to_string(fanoutOffset));
      fanoutFields.emplace("SELL", calcRecordName);
      fanoutFields.emplace("SELM", "Specified");
      for (int linkIndex = 0; linkIndex < 16; ++linkIndex) {
        auto const &linkTarget = fanoutLinkTargets.at(linkIndex);
        fanoutFields.emplace(
          fanoutLinkIndexToFieldName.at(linkIndex), linkTarget
        );
      }
      auto fanoutRecordName = std::move(nextFanoutRecordName);
      nextFanoutRecordName = recordNameGenerator.generateRecordName();
      fanoutFields.emplace("FLNK", nextFanoutRecordName);
      createRecord("fanout", fanoutRecordName, fanoutFields);
      // Prepare for generating the next fanout record. We deliberately do not
      // increment the iterator, because we have to handle the current entry
      // again, using the new offset.
      fanoutLinkTargets.clear();
      fanoutOffset -= 16;
    } else {
      // We now that the index of the newly created entry is linkIndex, because
      // we filled all lower indices previously.
      fanoutLinkTargets.push_back(targetLinkName);
      // We have handled the current entry, so we can proceed to the next one.
      ++fanoutRecordNameForIndexIterator;
    }
  }
  // We still need to generate the final fanout record.
  std::map<std::string, std::string> fanoutFields;
  fanoutFields.emplace("OFFS", std::to_string(fanoutOffset));
  fanoutFields.emplace("SELL", calcRecordName);
  fanoutFields.emplace("SELM", "Specified");
  while (fanoutLinkTargets.size() < 16) {
    fanoutLinkTargets.push_back(defaultFanoutRecordName);
  }
  for (int linkIndex = 0; linkIndex < 16; ++linkIndex) {
    auto const &linkTarget = fanoutLinkTargets.at(linkIndex);
    fanoutFields.emplace(
      fanoutLinkIndexToFieldName.at(linkIndex), linkTarget
    );
  }
  createRecord("fanout", nextFanoutRecordName, fanoutFields);
}

} // namespace cfa
} // namespace epics
