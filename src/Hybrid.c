#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "band.h"
#include "chs.h"
#include "lba.h"
#include "Hybrid.h"

double hybrid_calculate_usage(struct disk *disk)
{
    struct report *report = &disk->report;
    unsigned long usage = 0;
    for (int block = 0; block < report->max_track_num; block++)
    {
        if (disk->storage[block].status == status_in_use)
        {
            usage++;
        }
    }
    return (double)usage / report->max_track_num;
}

void do_checkpoint(struct disk *disk)
{
    for (int zone = 0; zone < journaling_zone_limit; zone++)
    {
        for (int block = 0; block < HYBRID_ZONE_TRACK_COUNT; block++)
        {
            unsigned long lba = hybrid_zone[zone].blocks[block].original_lba;
            unsigned long fid = hybrid_zone[zone].blocks[block].fileID;
            disk->d_op->read(disk, lba, 1, fid);
            disk->d_op->write(disk, lba, 1, fid);
        }
        hybrid_zone[zone].isOffline = true;
        hybrid_zone[zone].write_head_pointer = 0UL;
        hybrid_zone[zone].isFull = false;
        hybrid_zone[zone].isJournaling = false;
        journaling_zone_count--;
    }
}

int determine_commit_type(unsigned long blocks)
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
        hotness_bound = size_summation / 1000;
        size_summation = size_summation - size_record[record_pointer] + blocks;
        size_record[record_pointer] = blocks;
        record_pointer++;
    }

    return blocks >= hotness_bound;
}