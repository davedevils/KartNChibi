// the def trans table a catalogue key resolved to the shipped english line
#pragma once

#include <map>
#include <string>

namespace KnC::Client {

class AssetStore;

class TextTable {
public:
    // reads Define lang def trans index txt and def trans message txt line by line
    bool load(const AssetStore& assets, const std::string& language);
    // the line of a key or the key itself when the table does not know it
    const std::string& text(const std::string& key) const;
    bool knows(const std::string& key) const { return m_lines.count(key) != 0; }
    size_t size() const { return m_lines.size(); }

private:
    std::map<std::string, std::string> m_lines;
};

}
