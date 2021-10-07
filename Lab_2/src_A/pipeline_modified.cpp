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

    pipe_cycle_WB(p);
    pipe_cycle_MA(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_IF(p);

    // You can uncomment the following line to print out the pipeline state
    // after each clock cycle for debugging purposes.
    // Make sure you comment it out or remove it before you submit the lab.
    //pipe_print_state(p);
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

        //Check for validity of copied data
        if (!p->pipe_latch[EX_LATCH][i].valid) {
            p->pipe_latch[MA_LATCH][i].valid = false;
        }
        else {
            p->pipe_latch[MA_LATCH][i].valid = true;
        }
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

        //If the prior latch has a stall condition set, then this data is no longer valid since we need a bubble. 
        if (p->pipe_latch[ID_LATCH][i].stall) {
            p->pipe_latch[EX_LATCH][i].valid = false;
        }
        else {
            p->pipe_latch[EX_LATCH][i].valid = true;
        }
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

        //Added: conditional. Only want to move the pipeline forward if we didn't find a hazard that requires a stall. 
        //MOVED to the top based on feedback in Piazza 45_f1
        if (!p->pipe_latch[ID_LATCH][i].stall) {
            // Copy each instruction from the IF latch to the ID latch.
            p->pipe_latch[ID_LATCH][i] = p->pipe_latch[IF_LATCH][i];
        }

        //Always start a cycle assuming that you've cleared all stalls ahead of you and look for a reason to stall again. 
        //UPDATE: Since I'm moving data at the beginning, keep prior stall state. 
        //p->pipe_latch[ID_LATCH][i].stall = false;

        //Track reasons for stalling
        bool stall_src1_ma = false;
        bool stall_src2_ma = false;
        bool stall_cc_ma = false;

        bool stall_src1_ex = false;
        bool stall_src2_ex = false;
        bool stall_cc_ex = false;

        bool stall_age = false;

        //To accomodate super scalar implementation, iterate through each of the pipes for hazards.
        for (unsigned int ii = 0; ii < PIPE_WIDTH; ii++) {
            //Check for a colission between the EX dest reg and the first src reg for ID
            if (p->pipe_latch[EX_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed && p->pipe_latch[EX_LATCH][i].valid) {
                p->pipe_latch[ID_LATCH][i].stall = true;
                stall_src1_ex = true;
            }

            //Check for a colission between the EX dest reg and the second src reg for ID
            if (p->pipe_latch[EX_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed && p->pipe_latch[EX_LATCH][i].valid) {
                p->pipe_latch[ID_LATCH][i].stall = true;
                stall_src2_ex = true;
            }

            //Check for a colission between the EX cc_write reg and the cc_read ID
            if (p->pipe_latch[EX_LATCH][ii].trace_rec.cc_write == p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[EX_LATCH][i].valid) {
                p->pipe_latch[ID_LATCH][i].stall = true;
                stall_cc_ex = true;
            }

            //Check for a colission between the MA dest reg and the first src reg for ID
            if (p->pipe_latch[MA_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed && p->pipe_latch[MA_LATCH][i].valid) {
                p->pipe_latch[ID_LATCH][i].stall = true;
                stall_src1_ma = true;
            }

            //Check for a colission between the MA dest reg and the second src reg for ID
            if (p->pipe_latch[MA_LATCH][ii].trace_rec.dest_reg == p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg && p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed && p->pipe_latch[MA_LATCH][i].valid) {
                p->pipe_latch[ID_LATCH][i].stall = true;
                stall_src2_ma = true;
            }

            //Check for a colission between the MA cc_write reg and the cc_read ID
            if (p->pipe_latch[MA_LATCH][ii].trace_rec.cc_write == p->pipe_latch[ID_LATCH][i].trace_rec.cc_read && p->pipe_latch[MA_LATCH][i].valid) {
                p->pipe_latch[ID_LATCH][i].stall = true;
                stall_cc_ma = true;
            }
        }

        //After all data stalls have been set, we can check to see if there are age related stalls that need to be implemented
        for (unsigned int ii = 0; ii < PIPE_WIDTH; ii++) {
            if (i != ii) {
                if(p->pipe_latch[ID_LATCH][i].op_id < p->pipe_latch[ID_LATCH][ii].op_id && p->pipe_latch[ID_LATCH][ii].stall) {
                    p->pipe_latch[ID_LATCH][i].stall = true;
                    stall_age = true;
                }
            }
        }

        if (ENABLE_MEM_FWD)
        {
            // TODO: Handle forwarding from the MA stage.
            for (unsigned int ii = 0; ii < PIPE_WIDTH; ii++) {
                if (stall_src1_ma) {
                    if (p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg == p->pipe_latch[MA_LATCH][i].trace_rec.dest_reg) {
                        stall_src1_ma = false;
                    }
                }

                if (stall_src2_ma) {
                    if (p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg == p->pipe_latch[MA_LATCH][i].trace_rec.dest_reg) {
                        stall_src2_ma = false;
                    }
                }
                
                if (stall_cc_ma) {
                    if (p->pipe_latch[ID_LATCH][i].trace_rec.cc_read == p->pipe_latch[MA_LATCH][i].trace_rec.cc_write) {
                        stall_cc_ma = false;
                    }
                }
            }
        }

        if (ENABLE_EXE_FWD)
        {
            // TODO: Handle forwarding from the EX stage.
            for (unsigned int ii = 0; ii < PIPE_WIDTH; ii++) {
                if (!p->pipe_latch[EX_LATCH][i].trace_rec.op_type==OP_LD) {
                    if (stall_src1_ex) {
                        if (p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg == p->pipe_latch[EX_LATCH][i].trace_rec.dest_reg && p->pipe_latch[EX_LATCH][i].valid) {
                            stall_src1_ex = false;
                        }
                    }

                    if (stall_src2_ma) {
                        if (p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg == p->pipe_latch[EX_LATCH][i].trace_rec.dest_reg && p->pipe_latch[EX_LATCH][i].valid) {
                            stall_src2_ex = false;
                        }
                    }
                    
                    if (stall_cc_ma) {
                        if (p->pipe_latch[ID_LATCH][i].trace_rec.cc_read == p->pipe_latch[EX_LATCH][i].trace_rec.cc_write && p->pipe_latch[EX_LATCH][i].valid) {
                            stall_cc_ex = false;
                        }
                    }
                }
            }
        }

        if (!stall_src1_ex && !stall_src1_ma && !stall_src2_ex && !stall_src2_ma && !stall_cc_ex && !stall_cc_ma && !stall_age) {
            p->pipe_latch[ID_LATCH][i].stall = false;
        } else {
            p->pipe_latch[ID_LATCH][i].stall = true;
        }

        //If we end the entire logical sequence here with a stall, then set the valid bit on the next latch as false to create a bubble
        //Since I'm moving stage movement to the beginning of the cycle, I can do this on the next cycle in the EX stage. 
        //if (p->pipe_latch[ID_LATCH][i].stall) {
        //    p->pipe_latch[EX_LATCH][i].valid = false;
        //}
        //else {
        //    p->pipe_latch[EX_LATCH][i].valid = true;
        //}

        //ORIGINAL place I moved data from IF to ID. 
    }
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
        //Added: If we are stalled, but the ID latch ahead of you isn't, you want to unstall and start the pipeline moving again.
        //QUESTIONABLE
        if (p->pipe_latch[IF_LATCH][i].stall) {
            if (!p->pipe_latch[ID_LATCH][i].stall) {
                p->pipe_latch[IF_LATCH][i].stall = false;
            }
        }

        //Added: conditional. Only want to read in a new instruction if the pipeline isn't stalled.
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
        }

        //Added: If the ID latch is stalled ahead of you, you want to stall too. 
        if (p->pipe_latch[ID_LATCH][i].stall) {
            p->pipe_latch[IF_LATCH][i].stall = true;
        }

    }
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