// --------------------------------------------------------------------- //
// You will need to modify this file.                                    //
// You may add any code you need, as long as you correctly implement the //
// required pipe_cycle_*() functions already listed in this file.        //
// In part B, you will also need to implement pipe_check_bpred().        //
// --------------------------------------------------------------------- //

// pipeline.cpp
// Implements functions to simulate a pipelined processor.

#include "pipeline.h"
#include <cstdlib>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

/**
 * Read a single trace record from the trace file and use it to populate the
 * given fetch_op.
 * 
 * You should not modify this function.
 * 
 * @param p the pipeline whose trace file should be read
 * @param fetch_op the PipelineLatch struct to populate
 */
void pipe_get_fetch_op(Pipeline *p, PipelineLatch *fetch_op)
{
    TraceRec *trace_rec = &fetch_op->trace_rec;
    uint8_t *trace_rec_buf = (uint8_t *)trace_rec;
    size_t bytes_read_total = 0;
    ssize_t bytes_read_last = 0;
    size_t bytes_left = sizeof(*trace_rec);

    // Read a total of sizeof(TraceRec) bytes from the trace file.
    while (bytes_left > 0)
    {
        bytes_read_last = read(p->trace_fd, trace_rec_buf, bytes_left);
        if (bytes_read_last <= 0)
        {
            // EOF or error
            break;
        }

        trace_rec_buf += bytes_read_last;
        bytes_read_total += bytes_read_last;
        bytes_left -= bytes_read_last;
    }

    // Check for error conditions.
    if (bytes_left > 0 || trace_rec->op_type >= NUM_OP_TYPES)
    {
        fetch_op->valid = false;
        p->halt_op_id = p->last_op_id;

        if (p->last_op_id == 0)
        {
            p->halt = true;
        }

        if (bytes_read_last == -1)
        {
            fprintf(stderr, "\n");
            perror("Couldn't read from pipe");
            return;
        }

        if (bytes_read_total == 0)
        {
            // No more trace records to read
            return;
        }

        // Too few bytes read or invalid op_type
        fprintf(stderr, "\n");
        fprintf(stderr, "Error: Invalid trace file\n");
        return;
    }

    // Got a valid trace record!
    fetch_op->valid = true;
    fetch_op->stall = false;
    fetch_op->is_mispred_cbr = false;
    fetch_op->op_id = ++p->last_op_id;
}

/**
 * Allocate and initialize a new pipeline.
 * 
 * You should not need to modify this function.
 * 
 * @param trace_fd the file descriptor from which to read trace records
 * @return a pointer to a newly allocated pipeline
 */
Pipeline *pipe_init(int trace_fd)
{
    printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);

    // Allocate pipeline.
    Pipeline *p = (Pipeline *)calloc(1, sizeof(Pipeline));

    // Initialize pipeline.
    p->trace_fd = trace_fd;
    p->halt_op_id = (uint64_t)(-1) - 3;

    // Allocate and initialize a branch predictor if needed.
    if (BPRED_POLICY != BPRED_PERFECT)
    {
        p->b_pred = new BPred(BPRED_POLICY);
    }

    return p;
}

/**
 * Print out the state of the pipeline latches for debugging purposes.
 * 
 * You may use this function to help debug your pipeline implementation, but
 * please remove calls to this function before submitting the lab.
 * 
 * @param p the pipeline
 */
void pipe_print_state(Pipeline *p)
{
    printf("\n--------------------------------------------\n");
    printf("Cycle count: %lu, retired instructions: %lu\n",
           (unsigned long)p->stat_num_cycle,
           (unsigned long)p->stat_retired_inst);

    // Print table header
    for (uint8_t latch_type = 0; latch_type < NUM_LATCH_TYPES; latch_type++)
    {
        switch (latch_type)
        {
        case IF_LATCH:
            printf(" IF:    ");
            break;
        case ID_LATCH:
            printf(" ID:    ");
            break;
        case EX_LATCH:
            printf(" EX:    ");
            break;
        case MA_LATCH:
            printf(" MA:    ");
            break;
        default:
            printf(" ------ ");
        }
    }
    printf("\n");

    // Print row for each lane in pipeline width
    for (uint8_t i = 0; i < PIPE_WIDTH; i++)
    {
        for (uint8_t latch_type = 0; latch_type < NUM_LATCH_TYPES;
             latch_type++)
        {
            if (p->pipe_latch[latch_type][i].valid)
            {
                printf(" %6lu ",
                       (unsigned long)p->pipe_latch[latch_type][i].op_id);
            }
            else
            {
                printf(" ------ ");
            }
        }
        printf("\n");
    }
    printf("\n");
}

/**
 * Simulate one cycle of all stages of a pipeline.
 * 
 * You should not need to modify this function except for debugging purposes.
 * If you add code to print debug output in this function, remove it or comment
 * it out before you submit the lab.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;

    // In hardware, all pipeline stages execute in parallel, and each pipeline
    // latch is populated at the start of the next clock cycle.

    // In our simulator, we simulate the pipeline stages one at a time in
    // reverse order, from the Write Back stage (WB) to the Fetch stage (IF).
    // We do this so that each stage can read from the latch before it and
    // write to the latch after it without needing to "double-buffer" the
    // latches.

    // Additionally, it means that earlier pipeline stages can know about
    // stalls triggered in later pipeline stages in the same cycle, as would be
    // the case with hardware stall signals asserted by combinational logic.

    //std::cout << "BEGIN" << std::endl;
    pipe_cycle_WB(p);
    pipe_cycle_MA(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_IF(p);

    // You can uncomment the following line to print out the pipeline state
    // after each clock cycle for debugging purposes.
    // Make sure you comment it out or remove it before you submit the lab.
    //pipe_print_state(p);
    /*
    for (unsigned int i = 0; i < PIPE_WIDTH; i++) {
        std::cout << "valid: " << p->pipe_latch[IF_LATCH][i].valid << " " << p->pipe_latch[ID_LATCH][i].valid << " " << p->pipe_latch[EX_LATCH][i].valid << " "<< p->pipe_latch[MA_LATCH][i].valid << std::endl;
        std::cout << "stall: " << p->pipe_latch[IF_LATCH][i].stall << " " << p->pipe_latch[ID_LATCH][i].stall << " " << p->pipe_latch[EX_LATCH][i].stall << " "<< p->pipe_latch[MA_LATCH][i].stall << std::endl;
    }
    std::cout << "END" << std::endl << std::endl; */
}

/**
 * Simulate one cycle of the Write Back stage (WB) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_WB(Pipeline *p)
{
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        if (p->pipe_latch[MA_LATCH][i].valid)
        {
            p->stat_retired_inst++;

            if (p->pipe_latch[MA_LATCH][i].op_id >= p->halt_op_id)
            {
                // Halt the pipeline if we've reached the end of the trace.
                p->halt = true;
            }
        }
    }
}

/**
 * Simulate one cycle of the Memory Access stage (MA) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_MA(Pipeline *p)
{
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        // Copy each instruction from the EX latch to the MA latch.
        p->pipe_latch[MA_LATCH][i] = p->pipe_latch[EX_LATCH][i];
        p->pipe_latch[MA_LATCH][i].valid = p->pipe_latch[EX_LATCH][i].valid;
    }
}

/**
 * Simulate one cycle of the Execute stage (EX) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_EX(Pipeline *p)
{
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        // Copy each instruction from the ID latch to the EX latch.
        p->pipe_latch[EX_LATCH][i] = p->pipe_latch[ID_LATCH][i];
        p->pipe_latch[EX_LATCH][i].valid = p->pipe_latch[ID_LATCH][i].valid;
    }
}

/**
 * Simulate one cycle of the Instruction Decode stage (ID) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_ID(Pipeline *p)
{
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        // Copy each instruction from the IF latch to the ID latch.
        p->pipe_latch[ID_LATCH][i] = p->pipe_latch[IF_LATCH][i];
        p->pipe_latch[ID_LATCH][i].stall = false;
    }

    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        //Track reasons for stalling
        bool stall_src1_ma = false;
        bool stall_src2_ma = false;
        bool stall_cc_ma = false;

        bool stall_src1_ex = false;
        bool stall_src2_ex = false;
        bool stall_cc_ex = false;

        bool stall_cc_id = false;
        bool stall_src1_id = false;
        bool stall_src2_id = false;

        for (unsigned int ii = 0; ii < PIPE_WIDTH; ii++) {
            //Check for a colission between the EX dest reg and the first src reg for ID
            if (p->pipe_latch[EX_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed && p->pipe_latch[EX_LATCH][ii].trace_rec.dest_needed && p->pipe_latch[EX_LATCH][ii].valid) {
                stall_src1_ex = true;
            }

            //Check for a colission between the EX dest reg and the second src reg for ID
            if (p->pipe_latch[EX_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed && p->pipe_latch[EX_LATCH][ii].trace_rec.dest_needed && p->pipe_latch[EX_LATCH][ii].valid) {
                stall_src2_ex = true;
            }

            //Check for a colission between the EX cc_write reg and the cc_read ID
            //if (p->pipe_latch[EX_LATCH][ii].trace_rec.cc_write == p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[EX_LATCH][ii].valid) {
            if (p->pipe_latch[EX_LATCH][ii].trace_rec.cc_write && p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[EX_LATCH][ii].valid) {
                stall_cc_ex = true;
            }

            
            //Check for a colission between the MA dest reg and the first src reg for ID
            if (p->pipe_latch[MA_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed && p->pipe_latch[MA_LATCH][ii].trace_rec.dest_needed && p->pipe_latch[MA_LATCH][ii].valid) {
                stall_src1_ma = true;
            }

            //Check for a colission between the MA dest reg and the second src reg for ID
            if (p->pipe_latch[MA_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed && p->pipe_latch[MA_LATCH][ii].trace_rec.dest_needed && p->pipe_latch[MA_LATCH][ii].valid) {
                stall_src2_ma = true;
            }

            //Check for a colission between the MA cc_write reg and the cc_read ID
            //if (p->pipe_latch[MA_LATCH][ii].trace_rec.cc_write == p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[MA_LATCH][ii].valid) {
            if (p->pipe_latch[MA_LATCH][ii].trace_rec.cc_write && p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[MA_LATCH][ii].valid) {
                stall_cc_ma = true;
            }

            if (PIPE_WIDTH > 1) {
                if (p->pipe_latch[ID_LATCH][ii].op_id < p->pipe_latch[ID_LATCH][i].op_id) {
                    if (p->pipe_latch[ID_LATCH][ii].trace_rec.cc_write && p->pipe_latch[ID_LATCH][i].trace_rec.cc_read) {
                        stall_cc_id = true;
                    }

                    if (p->pipe_latch[ID_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed && p->pipe_latch[ID_LATCH][ii].trace_rec.dest_needed ) {
                        stall_src1_id = true;
                    }

                    if (p->pipe_latch[ID_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed && p->pipe_latch[ID_LATCH][ii].trace_rec.dest_needed ) {
                        stall_src1_id = true;
                    }
                }
            }
        }       

        if (ENABLE_MEM_FWD)
        {
            // TODO: Handle forwarding from the MA stage.
            if (stall_src1_ma) {
                unsigned int youngest = 256;
                for (unsigned int j = 0; j < PIPE_WIDTH; j++) {
                    if (p->pipe_latch[MA_LATCH][j].trace_rec.dest_reg==p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg && p->pipe_latch[MA_LATCH][j].trace_rec.dest_needed && p->pipe_latch[MA_LATCH][j].valid) {
                        if (youngest==256) {
                            youngest = j;
                        } else {
                            if (p->pipe_latch[MA_LATCH][j].op_id > p->pipe_latch[MA_LATCH][youngest].op_id) {
                                youngest = j;
                            }
                        }
                    } 
                }
                if (youngest != 256) {
                    stall_src1_ma = false;
                }
            }

            if (stall_src2_ma) {
                unsigned int youngest = 256;
                for (unsigned int j = 0; j < PIPE_WIDTH; j++) {
                    if (p->pipe_latch[MA_LATCH][j].trace_rec.dest_reg==p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg && p->pipe_latch[MA_LATCH][j].trace_rec.dest_needed && p->pipe_latch[MA_LATCH][j].valid) {
                        if (youngest==256) {
                            youngest = j;
                        } else {
                            if (p->pipe_latch[MA_LATCH][j].op_id > p->pipe_latch[MA_LATCH][youngest].op_id) {
                                youngest = j;
                            }
                        }
                    } 
                }
                if (youngest != 256) {
                    stall_src2_ma = false;
                }
            }

            if (stall_cc_ma) {
                unsigned int youngest = 256;
                for (unsigned int j = 0; j < PIPE_WIDTH; j++) {
                    if (p->pipe_latch[MA_LATCH][j].trace_rec.cc_write && p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[MA_LATCH][j].valid) {
                        if (youngest==256) {
                            youngest = j;
                        } else {
                            if (p->pipe_latch[MA_LATCH][j].op_id > p->pipe_latch[MA_LATCH][youngest].op_id) {
                                youngest = j;
                            }
                        }
                    } 
                }
                if (youngest != 256) {
                    stall_cc_ma = false;
                }
            }

            /*
            for (unsigned int ii = 0; ii < PIPE_WIDTH; ii++) {
                if (stall_src1_ma) {
                    if (p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg == p->pipe_latch[MA_LATCH][ii].trace_rec.dest_reg && p->pipe_latch[MA_LATCH][ii].valid && p->pipe_latch[MA_LATCH][ii].trace_rec.dest_needed && p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed ) {
                        stall_src1_ma = false;
                    }
                }

                if (stall_src2_ma) {
                    if (p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg == p->pipe_latch[MA_LATCH][ii].trace_rec.dest_reg && p->pipe_latch[MA_LATCH][ii].valid && p->pipe_latch[MA_LATCH][ii].trace_rec.dest_needed && p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed ) {
                        stall_src2_ma = false;
                    }
                }
                
                if (stall_cc_ma) {
                    if (p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[MA_LATCH][ii].trace_rec.cc_write && p->pipe_latch[MA_LATCH][ii].valid) {
                        stall_cc_ma = false;
                    }
                }
            }*/
        }

        if (ENABLE_EXE_FWD)
        {
            // TODO: Handle forwarding from the EX stage.
            // Rishov: New implementation based on youngest instruction in EX. 
            if (stall_src1_ex) {
                unsigned int youngest = 256;
                for (unsigned int j = 0; j < PIPE_WIDTH; j++) {
                    if (p->pipe_latch[EX_LATCH][j].trace_rec.dest_reg==p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg && p->pipe_latch[EX_LATCH][j].trace_rec.dest_needed && p->pipe_latch[EX_LATCH][j].valid) {
                        if (youngest==256) {
                            youngest = j;
                        } else {
                            if (p->pipe_latch[EX_LATCH][j].op_id > p->pipe_latch[EX_LATCH][youngest].op_id) {
                                youngest = j;
                            }
                        }
                    } 
                }
                if (youngest != 256) {
                    if (p->pipe_latch[EX_LATCH][youngest].trace_rec.op_type!=OP_LD) {
                        stall_src1_ex = false;
                    }
                }
            }

            if (stall_src2_ex) {
                unsigned int youngest = 256;
                for (unsigned int j = 0; j < PIPE_WIDTH; j++) {
                    if (p->pipe_latch[EX_LATCH][j].trace_rec.dest_reg==p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg && p->pipe_latch[EX_LATCH][j].trace_rec.dest_needed && p->pipe_latch[EX_LATCH][j].valid) {
                        if (youngest==256) {
                            youngest = j;
                        } else {
                            if (p->pipe_latch[EX_LATCH][j].op_id > p->pipe_latch[EX_LATCH][youngest].op_id) {
                                youngest = j;
                            }
                        }
                    } 
                }
                if (youngest != 256) {
                    if (p->pipe_latch[EX_LATCH][youngest].trace_rec.op_type!=OP_LD) {
                        stall_src2_ex = false;
                    }
                }
            }

            if (stall_cc_ex) {
                unsigned int youngest = 256;
                for (unsigned int j = 0; j < PIPE_WIDTH; j++) {
                    if (p->pipe_latch[EX_LATCH][j].trace_rec.cc_write && p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[EX_LATCH][j].valid) {
                        if (youngest==256) {
                            youngest = j;
                        } else {
                            if (p->pipe_latch[EX_LATCH][j].op_id > p->pipe_latch[EX_LATCH][youngest].op_id) {
                                youngest = j;
                            }
                        }
                    } 
                }
                if (youngest != 256) {
                    if (p->pipe_latch[EX_LATCH][youngest].trace_rec.op_type!=OP_LD) {
                        stall_cc_ex = false;
                    }
                }
            }
            
            /*
            for (unsigned int ii = 0; ii < PIPE_WIDTH; ii++) {
                if (!p->pipe_latch[EX_LATCH][ii].trace_rec.op_type==OP_LD) {
                    if (stall_src1_ex) {
                        if (p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg == p->pipe_latch[EX_LATCH][ii].trace_rec.dest_reg && p->pipe_latch[EX_LATCH][ii].valid && p->pipe_latch[EX_LATCH][ii].trace_rec.dest_needed && p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed) {
                        //if (p->pipe_latch[EX_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed && p->pipe_latch[EX_LATCH][ii].valid) {
                            stall_src1_ex = false;
                        }
                    }

                    if (stall_src2_ex) {
                        if (p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg == p->pipe_latch[EX_LATCH][ii].trace_rec.dest_reg && p->pipe_latch[EX_LATCH][ii].valid && p->pipe_latch[EX_LATCH][ii].trace_rec.dest_needed && p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed) {
                        //if (p->pipe_latch[EX_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed && p->pipe_latch[EX_LATCH][ii].valid) {
                            stall_src2_ex = false;
                        }
                    }
                    
                    if (stall_cc_ex) {
                        if (p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[EX_LATCH][ii].trace_rec.cc_write && p->pipe_latch[EX_LATCH][ii].valid) {
                            stall_cc_ex = false;
                        }
                    }
                } else {
                    for (unsigned int j = 0; j < PIPE_WIDTH; j++) {
                        if (p->pipe_latch[EX_LATCH][j].op_id < p->pipe_latch[EX_LATCH][ii].op_id && !p->pipe_latch[EX_LATCH][j].trace_rec.op_type==OP_LD) {
                            if (stall_src1_ex && p->pipe_latch[EX_LATCH][j].trace_rec.dest_reg==p->pipe_latch[EX_LATCH][ii].trace_rec.dest_reg && p->pipe_latch[EX_LATCH][j].trace_rec.dest_needed && p->pipe_latch[EX_LATCH][j].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed && p->pipe_latch[EX_LATCH][j].valid)
                        }
                    }
                }
            }*/
        }

        if (stall_src1_ex || stall_src1_ma || stall_src2_ex || stall_src2_ma || stall_cc_ex || stall_cc_ma || stall_cc_id || stall_src1_id || stall_src2_id || !p->pipe_latch[ID_LATCH][i].op_id>1) {
            p->pipe_latch[ID_LATCH][i].valid = false;
        } else {
            p->pipe_latch[ID_LATCH][i].valid = true;
        }

        if (!p->pipe_latch[IF_LATCH][i].valid) {
            p->pipe_latch[ID_LATCH][i].valid = false;
        }
        
        /*
        if (stall_src1_ex || stall_src2_ex) {
            std::cout << "EX stall " << stall_src1_ex << " " << stall_src2_ex << std::endl;
            if (p->pipe_latch[IF_LATCH][0].op_id==4689 && i==0) {
                bool dest0 = false;
                bool dest1 = false;
                if (p->pipe_latch[ID_LATCH][0].trace_rec.src2_reg==p->pipe_latch[EX_LATCH][0].trace_rec.dest_reg) {
                    dest0 = true;
                }
                if (p->pipe_latch[ID_LATCH][0].trace_rec.src2_reg==p->pipe_latch[EX_LATCH][1].trace_rec.dest_reg) {
                    dest1 = true;
                }
                bool ld = false;
                if (p->pipe_latch[EX_LATCH][1].trace_rec.op_type==OP_LD) {
                    ld = true;
                }

                bool src2_needed0 = p->pipe_latch[ID_LATCH][0].trace_rec.src2_needed;
                bool dest_needed0 = p->pipe_latch[EX_LATCH][0].trace_rec.dest_needed;
                bool src2_needed1 = p->pipe_latch[ID_LATCH][1].trace_rec.src2_needed;
                bool dest_needed1 = p->pipe_latch[EX_LATCH][1].trace_rec.dest_needed;

                bool younger = false;
                if (p->pipe_latch[EX_LATCH][1].op_id > p->pipe_latch[EX_LATCH][0].op_id) {
                    younger = true;
                }
                std::cout << "src2: " << stall_src2_ex << " " << dest0 << " " << src2_needed0 << " " << dest_needed0 << " " << dest1 << " " << src2_needed1 << " " << dest_needed1 << " " << ld << " " << younger << std::endl;
            }
            //std::cout << p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg << " " << p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg << " " << p->pipe_latch[EX_LATCH][i].trace_rec.dest_reg <<std::endl;
        }

        bool ex_pipe0 = p->pipe_latch[EX_LATCH][0].trace_rec.cc_write;
        bool ex_pipe1 = p->pipe_latch[EX_LATCH][1].trace_rec.cc_write;

        bool ma_pipe0 = p->pipe_latch[MA_LATCH][0].trace_rec.cc_write;
        bool ma_pipe1 = p->pipe_latch[MA_LATCH][1].trace_rec.cc_write;

        if (stall_cc_ex) {
            if (PIPE_WIDTH > 1) {
                std::cout << "EX stall CC " << p->pipe_latch[ID_LATCH][i].op_id <<  " " << ex_pipe0 << " " << ex_pipe1 << std::endl;
            } else {
                std::cout << "EX stall CC "  << p->pipe_latch[ID_LATCH][i].op_id << std::endl;
            }
            //std::cout << p->pipe_latch[ID_LATCH][i].trace_rec.cc_read << " " << p->pipe_latch[EX_LATCH][i].trace_rec.cc_write << std::endl;
        }

        if (stall_src1_ma || stall_src2_ma) {
            std::cout << "MA stall"  << stall_src1_ma << " " << stall_src2_ma << std::endl;
            //std::cout << p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg << " " << p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg << " " << p->pipe_latch[MA_LATCH][i].trace_rec.dest_reg <<std::endl;
        }

        if (stall_cc_ma) {
            if (PIPE_WIDTH > 1) {
                std::cout << "MA stall CC " << p->pipe_latch[ID_LATCH][i].op_id <<  " " << ma_pipe0 << " " << ma_pipe1 << std::endl;
            } else {
                std::cout << "MA stall CC " << p->pipe_latch[ID_LATCH][i].op_id <<  std::endl;
            }
            
            //std::cout << p->pipe_latch[ID_LATCH][i].trace_rec.cc_read << " " << p->pipe_latch[MA_LATCH][i].trace_rec.cc_write << std::endl;
        }*/

        /*
        if (p->pipe_latch[ID_LATCH][i].trace_rec.op_type==OP_CBR ) {
            std::cout << "PIPE " << i << " ID OP_ID: " << p->pipe_latch[ID_LATCH][i].op_id << " " << "BRANCH" <<  " " << stall_src1_ex << " " << stall_src2_ex << " " << stall_cc_ex << " " << stall_src1_ma << " " << stall_src2_ma << " " << stall_cc_ma << " " << p->pipe_latch[ID_LATCH][i].valid << std::endl;
        }*/
    }
    /*
    for (unsigned int i = 0; i < PIPE_WIDTH; i++) { 
        //After all data stalls have been set, we can check to see if there are age related stalls that need to be implemented
        for (unsigned int ii = 0; ii < PIPE_WIDTH; ii++) {
            if (i != ii) {
                if(p->pipe_latch[ID_LATCH][i].op_id < p->pipe_latch[ID_LATCH][ii].op_id && p->pipe_latch[ID_LATCH][ii].stall) {
                    //p->pipe_latch[ID_LATCH][i].stall = true;
                    p->pipe_latch[ID_LATCH][i].valid = false;
                }
            }
        }
    }*/
}

/**
 * Simulate one cycle of the Instruction Fetch stage (IF) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_IF(Pipeline *p)
{
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        if (!p->pipe_latch[ID_LATCH][i].valid && !p->pipe_latch[EX_LATCH][i].valid && !p->pipe_latch[MA_LATCH][i].valid && p->pipe_latch[IF_LATCH][i].op_id < PIPE_WIDTH) {
        //if (!p->pipe_latch[ID_LATCH][i].valid && !p->pipe_latch[EX_LATCH][i].valid) {
            p->pipe_latch[IF_LATCH][i].stall = false;
        } else if (!p->pipe_latch[ID_LATCH][i].valid) {
            p->pipe_latch[IF_LATCH][i].stall = true;
        } else {
            p->pipe_latch[IF_LATCH][i].stall = false;
        }

        if (PIPE_WIDTH > 1) {
            for (unsigned int j = 0; j < PIPE_WIDTH; j++) {
                if (!p->pipe_latch[ID_LATCH][j].valid && p->pipe_latch[IF_LATCH][j].op_id < p->pipe_latch[IF_LATCH][i].op_id) {
                    p->pipe_latch[IF_LATCH][i].stall = true;
                    p->pipe_latch[ID_LATCH][i].valid = false;
                    //std::cout << "IF valid check " << p->pipe_latch[IF_LATCH][i].op_id << " " << j << " " << i << " " << p->pipe_latch[ID_LATCH][j].valid << " " << (p->pipe_latch[IF_LATCH][j].op_id < p->pipe_latch[IF_LATCH][i].op_id) << std::endl;
                }
            }
        }

        /*bool ma_cc = false;
        bool ex_cc = false;
        bool if_cc = false;

        if (p->pipe_latch[MA_LATCH][i].trace_rec.cc_write && p->pipe_latch[MA_LATCH][i].valid ) {
            ma_cc = true;
        }
        if (p->pipe_latch[EX_LATCH][i].trace_rec.cc_write && p->pipe_latch[EX_LATCH][i].valid ) {
            ex_cc = true;
        }
        if (p->pipe_latch[IF_LATCH][i].trace_rec.cc_read ) {
            if_cc = true;
        }*/

        /*
        if (p->pipe_latch[IF_LATCH][i].trace_rec.op_type==OP_LD ) {
            std::cout <<  "PIPE " << i << " OP_ID: " << p->pipe_latch[IF_LATCH][i].op_id << " " << "LOAD" <<  " " << if_cc << " " << ex_cc << " " << ma_cc << std::endl;
        } else if (p->pipe_latch[IF_LATCH][i].trace_rec.op_type==OP_ST ) {
            std::cout <<  "PIPE " << i << " OP_ID: " << p->pipe_latch[IF_LATCH][i].op_id << " " << "STORE" <<  " " << if_cc << " " << ex_cc << " " << ma_cc << std::endl;
        } else if (p->pipe_latch[IF_LATCH][i].trace_rec.op_type==OP_ALU ) {
            std::cout <<  "PIPE " << i << " OP_ID: " << p->pipe_latch[IF_LATCH][i].op_id << " " << "ALU" <<  " " << if_cc << " " << ex_cc << " " << ma_cc << std::endl;
        } else if (p->pipe_latch[IF_LATCH][i].trace_rec.op_type==OP_CBR ) {
            std::cout << "PIPE " << i << " OP_ID: " << p->pipe_latch[IF_LATCH][i].op_id << " " << "BRANCH" <<  " " << if_cc << " " << ex_cc << " " << ma_cc << std::endl;
        } else if (p->pipe_latch[IF_LATCH][i].trace_rec.op_type==OP_OTHER ) {
            std::cout <<  "PIPE " << i << " OP_ID: " << p->pipe_latch[IF_LATCH][i].op_id << " " << "OTHER" <<  " " << if_cc << " " << ex_cc << " " << ma_cc << std::endl;
        }*/

        //std::cout <<  "IF " << p->pipe_latch[ID_LATCH][i].valid << std::endl;

        if (!p->pipe_latch[IF_LATCH][i].stall) {
            // Read an instruction from the trace file.
            PipelineLatch fetch_op;
            pipe_get_fetch_op(p, &fetch_op);

            // Handle branch (mis)prediction.
            if (BPRED_POLICY != BPRED_PERFECT)
            {
                pipe_check_bpred(p, &fetch_op);
            }

            // Copy the instruction to the IF latch.
            p->pipe_latch[IF_LATCH][i] = fetch_op;

            if (p->pipe_latch[IF_LATCH][i].op_id == 1) {
                p->pipe_latch[ID_LATCH][i].valid = false;
                //std::cout <<  "IF valid 2" << std::endl;
            }
        }

        //std::cout <<  "PIPE " << i << "IF end" << p->pipe_latch[ID_LATCH][i].valid << std::endl;
    }

    /*
    for (unsigned int i = 0; i < PIPE_WIDTH; i++) {
        std::cout <<  "PIPE " << i << "IF independent look" << p->pipe_latch[ID_LATCH][i].valid << std::endl;
    }*/
}

/**
 * If the instruction just fetched is a conditional branch, check for a branch
 * misprediction, update the branch predictor, and set appropriate flags in the
 * pipeline.
 * 
 * You must implement this function in part B of the lab.
 * 
 * @param p the pipeline
 * @param fetch_op the pipeline latch containing the operation fetched
 */
void pipe_check_bpred(Pipeline *p, PipelineLatch *fetch_op)
{
    // TODO: For a conditional branch instruction, get a prediction from the
    // branch predictor.

    // TODO: If the branch predictor mispredicted, mark the fetch_op
    // accordingly.

    // TODO: Immediately update the branch predictor.

    // TODO: If needed, stall the IF stage by setting the flag
    // p->fetch_cbr_stall.
}
