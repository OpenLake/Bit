#include "repository.h"
#include "object.h"
#include "commit.h"
#include "branch.h"
#include "utils.h"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

void BitRepository::add(const std::string& file) {
    if (!BitUtils::file_exists(file)) {
        std::cerr << "File not found: " << file << "\n";
        return;
    }

    std::ifstream in(file, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    in.close();

    std::string sha = BitObject::hash_object(buffer.str(), "blob");
    std::ofstream index(".bit/index", std::ios::app);
    index << sha << " " << file << "\n";
    index.close();

    std::cout << "Added " << file << "\n";
}

void BitRepository::commit(const std::string& message) {
    // create a tree placeholder
    std::ifstream index(".bit/index");
    std::stringstream buf;
    buf << index.rdbuf();
    std::string index_data = buf.str();
    std::string tree_hash = BitObject::hash_object(index_data, "tree");

    // get parent (HEAD)
    std::ifstream head(".bit/HEAD");
    std::string ref;
    std::getline(head, ref);
    head.close();
    if (ref.rfind("ref: ", 0) == 0)
        ref = ref.substr(5);

    std::string parent_hash;
    if (BitUtils::file_exists(".bit/" + ref)) {
        std::ifstream parent(".bit/" + ref);
        std::getline(parent, parent_hash);
        parent.close();
    }

    std::string commit_hash = BitCommit::create_commit(tree_hash, parent_hash, message, "User <user@example.com>");

    std::ofstream ref_file(".bit/" + ref);
    ref_file << commit_hash << "\n";
    ref_file.close();

    std::cout << "Committed as " << commit_hash.substr(0, 7) << "\n";
}

void BitRepository::log() {
    std::ifstream head(".bit/HEAD");
    std::string ref;
    std::getline(head, ref);
    head.close();

    if (ref.rfind("ref: ", 0) == 0)
        ref = ref.substr(5);

    std::ifstream ref_file(".bit/" + ref);
    std::string commit_hash;
    std::getline(ref_file, commit_hash);
    ref_file.close();

    if (commit_hash.empty()) {
        std::cout << "No commits yet.\n";
        return;
    }

    BitCommit::print_log(commit_hash);
}

void BitRepository::status() {
    if (!BitUtils::file_exists(".bit/index")) {
        std::cout << "No files added.\n";
        return;
    }
    std::ifstream idx(".bit/index");
    std::string line;
    std::cout << "Staged files:\n";
    while (std::getline(idx, line))
        std::cout << "  " << line.substr(41) << "\n";
}
