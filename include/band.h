#pragma once

#include "lba.h"

/* 128 Tracks */
//#define BAND_SIZE (TRACK_SIZE << 7)
//#define band_count(t) (t >> 7)
//#define band_track_count 128UL

/* 256 Tracks */
#define BAND_SIZE (TRACK_SIZE << 8)
#define band_count(t) (t >> 8)
#define band_track_count 256UL
#define BAND_BLOCK_COUNT (BAND_SIZE/BLOCK_SIZE)
#define BAND_BLOCK_COUNT_CMR 170UL

unsigned long total_bands;
unsigned long t_num;
//unsigned long disk_free_SMRBandCount;
//unsigned long disk_free_CMRBandCount;
//unsigned long disk_FreeBandCount;

struct Band {
    unsigned long write_head_p;
    unsigned long band_head;
    unsigned long band_end;

    unsigned long band_CMR_end;
    
    bool flagRMW;
    
    unsigned long RMW_start;
    unsigned long RMW_end;
    
    bool flagCMR;
    bool flagCACHE;
    //unsigned long max_Blocks;

    //bool inUsed;
};
struct Band *band;

void init_band(struct Band *band, unsigned long total_bands);
void methodSwitch(struct Band *band, size_t target, unsigned long total_bands);