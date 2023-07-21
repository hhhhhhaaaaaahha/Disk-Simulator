#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "batch.h"
#include "chs.h"
#include "fid_table.h"
#include "lba.h"
#include "list.h"
#include "op_mode.h"
#include "pba.h"


bool phase1_is_full = false;
bool initialized = false;

extern jmp_buf env;

unsigned long native_find_next_pba(struct disk *d, unsigned long t, unsigned long fid) {
    d->storage[t].status = status_booked;
    return t;
}

unsigned long native_journal_get_block(struct disk *d, unsigned long t, unsigned long fid) {
    unsigned long pba = t;
    d->storage[pba].status = status_booked;
    return pba;
}

unsigned long find_next_pba_jfs(struct disk *d, unsigned long t, unsigned long fid) {
    return native_journal_get_block(d, t, fid);
}

unsigned long find_next_pba(struct disk *d, unsigned long t, unsigned long fid) {
    return native_find_next_pba(d, t, fid);
}

int pba_read(struct disk *d, b_table_head_t *h) {
    size_t sum = 0;
    bb_head_t *bth = &h->block_head;
    sum = bth->size;
    struct time_size *t_s = (recording_mode == normal_op_mode) ? &d->report.normal : &d->report.journaling;
    t_s->total_read_block_size += sum * BLOCK_SIZE;
    return sum;
}

int pba_write(struct disk *d, b_table_head_t *h) {
    /* clear block data */
    bb_head_t *bbh = &h->block_head;
    size_t sum = bbh->size;
    struct time_size *t_s = (recording_mode == normal_op_mode) ? &d->report.normal : &d->report.journaling;
    for (size_t i = 0; i < bbh->size; i++) {
        unsigned long pba = bbh->table[i].pba;
        chs_write(d, pba);
    }
    t_s->total_write_block_size += sum * BLOCK_SIZE;
    return sum;
}

int pba_delete(struct disk *d, b_table_head_t *h) {
    bb_head_t *bbh = &h->block_head;
    size_t sum = bbh->size;
    for (size_t i = 0; i < bbh->size; i++) {
        unsigned long pba = bbh->table[i].pba;
        chs_delete(d, pba);
    }
    d->report.total_delete_write_block_size += sum * BLOCK_SIZE;
    return sum;
}

bool is_storage_data_valid(struct disk *d, unsigned long pba) {
    return d->storage[pba].status == status_in_use;
}

void update_ltp_table(struct disk *d, unsigned long lba, unsigned pba, unsigned long fid) {
    d->ltp_table_head->table[lba].pba = pba;
    d->ltp_table_head->table[lba].valid = true;
    d->ltp_table_head->table[lba].fid = fid;
}

/*
unsigned long pba_to_tba(struct disk *d, unsigned long pba) 
{
    return d->ptt_table_head->table[pba].tba;
}
*/

bool is_lba_valid(struct disk *d, unsigned long lba, unsigned long fid, unsigned long *p) {
    unsigned long pba = lba_to_pba(d, lba);
    *p = pba;
    if (is_ltp_mapping_valid(d, lba, fid) && is_storage_data_valid(d, pba)) {
        return true;
    }
    return false;
}

unsigned long pba_search_jfs(struct disk *d, unsigned long lba, unsigned long fid) {
    unsigned long pba;
    if (is_lba_valid(d, lba, fid, &pba))
        return pba;
    pba = find_next_pba_jfs(d, lba, fid);
    update_ltp_table(d, lba, pba, fid);
    return pba;
}

unsigned long pba_search(struct disk *d, unsigned long lba, unsigned long fid) {
    unsigned long pba;
    if (is_lba_valid(d, lba, fid, &pba)) {
        return pba;
    }
    pba = find_next_pba(d, lba, fid);
    update_ltp_table(d, lba, pba, fid);
    return pba;
}

/*
unsigned long lba_to_tba(struct disk *d, unsigned long lba) 
{
    unsigned long pba = d->ltp_table_head->table[lba].pba;
    unsigned long tba = d->ptt_table_head->table[pba].tba;
    if (d->ptt_table_head->table[pba].type == normal_type)
        return pba;
    else
        return tba;
}
*/

unsigned long lba_to_pba(struct disk *d, unsigned long lba) {
    return d->ltp_table_head->table[lba].pba;
}
