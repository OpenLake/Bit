#ifndef OBJECT_H
#define OBJECT_H

#include <string>

class BitObject {
public:
    static std::string hash_object(const std::string& data, const std::string& type);
    static std::string read_object(const std::string& sha);
    static void write_object(const std::string& sha, const std::string& data);
};

#endif
