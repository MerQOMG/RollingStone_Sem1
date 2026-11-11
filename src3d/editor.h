#pragma once
#include "config.h"
#include <array>
namespace rs3d {
struct EditorField {std::string key,label;int group=0;std::array<char,128>text{};};
class SceneEditor {
    SceneDraft base_;
public:
    std::vector<EditorField>fields;
    explicit SceneEditor(const SceneDraft&draft);
    void sync(const SceneDraft&draft);
    void set(const std::string&key,const std::string&value);
    SceneDraft parse()const;
    std::string glyphs()const;
};
}
