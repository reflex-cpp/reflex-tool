// Part of reflex-tool, under the Apache License 2.0.
// See https://www.apache.org/licenses/LICENSE-2.0.txt for license information.
// SPDX-License-Identifier: Apache-2.0

#ifndef REFLEX_TOOL_JSON_WRITER_HPP
#define REFLEX_TOOL_JSON_WRITER_HPP

#include <llvm/Support/JSON.h>

#include "common.hpp"

namespace reflex {

void serialize(const Metainformation &Metainfo, llvm::json::OStream &JOS);

} // namespace reflex

#endif // REFLEX_TOOL_JSON_WRITER_HPP
