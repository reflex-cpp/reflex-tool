// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#include "common.hpp"

#include <iostream>
#include <numeric>

#include <clang/AST/QualTypeNames.h>
#include <clang/Lex/Lexer.h>

namespace reflex {

namespace {

[[nodiscard]] auto getAllNamedNamespaces(const clang::DeclContext *Context) {
  llvm::SmallVector<const clang::NamespaceDecl *, 6> Namespaces;
  auto GetNextNamedNamespace = [](const clang::DeclContext *Context) {
    // Look past non-namespaces and anonymous namespaces on FromContext.
    while (Context &&
           (!isa<clang::NamespaceDecl>(Context) ||
            cast<clang::NamespaceDecl>(Context)->isAnonymousNamespace())) {
      Context = Context->getParent();
    }
    return Context;
  };

  for (Context = GetNextNamedNamespace(Context); Context != nullptr;
       Context = GetNextNamedNamespace(Context->getParent())) {
    Namespaces.push_back(cast<clang::NamespaceDecl>(Context));
  }

  return Namespaces;
}

} // namespace

std::string printExpression(const clang::Expr &Expr,
                            const clang::PrintingPolicy Policy) {
  auto String = std::string{};
  auto Stream = llvm::raw_string_ostream{String};
  Expr.printPretty(Stream, nullptr, Policy);
  Stream.flush();
  return String;
}

std::string printFullyQualified(const clang::Expr &Expr,
                                const clang::ASTContext &Context) {
  auto Policy = clang::PrintingPolicy{Context.getLangOpts()};
  Policy.SuppressScope = false;
  Policy.ConstantArraySizeAsWritten = true;
  Policy.IncludeNewlines = false;
  Policy.ConstantsAsWritten = true;
  Policy.FullyQualifiedName = true;
  Policy.PrintCanonicalTypes = true;

  return printExpression(Expr, Policy);
}

std::string getNamespaceString(const clang::DeclContext &Context) {
  const auto NamedNamespaces = getAllNamedNamespaces(&Context);
  return std::accumulate(
      NamedNamespaces.rbegin(), NamedNamespaces.rend(), std::string{},
      [](std::string Result, const clang::NamespaceDecl *const NamedNamespace) {
        if (Result.empty()) {
          return NamedNamespace->getNameAsString();
        }

        return Result + "::" + NamedNamespace->getNameAsString();
      });
}

std::vector<Property> getProperties(const clang::Decl &Item) {
  std::vector<Property> Properties;

  for (const auto *const PropAttr :
       Item.specific_attrs<clang::ReflexPropAttr>()) {
    for (auto *const PropExpr : PropAttr->properties()) {
//      const auto *ClassType = PropExpr->getBestDynamicClassType();
//      if (!ClassType) {
//        throw std::runtime_error("Failed to ger property: not a class type");
//      }
//      auto Name = ClassType->getQualifiedNameAsString();

      auto PropExprStr =
          clang::Lexer::getSourceText(
              clang::CharSourceRange::getTokenRange(PropExpr->getSourceRange()),
              Item.getASTContext().getSourceManager(),
              Item.getASTContext().getLangOpts())
              .str();

      auto NewProperty = Property{
          .Expression = std::move(PropExprStr),
          .Type = getTypeInfo(PropExpr->getType().getCanonicalType(), Item.getASTContext())};

      clang::Expr::EvalResult ER;
      PropExpr->EvaluateAsConstantExpr(ER, Item.getASTContext());

      if (ER.Val.isStruct()) {
        const auto FieldCount = ER.Val.getStructNumFields();
        for (std::size_t FieldID = 0; FieldID < FieldCount; ++FieldID) {
          const auto Field = ER.Val.getStructField(FieldID);

          auto FieldValue = ([&]() -> std::string {
            switch (Field.getKind()) {
            case clang::APValue::Int: {
              return std::to_string(Field.getInt().getExtValue());
            }
            case clang::APValue::Float: {
              return std::to_string(Field.getFloat().convertToDouble());
            }
            case clang::APValue::LValue: {
              const auto LVB = Field.getLValueBase();
              if (LVB) {
                const auto *const LVE = LVB.dyn_cast<const clang::Expr *>();
                if (LVE) {
                  if (isa_and_nonnull<const clang::StringLiteral>(LVE)) {
                    const auto *SL = dyn_cast<const clang::StringLiteral>(LVE);
                    return SL->getString().str();
                  }
                }
              }
              return "<unsupported>";
            }
            case clang::APValue::Struct: {
              return "<struct>";
            }
            default: {
  //            std::cerr << "Unknown field kind" << std::endl;
  //            Field.dump();
  //            std::cerr << "Parent:" << std::endl;
  //            PropExpr->dump();

              return "<unserializable>";
            }
            }
          })();

          NewProperty.Values.push_back({.Value = std::move(FieldValue)});
        }
      }
      Properties.push_back(std::move(NewProperty));
    }
  }

  return Properties;
}

TypeInfo getTypeInfo(const clang::QualType &Type,
                     const clang::ASTContext &AstContex) {
  clang::PrintingPolicy FullNamePrintPolicy(AstContex.getLangOpts());
  FullNamePrintPolicy.PrintCanonicalTypes = true;
  FullNamePrintPolicy.SuppressScope = false;
  FullNamePrintPolicy.SuppressUnwrittenScope = false;
  FullNamePrintPolicy.SuppressInlineNamespace = false;
  FullNamePrintPolicy.FullyQualifiedName = true;

  if (const auto* const TT = Type->getAs<clang::TypedefType>()) {
    return getTypeInfo(TT->getDecl()->getUnderlyingType().getCanonicalType(), AstContex);
  }

  const auto Name = Type.withoutLocalFastQualifiers().getAsString();
  const auto TypeFullName = clang::QualType::getAsString(Type->getLocallyUnqualifiedSingleStepDesugaredType().split(), FullNamePrintPolicy);

  auto Info = TypeInfo{.Name = Name,
                       .FullName = TypeFullName,
                       .IsConstQualified = Type.isConstQualified(),
                       .IsVolatile = Type.isVolatileQualified()};

  const auto *const TST = Type->getAs<clang::TemplateSpecializationType>();
  const auto * ContainedType = Type->getContainedDeducedType();
  const auto * const DTST = ContainedType ? ContainedType->getAs<clang::DeducedTemplateSpecializationType>() : nullptr;
  const auto* RecordDecl = Type->getAsCXXRecordDecl();
  const auto *const CTSD = RecordDecl ? dyn_cast<clang::ClassTemplateSpecializationDecl>(RecordDecl): nullptr;

  if (TST) {
    const auto *const SpecType = TST;

    Info.TemplateInfo = getTemplateTypeInfo(*SpecType, AstContex);

    if (SpecType->isTypeAlias()) {
      const auto AliasedType = SpecType->getAliasedType();
      Info.AliasInfo =
          TypeAliasMetainfo{.Type = getTypeInfo(AliasedType, AstContex)};
    }
  }
  else if (DTST){
    auto TemplateName = DTST->getTemplateName().getAsTemplateDecl()
                            ->getQualifiedNameAsString();
    Info.TemplateInfo = TemplateSpecializationMetainfo{.TemplateName = std::move(TemplateName)};
  }
  else if (CTSD) {
    const auto * const ST = CTSD->getSpecializedTemplate();
    if (!ST) {
      llvm::errs() << "Logic error: Unable to get template specialization";
      std::exit(1);
    }
    auto TemplateName = ST->getQualifiedNameAsString();
    auto &TemplateArgs = CTSD->getTemplateArgs();

    auto TemplateInfo = TemplateSpecializationMetainfo{
      .TemplateName = std::move(TemplateName)};

    for (const auto& Arg : TemplateArgs.asArray())
    {
      if (Arg.getKind() == clang::TemplateArgument::Pack)
      {
        for (const auto& PackArg : Arg.getPackAsArray())
        {
          TemplateInfo.TemplateArguments.push_back(getTemplateArgInfo(PackArg, AstContex));
        }
      }
      else
      {
        TemplateInfo.TemplateArguments.push_back(getTemplateArgInfo(Arg, AstContex));
      }
    }
    Info.TemplateInfo = TemplateInfo;
  }

  return Info;
}


TemplateArgumentInfo getTemplateArgInfo(
    const clang::TemplateArgument &TemplateArg,
    const clang::ASTContext &AstContext) {

  const auto Kind = TemplateArg.getKind();
  if (Kind == clang::TemplateArgument::ArgKind::Type) {
    const auto ArgType = TemplateArg.getAsType();
    // TODO support other param kinds

    return TemplateArgumentInfo{
        .Kind = Kind, .Type = getTypeInfo(ArgType, AstContext)};
  }
  if (Kind == clang::TemplateArgument::ArgKind::Integral) {
    const auto Value = TemplateArg.getAsIntegral();

    return TemplateArgumentInfo{
      .Kind = Kind,
      .IntegralValue = Value.getExtValue(),
      .Type = getTypeInfo(TemplateArg.getIntegralType(), AstContext)
    };
  }
  if (Kind == clang::TemplateArgument::ArgKind::Expression) {
    auto *const Expr = TemplateArg.getAsExpr();

    if (Expr->getDependence()) {
      return TemplateArgumentInfo{
        .Kind = clang::TemplateArgument::ArgKind::Expression};
    }

    const auto *const EnumDecl = Expr->getEnumConstantDecl();
    if (EnumDecl) {
      return TemplateArgumentInfo{
        .Kind = clang::TemplateArgument::ArgKind::Integral,
        .IntegralValue = EnumDecl->getInitVal().getExtValue(),
        .Type = getTypeInfo(Expr->getType(), AstContext)
      };
    }

    const auto ConstExpr = Expr->getIntegerConstantExpr(AstContext);
    if (ConstExpr.has_value()) {
      return TemplateArgumentInfo{
          .Kind = clang::TemplateArgument::ArgKind::Integral,
          .IntegralValue = ConstExpr->getExtValue(),
          .Type = getTypeInfo(Expr->getType(), AstContext)};
    }
    Expr->dumpColor();
    llvm::errs() << "Non-numeric or  non-constexpr expressions in template "
        "parameters not yet supported";
    std::exit(1);

  }
  //      llvm::errs() << "Unsupported template parameter kind";
  //      std::exit(1);
  return TemplateArgumentInfo{.Kind = Kind};

}


TemplateSpecializationMetainfo
getTemplateTypeInfo(const clang::TemplateSpecializationType &SpecType,
                    const clang::ASTContext &AstContext) {

  TemplateSpecializationMetainfo Info{};

  Info.TemplateName = SpecType.getTemplateName()
                          .getAsTemplateDecl()
                          ->getQualifiedNameAsString();
  for (auto &TemplateArg : SpecType.template_arguments()) {
    Info.TemplateArguments.push_back(getTemplateArgInfo(TemplateArg, AstContext));
  }

  return Info;
}

clang::SourceRange normalizeInitializerRange(const clang::SourceRange Range, const clang::SourceManager &SM,
                                             const clang::LangOptions &LangOpts) {
  clang::Token FirstToken;
  FirstToken.setKind(clang::tok::unknown);

  if (clang::Lexer::getRawToken(Range.getBegin(), FirstToken, SM, LangOpts)) {
    return Range; // Unable to get first token
  }
  if (FirstToken.is(clang::tok::equal)) {
    return {FirstToken.getEndLoc(), Range.getEnd()};;
  }
  return Range;
}

} // namespace reflex
