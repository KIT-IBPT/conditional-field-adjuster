/*************************************************************************
 * Copyright (c) 2026 Karlsruhe Institute of Technology.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE.txt that is included with this distribution.
 *************************************************************************/

#ifndef EPICS_CFA_RECORD_NAME_TEMPLATE_H
#define EPICS_CFA_RECORD_NAME_TEMPLATE_H

#include <string>
#include <variant>
#include <vector>

namespace epics {
namespace cfa {

/**
 * Template for generating a record name.
 */
class RecordNameTemplate {

public:

  /**
   * Component that is replaced with the name of the base record.
   */
  struct BaseComponent {
  };

  /**
   * Component that represents a literal string.
   */
  struct LiteralComponent {
    std::string value;
  };

  /**
   * Component that is replaced with an integer number that starts at one for
   * each base record and is then incremented each time a new name is
   * generated.
   */
  struct LocalSequenceComponent {
  };

  /**
   * Component that is replaced by a random string of the specified size.
   */
  struct RandomComponent {
    int size;
  };

  /**
   * Variant for the various types that can represent a template component.
   */
  using Component = std::variant<
    BaseComponent, LiteralComponent, LocalSequenceComponent, RandomComponent
  >;

  /**
   * Create a record name template from the specified template string.
   *
   * The template string can contain the following expressions:
   *
   * - “{random(N)}” where N is a positive number indicating the number of
   *   random characters that shall be inserted when the template is expanded.
   * - “{base}” which is replaced by the base record name.
   * - “{local_seq}” which is replaced by an integer number that is incremented
   *   when multiple names are generated for the same base record name.
   *
   * All other parts are the string are copied to the resulting record name
   * as-is. In order to use literal curly braces, they have to be doubled, so
   * that “{{” is replaced by “{” and “}}” is replaced by “}” in the final
   * name.
   */
  RecordNameTemplate(std::string const &templateString);

  /**
   * Return the components.
   */
  std::vector<Component> const &getComponents() const;

private:

  /**
   * Uniform distribution that is used to generate random characters.
   */
  std::vector<Component> components;

};

} // namespace cfa
} // namespace epics

#endif // EPICS_CFA_RECORD_NAME_TEMPLATE_H
