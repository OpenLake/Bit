#include "commit.h"
#include "object.h"
#include "utils.h"
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

std::string BitCommit::create_commit(const std::string& tree_hash, const std::string& parent_hash,
                                     const std::string& message, const std::string& author) {
    std::ostringstream commit;
    commit << "tree " << tree_hash << "\n";
    if (!parent_hash.empty())
        commit << "parent " << parent_hash << "\n";
    commit << "author " << author << " " << std::time(nullptr) << " +0000\n";
    commit << "committer " << author << " " << std::time(nullptr) << " +0000\n\n";
    commit << message << "\n";

    return BitObject::hash_object(commit.str(), "commit");
}

void BitCommit::print_log(const std::string& start_hash) {
    std::string current = start_hash;
    while (!current.empty()) {
        std::string obj = BitObject::read_object(current);
        std::cout << "commit " << current << "\n";

        std::istringstream iss(obj);
        std::string line;
        std::string parent;
        while (std::getline(iss, line)) {
            if (line.rfind("parent", 0) == 0)
                parent = line.substr(7);
            if (line.empty()) break;
        }

        std::string msg;
        while (std::getline(iss, line))
            msg += line + "\n";
        std::cout << "\n    " << msg << "\n";
        current = parent;
    }
}
