// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#include "class_finder.hpp"

#include <filesystem>

#include <clang/AST/QualTypeNames.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Sema/Sema.h>

namespace fs = std::filesystem;

extern clang::CompilerInstance * GCompilerInstance;

namespace reflex {

using namespace std::string_view_literals;

void ClassFinder::run(const MatchFinder::MatchResult &Result) {
  const auto *const Record =
      Result.Nodes.getNodeAs<CXXRecordDecl>("reflexDecl");
  if (!Record) {
    return;
  }

  const auto RecordFilename =
      Result.SourceManager->getFilename(Record->getLocation()).str();
  const auto BaseFileAbsPath = fs::absolute(BaseFilename);

  if (fs::absolute(RecordFilename) == BaseFileAbsPath) {
    processRecord(*Record, Result);
  }
}

void ClassFinder::processRecord(const CXXRecordDecl &Record,
                                const MatchFinder::MatchResult &Result) {
  auto StructName = Record.getIdentifier()->getName().str();
  auto StructFullName = Record.getQualifiedNameAsString();
  const auto Namespace = getNamespaceString(*Record.getDeclContext());

  auto &StructInfo = MetaInformation.Structures.emplace_back(
      StructureMetainfo{.Name = StructName,
                        .Namespace = Namespace,
                        .FullName = StructFullName,
                        .Properties = getProperties(Record)});

  const auto &TemplateSpecialization =
      dyn_cast<clang::ClassTemplateSpecializationDecl>(&Record);
  if (TemplateSpecialization) {
    if (!TemplateSpecialization->hasDefinition()) {
      llvm::errs() << "No definition for template specialization " << StructInfo
          .FullName << "\n";
      exit(1);
    }

    const clang::PrintingPolicy PrintPolicy(
        Record.getASTContext().getLangOpts());

    auto ShortNamePolicy =
        clang::PrintingPolicy{Record.getASTContext().getLangOpts()};
    ShortNamePolicy.SuppressScope = true;
    ShortNamePolicy.ConstantArraySizeAsWritten = true;
    ShortNamePolicy.IncludeNewlines = false;
    ShortNamePolicy.ConstantsAsWritten = true;
    ShortNamePolicy.FullyQualifiedName = false;
    ShortNamePolicy.PrintCanonicalTypes = true;

    const auto &DeclType = Record.getTypeForDecl();
    const auto TypeInfo = getTypeInfo(DeclType->getCanonicalTypeInternal(), Record.getASTContext());

    // TODO unify type handling of structures and other types
    if (TypeInfo.TemplateInfo.has_value()) {
      StructInfo.TemplateSpecializationInfo = std::any_cast<
        TemplateSpecializationMetainfo>(TypeInfo.TemplateInfo);
    }

    StructInfo.Name =
        DeclType->getCanonicalTypeInternal().getAsString(ShortNamePolicy);
    StructInfo.FullName =
        DeclType->getCanonicalTypeInternal().getAsString(PrintPolicy);
    StructInfo.IsTemplateInstantiation = true;
  }

  if (Record.isTemplated()) {
    TemplateInfo TemplInfo;

    const auto &DescribedTemplate = Record.getDescribedTemplate();

    const auto &TemplateParams = DescribedTemplate->getTemplateParameters();
    for (const auto *ParamIt = TemplateParams->begin();
         ParamIt < TemplateParams->end(); ++ParamIt) {

      const auto *TTP = dyn_cast<TemplateTypeParmDecl>(*ParamIt);
      if (TTP) {
        TemplInfo.Arguments.push_back({.Name = (*ParamIt)->getName().str(),
                                       .Kind = TemplateArgument::Type});
        continue;

      }
      const auto *NTTP = dyn_cast<NonTypeTemplateParmDecl>(*ParamIt);
      if (NTTP) {
        TemplInfo.Arguments.push_back({.Name = (*ParamIt)->getName().str(),
                                       .Kind = TemplateArgument::Integral});
        continue;
      }
      llvm::errs() << "Unsupported template parameter kind" << "\n";
      exit(1);
    }

    StructInfo.TemplateInfo = TemplInfo;
  }

  std::vector<const CXXRecordDecl *> BaseClassDeclarations;
    for (const auto &Base : Record.bases()) {
      if (Base.getAccessSpecifier() != AS_public) {
        // TODO remove if information of private or protected members will be
        // required someday
        continue;
      }

      if (!Base.getType()->isStructureOrClassType()) {
        llvm::errs() <<
            "Inheritance is currently supported only for class or struct as base"
            << "\n";
        exit(1);
      }

      //    TODO check if recursive handling is required and implement
      //    deduplication if necessary
      BaseClassDeclarations.emplace_back(Base.getType()->getAsCXXRecordDecl());
    }

  const auto ProcessMembers = [&](const CXXRecordDecl::field_range &Members,
                                  const bool IsStatic) {
    for (const auto &Member : Members) {
      std::optional<ClassMemberInitializer> FieldInitializer;

      if (Member->hasInClassInitializer()) {
        const auto *InitExpression = Member->getInClassInitializer();

        if (!InitExpression) {
          auto DIE = GCompilerInstance->getSema()
              .BuildCXXDefaultInitExpr(Member->getLocation(), Member);

          if (DIE.isInvalid()) {
            llvm::errs() << "Failed to build member default init expression" << "\n";
            exit(1);
          }

          InitExpression = Member->getInClassInitializer();
        }
        if (!InitExpression) {
          llvm::errs() << "Failed to get member init expression" << "\n";
          exit(1);
        }

        // remove redundant "=" symbol
        const auto FixedInitializerRange = normalizeInitializerRange(InitExpression->getSourceRange(), *Result.SourceManager, Result.Context->getLangOpts());
        const auto InitExpressionStr =
            Lexer::getSourceText(CharSourceRange::getTokenRange(FixedInitializerRange),
                                 *Result.SourceManager,
                                 Result.Context->getLangOpts()).trim()
                .str();

        FieldInitializer.emplace(ClassMemberInitializer{
            .Style = Member->getInClassInitStyle(),
            .Expression = InitExpressionStr,
        });
      }

      StructInfo.Members.push_back(MemberMetainfo{
          .Name = Member->getIdentifier()->getName().str(),
          .Type = getTypeInfo(Member->getType(), Member->getASTContext()),
          .AccessSpecifier = Member->getAccess(),
          .Properties = getProperties(*Member),
          .IsStatic = IsStatic,
          .Initializer = std::move(FieldInitializer)});
    }
  };


  const auto ProcessMethods = [&](const CXXRecordDecl::method_range &Methods, const bool Inherited) {
    for (const auto &Method : Methods) {
      if (Method->getIdentifier() == nullptr) {
        // constructor or assigment operator
        continue;
      }

      // TODO handle static/non-static info
      auto MethodName = Method->getIdentifier()->getName().str();

      auto &MethodInfo = StructInfo.Methods.emplace_back(MethodMetainfo{
          .Name = std::move(MethodName),
          .ReturnType =
              getTypeInfo(Method->getReturnType(), Method->getASTContext()),
          .AccessSpecifier = Method->getAccess(),
          .Properties = getProperties(*Method),
          .IsStatic = Method->isStatic(),
          .IsConst = Method->isConst(),
          .IsInherited = Inherited
      });

      for (const auto &Parameter : Method->parameters()) {
        MethodInfo.Arguments.emplace_back(ArgumentMetainfo{
            .Name = Parameter->getName().str(),
            .Type = getTypeInfo(Parameter->getType(), Method->getASTContext()),
            .Properties = getProperties(*Parameter)});
      }
    }
  };

  for (const auto *BaseClass : BaseClassDeclarations) {
    ProcessMembers(BaseClass->fields(), false);
    ProcessMethods(BaseClass->methods(), true);
  }

  ProcessMembers(Record.fields(), false);
  ProcessMethods(Record.methods(), false);
}

} // namespace reflex