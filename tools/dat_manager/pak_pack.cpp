// The pack of the console pak tool compiled here so both tools write one archive layout
#include <string>

#define main pak_tool_main
#include "../pak_tool/main.cpp"
#undef main

namespace KnC::Tools {

bool pack_folder(const std::string& out_pak, const std::string& folder, std::string& error) {
    return pack_directory(out_pak, folder, error);
}

}
