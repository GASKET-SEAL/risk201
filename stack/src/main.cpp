// main.cpp -- command-line driver
//
//   ./risc201_stack                run the built-in demo / self-test suite
//   ./risc201_stack prog.s         expand + run your own assembly file
//   flags: --naive  --no-shadow  --no-canary  -v
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "demos.hpp"

using namespace risc201;

int main(int argc, char** argv) {
    Options o;
    std::string file;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--naive")     o.naive = true;
        else if (a == "--no-shadow") o.shadow = false;
        else if (a == "--no-canary") o.canary = false;
        else if (a == "-v")          o.verbose = true;
        else                         file = a;
    }
    if (!file.empty()) {
        std::ifstream f(file);
        if (!f) { std::cerr << "cannot open " << file << "\n"; return 2; }
        std::stringstream ss;
        ss << f.rdbuf();
        return runSource(file, ss.str(), o, std::nullopt, {}) ? 0 : 1;
    }
    int pass = 0, total = 0;
    for (const Demo& d : demos(o)) {
        ++total;
        bool ok = runSource(d.name, d.src, o, d.expect, d.regs);
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << d.name << "\n";
        pass += ok;
    }
    std::cout << "\n" << pass << "/" << total << " demos behaved as expected\n";
    return pass == total ? 0 : 1;
}
