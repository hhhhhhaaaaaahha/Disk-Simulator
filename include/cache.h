#pragma once

#include "lba.h"

unsigned long long buffer_size;

struct Cache {
    bool inUsed;
    unsigned long diskAddress;
    unsigned long fid;
    unsigned long insDic;
};

struct Cache *cache_buffer;

void init_cache_buffer(struct Cache *cache_buffer, unsigned long buffer_size);