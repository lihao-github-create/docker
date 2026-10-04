#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    const char* name = argc > 1 ? argv[1] : "World";

    const char* greeting = std::getenv("GREETING");
    if (greeting == nullptr) {
        greeting = "Hello";
    }

    std::cout << greeting << ", " << name << "!" << std::endl;
    return 0;
}