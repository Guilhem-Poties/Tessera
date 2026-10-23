#include "ui/FileDialog.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace tessera {

void FileDialog::open(const std::string& title, Mode mode, std::vector<std::string> extensions, Callback onOk,
                      const std::string& suggestedName) {
    title_ = title;
    mode_ = mode;
    extensions_ = std::move(extensions);
    onOk_ = std::move(onOk);
    name_ = suggestedName;
    error_.clear();
    if (dir_.empty()) navigate(fs::current_path().string());
    request_ = true;
}

void FileDialog::navigate(const std::string& dir) {
    std::error_code ec;
    fs::path p = fs::weakly_canonical(fs::path(dir), ec);
    if (ec || !fs::is_directory(p, ec)) {
        error_ = "Not a folder: " + dir;
        dirInput_ = dir_;
        return;
    }
    dir_ = p.string();
    dirInput_ = dir_;
    error_.clear();
}

void FileDialog::draw() {
    if (request_) {
        ImGui::OpenPopup(title_.c_str());
        request_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(640, 460), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(title_.c_str(), nullptr)) return;

    if (ImGui::Button("Up")) navigate(fs::path(dir_).parent_path().string());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##dir", &dirInput_, ImGuiInputTextFlags_EnterReturnsTrue)) navigate(dirInput_);

    struct Entry {
        std::string name;
        bool dir;
    };
    std::vector<Entry> entries;
    std::error_code ec;
    for (fs::directory_iterator it(dir_, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
         it.increment(ec)) {
        std::error_code e2;
        bool isDir = it->is_directory(e2);
        std::string name = it->path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        if (!isDir) {
            if (mode_ == Mode::ChooseFolder) continue;
            if (!extensions_.empty()) {
                std::string ext = it->path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                if (std::find(extensions_.begin(), extensions_.end(), ext) == extensions_.end()) continue;
            }
        }
        entries.push_back({name, isDir});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.dir != b.dir ? a.dir : a.name < b.name;
    });

    bool accept = false;
    float footer = ImGui::GetFrameHeightWithSpacing() * 2 + 8;
    if (ImGui::BeginChild("entries", ImVec2(0, -footer), ImGuiChildFlags_Borders)) {
        for (const Entry& e : entries) {
            std::string label = (e.dir ? "[dir]  " : "       ") + e.name;
            bool selected = !e.dir && e.name == name_;
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (e.dir) {
                    if (ImGui::IsMouseDoubleClicked(0) || mode_ == Mode::OpenFile) navigate((fs::path(dir_) / e.name).string());
                    else name_ = e.name;
                } else {
                    name_ = e.name;
                    if (ImGui::IsMouseDoubleClicked(0)) accept = true;
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth(-200);
    ImGui::InputText(mode_ == Mode::ChooseFolder ? "Folder name" : "File name", &name_);
    if (!error_.empty()) ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", error_.c_str());
    if (ImGui::Button("OK", ImVec2(100, 0))) accept = true;
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();

    if (accept) {
        fs::path result = name_.empty() ? fs::path(dir_) : fs::path(dir_) / name_;
        if (mode_ == Mode::OpenFile && (name_.empty() || fs::is_directory(result, ec))) {
            error_ = "Choose a file";
        } else {
            ImGui::CloseCurrentPopup();
            if (onOk_) onOk_(result.string());
        }
    }
    ImGui::EndPopup();
}

} // namespace tessera
