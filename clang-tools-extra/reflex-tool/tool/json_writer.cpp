// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#include "json_writer.hpp"

#include <vector>

namespace reflex {

auto accessToString(const clang::AccessSpecifier Access) {
  if (Access == clang::AS_public) {
    return "public";
  }

  if (Access == clang::AS_private) {
    return "private";
  }

  if (Access == clang::AS_protected) {
    return "protected";
  }

  return "none";
}

auto initializerStyleToString(const clang::InClassInitStyle Style) {
  switch (Style) {
  case clang::ICIS_NoInit: {
    return "none";
  }
  case clang::ICIS_CopyInit: {
    return "copy";
  }
  case clang::ICIS_ListInit: {
    return "list";
  }
  }
}

constexpr auto typeKindToString(const TypeKind Kind) {
  switch (Kind) {

  case TypeKind::TemplateSpecialization: {
    return "template_specialization";
  }

  case TypeKind::Other: {
    return "other";
  }
  }
}

constexpr auto
argumentKindToString(const clang::TemplateArgument::ArgKind Kind) {

  switch (Kind) {
  case clang::TemplateArgument::Type: {
    return "type";
  }
  case clang::TemplateArgument::Integral: {
    return "integral";
  }
  case clang::TemplateArgument::Expression: {
    return "expression";
  }
  default: {
    assert(false);
    return "unhandled argument kind";
  }
  }
}

void serializeTypeInfo(llvm::json::OStream &JOS, const TypeInfo &Info);

void serializeTemplateSpecializationInfo(llvm::json::OStream &JOS, const TemplateSpecializationMetainfo& TemplateInfo, const std::string& FieldName) {
  JOS.attributeObject(FieldName, [&] {
  JOS.attribute("template_name", TemplateInfo.TemplateName);
  JOS.attributeArray("arguments", [&] {
    for (const auto &Argument : TemplateInfo.TemplateArguments) {
      JOS.object([&] {
        JOS.attribute("kind", argumentKindToString(Argument.Kind));

        if (Argument.Kind == clang::TemplateArgument::Type) {
          assert(Argument.Type.has_value());
          JOS.attributeObject("type", [&] {
            serializeTypeInfo(JOS, Argument.Type.value());
          });
        } else if (Argument.Kind == clang::TemplateArgument::Integral) {
          assert(Argument.IntegralValue.has_value());
          JOS.attributeObject("integral", [&] {
            JOS.attribute("value", Argument.IntegralValue.value());
          });

          assert(Argument.Type.has_value());
          JOS.attributeObject("type", [&]
          {
            serializeTypeInfo(JOS, Argument.Type.value());
          });
        } else if (Argument.Kind == clang::TemplateArgument::Expression) {
          // write nothng for now
        } else {
          llvm::errs() << "JSON writer: unhandled TemplateArgument kind";
          assert(false);
          std::exit(1);
        }
      });
    }
  });
  });
}

void serializeTypeInfo(llvm::json::OStream &JOS, const TypeInfo &Info) {
  JOS.attribute("name", Info.Name);
  JOS.attribute("full_name", Info.FullName);
  JOS.attribute("is_const", Info.IsConstQualified);
  JOS.attribute("is_volatile", Info.IsVolatile);

  if (Info.AliasInfo.has_value()) {
    const auto AliasInfo = any_cast<TypeAliasMetainfo>(Info.AliasInfo);
    JOS.attributeObject("alias_info", [&] {
      JOS.attributeObject("referenced_type",
                          [&] { serializeTypeInfo(JOS, AliasInfo.Type); });
    });
  }

  if (Info.TemplateInfo.has_value()) {
    const auto TemplateInfo =
        any_cast<TemplateSpecializationMetainfo>(Info.TemplateInfo);
    serializeTemplateSpecializationInfo(JOS, TemplateInfo, "template_info");
  }
}

auto serializeProperties(const std::vector<Property> &Properties,
                         llvm::json::OStream &JOS){
  JOS.attributeArray("properties", [&] {
    for (const auto &Property : Properties) {
      JOS.object([&]{
        JOS.attribute("expression", Property.Expression);

        JOS.attributeObject("type",
                            [&] { serializeTypeInfo(JOS, Property.Type); });

        JOS.attributeArray("values", [&]{
          for (const auto& Value: Property.Values) {
            JOS.object([&] { JOS.attribute("value", Value.Value); });
          }
        });

      });
    }
  });
}

auto serializeStructures(const std::vector<StructureMetainfo> &Structures,
                         llvm::json::OStream &JOS) {
  JOS.attributeArray("structures", [&] {
    for (const auto &Structure : Structures) {
      JOS.object([&] {
        JOS.attribute("name", Structure.Name);
        JOS.attribute("namespace", Structure.Namespace);
        JOS.attribute("full_name", Structure.FullName);
        JOS.attribute("is_template_instantiation",
                      Structure.IsTemplateInstantiation);
        serializeProperties(Structure.Properties, JOS);

        if (Structure.TemplateSpecializationInfo.has_value()) {
          const auto TemplateInfo =Structure.TemplateSpecializationInfo.value();
          serializeTemplateSpecializationInfo(JOS, TemplateInfo, "template_specialization_info");
        } else if (Structure.TemplateInfo.has_value()) {
          JOS.attributeObject("template_info", [&] {
            JOS.attributeArray("arguments", [&] {
              for (const auto &Argument : Structure.TemplateInfo->Arguments) {
                JOS.object([&] {
                  JOS.attribute("name", Argument.Name);
                  JOS.attribute("kind", argumentKindToString(Argument.Kind));
                });
              }
            });
          });
        }

        JOS.attributeArray("data_members", [&] {
          for (const auto &Member : Structure.Members) {
            JOS.object([&] {
              JOS.attribute("name", Member.Name);
              JOS.attribute("access", accessToString(Member.AccessSpecifier));

              JOS.attributeObject("type",
                                  [&] { serializeTypeInfo(JOS, Member.Type); });

              if (Member.Initializer.has_value()) {
                const auto &Initializer = Member.Initializer.value();
                JOS.attributeObject("initializer", [&] {
                  JOS.attribute("style",
                                initializerStyleToString(Initializer.Style));
                  JOS.attribute("expression", Initializer.Expression);
                });
              }

              serializeProperties(Member.Properties, JOS);
            });
          }
        });
        JOS.attributeArray("methods", [&] {
          for (const auto &Method : Structure.Methods) {
            JOS.object([&] {
              JOS.attribute("name", Method.Name);
              JOS.attribute("access", accessToString(Method.AccessSpecifier));
              JOS.attribute("is_static", Method.IsStatic);
              JOS.attribute("is_const", Method.IsConst);
              JOS.attribute("is_inherited", Method.IsInherited);

              JOS.attributeObject("return_type", [&] {
                serializeTypeInfo(JOS, Method.ReturnType);
              });
              JOS.attributeArray("properties", [&] {
                for (const auto &Property : Method.Properties) {
                  // TODO
                  JOS.value(Property.Expression);
                }
              });

              JOS.attributeArray("arguments", [&] {
                for (const auto &Argument : Method.Arguments) {
                  JOS.object([&] {
                    JOS.attribute("name", Argument.Name);
                    JOS.attributeObject(
                        "type", [&] { serializeTypeInfo(JOS, Argument.Type); });
                    if (Argument.DefaultValue.has_value()) {
                      JOS.attribute("default_value",
                                    Argument.DefaultValue.value());
                    }

                    serializeProperties(Argument.Properties, JOS);
                  });
                }
              });
            });
          }
        });
      });
    }
  });
}



auto serializeEnumerations(const std::vector<EnumerationMetainfo> &Enumerations,
                           llvm::json::OStream &JOS) {
  JOS.attributeArray("enumerations", [&] {
    for (const auto &Enumeration : Enumerations) {
      JOS.object([&] {
        JOS.attribute("name", Enumeration.Name);
        JOS.attribute("namespace", Enumeration.Namespace);
        JOS.attribute("full_name", Enumeration.FullName);
        serializeProperties(Enumeration.Properties, JOS);

        JOS.attributeArray("enumerators", [&] {
          for (const auto &Enumerator : Enumeration.Items) {
            JOS.object([&] {
              JOS.attribute("name", Enumerator.Name);
              JOS.attribute("value", Enumerator.Value);

              serializeProperties(Enumerator.Properties, JOS);
            });
          }
        });
      });
    }
  });
}

auto serializeFunctions(const std::vector<FunctionMetainfo> &Functions,
                        llvm::json::OStream &JOS) {
  JOS.attributeArray("functions", [&] {
    for (const auto &Function : Functions) {
      JOS.object([&] {
        JOS.attribute("name", Function.Name);
        JOS.attribute("namespace", Function.Namespace);
        JOS.attribute("full_name", Function.FullName);
        JOS.attributeObject("return_type", [&] {
          serializeTypeInfo(JOS, Function.ReturnType);
        });
        serializeProperties(Function.Properties, JOS);

        JOS.attributeArray("arguments", [&] {
          for (const auto &Argument : Function.Arguments) {
            JOS.object([&] {
              JOS.attribute("name", Argument.Name);
              JOS.attributeObject(
                  "type", [&] { serializeTypeInfo(JOS, Argument.Type); });
              if (Argument.DefaultValue.has_value()) {
                JOS.attribute("default_value", Argument.DefaultValue.value());
              }

              serializeProperties(Argument.Properties, JOS);
            });
          }
        });
      });
    }
  });
}

void serialize(const Metainformation &Metainfo, llvm::json::OStream &JOS) {
  JOS.object([&] {
    serializeEnumerations(Metainfo.Enumerations, JOS);
    serializeStructures(Metainfo.Structures, JOS);
    serializeFunctions(Metainfo.Functions, JOS);
  });
}

} // namespace reflex