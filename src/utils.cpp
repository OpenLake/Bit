#include "utils.h"
#include <openssl/sha.h>
#include <zlib.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

namespace BitUtils {

std::string sha1(const std::string& data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);
    std::ostringstream oss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

std::string compress_data(const std::string& data) {
    uLongf compressed_size = compressBound(data.size());
    std::string compressed(compressed_size, '\0');
    if (compress(reinterpret_cast<Bytef*>(&compressed[0]), &compressed_size,
                 reinterpret_cast<const Bytef*>(data.data()), data.size()) != Z_OK) {
        throw std::runtime_error("Compression failed");
    }
    compressed.resize(compressed_size);
    return compressed;
}

std::string decompress_data(const std::string& data) {
    uLongf uncompressed_size = data.size() * 20; // rough guess
    std::string uncompressed(uncompressed_size, '\0');
    if (uncompress(reinterpret_cast<Bytef*>(&uncompressed[0]), &uncompressed_size,
                   reinterpret_cast<const Bytef*>(data.data()), data.size()) != Z_OK) {
        throw std::runtime_error("Decompression failed");
    }
    uncompressed.resize(uncompressed_size);
    return uncompressed;
}

void ensure_dir(const std::string& path) {
    if (!fs::exists(path))
        fs::create_directories(path);
}

bool file_exists(const std::string& path) {
    return fs::exists(path);
}

}
