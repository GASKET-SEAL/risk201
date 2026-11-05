#ifndef EXECUTE_STAGE_HPP
#define EXECUTE_STAGE_HPP

#include "alu.hpp"
#include "pipeline_registers.hpp"

namespace risc201 {

class ExecuteStage {
public:
    explicit ExecuteStage(const Alu& alu);

    EXMARegister execute(
        const RREXRegister& input,
        AluFlags current_flags
    ) const;

private:
    const Alu& alu;
};

} 

#endif