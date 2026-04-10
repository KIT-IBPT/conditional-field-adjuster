#ifndef EPICS_CFA_RECORD_NAME_GENERATOR_H
#define EPICS_CFA_RECORD_NAME_GENERATOR_H

#include <cstdint>
#include <random>
#include <string>
#include <vector>

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
   * All generated record names start with the specified prefix. The
   * baseRecordName combined with the seeds is used to seed the internal pseudo
   * random number generator (PRNG).
   */
  RecordNameGenerator(
    std::string const &prefix,
    std::string const &baseRecordName,
    std::vector<std::uint_least32_t> const &seeds
  );

  /**
   * Generate and return the next record name.
   */
  std::string generateRecordName();

private:

  /**
   * Uniform distribution that is used to generate random characters.
   */
  std::uniform_int_distribution<int> intDistribution;

  /**
   * Prefix that is prepended to all generated names.
   */
  std::string prefix;

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

#endif // EPICS_CFA_GENERATOR_H
