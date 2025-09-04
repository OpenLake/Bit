#include <iostream>

class BitRepository
{
    public:
    std::string worktree;
    std::string bitdir;
    std::string conf;
};

int init(std::string path, bool force)
{
    std::cout << "Initializing repository at: " << path << " with force: " << force << std::endl;
    return 0;
}

int main() {
    std::string path = "";
    bool force = "false";
    char temp = 'n';
    std::cout << "Enter path:";
    std::cin >> path;
    std::cout << "Force? (Y/n): ";
    std::cin >> temp;
    if (temp == 'Y' || temp == 'y') {
        force = true;
    } else {
        force = false;
    }
    init(path, force);
    return 0;
}