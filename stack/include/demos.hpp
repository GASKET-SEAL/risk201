// demos.hpp -- built-in self-test programs and the runner used by main.cpp
#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "fault.hpp"

namespace risc201 {

struct Options {
    bool naive = false, shadow = true, canary = true, verbose = false;
};

struct Demo {
    std::string             name;
    std::string             src;
    std::optional<Fault>    expect;   // none = must run to halt
    std::map<int, uint32_t> regs;     // expected register values at halt
};

// Expand + run one program; returns true if it behaved as `expect` says.
bool runSource(const std::string& name, const std::string& src, const Options& o,
               std::optional<Fault> expect, const std::map<int, uint32_t>& regs);

// The built-in suite (expectations depend on which defences are enabled).
std::vector<Demo> demos(const Options& o);

}  // namespace risc201
