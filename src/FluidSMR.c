#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FluidSMR.h"
#include "chs.h"

/*
void init_fluid_zone(struct FLUID_ZONE *fluid_zone, unsigned long Max_zone_num) {
    unsigned long zone_size = 0;
    size_t i;
    for (i = 0; i < Max_zone_num; i++) {
        fluid_zone[i].zone_mode = 0;
        fluid_zone[i].write_head_p = zone_size;

        fluid_zone[i].flagRMW = false;
        
        fluid_zone[i].zone_head = zone_size;
        
        if (i > 0) {
            fluid_zone[i-1].zone_end = zone_size - TRACK_SIZE;
        }
        else if (i == Max_zone_num-1) {
            zone_size += FLUID_ZONE_SIZE;
            fluid_zone[i].zone_end = zone_size - TRACK_SIZE;
        }
        
        zone_size += FLUID_ZONE_SIZE;
    }
}
*/
/*
void init_fluid_zone(struct FLUID_ZONE *fluid_zone) {

}
*/