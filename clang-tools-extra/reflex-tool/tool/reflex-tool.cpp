// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#include <llvm/Support/CommandLine.h>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Basic/MacroBuilder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

#include "alias_finder.hpp"
#include "class_finder.hpp"
#include "function_finder.hpp"
#include "enum_finder.hpp"
#include "json_writer.hpp"

using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory ReflexToolCategory("reflex-tool options");

clang::CompilerInstance * GCompilerInstance = nullptr;

static cl::opt<std::string> MetadataFilename(
    "metadata-filename",
    cl::desc("The path to output file with metadata in JSON format"),
    cl::cat(ReflexToolCategory));

struct PreprocessorConfigCallback : public SourceFileCallbacks {
  bool handleBeginSource(clang::CompilerInstance &CI) override {
    auto &Preprocessor = CI.getPreprocessor();
    auto PredefineBuffer = Preprocessor.getPredefines();
    llvm::raw_string_ostream Predefines(PredefineBuffer);
    clang::MacroBuilder Builder(Predefines);
    Builder.defineMacro("__REFLEX_TOOL__");

    CI.getFrontendOpts().SkipFunctionBodies = true;
    GCompilerInstance = &CI;

    Preprocessor.setPredefines(Predefines.str());

    return true;
  }
};

int main(int argc, const char **argv) {
  using namespace reflex;
  namespace clam = clang::ast_matchers;

  auto ExpectedParser =
      tooling::CommonOptionsParser::create(argc, argv, ReflexToolCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  tooling::CommonOptionsParser &OptionsParser = ExpectedParser.get();

  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  Tool.clearArgumentsAdjusters(); // used to allow depfile generation

  const auto Filename = OptionsParser.getSourcePathList()[0];
  const auto MetadataJsonFilename = MetadataFilename.getValue();

  Metainformation Metainfo;
  ClassFinder ClassFinder{Filename, Metainfo};
  EnumFinder EnumFinder{Filename, Metainfo};
  FunctionFinder FunctionFinder{Filename, Metainfo};
  AliasFinder AliasFinder{Filename, Metainfo, ClassFinder, EnumFinder};

  const auto ClassMatcher =
      cxxRecordDecl(clam::decl(clam::hasAttr(clang::attr::Kind::ReflexReflect))
                        .bind("reflexDecl"));

  const auto EnumMatcher =
      enumDecl(clam::hasAttr(clang::attr::Kind::ReflexReflect)).bind("id");

  const auto AliasMatcher =
      typeAliasDecl(clam::hasAttr(clang::attr::Kind::ReflexReflect)).bind("id");

  const auto FunctionMatcher = functionDecl(clam::hasAttr(clang::attr::Kind::ReflexReflect)).bind("id");

  MatchFinder Finder;
  Finder.addMatcher(ClassMatcher, &ClassFinder);
  Finder.addMatcher(EnumMatcher, &EnumFinder);
  Finder.addMatcher(FunctionMatcher, &FunctionFinder);
  Finder.addMatcher(AliasMatcher, &AliasFinder);

  PreprocessorConfigCallback SourceFileCallback;
  const auto Result =
      Tool.run(newFrontendActionFactory(&Finder, &SourceFileCallback).get());

  std::error_code ErrorCode{};
  raw_fd_ostream OutputStream{MetadataJsonFilename, ErrorCode};

  if (ErrorCode) {
    llvm::errs() << "Unable to open metadata file for writing: "
                 << ErrorCode.message() << "\n";
    return 1;
  }

  json::OStream JOS(OutputStream, 2);
  serialize(Metainfo, JOS);

  return Result;
}
