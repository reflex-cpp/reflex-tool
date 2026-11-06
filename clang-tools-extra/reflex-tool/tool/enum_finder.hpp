// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#ifndef REFLEX_TOOL_ENUM_FINDER_HPP
#define REFLEX_TOOL_ENUM_FINDER_HPP

#include <string_view>

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "common.hpp"

using namespace clang::ast_matchers;
using namespace clang;

namespace reflex {

class EnumFinder : public MatchFinder::MatchCallback {
public:
  EnumFinder(std::string_view BaseFilename, Metainformation &Metainformation)
      : BaseFilename{BaseFilename}, MetaInformation{Metainformation} {}

  void run(MatchFinder::MatchResult const &Result) override;

  void processEnumeration(const EnumDecl &Enumeration,
                          const MatchFinder::MatchResult &Result);
private:

  std::string_view BaseFilename;
  Metainformation &MetaInformation;
};

} // namespace reflex

#endif // REFLEX_TOOL_ENUM_FINDER_HPP
