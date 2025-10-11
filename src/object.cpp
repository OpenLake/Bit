#include "object.h"
#include "utils.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

std::string BitObject::hash_object(const std::string& data, const std::string& type) {
    std::string header = type + " " + std::to_string(data.size()) + '\0';
    std::string store = header + data;

    std::string sha = BitUtils::sha1(store);
    std::string compressed = BitUtils::compress_data(store);

    std::string dir = ".bit/objects/" + sha.substr(0, 2);
    std::string file = dir + "/" + sha.substr(2);
    BitUtils::ensure_dir(dir);

    std::ofstream out(file, std::ios::binary);
    out.write(compressed.data(), compressed.size());
    out.close();

    return sha;
}

std::string BitObject::read_object(const std::string& sha) {
    std::string path = ".bit/objects/" + sha.substr(0, 2) + "/" + sha.substr(2);
    if (!BitUtils::file_exists(path))
        throw std::runtime_error("Object not found: " + sha);

    std::ifstream in(path, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    in.close();

    return BitUtils::decompress_data(buffer.str());
}

void BitObject::write_object(const std::string& sha, const std::string& data) {
    std::string dir = ".bit/objects/" + sha.substr(0, 2);
    std::string file = dir + "/" + sha.substr(2);
    BitUtils::ensure_dir(dir);

    std::ofstream out(file, std::ios::binary);
    out.write(data.data(), data.size());
    out.close();
}
