#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <string>

class BitRepository {
public:
    std::string worktree;
    std::string bitdir;
    std::string conf;

    BitRepository() = default;
    void init(const std::string& path);
    void load(const std::string& path);
    void add(const std::string& file);
    void commit(const std::string& message);
    void log();
    void status();
    void branch(const std::string& name = "");
    void checkout(const std::string& branch);

private:
    std::string repo_dir(const std::string& path);
};

#endif
