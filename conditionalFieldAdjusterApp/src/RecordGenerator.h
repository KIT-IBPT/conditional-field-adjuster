/*************************************************************************
 * Copyright (c) 2026 Karlsruhe Institute of Technology.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE.txt that is included with this distribution.
 *************************************************************************/

#ifndef EPICS_CFA_RECORD_GENERATOR_H
#define EPICS_CFA_RECORD_GENERATOR_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "RecordNameTemplate.h"

namespace epics {
namespace cfa {

/**
 * Generates auxilliary records needed to dynamically adjust record fields.
 *
 * This is done based on information that is attached to the target record.
 */
class RecordGenerator {

public:

  /**
   * Prefix for the info entries.
   *
   * The name of each info entry that is handled by this module must start with
   * this prefix.
   */
  static std::string const infoNamePrefix;

  /**
   * Create a new record generator.
   */
  RecordGenerator(RecordNameTemplate const &nameTemplate);

  /**
   * Process the info fields associated with a record.
   *
   * The name of the info entries and their associated value strings must be
   * passed as a map. Only entries with names starting with the infoNamePrefix
   * must be included in the passed map.
   *
   * This method generates the auxillary records needed to dynamically adjust
   * the target record’s fields according to the configuration in the passed
   * map.
   */
  void processInfoFields(
    std::string const &recordName,
    std::map<std::string, std::string> const &infoFields
  );

private:

  /**
   * Template that is used for generating record names.
   */
  RecordNameTemplate nameTemplate;

  /**
   * Random seed that is used to seed the instances of RecordNameGenerator that
   * are used by this class.
   */
  std::vector<std::uint_least32_t> randomSeed;

};

} // namespace cfa
} // namespace epics

#endif // EPICS_CFA_RECORD_GENERATOR_H
