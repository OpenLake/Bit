#include "repository.h"
#include "object.h"
#include "commit.h"
#include "branch.h"
#include "utils.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: bit <command> [args]\n";
        return 1;
    }

    std::string cmd = argv[1];
    BitRepository repo;

    try {
        if (cmd == "init") {
            repo.init(".");
        }
        else if (cmd == "add" && argc >= 3) {
            repo.load(".");
            repo.add(argv[2]);
        }
        else if (cmd == "commit" && argc >= 3) {
            repo.load(".");
            repo.commit(argv[2]);
        }
        else if (cmd == "log") {
            repo.load(".");
            repo.log();
        }
        else if (cmd == "status") {
            repo.load(".");
            repo.status();
        }
        else if (cmd == "branch") {
            repo.load(".");
            if (argc == 2)
                repo.branch();
            else
                repo.branch(argv[2]);
        }
        else if (cmd == "checkout" && argc >= 3) {
            repo.load(".");
            repo.checkout(argv[2]);
        }
        else {
            std::cerr << "Unknown command: " << cmd << "\n";
        }
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
