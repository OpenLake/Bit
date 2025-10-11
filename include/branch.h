#ifndef BRANCH_H
#define BRANCH_H

#include <string>

class BitBranch {
public:
    static void list_branches();
    static void create_branch(const std::string& name);
    static void checkout_branch(const std::string& name);
};

#endif
