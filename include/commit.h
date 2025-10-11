#ifndef COMMIT_H
#define COMMIT_H

#include <string>

class BitCommit {
public:
    static std::string create_commit(const std::string& tree_hash, const std::string& parent_hash,
                                     const std::string& message, const std::string& author);
    static void print_log(const std::string& start_hash);
};

#endif
