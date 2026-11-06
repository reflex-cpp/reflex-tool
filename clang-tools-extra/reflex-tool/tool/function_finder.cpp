// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#include "function_finder.hpp"

#include <filesystem>

#include <clang/Tooling/Tooling.h>
#include <clang/AST/QualTypeNames.h>

namespace fs = std::filesystem;

namespace reflex {

using namespace std::string_view_literals;

void FunctionFinder::run(const MatchFinder::MatchResult &Result) {
  const auto *const Function = Result.Nodes.getNodeAs<FunctionDecl>("id");
  if (!Function) {
    return;
  }

  const auto RecordFilename =
      Result.SourceManager->getFilename(Function->getLocation()).str();
  const auto BaseFileAbsPath = fs::absolute(BaseFilename);

  if (fs::absolute(RecordFilename) == BaseFileAbsPath) {
    processFunction(*Function, Result);
  }
}

void FunctionFinder::processFunction(const FunctionDecl &Function,
                                     const MatchFinder::MatchResult &Result) {
  const auto FunctionName = Function.getIdentifier()->getName().str();
  const auto StructFullName = Function.getQualifiedNameAsString();
  const auto Namespace = getNamespaceString(*Function.getDeclContext());

  auto &FunctionInfo = MetaInformation.Functions.emplace_back(FunctionMetainfo{
      .Name = FunctionName,
      .Namespace = Namespace,
      .FullName = StructFullName,
      .ReturnType = getTypeInfo(Function.getReturnType(), Function.getASTContext()),
      .Properties = getProperties(Function)});

  for (const auto &Parameter : Function.parameters()) {
    std::optional<std::string> DefaultArgValue;
    const auto * const DefaultArg = Parameter->getDefaultArg();
    if (DefaultArg != nullptr) {
      assert(DefaultArg);
      DefaultArgValue =
          Lexer::getSourceText(
              CharSourceRange::getTokenRange(DefaultArg->getSourceRange()),
              *Result.SourceManager, Result.Context->getLangOpts())
              .str();
    }

    FunctionInfo.Arguments.emplace_back(ArgumentMetainfo{
        .Name = Parameter->getName().str(),
        .Type = getTypeInfo(Parameter->getType(), Function.getASTContext()),
        .DefaultValue = DefaultArgValue,
        .Properties = getProperties(*Parameter)});
  }
}

} // namespace reflex