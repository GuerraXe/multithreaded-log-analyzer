#include "app/run.hpp"
#include "cli/parser.hpp"

#include <iostream>

int main(int argc, char** argv) {
    const la::ArgParse parsed = la::parse_args(argc, argv);
    if (!parsed.ok) {
        std::cerr << "loganalyzer: " << parsed.error << "\n"
                  << "try 'loganalyzer help'\n";
        return 1;
    }
    return la::run(parsed.options, std::cout, std::cerr);
}
