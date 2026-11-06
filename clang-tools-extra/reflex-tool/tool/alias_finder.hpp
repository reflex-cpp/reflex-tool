// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#ifndef REFLEX_TOOL_ALIAS_FINDER_HPP
#define REFLEX_TOOL_ALIAS_FINDER_HPP

#include <string_view>

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "common.hpp"
#include "class_finder.hpp"
#include "enum_finder.hpp"

using namespace clang::ast_matchers;
using namespace clang;

namespace reflex {

class AliasFinder : public MatchFinder::MatchCallback {
public:
  AliasFinder(std::string_view BaseFilename, Metainformation &MetaInformation, ClassFinder& ClassFinderInstance, EnumFinder& EnumFinderInstance)
      : BaseFilename{BaseFilename}, MetaInformation{MetaInformation}, ClassFinderInstance{ClassFinderInstance}, EnumFinderInstance{EnumFinderInstance} {}

  void run(MatchFinder::MatchResult const &Result) override;

private:
  void processRecord(const TypeAliasDecl &Decl,
                     const MatchFinder::MatchResult &Result);

  std::string_view BaseFilename;
  Metainformation &MetaInformation;
  ClassFinder& ClassFinderInstance;
  EnumFinder& EnumFinderInstance;
};

} // namespace reflex

#endif // REFLEX_TOOL_ALIAS_FINDER_HPP
