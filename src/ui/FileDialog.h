#pragma once

// A minimal ImGui file browser (std::filesystem), so the prototype needs no
// native dialog library.

#include <functional>
#include <string>
#include <vector>

namespace tessera {

class FileDialog {
public:
    enum class Mode { OpenFile, ChooseFolder };
    using Callback = std::function<void(const std::string& path)>;

    // `extensions` filters files in OpenFile mode (lower case, with the dot);
    // empty shows every file.
    void open(const std::string& title, Mode mode, std::vector<std::string> extensions, Callback onOk,
              const std::string& suggestedName = {});
    void draw();

private:
    void navigate(const std::string& dir);

    bool request_ = false;
    std::string title_;
    Mode mode_ = Mode::OpenFile;
    std::vector<std::string> extensions_;
    Callback onOk_;
    std::string dir_;
    std::string dirInput_;
    std::string name_;
    std::string error_;
};

} // namespace tessera
