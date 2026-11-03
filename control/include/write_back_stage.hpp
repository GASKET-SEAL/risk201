#ifndef WRITE_BACK_STAGE_HPP
#define WRITE_BACK_STAGE_HPP

#include "register_file.hpp"
#include "pipeline_registers.hpp"

namespace risc201 {

class WriteBackStage {
public:
    explicit WriteBackStage(RegisterFile& registers);

    void execute(const MARWRegister& input) const;

private:
    RegisterFile& registers;
};

}

#endif