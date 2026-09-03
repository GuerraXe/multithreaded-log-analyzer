#include "app/commands.hpp"

#include "gen/generator.hpp"

#include <fstream>
#include <ostream>

namespace la {

int run_gen(const Options& opt, std::ostream& out, std::ostream& err) {
    std::ofstream file(opt.input_path, std::ios::binary);
    if (!file) {
        err << "loganalyzer: cannot write '" << opt.input_path << "'\n";
        return 2;
    }

    GenOptions g;
    g.lines = opt.gen_lines;
    g.seed = opt.gen_seed;
    generate_log(file, g);

    file.flush();
    if (!file) {
        err << "loganalyzer: write error on '" << opt.input_path << "'\n";
        return 2;
    }

    if (!opt.quiet) {
        out << "wrote " << opt.gen_lines << " lines to " << opt.input_path
            << " (seed " << opt.gen_seed << ")\n";
    }
    return 0;
}

} // namespace la
