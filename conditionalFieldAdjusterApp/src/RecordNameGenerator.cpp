/*************************************************************************
 * Copyright (c) 2026 Karlsruhe Institute of Technology.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE.txt that is included with this distribution.
 *************************************************************************/

#include <type_traits>

#include "RecordNameGenerator.h"

namespace epics {
namespace cfa {

RecordNameGenerator::RecordNameGenerator(
  RecordNameTemplate const &nameTemplate,
  std::string const &baseRecordName,
  std::vector<std::uint_least32_t> const &seeds
) :
  baseRecordName(baseRecordName),
  intDistribution(0, 61),
  localSequenceNumber(1),
  nameTemplate(nameTemplate)
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
  for (auto const &component : nameTemplate.getComponents()) {
    std::visit(
      [&recordName, this](auto &&arg) {
        using ComponentType = std::decay_t<decltype(arg)>;
        if constexpr (
          std::is_same_v<ComponentType, RecordNameTemplate::BaseComponent>
        ) {
          recordName += this->baseRecordName;
        } else if constexpr (
          std::is_same_v<ComponentType, RecordNameTemplate::LiteralComponent>
        ) {
          recordName += arg.value;
        } else if constexpr (
          std::is_same_v<ComponentType, RecordNameTemplate::LocalSequenceComponent>
        ) {
          recordName += std::to_string(this->localSequenceNumber);
        } else if constexpr (
          std::is_same_v<ComponentType, RecordNameTemplate::RandomComponent>
        ) {
          for (int i = 0; i < arg.size; ++i) {
            int randomOffset = this->intDistribution(randomEngine);
            if (randomOffset < 10) {
              recordName.push_back('0' + randomOffset);
            } else if (randomOffset < 36) {
              recordName.push_back('A' + randomOffset - 10);
            } else {
              recordName.push_back('a' + randomOffset - 36);
            }
          }
        } else {
          static_assert(false, "Unhandled component variant.");
        }
      },
      component
    );
  }
  ++localSequenceNumber;
  return recordName;
}

} // namespace cfa
} // namespace epics
