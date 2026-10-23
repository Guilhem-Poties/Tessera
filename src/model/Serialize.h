#pragma once

#include "model/Model.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace tessera {

using json = nlohmann::ordered_json;   // keeps record fields in declaration order

// Field-level conversion, shared by the serialiser and the data model view.
json fieldToJson(const Uid& v);
json fieldToJson(const std::string& v);
json fieldToJson(float v);
json fieldToJson(int v);
json fieldToJson(bool v);
json fieldToJson(const Vec2& v);
json fieldToJson(BlendMode v);
json fieldToJson(const Value& v);

template <class T>
json recordToJson(const T& record) {
    json j = json::object();
    T::fields(record, [&](const char* name, const auto& field) { j[name] = fieldToJson(field); });
    return j;
}

json modelToJson(const Model& model);
// Returns false and fills `error` when the JSON is not a Tessera model.
bool modelFromJson(const json& j, Model& model, std::string& error);

// A uid-valued field of a record that points at another record.
struct Reference {
    Kind fromKind;
    Uid fromUid;
    std::string field;
    Uid target;
};

std::vector<Reference> referencesFrom(const Model& model, const Uid& uid);   // what it points to
std::vector<Reference> referencesTo(const Model& model, const Uid& uid);     // what references it

} // namespace tessera
