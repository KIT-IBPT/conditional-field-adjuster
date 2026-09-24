/*************************************************************************
 * Copyright (c) 2026 Karlsruhe Institute of Technology.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE.txt that is included with this distribution.
 *************************************************************************/

#include <sstream>

#include "RecordNameTemplate.h"

namespace epics {
namespace cfa {

namespace {

class Parser {

public:

  Parser(std::string const &inputString)
      : inputString(inputString), position(0) {
  }

  std::vector<RecordNameTemplate::Component> parse() {
    std::vector<RecordNameTemplate::Component> components;
    std::string literalBuffer;
    while (!isEndOfString()) {
      if (accept("{")) {
        if (accept("{")) {
          literalBuffer += "{";
        } else {
          if (!literalBuffer.empty()) {
            components.emplace_back(
              RecordNameTemplate::LiteralComponent{literalBuffer}
            );
            literalBuffer.clear();
          }
          components.emplace_back(expression());
          expect("}");
        }
      } else if (accept("}")) {
        expect("}");
        literalBuffer += "}";
      } else {
        literalBuffer += peek();
        ++position;
      }
    }
    if (!literalBuffer.empty()) {
      components.emplace_back(
        RecordNameTemplate::LiteralComponent{literalBuffer}
      );
    }
    return components;
  }

private:

  static std::string const digitChars;

  std::string inputString;
  std::size_t position;

  bool accept(std::string const &str) {
    if (inputString.length() - position < str.length()) {
      return false;
    }
    if (inputString.substr(position, str.length()) == str) {
      position += str.length();
      return true;
    } else {
      return false;
    }
  }

  bool acceptAnyOf(std::string const &characters) {
    if (isEndOfString()) {
      return false;
    }
    if (characters.find(peek()) != std::string::npos) {
      ++position;
      return true;
    } else {
      return false;
    }
  }

  std::string excerpt() {
    if (inputString.length() - position > 5) {
      return inputString.substr(position, 5);
    } else {
      return inputString.substr(position);
    }
  }

  void expect(std::string const &str) {
    if (!accept(str)) {
      if (isEndOfString()) {
        throwException(std::string("Expected \"") + str
          + "\", but found end of string.");
      } else {
        throwException(std::string("Expected \"") + str
          + "\", but found \"" + excerpt() + "\".");
      }
    }
  }

  void expectAnyOf(std::string const &characters) {
    if (!acceptAnyOf(characters)) {
      if (isEndOfString()) {
        throwException(std::string("Expected any of \"") + characters
          + "\", but found end of string.");
      } else {
        throwException(std::string("Expected any of \"") + characters
          + "\", but found \"" + peek() + "\".");
      }
    }
  }

  RecordNameTemplate::Component expression() {
    if (accept("base")) {
      return RecordNameTemplate::BaseComponent();
    }
    if (accept("local_seq")) {
      return RecordNameTemplate::LocalSequenceComponent();
    }
    if (accept("random")) {
      expect("(");
      auto component = RecordNameTemplate::RandomComponent{positiveInteger()};
      expect(")");
      return component;
    }
    throwException(
      std::string("Expected 'base', 'local_seq', or 'random' but found '")
      + excerpt()
      + "'."
    );
  }

  bool isEndOfString() {
    return position == inputString.length();
  }

  char peek() {
    return inputString.at(position);
  }

  int positiveInteger() {
    auto startPos = position;
    expectAnyOf(digitChars);
    do {
    } while (acceptAnyOf(digitChars));
    auto endPos = position;
    return std::stoi(inputString.substr(startPos, endPos - startPos));
  }

  [[noreturn]] void throwException(std::string const &message) const {
    std::ostringstream os;
    os << "Error at character " << (position + 1)
      << " of the template string: " << message;
    throw std::invalid_argument(os.str());
  }

};

std::string const Parser::digitChars = std::string("0123456789");

} // anonymous namespace

RecordNameTemplate::RecordNameTemplate(
  std::string const &templateString
) : components(Parser(templateString).parse()) {
}

std::vector<
  RecordNameTemplate::Component
> const &RecordNameTemplate::getComponents() const {
  return components;
}

} // namespace cfa
} // namespace epics
