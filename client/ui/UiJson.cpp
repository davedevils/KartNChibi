#include "UiJson.h"

#include <cctype>
#include <cstdio>
#include <map>

namespace KnC::Client {

namespace {

// a small JSON reader enough for the UI files
class Value {
public:
    enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };

    Value() = default;
    explicit Value(bool b) : type_(BOOL), bool_(b) {}
    explicit Value(double n) : type_(NUMBER), number_(n) {}
    explicit Value(const std::string& s) : type_(STRING), string_(s) {}

    Type type() const { return type_; }
    bool asBool() const { return type_ == BOOL ? bool_ : number_ != 0.0; }
    int asInt() const { return static_cast<int>(number_); }
    double asDouble() const { return number_; }
    const std::string& asString() const { return string_; }

    size_t size() const { return array_.size(); }
    const Value& operator[](size_t index) const {
        static const Value none;
        return index < array_.size() ? array_[index] : none;
    }
    bool has(const std::string& key) const { return object_.find(key) != object_.end(); }
    const Value& operator[](const std::string& key) const {
        static const Value none;
        const auto it = object_.find(key);
        return it != object_.end() ? it->second : none;
    }

    void setObject() { type_ = OBJECT; object_.clear(); }
    void push(const Value& v) {
        if (type_ != ARRAY) { type_ = ARRAY; array_.clear(); }
        array_.push_back(v);
    }
    void set(const std::string& key, const Value& v) {
        if (type_ != OBJECT) { type_ = OBJECT; object_.clear(); }
        object_[key] = v;
    }

private:
    Type type_ = NUL;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Value> array_;
    std::map<std::string, Value> object_;
};

class Parser {
public:
    static Value parse(const std::string& json) {
        Parser p(json);
        return p.value();
    }

private:
    explicit Parser(const std::string& json) : json_(json) {}

    char peek() {
        skip();
        return pos_ < json_.size() ? json_[pos_] : '\0';
    }
    char get() {
        skip();
        return pos_ < json_.size() ? json_[pos_++] : '\0';
    }
    void skip() {
        while (pos_ < json_.size() && (json_[pos_] == ' ' || json_[pos_] == '\t' || json_[pos_] == '\n' || json_[pos_] == '\r'))
            pos_++;
    }
    Value value() {
        const char c = peek();
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return string();
        if (c == 't' || c == 'f') return boolean();
        if (c == 'n') return null();
        if (c == '-' || (c >= '0' && c <= '9')) return number();
        return Value();
    }
    Value object() {
        Value obj;
        obj.setObject();
        get();
        while (peek() != '}' && peek() != '\0') {
            if (peek() != '"') break;
            const std::string key = string().asString();
            if (get() != ':') break;
            obj.set(key, value());
            if (peek() == ',') get();
        }
        get();
        return obj;
    }
    Value array() {
        Value arr;
        get();
        while (peek() != ']' && peek() != '\0') {
            arr.push(value());
            if (peek() == ',') get();
        }
        get();
        return arr;
    }
    Value string() {
        get();
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
        get();
        return Value(str);
    }
    Value number() {
        const size_t start = pos_;
        if (json_[pos_] == '-') pos_++;
        while (pos_ < json_.size() && ((json_[pos_] >= '0' && json_[pos_] <= '9') || json_[pos_] == '.' ||
                                       json_[pos_] == 'e' || json_[pos_] == 'E' || json_[pos_] == '+' || json_[pos_] == '-'))
            pos_++;
        return Value(std::atof(json_.substr(start, pos_ - start).c_str()));
    }
    Value boolean() {
        if (json_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value(true); }
        if (json_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value(false); }
        return Value();
    }
    Value null() {
        if (json_.compare(pos_, 4, "null") == 0) pos_ += 4;
        return Value();
    }

    std::string json_;
    size_t pos_ = 0;
};

std::string lowerCopy(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// the z order guessed from the asset name the way the editor does it
int guessZ(const LayoutElement& e) {
    // inputs text and buttons sit over every image whatever their asset says
    if (e.type == ElementType::Input || e.type == ElementType::Text) return 25;
    if (e.type == ElementType::Button || e.type == ElementType::Checkbox) return 20;
    if (e.asset.empty()) return 10;
    const std::string lower = lowerCopy(e.asset);
    if (lower.find("_back") != std::string::npos || lower.find("wallpaper") != std::string::npos ||
        lower.find("background") != std::string::npos)
        return 0;
    if (lower.find("_top") != std::string::npos) return 5;
    if (e.type == ElementType::Image) return 12;
    return 10;
}

std::string stateSuffix(const std::string& asset, const char* suffix) {
    return asset + (!asset.empty() && asset.back() == '_' ? suffix : "");
}

}

const char* elementTypeName(ElementType type) {
    switch (type) {
    case ElementType::Image: return "image";
    case ElementType::Button: return "button";
    case ElementType::Text: return "text";
    case ElementType::Input: return "input";
    case ElementType::List: return "list";
    case ElementType::Panel: return "panel";
    case ElementType::Checkbox: return "checkbox";
    }
    return "image";
}

ElementType elementTypeFromString(const std::string& text) {
    const std::string lower = lowerCopy(text);
    for (ElementType t : {ElementType::Image, ElementType::Button, ElementType::Text, ElementType::Input,
                          ElementType::List, ElementType::Panel, ElementType::Checkbox})
        if (lower == elementTypeName(t)) return t;
    return ElementType::Image;
}

bool parseScreenLayout(const std::string& json, ScreenLayout& out) {
    const Value root = Parser::parse(json);
    if (root.type() != Value::OBJECT) return false;
    out.state = root["state"].asInt();
    out.name = root["name"].asString();
    if (root.has("resolution") && root["resolution"].size() >= 2) {
        out.width = static_cast<float>(root["resolution"][0].asDouble());
        out.height = static_cast<float>(root["resolution"][1].asDouble());
    }
    out.elements.clear();
    const Value& elements = root["elements"];
    for (size_t i = 0; i < elements.size(); ++i) {
        const Value& ej = elements[i];
        LayoutElement e;
        e.id = ej["id"].asString();
        e.type = elementTypeFromString(ej["type"].asString());
        if (ej.has("position") && ej["position"].size() >= 2) {
            e.x = static_cast<float>(ej["position"][0].asDouble());
            e.y = static_cast<float>(ej["position"][1].asDouble());
        }
        if (ej.has("size") && ej["size"].size() >= 2) {
            e.width = static_cast<float>(ej["size"][0].asDouble());
            e.height = static_cast<float>(ej["size"][1].asDouble());
            e.hasSize = e.width > 0.f && e.height > 0.f;
        }
        if (ej.has("asset")) {
            e.asset = ej["asset"].asString();
            if (e.type == ElementType::Button) {
                e.hoverAsset = ej.has("hoverAsset") ? ej["hoverAsset"].asString() : stateSuffix(e.asset, "01.png");
                e.pressedAsset = ej.has("pressedAsset") ? ej["pressedAsset"].asString() : stateSuffix(e.asset, "02.png");
                e.disabledAsset = ej.has("disabledAsset") ? ej["disabledAsset"].asString() : stateSuffix(e.asset, "03.png");
            }
        }
        if (ej.has("visible")) e.visible = ej["visible"].asBool();
        if (ej.has("enabled")) e.enabled = ej["enabled"].asBool();
        if (ej.has("action")) e.action = ej["action"].asString();
        if (ej.has("text")) e.text = ej["text"].asString();
        if (ej.has("fontSize")) e.fontSize = ej["fontSize"].asInt();
        if (ej.has("properties")) {
            const Value& props = ej["properties"];
            if (props.has("maxLength")) e.maxLength = props["maxLength"].asInt();
            if (props.has("placeholder")) e.placeholder = props["placeholder"].asString();
            if (props.has("password")) e.password = props["password"].asBool();
            if (props.has("checked")) e.checked = props["checked"].asBool();
            if (props.has("content")) e.text = props["content"].asString();
            if (props.has("fontSize")) e.fontSize = props["fontSize"].asInt();
            if (props.has("size")) e.fontSize = props["size"].asInt();
            if (props.has("color") && props["color"].size() >= 3) {
                for (size_t k = 0; k < 4 && k < props["color"].size(); ++k)
                    e.colour[k] = static_cast<uint8_t>(props["color"][k].asInt());
            }
        }
        e.zIndex = guessZ(e);
        out.elements.push_back(e);
    }
    return true;
}

}
