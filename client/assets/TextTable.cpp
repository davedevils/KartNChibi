#include "TextTable.h"

#include "AssetStore.h"

#include <cstdio>
#include <vector>

namespace KnC::Client {

namespace {

// splits on cr lf the two def trans files always use the windows pair
std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> out;
    std::string line;
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n') { out.push_back(line); line.clear(); continue; }
        line.push_back(c);
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

// the message file is utf16 little endian with a byte order mark
std::vector<std::string> wideLines(const std::vector<uint8_t>& bytes) {
    size_t at = 0;
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) at = 2;
    std::vector<std::string> out;
    std::string line;
    bool cr = false;
    for (; at + 1 < bytes.size(); at += 2) {
        const uint32_t c = static_cast<uint32_t>(bytes[at]) | (static_cast<uint32_t>(bytes[at + 1]) << 8);
        if (c == 0x0D) { cr = true; continue; }
        if (c == 0x0A) { out.push_back(line); line.clear(); cr = false; continue; }
        if (cr) { line.push_back('\r'); cr = false; }
        if (c < 0x80) {
            line.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            line.push_back(static_cast<char>(0xC0 | (c >> 6)));
            line.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            line.push_back(static_cast<char>(0xE0 | (c >> 12)));
            line.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            line.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

}

bool TextTable::load(const AssetStore& assets, const std::string& language) {
    m_lines.clear();
    const std::string lang = language.empty() ? std::string("Eng") : language;
    std::string index;
    std::vector<uint8_t> message;
    if (!assets.readText("Define/" + lang + "/def_trans_index.txt", index)) {
        std::printf("[text] no Define/%s/def_trans_index.txt keys stay raw\n", lang.c_str());
        return false;
    }
    if (!assets.readBytes("Define/" + lang + "/def_trans_message.txt", message)) {
        std::printf("[text] no Define/%s/def_trans_message.txt keys stay raw\n", lang.c_str());
        return false;
    }
    const std::vector<std::string> keys = splitLines(index);
    const std::vector<std::string> values = wideLines(message);
    // line one is the INDEX and Eng header pair the rest line up one for one
    for (size_t i = 1; i < keys.size() && i < values.size(); ++i) {
        if (keys[i].empty()) continue;
        m_lines[keys[i]] = values[i];
    }
    std::printf("[text] %zu def trans lines from Define/%s\n", m_lines.size(), lang.c_str());
    return !m_lines.empty();
}

const std::string& TextTable::text(const std::string& key) const {
    const auto it = m_lines.find(key);
    return it == m_lines.end() ? key : it->second;
}

}
