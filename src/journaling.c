#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "journaling.h"

void init_jarea(struct Jarea *jarea, unsigned long jareaSize) {
    for(size_t i = 0; i < jareaSize; i++) {
        jarea[i].inUsed = false;
    }
}

void init_jarea_hybrid(struct jHybrid_Zone *jH_Z, unsigned long jareaSize, unsigned long jareaZoneCount) {
    for(size_t i = 0; i < jareaZoneCount; i++) {
        jH_Z[i].inUsed = false;
        jH_Z[i].flagCMR = false;
        jH_Z[i].max_blocks = jareaSize/jareaZoneCount;
        
        for(size_t j = 0; j < jH_Z[i].max_blocks; j++) {
            jH_Z[i].jH_B[j].inUsed = false;
        }
    }
}

void init_jHybrid(struct jHybrid *jHybrid_Area, unsigned long jareaSize) {
    for(size_t i = 0; i < jareaSize; i++) {
        jHybrid_Area[i].inUsed = false;
    }
}