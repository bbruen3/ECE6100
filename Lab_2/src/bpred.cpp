// --------------------------------------------------------------------- //
// For part B, you will need to modify this file.                        //
// You may add any code you need, as long as you correctly implement the //
// three required BPred methods already listed in this file.             //
// --------------------------------------------------------------------- //

// bpred.cpp
// Implements the branch predictor class.

#include "bpred.h"
#include <algorithm>
#include <iostream>
#include <bitset>
/**
 * Construct a branch predictor with the given policy.
 * 
 * In part B of the lab, you must implement this constructor.
 * 
 * @param policy the policy this branch predictor should use
 */
BPred::BPred(BPredPolicy policy_)
{
    // TODO: Initialize member variables here.
    this->policy=policy_;
    // As a reminder, you can declare any additional member variables you need
    // in the BPred class in bpred.h and initialize them here.
    this->stat_num_branches = 0;
    this->stat_num_mispred = 0;
    /*for (unsigned int i = 0; i < 12; i++) {
        this->ghr[i] = 0;
    }*/
    std::fill_n(this->ghr, 12, 0);
    /*for (unsigned int i = 0; i < 4096; i++) {
        this->pht[i] = 2;
    }*/
    std::fill_n(this->pht, 4096, 2);
}

/**
 * Get a prediction for the branch with the given address.
 * 
 * In part B of the lab, you must implement this method.
 * 
 * @param pc the address (program counter) of the branch to predict
 * @return the prediction for whether the branch is taken or not taken
 */
BranchDirection BPred::predict(uint64_t pc)
{
    BranchDirection dir = TAKEN;
    // TODO: Return a prediction for whether the branch at address pc will be
    // TAKEN or NOT_TAKEN according to this branch predictor's policy.

    // Note that you do not have to handle the BPRED_PERFECT policy here; this
    // function will not be called for that policy.
    bool debug = false; 
    if (this->policy==BPRED_GSHARE) {
        //TODO: Create XOR logic implementing GHR vs last 12 bits of address
        //std::cout << pc << std::endl;
        //std::cout << "Full PC: " << std::bitset<64>(pc) << std::endl; 
        //std::cout << "Lower 12 bits " << std::bitset<12>(pc) << std::endl; 
        std::bitset<12> lower_range = std::bitset<12>(pc);
        std::bitset<12> ghr;
        for (unsigned int i = 0; i < 12; i++) {
            ghr[i] = this->ghr[i];
        }
        std::bitset<12> index; 
        for (unsigned int i = 0; i < 12; i++) {
            unsigned int bit = 0;
            if ((lower_range[i]==1 && ghr[i]==0) || (lower_range[i]==0 && ghr[i]==1)) {
                bit = 1;
            }
            index[i] = bit;
        }

        

        //int index_int = (int)(index.to_ulong());
        int index_int = (2048*index[0]) + (1024*index[1]) + (512*index[2]) + (256*index[3]) + (128*index[4]) + (64*index[5]) + (32*index[6]) + (16*index[7]) + (8*index[8]) + (4*index[9]) + (2*index[10]) + index[11];
        
        if (debug) {
            std::cout << "IN PREDICT" << std::endl; 
            std::cout << lower_range << std::endl; 
            std::cout << ghr << std::endl;
            std::cout << index << std::endl;
            std::cout << index_int << std::endl;
        }
        //std::cout << "index: " << index_int << std::endl; 
        int prediction = pht[index_int];
        //std::cout << prediction << std::endl; 
        if (prediction < 2) {
            dir = NOT_TAKEN;
        }
        //ggstd::cout << dir << std::endl;
    }

    return dir; // This is just a placeholder.
}


/**
 * Update the branch predictor statistics (stat_num_branches and
 * stat_num_mispred), as well as any other internal state you may need to
 * update in the branch predictor.
 * 
 * In part B of the lab, you must implement this method.
 * 
 * @param pc the address (program counter) of the branch
 * @param prediction the prediction made by the branch predictor
 * @param resolution the actual outcome of the branch
 */
void BPred::update(uint64_t pc, BranchDirection prediction,
                   BranchDirection resolution)
{
    // TODO: Update the stat_num_branches and stat_num_mispred member variables
    // according to the prediction and resolution of the branch.
    bool debug = false;
    this->stat_num_branches += 1;

    if (prediction != resolution) {
        this->stat_num_mispred += 1;
    }

    // TODO: Update any other internal state you may need to keep track of.
    if (this->policy==BPRED_GSHARE) {
        //TODO: Update PHT
        std::bitset<12> lower_range = std::bitset<12>(pc);
        std::bitset<12> ghr;
        for (unsigned int i = 0; i < 12; i++) {
            ghr[i] = this->ghr[i];
        }
        //std::cout << lower_range << " " << ghr << " " << lower_range[1] << std::endl; 
        std::bitset<12> index; 
        for (unsigned int i = 0; i < 12; i++) {
            unsigned int bit = 0;
            if ((lower_range[i]==1 && ghr[i]==0) || (lower_range[i]==0 && ghr[i]==1)) {
                bit = 1;
            }
            index[i] = bit;
        }

        int index_int = (int)(index.to_ulong());
        int self_calc = (2048*index[0]) + (1024*index[1]) + (512*index[2]) + (256*index[3]) + (128*index[4]) + (64*index[5]) + (32*index[6]) + (16*index[7]) + (8*index[8]) + (4*index[9]) + (2*index[10]) + index[11];
        int pred_orig = pht[index_int];
        int pred = pht[self_calc];
        if (debug) {
            std::cout << "IN UPDATE" << std::endl;
            std::cout << lower_range << std::endl; 
            std::cout << ghr << " " << ghr[0] << std::endl;
            std::cout << index << std::endl;
            std::cout << index_int << " " << self_calc << std::endl;
            std::cout << pred_orig << " " << pred << std::endl;
        }

        
        
        bool compare = false;
        if (resolution==TAKEN) {
            compare = true; 
        }
        if (debug) {
            std::cout << "index: " << self_calc << std::endl; 
            std::cout <<"PREDICTION: " << pred << std::endl; 
            std::cout << "RESOLUTION " << resolution << " " << compare << std::endl;
        }
         
        if (resolution==TAKEN) { 
            if (this->pht[self_calc] < 3 ) {
                this->pht[self_calc] += 1;
                if (debug) {
                    std::cout << "PHT increased" << std::endl; 
                }
                
            }
        } else {
            if (this->pht[self_calc] > 0) {
                this->pht[self_calc] -= 1;
                if (debug) {
                    std::cout << "PHT decreased" << std::endl;
                }
            }
        }
        pred = pht[self_calc];
        if (debug) {
            std::cout <<"UPDATED: " << pred << std::endl;
        }


        //TODO: Update GHR
        for (unsigned int i=11; i > 0; i--) {
        //for (unsigned int i=0; i < 11; i++) {
            //this->ghr[i] = this->ghr[i+1];
            this->ghr[i] = this->ghr[i-1];
        }

        if (resolution==TAKEN) {
            this->ghr[0] = 1;
        } else {
            this->ghr[0] = 0;
        }
    }
    

    // Note that you do not have to handle the BPRED_PERFECT policy here; this
    // function will not be called for that policy.
}
