// Runs the KFM reader over a directory and reports what parsed formats only
#include "engine/formats/kfm_reader.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: kfm_smoke <directory>\n";
        return 2;
    }

    std::map<std::string, size_t> failures;
    size_t total = 0, parsed = 0, sequences = 0;

    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(argv[1], ec)) {
        if (entry.path().extension() != ".kfm") continue;
        ++total;
        KnC::KfmFile kfm;
        std::string error;
        if (!KnC::read_kfm(entry.path().string(), kfm, error)) {
            ++failures[error];
            continue;
        }
        ++parsed;
        sequences += kfm.sequences.size();
    }

    std::cout << parsed << "/" << total << " files parse; " << sequences
              << " sequences total\n";
    for (const auto& entry : failures)
        std::cout << "  " << entry.second << " x " << entry.first << "\n";
    return failures.empty() ? 0 : 1;
}
