#include "grammergen.hpp"

int main(int argc, const char** argv) {
    using namespace grammergen;

    generic_programming gp;
    gp.read_input("./input.txt");
    gp.init_grammer(10, 100);
    gp.print_grammer();

    std::shared_ptr<grammer> ptr =
        std::make_shared<cat>(
                std::make_shared<word>("foo"),
                std::make_shared<word>("bar")
        );
    std::cout << ptr->match("foobar") << std::endl;
    std::cout << *ptr << std::endl;
    return 0;
}
