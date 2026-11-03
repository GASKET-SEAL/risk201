#ifndef REGISTER_READ_STAGE_HPP
#define REGISTER_READ_STAGE_HPP

#include "pipeline_registers.hpp"
#include "register_file.hpp"

namespace risc201 {

class RegisterReadStage {
public:
    RREXRegister execute(
        const IDRRRegister& input,
        const RegisterFile& registers
    ) const;
};

} 

#endif
