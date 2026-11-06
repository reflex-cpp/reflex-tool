// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#include "enum_finder.hpp"

#include <filesystem>

#include <clang/Tooling/Tooling.h>

namespace fs = std::filesystem;

namespace reflex {

void EnumFinder::run(const MatchFinder::MatchResult &Result) {
  const auto *const Enumeration = Result.Nodes.getNodeAs<EnumDecl>("id");

  if (!Enumeration) {
    return;
  }

  const auto LocationFilename =
      Result.SourceManager->getFilename(Enumeration->getLocation()).str();
  const auto BaseFileAbsPath = fs::absolute(BaseFilename);

  if (fs::absolute(LocationFilename) == BaseFileAbsPath) {
    processEnumeration(*Enumeration, Result);
  }
}

void EnumFinder::processEnumeration(const EnumDecl &Enumeration,
                                    const MatchFinder::MatchResult &Result) {
  const auto Name = Enumeration.getIdentifier()->getName().str();
  const auto FullName = Enumeration.getQualifiedNameAsString();
  const auto Namespace = getNamespaceString(*Enumeration.getDeclContext());

  auto &EnumMetainfo = MetaInformation.Enumerations.emplace_back(
      EnumerationMetainfo{.Name = Name,
                          .Namespace = Namespace,
                          .FullName = FullName,
                          .Properties = getProperties(Enumeration)});

  for (const auto *const Enumerator : Enumeration.enumerators()) {
    EnumMetainfo.Items.emplace_back(
        EnumeratorMetainfo{.Name = Enumerator->getNameAsString(),
                           .Value = Enumerator->getInitVal().getExtValue(),
                           .Properties = getProperties(*Enumerator)});
  }
}

} // namespace reflex