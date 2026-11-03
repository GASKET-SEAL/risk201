#ifndef MEMORY_ACCESS_STAGE_HPP
#define MEMORY_ACCESS_STAGE_HPP

#include "data_memory.hpp"
#include "pipeline_registers.hpp"

namespace risc201 {

class MemoryAccessStage {
public:
    explicit MemoryAccessStage(DataMemory& memory);

    MARWRegister execute(const EXMARegister& input) const;

private:
    DataMemory& memory;
};

}

#endif
