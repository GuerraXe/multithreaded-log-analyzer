#include "app/analyze.hpp"
#include "app/version.hpp"

#include <iostream>
#include <string_view>

namespace {

int print_usage(std::ostream& os) {
    os << la::version_string() << "\n\n"
       << "usage: loganalyzer <command> [options]\n\n"
       << "commands:\n"
       << "  analyze <file>     parse a log file and print a statistics report\n"
       << "  benchmark <file>   compare sequential vs multithreaded throughput\n"
       << "  gen <file>         generate a synthetic dataset (seeded)\n"
       << "  version            print version and exit\n"
       << "  help [command]     show help\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(std::cerr);
        return 1; // usage error
    }

    const std::string_view command = argv[1];

    if (command == "version" || command == "--version" || command == "-v") {
        std::cout << la::version_string() << '\n';
        return 0;
    }
    if (command == "help" || command == "--help" || command == "-h") {
        return print_usage(std::cout);
    }

    if (command == "analyze") {
        if (argc < 3) {
            std::cerr << "loganalyzer: analyze requires a <file> argument\n";
            return 1;
        }
        // Option parsing (filters, --threads, --report) arrives in M2.
        return la::run_analyze(argv[2], std::cout, std::cerr);
    }

    // benchmark / gen are wired up in later milestones.
    std::cerr << "loganalyzer: command '" << command << "' is not implemented yet\n";
    return 1;
}
