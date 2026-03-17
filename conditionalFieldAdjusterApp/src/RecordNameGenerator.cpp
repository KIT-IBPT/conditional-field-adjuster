#include "RecordNameGenerator.h"

namespace epics {
namespace cfa {

RecordNameGenerator::RecordNameGenerator(
  std::string const &prefix,
  std::string const &baseRecordName,
  std::vector<std::uint_least32_t> const &seeds
) :
  intDistribution(0, 61), prefix(prefix)
{
  std::vector<std::uint_least32_t> seedValues(
    baseRecordName.length() + seeds.size()
  );
  seedValues.insert(
    seedValues.end(), baseRecordName.begin(), baseRecordName.end()
  );
  seedValues.insert(seedValues.end(), seeds.begin(), seeds.end());
  std::seed_seq seedSeq(seedValues.cbegin(), seedValues.cend());
  randomEngine.seed(seedSeq);
}

std::string RecordNameGenerator::generateRecordName() {
  std::string recordName;
  recordName.reserve(prefix.length() + 30);
  recordName += prefix;
  for (int i = 0; i < 30; ++i) {
    char randomOffset = intDistribution(randomEngine);
    if (randomOffset < 10) {
      recordName.push_back('0' + randomOffset);
    } else if (randomOffset < 36) {
      recordName.push_back('A' + randomOffset - 10);
    } else {
      recordName.push_back('a' + randomOffset - 36);
    }
  }
  return recordName;
}

} // namespace cfa
} // namespace epics
