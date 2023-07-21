#pragma once
#include "lba.h"

/* 256 Tracks */
#define FLUID_ZONE_SIZE (TRACK_SIZE << 8)
#define FLUID_ZONE_COUNT(T) (T >> 8)
#define FLUID_ZONE_TRACK_COUNT 256UL
#define FLUID_ZONE_CMR_TRACK_COUNT 170UL
#define FLUID_DENSITY_GAIN 1.5

unsigned long data_usage;

unsigned long used_zone_count;
unsigned long cmr_boundary;
unsigned long smr_boundary;
unsigned long block_remain;

unsigned long cmr_count;
unsigned long smr_count;

struct EXTENDABLE_SPACE_BLOCK {
    bool isSEALED;
    unsigned long fileID;
    unsigned long original_lba;
};

struct EXTENDABLE_SPACE_ZONE {
    bool isUsed;
    bool isFull;
    int zone_type;
    unsigned long NSW_amount;
    unsigned long logical_disk_LBA;
    unsigned long write_head_pointer;
    struct EXTENDABLE_SPACE_BLOCK blocks[FLUID_ZONE_TRACK_COUNT];
    bool isCache;
};

struct EXTENDABLE_SPACE_ZONE *extendable_zone;

//unsigned long min_zone_num; //pure cmr
//unsigned long MAX_zone_num; //pure smr
/*
unsigned long fluid_cmr_write_pointer;
unsigned long fluid_smr_write_pointer;

struct FLUID_BLOCK {
    bool isSEALED;
    unsigned long fileID;
    unsigned long original_lba;
};

struct FLUID_ZONE {
    bool isCMR;
    unsigned long NSW_Range;
    struct FLUID_BLOCK block[FLUID_ZONE_TRACK_COUNT];
};

struct FLUID_ZONE *fluid_zone_mapping_table;
*/
/*
struct FLUID_ZONE_MAPPING_TABLE {
    unsigned long fileID;
    unsigned long n_length;
    unsigned long original_lba;
    unsigned long modify_lba;
};
*/
/*
struct FLUID_ZONE {
    unsigned long zone_type;

};
*/
/*
struct FLUID_ZONE_MAPPING_TABLE *cmr_space;
struct FLUID_ZONE_MAPPING_TABLE *smr_space;

struct FLUID_QUEUE {
    unsigned long band_num;
    unsigned long NSW;
};

struct FLUID_QUEUE* q_cmr;
struct FLUID_QUEUE* q_smr;
*/
//void init_fluid_zone(struct FLUID_ZONE *band, unsigned long total_bands);