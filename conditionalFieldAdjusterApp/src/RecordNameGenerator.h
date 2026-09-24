/*************************************************************************
 * Copyright (c) 2026 Karlsruhe Institute of Technology.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE.txt that is included with this distribution.
 *************************************************************************/

#ifndef EPICS_CFA_RECORD_NAME_GENERATOR_H
#define EPICS_CFA_RECORD_NAME_GENERATOR_H

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "RecordNameTemplate.h"

namespace epics {
namespace cfa {

/**
 * Generates random record names.
 */
class RecordNameGenerator {

public:

  /**
   * Create a record name generator.
   *
   * The generator uses the specified template in order to generate record
   * names. The baseRecordName combined with the seeds is used to seed the
   * internal pseudo random number generator (PRNG).
   */
  RecordNameGenerator(
    RecordNameTemplate const &nameTemplate,
    std::string const &baseRecordName,
    std::vector<std::uint_least32_t> const &seeds
  );

  /**
   * Generate and return the next record name.
   */
  std::string generateRecordName();

private:

  /**
   * Name of the base record for which the name is generated.
   */
  std::string baseRecordName;

  /**
   * Uniform distribution that is used to generate random characters.
   */
  std::uniform_int_distribution<int> intDistribution;

  /**
   * Sequence number that can be used when generating record names.
   */
  int localSequenceNumber;

  /**
   * Template that is used for generating record names.
   */
  RecordNameTemplate nameTemplate;

  /**
   * Random engine that is used to generate (pseudo) random numbers.
   *
   * These random numbers are the base for generating a (pseudo) random
   * character sequence.
   */
  std::mt19937 randomEngine;

};

} // namespace cfa
} // namespace epics

#endif // EPICS_CFA_RECORD_NAME_GENERATOR_H
