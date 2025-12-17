#pragma once

#include "NKZIPFile.h"
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

class NKZIP {
public:
	NKZIP(const std::string &filePath);
	NKZIP(const std::vector<NKZIPFile> &files, const std::string &version);
	void extract(const std::string &outputDir);
	void pack(const std::string &outputFilePath);

private:
	void parse(std::ifstream &fileStream);
	std::vector<NKZIPFile> fileList;
	std::string version;
	uint32_t dataBytes;
	uint32_t fileCount;
};
