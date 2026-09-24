// reads the ui state JSON files the same shape the ui editor reads
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Client {

enum class ElementType { Image, Button, Text, Input, List, Panel, Checkbox };

const char* elementTypeName(ElementType type);
ElementType elementTypeFromString(const std::string& text);

// one element as the JSON names it sizes come from the texture when absent
struct LayoutElement {
    ElementType type = ElementType::Image;
    std::string id;
    std::string asset;
    std::string hoverAsset;
    std::string pressedAsset;
    std::string disabledAsset;
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
    bool hasSize = false;
    bool visible = true;
    bool enabled = true;
    std::string action;
    std::string text;
    int fontSize = 20;
    uint8_t colour[4] = {255, 255, 255, 255};
    int maxLength = 64;
    std::string placeholder;
    bool password = false;
    bool checked = false;
    int zIndex = 0;
};

struct ScreenLayout {
    int state = 0;
    std::string name;
    float width = 1024.f;
    float height = 768.f;
    std::vector<LayoutElement> elements;
};

// parses one screen file text false when the JSON is not an object
bool parseScreenLayout(const std::string& json, ScreenLayout& out);

}
