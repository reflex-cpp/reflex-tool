// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#include "alias_finder.hpp"

#include <filesystem>

#include <clang/Sema/Sema.h>
#include <clang/AST/QualTypeNames.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>


extern clang::CompilerInstance * GCompilerInstance;

namespace reflex {

using namespace std::string_view_literals;
namespace fs = std::filesystem;

void AliasFinder::run(const MatchFinder::MatchResult &Result) {
  const auto *const TypeAlias =
      Result.Nodes.getNodeAs<TypeAliasDecl>("id");
  if (!TypeAlias) {
    return;
  }

  const auto RecordFilename =
      Result.SourceManager->getFilename(TypeAlias->getLocation()).str();
  const auto BaseFileAbsPath = fs::absolute(BaseFilename);

  if (fs::absolute(RecordFilename) == BaseFileAbsPath) {
    processRecord(*TypeAlias, Result);
  }
}

void AliasFinder::processRecord(const TypeAliasDecl &Decl,
                                const MatchFinder::MatchResult &Result) {
  const auto UnderlyingType = Decl.getUnderlyingType();
  const auto *const TagDecl = UnderlyingType->getAsTagDecl();
  if (!TagDecl) {
    llvm::errs() << "Unable to get tag declaration for alias:" << Decl.
        getQualifiedNameAsString();
    std::exit(111);
  }

  auto &Sema = GCompilerInstance->getSema();
  if (Sema.RequireCompleteType(TagDecl->getLocation(), UnderlyingType,
                               DiagnosticIDs::Error)) {
    llvm::errs() << "Type alias is not resolvable:" << TagDecl->
        getQualifiedNameAsString();
    std::exit(111);
  }

  const auto IsEnum = UnderlyingType->getAsTagDecl()->isEnum();
  if (IsEnum) {
    const auto *const AsEnumDecl = dyn_cast<EnumDecl>(
        UnderlyingType->getAsTagDecl());
    assert(AsEnumDecl != nullptr);
    EnumFinderInstance.processEnumeration(*AsEnumDecl, Result);
    return;
  }

  const auto *const CxxRecordDecl = UnderlyingType->getAsCXXRecordDecl();
  if (CxxRecordDecl) {
    ClassFinderInstance.processRecord(*CxxRecordDecl, Result);
    return;
  }

  llvm::errs() << "Unhandled alias type";
  std::exit(111);
}

} // namespace reflex