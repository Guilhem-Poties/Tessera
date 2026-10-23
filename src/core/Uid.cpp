#include "core/Uid.h"

#include <random>

namespace tessera {

Uid newUid() {
    static std::mt19937 engine = [] {
        std::random_device rd;
        std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
        return std::mt19937(seq);
    }();
    static uuids::uuid_random_generator gen(engine);
    return gen();
}

Uid nameUid(std::string_view name) {
    static const Uid ns = uuids::uuid::from_string("6f1c2b9e-5d0a-4c1e-9a43-7e2a5b8d0c11").value();
    uuids::uuid_name_generator gen(ns);
    return gen(name);
}

std::string toString(const Uid& uid) { return uuids::to_string(uid); }

Uid uidFromString(std::string_view text) {
    auto parsed = uuids::uuid::from_string(text);
    return parsed ? *parsed : Uid{};
}

std::string shortUid(const Uid& uid) {
    if (uid.is_nil()) return "null";
    return toString(uid).substr(0, 8);
}

} // namespace tessera
