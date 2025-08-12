#include <iostream>

class ClosureManager {
public:
    void open() { std::cout << "Opening closure" << std::endl; }
    void close() { std::cout << "Closing closure" << std::endl; }
    bool isOpen() const { return open_state; }
private:
    bool open_state = false;
};
