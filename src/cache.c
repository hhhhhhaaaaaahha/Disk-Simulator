#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"

void init_cache_buffer(struct Cache *cache_buffer, unsigned long buffer_size) {
    for (size_t i = 0; i < buffer_size; i++) {
        cache_buffer[i].inUsed = false;
        //cache_buffer[i].diskAddress = 
    }
}