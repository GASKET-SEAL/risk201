# Build: make        (produces risc201sim)
CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I../alu/include -I../fpu/include -I../control/include -I../stack/include
SRCS     = simulator.cpp \
           ../alu/src/alu.cpp ../alu/src/units.cpp ../fpu/src/fpu.cpp \
           ../control/src/control_memory.cpp ../control/src/control_packet.cpp \
           ../control/src/microprogram_sequencer.cpp \
           ../control/src/instruction_fetch_stage.cpp ../control/src/instruction_decode_stage.cpp \
           ../control/src/register_read_stage.cpp ../control/src/execute_stage.cpp \
           ../control/src/memory_access_stage.cpp ../control/src/write_back_stage.cpp \
           ../control/src/four_stage_components.cpp ../control/src/register_file.cpp \
           ../control/src/data_memory.cpp ../control/src/instruction_memory.cpp \
           ../stack/src/common.cpp ../stack/src/fault.cpp ../stack/src/macro_expander.cpp \
           ../stack/src/stack_guard.cpp ../stack/src/stack_visualizer.cpp \
           $(filter-out ../assembler/src/main.cpp,$(wildcard ../assembler/src/*.cpp))

risc201sim: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

clean:
	rm -f risc201sim risc201sim.exe
