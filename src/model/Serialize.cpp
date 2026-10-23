#include "model/Serialize.h"

#include <type_traits>

namespace tessera {

json fieldToJson(const Uid& v) { return v.is_nil() ? json(nullptr) : json(toString(v)); }
json fieldToJson(const std::string& v) { return v; }
json fieldToJson(float v) { return v; }
json fieldToJson(int v) { return v; }
json fieldToJson(bool v) { return v; }
json fieldToJson(const Vec2& v) { return json::array({v.x, v.y}); }
json fieldToJson(BlendMode v) { return toString(v); }

json fieldToJson(const Value& v) {
    return std::visit([](const auto& x) -> json {
        using X = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<X, std::monostate>) return nullptr;
        else if constexpr (std::is_same_v<X, Color>) return toHex(x);
        else if constexpr (std::is_same_v<X, ColorList>) return moduleFor(ParamType::ColorList).save(x);
        else return x;
    }, v);
}

namespace {

void fieldFromJson(const json& j, Uid& v) { v = j.is_string() ? uidFromString(j.get<std::string>()) : Uid{}; }
void fieldFromJson(const json& j, std::string& v) { if (j.is_string()) v = j.get<std::string>(); }
void fieldFromJson(const json& j, float& v) { if (j.is_number()) v = j.get<float>(); }
void fieldFromJson(const json& j, int& v) { if (j.is_number()) v = j.get<int>(); }
void fieldFromJson(const json& j, bool& v) { if (j.is_boolean()) v = j.get<bool>(); }
void fieldFromJson(const json& j, Vec2& v) {
    if (j.is_array() && j.size() == 2) v = {j[0].get<float>(), j[1].get<float>()};
}
void fieldFromJson(const json& j, BlendMode& v) { if (j.is_string()) v = blendModeFromString(j.get<std::string>()); }
// A parameter value needs its type; Parameter rows are fixed up after loading.
void fieldFromJson(const json&, Value&) {}

template <class T>
T recordFromJson(const json& j) {
    T record;
    T::fields(record, [&](const char* name, auto& field) {
        if (j.contains(name)) fieldFromJson(j[name], field);
    });
    if constexpr (std::is_same_v<T, Parameter>) {
        if (j.contains("value")) record.value = moduleFor(record.paramType()).load(j["value"]);
    }
    return record;
}

} // namespace

json modelToJson(const Model& model) {
    json j = json::object();
    Model::forEachTable(model, [&](Kind, const char* name, const auto& table) {
        json rows = json::array();
        for (const auto& r : table) rows.push_back(recordToJson(r));
        j[name] = std::move(rows);
    });
    return j;
}

bool modelFromJson(const json& j, Model& model, std::string& error) {
    if (!j.is_object() || !j.contains("Project")) {
        error = "Not a Tessera model: missing the Project table";
        return false;
    }
    Model out;
    Model::forEachTable(out, [&](Kind, const char* name, auto& table) {
        using Row = typename std::decay_t<decltype(table.rows)>::value_type;
        if (!j.contains(name) || !j[name].is_array()) return;
        for (const json& row : j[name]) {
            Row r = recordFromJson<Row>(row);
            if (!r.uid.is_nil() && !table.contains(r.uid)) table.add(std::move(r));
        }
    });
    model = std::move(out);
    return true;
}

std::vector<Reference> referencesFrom(const Model& model, const Uid& uid) {
    std::vector<Reference> out;
    Model::forEachTable(model, [&](Kind kind, const char*, const auto& table) {
        if (const auto* r = table.find(uid)) {
            std::decay_t<decltype(*r)>::fields(*r, [&](const char* name, const auto& field) {
                if constexpr (std::is_same_v<std::decay_t<decltype(field)>, Uid>) {
                    if (std::string(name) != "uid" && !field.is_nil()) out.push_back({kind, uid, name, field});
                }
            });
        }
    });
    return out;
}

std::vector<Reference> referencesTo(const Model& model, const Uid& uid) {
    std::vector<Reference> out;
    Model::forEachTable(model, [&](Kind kind, const char*, const auto& table) {
        for (const auto& r : table) {
            std::decay_t<decltype(r)>::fields(r, [&](const char* name, const auto& field) {
                if constexpr (std::is_same_v<std::decay_t<decltype(field)>, Uid>) {
                    if (std::string(name) != "uid" && field == uid) out.push_back({kind, r.uid, name, uid});
                }
            });
        }
    });
    return out;
}

} // namespace tessera
