#include "grammergen.hpp"

int main(int argc, const char** argv) {
    using namespace grammergen;

    generic_programming gp;
    gp.read_input("./input.txt");
    gp.init_grammer(100, 100);
    gp.set_elite_ratio(0.05);
    gp.set_mutation_ratio(0.05);
    gp.set_max_unmodified_count(1000);
    gp.run();

    return EXIT_SUCCESS;
}
