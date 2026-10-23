#pragma once

#include <uuid.h>
#include <string>
#include <string_view>

namespace tessera {

// Every entity in the model is identified by a uuid. References between
// entities are uuids, never pointers.
using Uid = uuids::uuid;

Uid newUid();
// Deterministic uid derived from a name. Used for built-in rows so that they
// keep the same uid across sessions and saved projects.
Uid nameUid(std::string_view name);

std::string toString(const Uid& uid);
Uid uidFromString(std::string_view text);   // nil on failure
std::string shortUid(const Uid& uid);       // first 8 hex digits, for display

} // namespace tessera
