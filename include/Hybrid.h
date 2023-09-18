#pragma once
#include "lba.h"

/* 256 Tracks */
#define HYBRID_ZONE_SIZE (TRACK_SIZE << 8)
#define HYBRID_ZONE_COUNT(T) (T >> 8)
#define HYBRID_ZONE_TRACK_COUNT 256UL
#define HYBRID_ZONE_CMR_TRACK_COUNT 170UL
#define HYBRID_DENSITY_GAIN 1.5
#define JOURNALING_PREDICT_REQ_NUM 1000UL
#define JOURNALING_PERCENTAGE 10UL

/* Journaling */
unsigned long hybrid_journaling_percent;
unsigned long hybrid_journaling_zone_limit;
unsigned long hybrid_journaling_zone_count;
unsigned long hybrid_journaling_block_remain;
// unsigned long hybrid_journaling_cmr_count;
// unsigned long hybrid_journaling_smr_count;
unsigned long hybrid_journaling_hotness_bound;

/* Data Storage */
unsigned long hybrid_data_usage;
unsigned long hybrid_used_zone_count;
unsigned long hybrid_block_remain;
unsigned long hybrid_cmr_count;
unsigned long hybrid_smr_count;
unsigned long hybrid_hotness_bound;

/* Journaling */
unsigned long *size_record;
unsigned long record_pointer;
unsigned long size_summation;
unsigned long commit_count;
bool commit_ammount_not_enough;

struct HYBRID_BLOCK
{
    bool isSEALED;
    unsigned long fileID;
    unsigned long original_lba;
};

struct HYBRID_ZONE
{
    bool isOffline;
    bool isFull;
    bool isJournaling;
    int zone_type;
    unsigned long logical_disk_LBA;
    unsigned long write_head_pointer;
    struct HYBRID_BLOCK blocks[HYBRID_ZONE_TRACK_COUNT];
};

struct HYBRID_ZONE *hybrid_zone;

int determine_commit_type(unsigned long blocks);
int do_checkpoint();