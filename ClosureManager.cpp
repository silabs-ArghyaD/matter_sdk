#include <iostream>

class ClosureManager {
public:
    void open() {
        open_state = true;
        std::cout << "Opening closure" << std::endl;
    }
    void close() {
        open_state = false;
        std::cout << "Closing closure" << std::endl;
    }
    bool isOpen() const { return open_state; }
private:
    bool open_state = false;
};
