#pragma once

// Operations on the model. The UI edits the model through these (create,
// link, delete…), then commits a snapshot.

#include "model/Model.h"

#include <string>

namespace tessera::ops {

Document newProject(const std::string& name);

// Brings an owner's parameters in line with its declarations: adds the
// missing ones, retypes changed ones, removes the extra ones. Existing values
// are kept.
void syncParams(Document& doc, const Uid& ownerUid);

// ── Graph ──
Uid addGraph(Document& doc, const std::string& name);   // with an Output node
Uid addNode(Document& doc, const Uid& graphUid, const Uid& nodeTypeUid);
// The NodeType for a NodeType, Shader, Media or Converter uid (null otherwise).
Uid nodeTypeForAsset(const Model& model, const Uid& uid);
// Accepts the two parameters in either order (output → input).
bool canLink(const Document& doc, Uid fromParam, Uid toParam, std::string* why = nullptr);
Uid link(Document& doc, Uid fromParam, Uid toParam, std::string* why = nullptr);

// Deletes an entity and everything that depends on it.
bool deleteEntity(Document& doc, const Uid& uid, std::string* why = nullptr);

// ── Assets ──
std::string folderDir(const Model& model, const Uid& folderUid);   // "" for the root
std::string uniquePath(const Document& doc, const std::string& path);
Uid newFolder(Document& doc, const Uid& parentUid, const std::string& label);
Uid addShaderFile(Document& doc, const Uid& folderUid, const std::string& fileName, const std::string& source);
Uid newShader(Document& doc, const Uid& folderUid);
Uid copyShaderIntoProject(Document& doc, const Uid& shaderUid, const Uid& folderUid);
// Writes a user shader's source and applies its header: updates the label and
// the NodeType, and syncs every Node, Effect and effect Clip that uses it.
bool setShaderSource(Document& doc, const Uid& shaderUid, const std::string& source, std::string* error = nullptr);
// `data` holds the copied file, or is null for an external reference.
Uid addMedia(Document& doc, const Uid& folderUid, const std::string& path, Kind kind, int width, int height,
             float duration, FileData data);

// ── Timeline ──
Uid addTrack(Document& doc);
Uid addClip(Document& doc, const Uid& trackUid, const Uid& sourceUid, float start, float duration);
Uid addEffect(Document& doc, const Uid& clipUid, const Uid& shaderUid);
void moveEffect(Document& doc, const Uid& effectUid, int delta);
Uid addMarker(Document& doc, float position, Kind kind, const std::string& label = {});   // Marker, CuePoint or HoldPoint
Uid addLoop(Document& doc, float start, float end);

} // namespace tessera::ops
