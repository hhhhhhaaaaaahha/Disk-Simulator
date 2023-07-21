#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "chs.h"
#include "lba.h"
#include "pba.h"


int chs_read(struct disk *d, unsigned long base) {
    return 1;
}

void chs_write(struct disk *d, unsigned long pba) {
    block_status_t status = d->storage[pba].status;
    assert((status == status_booked) || (status == status_in_use));
    if (status != status_in_use) {
        d->storage[pba].status = status_in_use;
        d->report.current_use_block_num++;
    }
    d->storage[pba].count++;
}

void chs_delete(struct disk *d, unsigned long pba) {
    unsigned long lba = d->storage[pba].lba;
    block_status_t status = d->storage[pba].status;
    if (status != status_in_use) {
        fprintf(stderr, "Error: Delete block failed. status != in_use.\n");
        fprintf(stderr, "status = %d\n", status);
        exit(EXIT_FAILURE);
    }
    d->storage[pba].status = status_invalid;

    bool valid = d->ltp_table_head->table[lba].valid;
    assert(valid == true);

    d->ltp_table_head->table[lba].valid = false;
}