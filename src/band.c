#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "band.h"
#include "chs.h"

void init_band(struct Band *band, unsigned long total_bands) {
    //SMRBandCount = 0;
    //CMRBandCount = 0;
    //size_t smrPercentage = 80; //80%
    //unsigned long smr_initCount = (total_bands * smrPercentage) / 100;;
    unsigned long b_size = 0;
    size_t i;
    for(i = 0; i < total_bands; i++) {
        band[i].write_head_p = b_size;

        band[i].flagCMR = false;

        band[i].flagCACHE = false;
        //band[i].flagSMR = false;
        /*
        if(i < smr_initCount) {
            band[i].flagCMR = false;
        }
        else {
            band[i].flagCMR = true;
        }
        */

        band[i].flagRMW = false;
        
        band[i].band_head = b_size;
        if(i > 0) {
            band[i-1].band_end = b_size - TRACK_SIZE;
            band[i-1].band_CMR_end = band[i-1].band_end - (BAND_BLOCK_COUNT - BAND_BLOCK_COUNT_CMR)*TRACK_SIZE;
        }
        else if(i == total_bands-1) {
            b_size += BAND_SIZE;
            band[i].band_end = b_size - TRACK_SIZE;
            band[i-1].band_CMR_end = band[i-1].band_end - (BAND_BLOCK_COUNT - BAND_BLOCK_COUNT_CMR)*TRACK_SIZE;
        }
        
        b_size += BAND_SIZE;

        //band[i].inUsed = false;
    }
}

void methodSwitch(struct Band *band, size_t target, unsigned long total_bands) {
    band[target].flagCMR = !band[target].flagCMR;
}