#include "branch.h"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

void BitBranch::list_branches() {
    for (auto& entry : fs::directory_iterator(".bit/refs/heads"))
        std::cout << entry.path().filename().string() << "\n";
}

void BitBranch::create_branch(const std::string& name) {
    std::ifstream head(".bit/HEAD");
    std::string ref;
    std::getline(head, ref);
    head.close();

    if (ref.rfind("ref: ", 0) == 0)
        ref = ref.substr(5);

    std::ifstream branch_file(".bit/" + ref);
    std::string commit_hash;
    std::getline(branch_file, commit_hash);
    branch_file.close();

    std::ofstream new_branch(".bit/refs/heads/" + name);
    new_branch << commit_hash << "\n";
    new_branch.close();

    std::cout << "Created branch " << name << "\n";
}

void BitBranch::checkout_branch(const std::string& name) {
    std::ofstream head(".bit/HEAD");
    head << "ref: refs/heads/" << name << "\n";
    head.close();

    std::cout << "Switched to branch " << name << "\n";
}
