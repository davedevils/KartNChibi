#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include "NKZIP.h"

namespace fs = std::filesystem;

void PackDat(const std::string &dirPath, const std::string &outputFilePath, const std::string &version) {
	std::vector<NKZIPFile> files;

	for (const auto &entry : fs::recursive_directory_iterator(dirPath)) {
		if (entry.is_regular_file()) {
			NKZIPFile nkZipFile;
			std::ifstream inFile(entry.path(), std::ios::binary);

			inFile.seekg(0, std::ios::end);
			std::streamoff fileSize = inFile.tellg();
			if (fileSize > std::numeric_limits<uint32_t>::max()) {
				throw std::runtime_error("File size exceeds 4GB limit for file: " + entry.path().string());
			}
			nkZipFile.fileSize = static_cast<uint32_t>(fileSize);
			inFile.seekg(0, std::ios::beg);

			nkZipFile.fileBytes.resize(nkZipFile.fileSize);
			inFile.read(reinterpret_cast<char *>(nkZipFile.fileBytes.data()), nkZipFile.fileSize);

			std::string relativePath = fs::relative(entry.path(), dirPath).string();
			std::memset(nkZipFile.fileName, 0, sizeof(nkZipFile.fileName));
			std::memcpy(nkZipFile.fileName, relativePath.c_str(), std::min(relativePath.size(), sizeof(nkZipFile.fileName) - 1));

			files.push_back(nkZipFile);
		}
	}

	NKZIP nkZip(files, version);
	nkZip.pack(outputFilePath);
}

void UnPackDat(const std::vector<std::string> &filePaths) {
	for (const auto &filePath : filePaths) {
		std::cout << "Start unpack of "<< filePath << "\n";
		try {
			NKZIP nkZip(filePath);
			std::string outputDir = fs::path(filePath).parent_path().string();
			nkZip.extract(outputDir);
			std::cout << filePath << " Unpacked.\n";
		}
		catch (const std::exception &e) {
			std::cerr << "Error when unpacking " << filePath << ": " << e.what() << std::endl;
		}
	}
}

int main(int argc, char *argv[]) {

	std::cout << "-----------------------------------------------\n";
	std::cout << "|              NKZIP Pack/Unpack              |\n";
	std::cout << "|               Chibi Kart / KNC              |\n";
	std::cout << "|                 Version 1.0                 |\n";
	std::cout << "-----------------------------------------------\n";
	std::cout << "|                by davedevils                |\n";
	std::cout << "-----------------------------------------------\n";
	std::cout << " \n";

	if (argc == 1)
	{
		std::cout << "No input file ...\n";
		std::cout << "For pack -> drag and drop folder\n";
		std::cout << "For unpack -> drag and drop dat file\n";
		system("pause");
		return 1;
	}

	try {
		std::string inputPath = argv[1];
		if (fs::is_directory(inputPath)) {
			std::string version = "1";
			std::string outputFilePath = fs::path(inputPath).filename().string() + ".dat";
			PackDat(inputPath, outputFilePath, version);
			std::cout << "Packed to " << outputFilePath << std::endl;
		}
		else {
			std::vector<std::string> filePaths;
			for (int i = 1; i < argc; ++i) {
				filePaths.push_back(argv[i]);
			}
			UnPackDat(filePaths);
		}
	}
	catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}