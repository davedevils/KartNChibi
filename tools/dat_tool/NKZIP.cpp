#include "NKZIP.h"

NKZIP::NKZIP(const std::string &filePath) {
	std::ifstream fileStream(filePath, std::ios::binary);
	if (!fileStream) {
		throw std::runtime_error("Failed to open file !!!");
	}
	parse(fileStream);
}

NKZIP::NKZIP(const std::vector<NKZIPFile> &newfileList, const std::string &selectedVersion) {
	fileList = newfileList;
	version = selectedVersion;
	fileCount = fileList.size();
	dataBytes = 0;
	for (const auto &file : fileList) {
		dataBytes += file.fileSize;
	}
}

void NKZIP::extract(const std::string &outputDir) {
	for (const auto &nkZipFile : fileList) {
		fs::path filePath(outputDir);
		filePath /= nkZipFile.fileName;
		fs::create_directories(filePath.parent_path());
		std::ofstream outFile(filePath, std::ios::binary);
		outFile.write(reinterpret_cast<const char *>(nkZipFile.fileBytes.data()), nkZipFile.fileBytes.size());
		std::cout << "Extracted: " << filePath << "\n";
	}
}

void NKZIP::pack(const std::string &outputFilePath) {
	std::ofstream outFile(outputFilePath, std::ios::binary);
	if (!outFile) {
		throw std::runtime_error("Error can't create the output pak file !");
	}

	char magic[16] = "NKZIP";
	char versionReaded[16];
	std::memset(versionReaded, 0, sizeof(versionReaded));
	std::memcpy(versionReaded, version.c_str(), std::min(version.size(), sizeof(versionReaded) - 1));

	outFile.write(magic, sizeof(magic));
	outFile.write(versionReaded, sizeof(versionReaded));
	outFile.write(reinterpret_cast<const char *>(&dataBytes), sizeof(dataBytes));
	outFile.write(reinterpret_cast<const char *>(&fileCount), sizeof(fileCount));

	for (const auto &nkZipFile : fileList) {
		outFile.write(reinterpret_cast<const char *>(&nkZipFile.fileSize), sizeof(nkZipFile.fileSize));
		outFile.write(nkZipFile.fileName, sizeof(nkZipFile.fileName));
		outFile.write(reinterpret_cast<const char *>(nkZipFile.fileBytes.data()), nkZipFile.fileBytes.size());
	}
}

void NKZIP::parse(std::ifstream &fileStream) {
	char magic[16];
	fileStream.read(magic, sizeof(magic));

	if (std::string(magic, 5) != "NKZIP") {
		throw std::runtime_error("This is not a NKZIP or not from KNC ?!");
	}

	char versionRead[16];
	fileStream.read(versionRead, sizeof(versionRead));
	version = std::string(versionRead, strnlen(versionRead, sizeof(versionRead)));

	fileStream.read(reinterpret_cast<char *>(&dataBytes), sizeof(dataBytes));
	fileStream.read(reinterpret_cast<char *>(&fileCount), sizeof(fileCount));

	fileList.resize(fileCount);

	for (auto &nkZipFile : fileList) {
		fileStream.read(reinterpret_cast<char *>(&nkZipFile.fileSize), sizeof(nkZipFile.fileSize));
		fileStream.read(nkZipFile.fileName, sizeof(nkZipFile.fileName));
		nkZipFile.fileBytes.resize(nkZipFile.fileSize);
		fileStream.read(reinterpret_cast<char *>(nkZipFile.fileBytes.data()), nkZipFile.fileSize);
	}
}
