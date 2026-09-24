#include "ui_json.h"

#include "ui_assets.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace KnC::Tools {
namespace {

// A small JSON reader enough for the UI files
class Value {
public:
    enum Type { NULL_TYPE, BOOL, NUMBER, STRING, ARRAY, OBJECT };

    Value() = default;
    explicit Value(bool b) : type_(BOOL), bool_(b) {}
    explicit Value(double n) : type_(NUMBER), number_(n) {}
    explicit Value(const std::string& s) : type_(STRING), string_(s) {}

    Type GetType() const { return type_; }
    bool AsBool() const { return bool_; }
    int AsInt() const { return static_cast<int>(number_); }
    double AsDouble() const { return number_; }
    const std::string& AsString() const { return string_; }

    size_t ArraySize() const { return array_.size(); }
    const Value& operator[](size_t index) const {
        static const Value null_value;
        return index < array_.size() ? array_[index] : null_value;
    }

    bool HasKey(const std::string& key) const { return object_.find(key) != object_.end(); }
    const Value& operator[](const std::string& key) const {
        static const Value null_value;
        const auto it = object_.find(key);
        return it != object_.end() ? it->second : null_value;
    }

    void SetObject() {
        type_ = OBJECT;
        object_.clear();
    }
    void AddArrayElement(const Value& v) {
        if (type_ != ARRAY) { type_ = ARRAY; array_.clear(); }
        array_.push_back(v);
    }
    void SetObjectKey(const std::string& key, const Value& v) {
        if (type_ != OBJECT) { type_ = OBJECT; object_.clear(); }
        object_[key] = v;
    }

private:
    Type type_ = NULL_TYPE;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Value> array_;
    std::map<std::string, Value> object_;
};

class Parser {
public:
    static Value Parse(const std::string& json) {
        Parser p(json);
        return p.ParseValue();
    }

    static Value ParseFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return Value();
        std::stringstream buffer;
        buffer << file.rdbuf();
        return Parse(buffer.str());
    }

private:
    explicit Parser(const std::string& json) : json_(json) {}

    char Peek() {
        SkipWhitespace();
        return pos_ < json_.size() ? json_[pos_] : '\0';
    }

    char Get() {
        SkipWhitespace();
        return pos_ < json_.size() ? json_[pos_++] : '\0';
    }

    void SkipWhitespace() {
        while (pos_ < json_.size() && (json_[pos_] == ' ' || json_[pos_] == '\t' || json_[pos_] == '\n' || json_[pos_] == '\r'))
            pos_++;
    }

    Value ParseValue() {
        const char c = Peek();
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return ParseString();
        if (c == 't' || c == 'f') return ParseBool();
        if (c == 'n') return ParseNull();
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber();
        return Value();
    }

    Value ParseObject() {
        Value obj;
        obj.SetObject();
        Get();
        while (Peek() != '}' && Peek() != '\0') {
            if (Peek() != '"') break;
            const std::string key = ParseString().AsString();
            if (Get() != ':') break;
            obj.SetObjectKey(key, ParseValue());
            if (Peek() == ',') Get();
        }
        Get();
        return obj;
    }

    Value ParseArray() {
        Value arr;
        Get();
        while (Peek() != ']' && Peek() != '\0') {
            arr.AddArrayElement(ParseValue());
            if (Peek() == ',') Get();
        }
        Get();
        return arr;
    }

    Value ParseString() {
        Get();
        std::string str;
        while (pos_ < json_.size() && json_[pos_] != '"') {
            if (json_[pos_] == '\\' && pos_ + 1 < json_.size()) {
                pos_++;
                const char c = json_[pos_++];
                if (c == 'n') str += '\n';
                else if (c == 't') str += '\t';
                else if (c == 'r') str += '\r';
                else str += c;
            } else {
                str += json_[pos_++];
            }
        }
        Get();
        return Value(str);
    }

    Value ParseNumber() {
        const size_t start = pos_;
        if (json_[pos_] == '-') pos_++;
        while (pos_ < json_.size() && ((json_[pos_] >= '0' && json_[pos_] <= '9') || json_[pos_] == '.' ||
                                       json_[pos_] == 'e' || json_[pos_] == 'E' || json_[pos_] == '+' || json_[pos_] == '-'))
            pos_++;
        try {
            return Value(std::stod(json_.substr(start, pos_ - start)));
        } catch (...) {
            return Value(0.0);
        }
    }

    Value ParseBool() {
        if (json_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value(true); }
        if (json_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value(false); }
        return Value();
    }

    Value ParseNull() {
        if (json_.compare(pos_, 4, "null") == 0) pos_ += 4;
        return Value();
    }

    std::string json_;
    size_t pos_ = 0;
};

std::string lower_copy(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// The z order guessed from the asset name the way the old editor did
int guess_z_index(const UIElement& element) {
    if (element.assetPath.empty()) return 10;
    const std::string lower = lower_copy(element.assetPath);
    if (lower.find("_back") != std::string::npos || lower.find("wallpaper") != std::string::npos ||
        lower.find("background") != std::string::npos)
        return 0;
    if (lower.find("_top") != std::string::npos) return 5;
    if (element.type == ELEM_BUTTON) return 20;
    if (element.type == ELEM_IMAGE) return 12;
    if (element.type == ELEM_INPUT || element.type == ELEM_TEXT) return 25;
    return 10;
}

std::string state_suffix(const std::string& full_path, const char* suffix) {
    return full_path + (!full_path.empty() && full_path.back() == '_' ? suffix : "");
}

std::string escaped(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

const char* bool_word(bool value) { return value ? "true" : "false"; }

}

std::string json_path_for_state(int state) {
    static const char* const kFiles[] = {
        "ui_state_01_logo.json",
        "ui_state_00_intro.json",
        "ui_state_02_login.json",
        "ui_state_03_channel.json",
        "ui_state_04_menu.json",
        "ui_state_06_garage.json",
        "ui_state_13_shop.json",
        "ui_state_07_lobby.json",
        "ui_state_08_room.json",
    };
    constexpr int kFileCount = static_cast<int>(sizeof(kFiles) / sizeof(kFiles[0]));
    if (state < 0 || state >= kFileCount) return "";
    const std::string filename = kFiles[state];

    std::vector<std::string> dirs;
    if (!g_editor.uiDir.empty()) {
        std::string dir = g_editor.uiDir;
        if (dir.back() != '/' && dir.back() != '\\') dir += '/';
        dirs.push_back(dir);
    }
    for (const char* dir : {"clone/Data/Public/UI/", "Data/Public/UI/", "tools/ui_extractor/extracted_ui/", "../Data/Public/UI/",
                            "../tools/ui_extractor/extracted_ui/"})
        dirs.push_back(dir);

    for (const std::string& dir : dirs) {
        const std::string path = dir + filename;
        std::ifstream test(path);
        if (test.good()) return path;
    }
    return dirs.front() + filename;
}

bool load_screen_from_json(UIScreen& screen, const std::string& path) {
    std::printf("[JSON] loading %s\n", path.c_str());
    const Value root = Parser::ParseFile(path);
    if (root.GetType() != Value::OBJECT) {
        std::printf("[JSON] cannot parse %s\n", path.c_str());
        return false;
    }

    screen.state = root["state"].AsInt();
    screen.name = root["name"].AsString();
    screen.elements.clear();

    const Value& elements = root["elements"];
    for (size_t i = 0; i < elements.ArraySize(); ++i) {
        const Value& ej = elements[i];
        UIElement element;
        element.id = ej["id"].AsString();
        element.name = element.id;
        element.type = element_type_from_string(ej["type"].AsString());

        const float x = static_cast<float>(ej["position"][0].AsDouble());
        const float y = static_cast<float>(ej["position"][1].AsDouble());
        float w = 100.f;
        float h = 100.f;
        if (ej.HasKey("size")) {
            w = static_cast<float>(ej["size"][0].AsDouble());
            h = static_cast<float>(ej["size"][1].AsDouble());
        }
        element.bounds = {x, y, w, h};

        if (ej.HasKey("asset")) {
            element.assetPath = ej["asset"].AsString();
            const std::string full_path = "./Data/Eng/Image/" + element.assetPath;
            element.texture = load_texture(full_path);
            if (element.texture != nullptr && element.texture->valid() && !ej.HasKey("size")) {
                element.bounds.width = static_cast<float>(element.texture->width);
                element.bounds.height = static_cast<float>(element.texture->height);
            }
            if (element.type == ELEM_BUTTON) {
                element.normalAsset = full_path;
                element.hoverAsset = ej.HasKey("hoverAsset") ? ej["hoverAsset"].AsString() : state_suffix(full_path, "01.png");
                element.pressedAsset = ej.HasKey("pressedAsset") ? ej["pressedAsset"].AsString() : state_suffix(full_path, "02.png");
                element.disabledAsset = ej.HasKey("disabledAsset") ? ej["disabledAsset"].AsString() : state_suffix(full_path, "03.png");
                element.hoverTexture = load_texture(element.hoverAsset);
                element.pressedTexture = load_texture(element.pressedAsset);
                element.disabledTexture = load_texture(element.disabledAsset);
            }
        }

        if (ej.HasKey("visible")) element.visible = ej["visible"].AsBool();
        if (ej.HasKey("enabled")) element.enabled = ej["enabled"].AsBool();
        if (ej.HasKey("action")) element.action = ej["action"].AsString();
        if (ej.HasKey("text")) element.text = ej["text"].AsString();
        if (ej.HasKey("fontSize")) element.fontSize = ej["fontSize"].AsInt();

        if (ej.HasKey("properties")) {
            const Value& props = ej["properties"];
            if (props.HasKey("maxLength")) element.maxLength = props["maxLength"].AsInt();
            if (props.HasKey("placeholder")) element.placeholder = props["placeholder"].AsString();
            if (props.HasKey("password")) element.isPassword = props["password"].AsBool();
            if (props.HasKey("checked")) element.checked = props["checked"].AsBool();
        }

        element.zIndex = guess_z_index(element);
        std::printf("    [%zu] %s (%s) at (%.0f, %.0f) z=%d\n", i, element.id.c_str(), element_type_json(element.type), x, y,
                    element.zIndex);
        screen.elements.push_back(element);
    }

    screen.loaded = true;
    std::printf("[JSON] %s has %zu elements\n", screen.name.c_str(), screen.elements.size());
    return true;
}

std::string default_export_name(const UIScreen& screen, int state) {
    std::string filename = "ui_state_" + std::to_string(state) + "_" + screen.name + ".json";
    std::replace(filename.begin(), filename.end(), ' ', '_');
    return filename;
}

bool export_screen_json(const UIScreen& screen, int state, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::printf("[EXPORT] cannot open %s\n", path.c_str());
        return false;
    }

    f << "{\n";
    f << "  \"version\": \"1.0\",\n";
    f << "  \"state\": " << state << ",\n";
    f << "  \"name\": \"" << escaped(screen.name) << "\",\n";
    f << "  \"resolution\": [" << kGameWidth << ", " << kGameHeight << "],\n";
    f << "  \"elements\": [\n";

    for (size_t i = 0; i < screen.elements.size(); ++i) {
        const UIElement& elem = screen.elements[i];
        f << "    {\n";
        f << "      \"id\": \"" << escaped(elem.id.empty() ? elem.name : elem.id) << "\",\n";
        f << "      \"type\": \"" << element_type_json(elem.type) << "\",\n";

        if (!elem.assetPath.empty()) f << "      \"asset\": \"" << escaped(elem.assetPath) << "\",\n";

        if (elem.type == ELEM_BUTTON) {
            if (!elem.hoverAsset.empty()) f << "      \"hoverAsset\": \"" << escaped(elem.hoverAsset) << "\",\n";
            if (!elem.pressedAsset.empty()) f << "      \"pressedAsset\": \"" << escaped(elem.pressedAsset) << "\",\n";
            if (!elem.disabledAsset.empty()) f << "      \"disabledAsset\": \"" << escaped(elem.disabledAsset) << "\",\n";
        }

        if (!elem.action.empty()) f << "      \"action\": \"" << escaped(elem.action) << "\",\n";

        if (elem.type == ELEM_TEXT && !elem.text.empty()) {
            f << "      \"text\": \"" << escaped(elem.text) << "\",\n";
            f << "      \"fontSize\": " << elem.fontSize << ",\n";
        }

        if (elem.type == ELEM_INPUT) {
            f << "      \"properties\": {\n";
            f << "        \"maxLength\": " << elem.maxLength << ",\n";
            f << "        \"password\": " << bool_word(elem.isPassword) << "\n";
            f << "      },\n";
        }

        if (elem.type == ELEM_CHECKBOX) {
            f << "      \"properties\": {\n";
            f << "        \"checked\": " << bool_word(elem.checked) << "\n";
            f << "      },\n";
        }

        f << "      \"position\": [" << static_cast<int>(elem.bounds.x) << ", " << static_cast<int>(elem.bounds.y) << "],\n";
        f << "      \"size\": [" << static_cast<int>(elem.bounds.width) << ", " << static_cast<int>(elem.bounds.height) << "],\n";
        f << "      \"visible\": " << bool_word(elem.visible) << ",\n";
        f << "      \"enabled\": " << bool_word(elem.enabled) << "\n";
        f << "    }";
        if (i + 1 < screen.elements.size()) f << ",";
        f << "\n";
    }

    f << "  ]\n";
    f << "}\n";
    f.close();
    std::printf("[EXPORT] wrote %s\n", path.c_str());
    return true;
}

}
