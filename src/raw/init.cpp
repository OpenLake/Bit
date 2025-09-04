#include <iostream>

class BitRepository
{
    public:
    std::string worktree;
    std::string bitdir;
    std::string conf;
};

std::string getPath()
{
    std::string path = "";
    std::cout << "Enter path:";
    std::cin >> path;
    return path;
};

bool getForce()
{
    char temp = 'n';
    std::cout << "Force? (Y/n): ";
    std::cin >> temp;
    if (temp == 'Y' || temp == 'y') {
        return true;
    } else {
        return false;
    }
}

int init(std::string path, bool force)
{
    std::cout << "Initializing repository at: " << path << " with force: " << force << std::endl;
    return 0;
}

int main() {
    std::string path = getPath();
    bool force = getForce();
    init(path, force);
    return 0;
}