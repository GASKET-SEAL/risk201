#include <cassert>
#include <iostream>

#include "data_memory.hpp"
#include "memory_access_stage.hpp"
#include "write_back_stage.hpp"

using namespace risc201;

int main() {
    DataMemory memory;
    RegisterFile registers;

    MemoryAccessStage ma(memory);
    WriteBackStage rw(registers);

    // ST R2, 4[R1]: store 42 at address 4
    EXMARegister st;
    st.valid = true;
    st.alu_result = 4;
    st.store_data = 42;
    st.control.memory_write = true;

    ma.execute(st);

    // LD R3, 4[R1]: load from address 4 into R3
    EXMARegister ld;
    ld.valid = true;
    ld.alu_result = 4;
    ld.rd = 3;
    ld.control.memory_read = true;
    ld.control.register_write = true;
    ld.control.write_back = WriteBackControl::MEMORY_DATA;

    MARWRegister loaded = ma.execute(ld);
    rw.execute(loaded);

    assert(registers.read(3) == 42);

    // Test normal ALU result write-back
    EXMARegister add;
    add.valid = true;
    add.alu_result = 100;
    add.rd = 5;
    add.control.register_write = true;
    add.control.write_back = WriteBackControl::ALU_RESULT;

    MARWRegister result = ma.execute(add);
    rw.execute(result);

    assert(registers.read(5) == 100);

    std::cout << "MA-RW tests passed!" << std::endl;

    return 0;
}
