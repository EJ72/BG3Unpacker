#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include "lz4.h"
#include "zlib.h"
#include "zstd.h"

namespace fs = std::filesystem;
using namespace std::chrono;

const size_t TABLE_ENTRY_SIZE = 272;
const std::string MAGIC_STRING = "LSPK";

struct FileEntry {
	std::string name;
	uint32_t offset = 0, compressionType = 0, compressedSize = 0, size = 0;
};

static std::vector<char> lz4Uncmp(const std::vector<char>& input, uint32_t inputSize, uint32_t outputSize) {
	std::vector<char> output(outputSize);
	int decompressedSize = LZ4_decompress_safe(input.data(), output.data(), inputSize, outputSize);

	if (decompressedSize < 0) throw std::runtime_error("Error during LZ4 decompression: " + std::to_string(decompressedSize));
	return output;
}

static std::vector<char> zlibUncmp(const std::vector<char>& input, uint32_t inputSize, uint32_t outputSize) {
	std::vector<char> output(outputSize);

	z_stream stream{};
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = static_cast<uInt>(inputSize);
	stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
	stream.avail_out = static_cast<uInt>(outputSize);
	stream.next_out = reinterpret_cast<Bytef*>(output.data());

	int ret = inflateInit(&stream);
	if (ret != Z_OK) {
		throw std::runtime_error("Error initializing zlib inflate: " + std::to_string(ret));
	}

	ret = inflate(&stream, Z_FINISH);
	if (ret != Z_STREAM_END) {
		inflateEnd(&stream);
		throw std::runtime_error("Error during zlib decompression: " + std::to_string(ret));
	}

	inflateEnd(&stream);
	return output;
}

static std::vector<char> zstdUncmp(const std::vector<char>& input, uint32_t inputSize, uint32_t outputSize) {
	std::vector<char> output(outputSize);
	ZSTD_DCtx* ctx = ZSTD_createDCtx();
	size_t decompressedSize = ZSTD_decompressDCtx(ctx, output.data(), outputSize, input.data(), inputSize);

	if (ZSTD_isError(decompressedSize)) {
		ZSTD_freeDCtx(ctx);
		throw std::runtime_error("Error during ZSTD decompression.");
	}

	ZSTD_freeDCtx(ctx);
	return output;
}

static void extractLZ4(std::ifstream& input, const std::string& outputDirectory, const FileEntry& entry, int& filesExtracted, int totalFiles) {
	try {
		input.seekg(entry.offset, std::ios::beg);

		fs::create_directories(fs::path(outputDirectory) /= fs::path(entry.name).remove_filename());
		std::ofstream output(fs::path(outputDirectory) / fs::path(entry.name), std::ios::binary);
		if (!output.is_open()) throw std::runtime_error("Error opening output file for writing: " + entry.name);

		std::vector<char> data(entry.size, 0);

		if (entry.compressionType == 0) {
			if (entry.size > 0) {
				input.read(data.data(), entry.size);
				if (input.gcount() != entry.size) throw std::runtime_error("Error: Incorrect data size for file: " + entry.name);
				output.write(data.data(), entry.size);
			}
			else {
				std::cout << "Skipping entry with size 0: " << entry.name << std::endl;
			}
		}
		else {
			std::vector<char> compressedData(entry.compressedSize, 0);
			input.read(compressedData.data(), entry.compressedSize);
			if (compressedData.size() != entry.compressedSize) throw std::runtime_error("Error: Incorrect compressed data size for file: " + entry.name);
			
			std::vector<char> outputBuffer(entry.size, 0);
			size_t decompressedSize = LZ4_decompress_safe(compressedData.data(), outputBuffer.data(), entry.compressedSize, entry.size);

			std::cout << "Actual Decompressed Size: " << decompressedSize << std::dec << " bytes" << std::endl;
			if (decompressedSize != entry.size) throw std::runtime_error("Unexpected decompressed size for file: " + entry.name);

			output.write(outputBuffer.data(), decompressedSize);
		}

		++filesExtracted;
		float progress = static_cast<float>(filesExtracted) / totalFiles * 100;
		std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << progress << "%\n";
		std::cout.flush();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << " Skipping file: " << entry.name << std::endl;
	}
}

static void extractZLIB(std::ifstream& input, const std::string& outputDirectory, const FileEntry& entry, int& filesExtracted, int totalFiles) {
	try {
		input.seekg(entry.offset, std::ios::beg);

		fs::create_directories(fs::path(outputDirectory) /= fs::path(entry.name).remove_filename());
		std::ofstream output(fs::path(outputDirectory) / fs::path(entry.name), std::ios::binary);
		if (!output.is_open()) throw std::runtime_error("Error opening output file for writing: " + entry.name);

		std::vector<char> data(entry.compressedSize, 0);
		input.read(data.data(), entry.compressedSize);
		if (input.gcount() != entry.compressedSize) throw std::runtime_error("Error: Incorrect compressed data size for file: " + entry.name);

		std::vector<char> outputBuffer(entry.size, 0);
		outputBuffer = zlibUncmp(data, entry.compressedSize, entry.size);

		output.write(outputBuffer.data(), entry.size);
		std::cout << "Decompressing file: " << entry.name << "\nCompressed Size: " << std::dec << entry.compressedSize << " bytes\nExpected Decompressed Size: " << entry.size << " bytes" << std::endl;

		++filesExtracted;
		float progress = static_cast<float>(filesExtracted) / totalFiles * 100;
		std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << progress << "%\n";
		std::cout.flush();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << " Skipping file: " << entry.name << std::endl;
	}
}

static void extractZSTD(std::ifstream& input, const std::string& outputDirectory, const FileEntry& entry, int& filesExtracted, int totalFiles) {
	try {
		input.seekg(entry.offset, std::ios::beg);

		fs::create_directories(fs::path(outputDirectory) /= fs::path(entry.name).remove_filename());
		std::ofstream output(fs::path(outputDirectory) / fs::path(entry.name), std::ios::binary);
		if (!output.is_open()) throw std::runtime_error("Error opening output file for writing: " + entry.name);

		std::vector<char> data(entry.compressedSize, 0);
		input.read(data.data(), entry.compressedSize);
		if (input.gcount() != entry.compressedSize) throw std::runtime_error("Error: Incorrect compressed data size for file: " + entry.name);

		std::vector<char> outputBuffer(entry.size, 0);
		outputBuffer = zstdUncmp(data, entry.compressedSize, entry.size);

		output.write(outputBuffer.data(), entry.size);
		std::cout << "Decompressing file: " << entry.name << "\nCompressed Size: " << std::dec << entry.compressedSize << " bytes\nExpected Decompressed Size: " << entry.size << " bytes" << std::endl;

		++filesExtracted;
		float progress = static_cast<float>(filesExtracted) / totalFiles * 100;
		std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << progress << "%\n";
		std::cout.flush();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << " Skipping file: " << entry.name << std::endl;
	}
}

static void processFile(const std::string& inputFilename, const std::string& outputDirectory) {
	try {
		std::ifstream input(inputFilename, std::ios::binary);
		if (!input.is_open()) throw std::runtime_error("Error opening input file.");

		// Magic Check
		char idString[5] = { 0 };
		input.read(idString, 4);
		if (std::string(idString) != MAGIC_STRING) throw std::runtime_error("Invalid ID string. Not a valid LSPK file. Skipping extraction.");

		uint32_t version = 0, numFiles = 0, tableCompressedSize = 0;
		uint64_t tableOffset = 0;

		input.read(reinterpret_cast<char*>(&version), sizeof(version));

		// Version check
		if (version != 18) throw std::runtime_error("Package version is not 18. Skipping extraction.");

		input.read(reinterpret_cast<char*>(&tableOffset), sizeof(tableOffset));

		input.seekg(tableOffset);

		input.read(reinterpret_cast<char*>(&numFiles), sizeof(numFiles));
		input.read(reinterpret_cast<char*>(&tableCompressedSize), sizeof(tableCompressedSize));

		std::cout << "Package Version: " << version << "\nExtracting " << numFiles << " files." << std::endl;

		std::vector<char> tableBuffer(tableCompressedSize, 0);
		std::vector<char> tableData(numFiles * TABLE_ENTRY_SIZE, 0);

		input.read(tableBuffer.data(), tableCompressedSize);
		tableData = lz4Uncmp(tableBuffer, tableCompressedSize, numFiles * TABLE_ENTRY_SIZE);

		const fs::path extractDir = outputDirectory;
		fs::create_directory(extractDir);

		int filesExtracted = 0;

		// Open a file to write the file list
		std::ofstream fileList(inputFilename.substr(0, inputFilename.find_last_of('.')) + "_files.log");
		if (!fileList.is_open()) throw std::runtime_error("Error opening file list file for writing.");

		for (uint32_t i = 0; i < numFiles; ++i) {
			FileEntry entry;
			entry.name.assign(tableData.begin() + i * TABLE_ENTRY_SIZE, tableData.begin() + i * TABLE_ENTRY_SIZE + 256);
			entry.offset = *reinterpret_cast<uint32_t*>(&tableData[i * TABLE_ENTRY_SIZE + 256]);
			entry.compressionType = *reinterpret_cast<uint32_t*>(&tableData[i * TABLE_ENTRY_SIZE + 260]);
			entry.compressedSize = *reinterpret_cast<uint32_t*>(&tableData[i * TABLE_ENTRY_SIZE + 264]);
			bool containsUncompressed = (entry.compressionType == 0);
			entry.size = *reinterpret_cast<uint32_t*>(&tableData[i * TABLE_ENTRY_SIZE + (containsUncompressed ? 264 : 268)]);

			// Trim the file name to remove any trailing null characters
			entry.name.erase(std::find(entry.name.begin(), entry.name.end(), '\0'), entry.name.end());

			// Write entry information to the file list
			fileList << "Name: " << entry.name << std::endl;
			fileList << "Offset: " << "0x" << std::hex << entry.offset << std::endl;
			fileList << "Compression Type: " << "0x" << std::hex << entry.compressionType << std::endl;
			fileList << "Compressed Size: " << std::dec << entry.compressedSize << " bytes." << std::endl;
			fileList << "Decompressed Size: " << std::dec << entry.size << " bytes." << std::endl;

			// Output an empty line only if it's not the last entry
			if (i < numFiles - 1)
				fileList << "\n";

			std::cout << "\nFile: " << entry.name << std::endl;
			std::cout << "Offset: 0x" << std::hex << entry.offset << std::endl;
			std::cout << "Compression Type: 0x" << std::hex << entry.compressionType << std::endl;
			std::cout << "Compressed Size: " << std::dec << entry.compressedSize << " bytes." << std::endl;
			std::cout << "Decompressed Size: " << std::dec << entry.size << " bytes." << std::endl;

			// Check the compression type and call the appropriate function
			if (entry.compressionType == 0x0) {
				std::cout << "\nCompression type: 0 (Uncompressed)" << std::endl;
				extractLZ4(input, outputDirectory, entry, filesExtracted, numFiles);
			}
			else if (entry.compressionType == 0x21000000) {
				std::cout << "\nCompression type: 1 (Pre-Patch 6 - ZLIB)" << std::endl;
				extractZLIB(input, outputDirectory, entry, filesExtracted, numFiles);
			}
			else if (entry.compressionType == 0x42000000) {
				std::cout << "\nCompression type: 2 (LZ4)" << std::endl;
				extractLZ4(input, outputDirectory, entry, filesExtracted, numFiles);
			}
			else if (entry.compressionType == 0x23000000) {
				std::cout << "\nCompression type: 3 (Patch 6 - ZSTD)" << std::endl;
				extractZSTD(input, outputDirectory, entry, filesExtracted, numFiles);
			}
			else {
				std::cout << "\n4GB+ Sized files don't extract fully! Could use some help here :)" << std::endl;
			}
		}

		std::cout << "\nExtraction complete!";
		fileList.close();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
	}
}

int main(int argc, char* argv[]) {
	try {
		if (argc < 2) {
			throw std::runtime_error("Usage: " + std::string(argv[0]) + " <input_file>");
		}

		// Concatenate all command-line arguments into a single string for the input filename
		std::string inputFilename;
		for (int i = 1; i < argc; ++i) {
			inputFilename += std::string(argv[i]);
			if (i < argc - 1) {
				inputFilename += ' '; // Add a space between arguments
			}
		}

		// Check if the file has a valid extension
		std::filesystem::path inputFilepath(inputFilename);
		std::string extension = inputFilepath.extension().string();

		if (extension != ".pak" && extension != ".lsv") {
			throw std::runtime_error("Invalid file extension. Supported extensions are .pak and .lsv.");
		}

		const std::string outputDirectory = inputFilepath.stem().string();

		const auto startTotalTime = high_resolution_clock::now();
		processFile(inputFilename, outputDirectory);
		const auto endTotalTime = high_resolution_clock::now();
		const auto totalDuration = duration_cast<seconds>(endTotalTime - startTotalTime);
		std::cout << "\nTotal Time Elapsed: " << totalDuration.count() << " seconds" << std::endl;

		return EXIT_SUCCESS;
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
