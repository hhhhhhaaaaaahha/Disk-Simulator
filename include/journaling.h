#pragma once
#include "lba.h"
#include "band.h"

unsigned long jareaZoneCount;
unsigned long jareaSMRCount;
unsigned long jareaCMRCount;
unsigned long jareaSize;
struct Jarea {
    bool inUsed;
    unsigned long fid;
    unsigned long insDic;
    unsigned long address;
};
struct Jarea *jarea;

struct jHybrid_Block {
    bool inUsed;
    unsigned long reqNum;
    unsigned long fid;
};
//struct jHybrid_Block *jH_B;

struct jHybrid_Zone {
    bool inUsed;
    bool flagCMR;
    unsigned long max_blocks;
    struct jHybrid_Block jH_B[BAND_BLOCK_COUNT];
};
struct jHybrid_Zone *jH_Z;

struct jHybrid {
    bool inUsed;
    unsigned long reqNum;
    unsigned long fid;
};
struct jHybrid *jHybrid_Area;

void init_jarea(struct Jarea *jarea, unsigned long jareaSize);

void init_jarea_hybrid(struct jHybrid_Zone *jH_Z, unsigned long jareaSize, unsigned long jareaZoneCount);

void init_jHybrid(struct jHybrid *jHybrid_Area, unsigned long jareaSize);