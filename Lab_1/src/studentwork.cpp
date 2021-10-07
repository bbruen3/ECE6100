// -------------------------------------------------- //
// Implement all your code for this lab in this file. //
// This will be the only C++ file you will submit.    //
// -------------------------------------------------- //

// studentwork.cpp
// Analyzes a record in a CPU trace file.
// Author: Brandon Bruen

#include "trace.h"
#include <assert.h>
#include <list>
#include <iostream>
// You may include any other standard C or C++ headers you need here,
// e.g. #include <vector> or #include <algorithm>.
// Make sure this compiles on the reference machine!

// ----------------------------------------------------------------- //
// Do not modify the definitions of the four global variables below. //
// You may add more global variables if you need them.               //
// ----------------------------------------------------------------- //

/**
 * Total number of instructions executed.
 *
 * This variable is updated automatically by the code in sim.cpp.
 * You should not modify it, but you may find it useful for debugging,
 * e.g. as a unique identifier for a record in a trace file.
 */
uint64_t stat_num_inst = 0;

/**
 * Array of number of instructions executed by op type.
 *
 * For instance, stat_optype_dyn[OP_ALU] should be the number of times an ALU
 * instruction was executed in the trace.
 *
 * You must update this on every call to analyze_trace_record().
 */
uint64_t stat_optype_dyn[NUM_OP_TYPES] = {0};

/**
 * Total number of CPU cycles.
 *
 * You must update this on every call to analyze_trace_record().
 */
uint64_t stat_num_cycle = 0;

/**
 * Total number of unique instructions executed.
 *
 * Instructions with the same address should only be counted once.
 *
 * You must update this on every call to analyze_trace_record().
 */
uint64_t stat_unique_pc = 0;

// ------------------------------------------------------------------------- //
// You must implement the body of the analyze_trace_record() function below. //
// Do not modify its return type or argument type.                           //
// You may add helper functions if you need them.                            //
// ------------------------------------------------------------------------- //

/**
 * Updates the global variables stat_num_cycle, stat_optype_dyn, and
 * stat_unique_pc according to the given trace record.
 *
 * You must write code to implement this function.
 * Make sure you DO NOT update stat_num_inst; it is updated by the code in
 * sim.cpp.
 *
 * @param t the trace record to process. Refer to the trace.h header file for
 * details on the TraceRec type.
 */

std::list<uint64_t> addresses;

void analyze_trace_record(TraceRec *t) {
    assert(t);

    //std::cout << stat_num_inst << std::endl;
    // TODO: Task 1: Quantify the mix of the dynamic instruction stream.
    // Update stat_optype_dyn according to the trace record t.

    stat_optype_dyn[t->optype]++;

    // TODO: Task 2: Estimate the overall CPI using a simple CPI model in which
    // the CPI for each category of instructions is provided.
    // Update stat_num_cycle according to the trace record t.

    if (t->optype == OP_ALU) {
        stat_num_cycle += 1;
    } else if (t->optype == OP_LD) {
        stat_num_cycle += 2;
    } else if (t->optype == OP_ST) {
        stat_num_cycle += 2;
    } else if (t->optype == OP_CBR) {
        stat_num_cycle += 3;
    } else {
        stat_num_cycle += 1;
    }

    // TODO: Task 3: Estimate the instruction footprint by counting the number
    // of unique PCs in the benchmark trace.
    // Update stat_unique_pc according to the trace record t.
    addresses.push_back(t->inst_addr);
    addresses.sort();
    addresses.unique();
    stat_unique_pc = addresses.size();
    // Make sure you DO NOT update stat_num_inst.
}
