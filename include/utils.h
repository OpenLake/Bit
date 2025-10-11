#ifndef UTILS_H
#define UTILS_H

#include <string>

namespace BitUtils {
    std::string sha1(const std::string& data);
    std::string compress_data(const std::string& data);
    std::string decompress_data(const std::string& data);
    void ensure_dir(const std::string& path);
    bool file_exists(const std::string& path);
}

#endif
