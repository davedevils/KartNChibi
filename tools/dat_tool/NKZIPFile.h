#include <iostream>
#include <vector>

struct NKZIPFile {
	uint32_t fileSize;
	char fileName[260];
	std::vector<uint8_t> fileBytes;
};

