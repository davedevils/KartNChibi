#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

std::vector<unsigned char> hexToBytes(const std::string& hex) {
	std::vector<unsigned char> bytes;
	for (size_t i = 0; i < hex.length(); i += 2) {
		std::string byteString = hex.substr(i, 2);
		unsigned char byte = static_cast<unsigned char>(strtol(byteString.c_str(), nullptr, 16));
		bytes.push_back(byte);
	}
	return bytes;
}

std::string bytesToHex(const std::vector<unsigned char>& bytes) {
	std::ostringstream oss;
	for (auto byte : bytes) {
		oss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
	}
	return oss.str();
}

void decrypt(const std::vector<unsigned char>& input, const std::string& key, std::vector<unsigned char>& output) {
    size_t input_length = input.size();
    size_t key_length = key.length();
    output.resize(input_length);

    for (size_t i = 0; i < input_length; ++i) {
        int v8 = static_cast<int>(input[i]);
        if (v8 <= 31 || v8 > 122) {
            output[i] = v8;
        } else {
            output[i] = (v8 - key[i % key_length] + 91) % 91 + 32;
        }
    }
}

void encrypt(const std::vector<unsigned char>& input, const std::string& key, std::vector<unsigned char>& output) {
	size_t input_length = input.size();
	size_t key_length = key.length();
	output.resize(input_length);

	for (size_t i = 0; i < input_length; ++i) {
		int v8 = static_cast<int>(input[i]);
		if (v8 <= 31 || v8 > 122) {
			output[i] = v8;
		}
		else {
			output[i] = (v8 - 32 + key[i % key_length] - 32) % 91 + 32;
		}
	}
}

std::vector<unsigned char> readBinaryFile(const std::string& filename) {
	std::ifstream infile(filename, std::ios::binary);
	return std::vector<unsigned char>((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
}

void writeBinaryFile(const std::string& filename, const std::vector<unsigned char>& content) {
	std::ofstream outfile(filename, std::ios::binary);
	outfile.write(reinterpret_cast<const char*>(content.data()), content.size());
}

void processFile(const std::string& filename) {
	// LOOOOL THAT KEY IS FUUNNY
	std::string key = "WindySoftKnCOnGame";
	std::vector<unsigned char> content = readBinaryFile(filename);
	std::vector<unsigned char> result;

	auto it = std::find(content.begin(), content.end(), '\r');
	if (it == content.end() || *(it + 1) != '\n') {
		std::cerr << "File invalid no CRLF found !" << std::endl;
		return;
	}

	if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".ini") {
		std::vector<unsigned char> part1(content.begin(), it);
		std::vector<unsigned char> part2(it + 2, content.end());

		std::vector<unsigned char> decryptedPart1, decryptedPart2;
		decrypt(part1, key, decryptedPart1);
		decrypt(part2, key, decryptedPart2);

		result.insert(result.end(), decryptedPart1.begin(), decryptedPart1.end());
		result.insert(result.end(), { '\r', '\n' });
		result.insert(result.end(), decryptedPart2.begin(), decryptedPart2.end());
		writeBinaryFile(filename + ".dec", result);
		std::cout << "Decrypted -> " << filename + ".dec\n";
	}
	else if (filename.size() > 8 && filename.substr(filename.size() - 8) == ".ini.dec") {
		std::vector<unsigned char> part1(content.begin(), it);
		std::vector<unsigned char> part2(it+2, content.end());

		std::vector<unsigned char> encryptedPart1, encryptedPart2;
		encrypt(part1, key, encryptedPart1);
		encrypt(part2, key, encryptedPart2);

		result.insert(result.end(), encryptedPart1.begin(), encryptedPart1.end());
		result.insert(result.end(), { '\r', '\n' });
		result.insert(result.end(), encryptedPart2.begin(), encryptedPart2.end());
		writeBinaryFile(filename.substr(0, filename.size() - 4) , result);
		std::cout << "Encrypted -> " << filename.substr(0, filename.size() - 4) + "\n";
	}
	else {
		std::cerr << "Invalid file type." << std::endl;
	}
}


int main(int argc, char* argv[]) {
	std::cout << "-----------------------------------------------\n";
	std::cout << "|           Network2.ini Pack/Unpack          |\n";
	std::cout << "|               Chibi Kart / KNC              |\n";
	std::cout << "|                 Version 1.0                 |\n";
	std::cout << "-----------------------------------------------\n";
	std::cout << "|                by davedevils                |\n";
	std::cout << "-----------------------------------------------\n";
	std::cout << " \n";

	if (argc != 2) {
		std::cout << "No input file ...\n";
		std::cout << "Drag and drop Network2.ini -> decrypt\n";
		std::cout << "Drag and drop Network2.dec -> encrypt\n";
		system("pause");
		return 1;
	}

	std::string filename = argv[1];
	processFile(filename);

	return 0;
}
