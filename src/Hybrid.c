#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Hybrid.h"
#include "chs.h"

int do_checkpoint() { return 0; }
int define_commit_type(unsigned long blocks)
{
    if (commit_ammount_not_enough)
    {

        size_record[record_pointer] = blocks;
        size_summation += blocks;
        record_pointer++;
        if (record_pointer == JOURNALING_PREDICT_REQ_NUM)
        {
            commit_ammount_not_enough = false;
        }
    }
    else
    {
        if (record_pointer == JOURNALING_PREDICT_REQ_NUM)
        {
            record_pointer = 0;
        }
        hybrid_journaling_hotness_bound = size_summation / 1000;
        size_summation = size_summation - size_record[record_pointer] + blocks;
        size_record[record_pointer] = blocks;
        record_pointer++;
    }

    return blocks >= hybrid_journaling_hotness_bound;
}