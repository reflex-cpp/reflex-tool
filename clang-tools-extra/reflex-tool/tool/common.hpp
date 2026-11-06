// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#ifndef REFLEX_TOOL_COMMON_HPP
#define REFLEX_TOOL_COMMON_HPP

#include <any>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Basic/Specifiers.h>

namespace reflex {

struct ClassMemberInitializer {
  clang::InClassInitStyle Style;
  std::string Expression;
};

enum class TypeKind {
  TemplateSpecialization,
  Other
};

struct TypeInfo;
struct TemplateArgumentInfo;
struct TemplateSpecializationMetainfo;


struct TypeInfo {
  std::string Name;
  std::string FullName;

  std::any TemplateInfo{}; // TODO rename to template specialization info?
  std::any AliasInfo{};

  bool IsConstQualified;
  bool IsVolatile;
};

// TODO rename to template specialization argument info
struct TemplateArgumentInfo {
  // TODO: handle other argument kinds
   clang::TemplateArgument::ArgKind Kind;

  std::optional<std::int64_t> IntegralValue{};
  std::optional<TypeInfo> Type{};
};

struct TemplateSpecializationMetainfo {
  std::string TemplateName;
  std::vector<TemplateArgumentInfo> TemplateArguments{};
};

struct TypeAliasMetainfo {
  TypeInfo Type;
};

struct PropertyValue {
  std::string Value;
};

struct Property {
  std::string Expression;
  TypeInfo Type;
  std::vector<PropertyValue> Values{};
};

struct MemberMetainfo {
  std::string Name;
  TypeInfo Type;
  clang::AccessSpecifier AccessSpecifier;
  std::vector<Property> Properties;
  bool IsStatic;
  std::optional<ClassMemberInitializer> Initializer;
};

struct ArgumentMetainfo {
  std::string Name;
  TypeInfo Type;
  std::optional<std::string> DefaultValue{};
  std::vector<Property> Properties;
};

struct MethodMetainfo {
  std::string Name;
  TypeInfo ReturnType;
  std::vector<ArgumentMetainfo> Arguments{};

  clang::AccessSpecifier AccessSpecifier;
  std::vector<Property> Properties{};
  bool IsStatic;
  bool IsConst;
  bool IsInherited;
};

struct TemplateArgInfo{
    std::string Name;
    clang::TemplateArgument::ArgKind Kind;
};

struct TemplateInfo {
    std::vector<TemplateArgInfo> Arguments;
};

struct StructureMetainfo {
  std::string Name;
  std::string Namespace;
  std::string FullName;
  std::vector<MemberMetainfo> Members{};
  std::vector<MethodMetainfo> Methods{};
  std::vector<Property> Properties{};

  bool IsTemplateInstantiation = false;

  std::optional<TemplateInfo> TemplateInfo{};
  std::optional<TemplateSpecializationMetainfo> TemplateSpecializationInfo{};
};

struct EnumeratorMetainfo {
  std::string Name;
  std::int64_t Value;

  std::vector<Property> Properties;
};

struct EnumerationMetainfo {
  std::string Name;
  std::string Namespace;
  std::string FullName;
  std::vector<EnumeratorMetainfo> Items{};
  std::vector<Property> Properties{};
};


struct FunctionMetainfo {
  std::string Name;
  std::string Namespace;
  std::string FullName;

  TypeInfo ReturnType;
  std::vector<ArgumentMetainfo> Arguments{};

  std::vector<Property> Properties{};
};

struct Metainformation {
  std::vector<StructureMetainfo> Structures;
  std::vector<EnumerationMetainfo> Enumerations;
  std::vector<FunctionMetainfo> Functions;
};

[[nodiscard]] std::string printFullyQualified(const clang::Expr &Expr,
                                              const clang::ASTContext &Context);
[[nodiscard]] std::string printExpression(const clang::Expr &Expr,
                                          const clang::PrintingPolicy Policy);

[[nodiscard]] std::string getNamespaceString(const clang::DeclContext &Context);

[[nodiscard]] std::vector<Property> getProperties(const clang::Decl &Item);

TypeInfo getTypeInfo(const clang::QualType &Type, const clang::ASTContext &AstContex);


TemplateArgumentInfo getTemplateArgInfo(const clang::TemplateArgument &TemplateArg,
                                    const clang::ASTContext &AstContext);
TemplateSpecializationMetainfo getTemplateTypeInfo(const clang::TemplateSpecializationType &SpecType,
                                                   const clang::ASTContext &AstContext);

clang::SourceRange normalizeInitializerRange(
    clang::SourceRange Range, const clang::SourceManager &SM,
    const clang::LangOptions &LangOpts);

} // namespace reflex

#endif // REFLEX_TOOL_COMMON_HPP
