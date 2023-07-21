#include <assert.h>

#include "batch.h"
#include "lba.h"
#include "pba.h"
#include "record_op.h"
#include "band.h"
#include "FluidSMR.h"

/* normal use */
b_table_head_t gbtable;
/* for scp */
b_table_head_t mbtable;
/* for band buffer */
b_table_head_t bbtable;

/*
 * return int
 * 1 for error
 * 0 for success
 */
int init_batch_table(b_table_head_t *table) {
    size_t capacity = 4096;
    if (!(table->block_head.table = (bb_entry_t *) calloc(capacity, sizeof(*table->block_head.table)))) {
        fprintf(stderr, "Error: malloc batch table failed\n");
        goto done_block_table;
    }
    table->block_head.capacity = capacity;
    table->block_head.size = 0;

    if (!(table->extend_head.table = (bb_entry_t *) calloc(capacity * 3, sizeof(*table->extend_head.table)))) {
        fprintf(stderr, "Error: malloc batch table failed\n");
        goto done_extend_table;
    }
    table->extend_head.capacity = 3 * capacity;
    table->extend_head.size = 0;
    return 0;

done_extend_table:
    free(table->block_head.table);
done_block_table:
    return -1;
}

void end_batch_table(b_table_head_t *table) {
    free(table->block_head.table);
    table->block_head.capacity = 0;
    table->block_head.size = 0;
    free(table->extend_head.table);
    table->extend_head.capacity = 0;
    table->extend_head.size = 0;
}

static void increase_bb_head_capacity(bb_head_t *h, int i) {
    if (!(h->table = (bb_entry_t *) realloc(h->table, h->capacity * i * sizeof(*h->table)))) {
        fprintf(stderr, "ERROR: Fail to increase capacity of batch block table. capacity: %lu, i: %d\n", h->capacity, i);
        exit(EXIT_FAILURE);
    }
    h->capacity *= i;
}

int batch_cmp(const void *a, const void *b) {
    unsigned long first = ((bb_entry_t *) a)->pba;
    unsigned long second = ((bb_entry_t *) b)->pba;
    return first > second ? 1 : (first < second) ? -1 : 0;
}

static bool is_bb_table_full(bb_head_t *h) {
    return h->capacity <= h->size;
}

void bb_table_add(bb_head_t *h, unsigned long pba, bool isVirtual) {
    if (is_bb_table_full(h))
        increase_bb_head_capacity(h, 2);
    bb_entry_t *e = &h->table[h->size];
    e->pba = pba;
    e->isVirtual = isVirtual;
    h->size++;
}

bool bb_table_get_last_pba(bb_head_t *h, unsigned long *result) {
    if (h->size == 0)
        return false;
    *result = h->table[h->size - 1].pba;
    return true;
}

void batch_add(struct disk *d, unsigned long pba, b_table_head_t *h) {
    bb_table_add(&h->block_head, pba, false);
}

void batch_clear(b_table_head_t *table) {
    table->block_head.size = 0;
    table->extend_head.size = 0;
}

void bb_table_add_entry_virtual(bb_head_t *h, bb_entry_t *e) {
    bb_table_add(h, e->pba, true);
}

void bb_table_add_entry(bb_head_t *h, bb_entry_t *e) {
    bb_table_add(h, e->pba, e->isVirtual);
}

size_t bb_table_get_size(bb_head_t *h) {
    return h->size;
}

void bb_table_modify_last_to_physical(bb_head_t *h) {
    if (h->size == 0)
        return;
    long pos = h->size - 1;
    h->table[pos].isVirtual = false;
}

void bb_table_modify(bb_head_t *h, unsigned long index, unsigned long pba, bool isVirtual) {
    bb_entry_t *p = h->table;
    p[index].pba = pba;
    p[index].isVirtual = isVirtual;
}

/*
//int batch_extend(struct disk *d, b_table_head_t *h)
void batch_extend(struct disk *d, b_table_head_t *h)
{
    bb_head_t *merge_head, *extend_head;
    bb_entry_t *merge_table;
    unsigned long track, prev_track, next_track, pba;
    size_t i, count = 0;

    merge_head = &h->block_head;
    extend_head = &h->extend_head;
    merge_table = merge_head->table;

    for (i = 0; i < merge_head->size; i++) {
        track = merge_table[i].pba;

        if (is_toptrack(track)) {
            if (bb_table_get_last_pba(extend_head, &pba) && (pba == track))
                bb_table_modify_last_to_physical(extend_head);
            else
                bb_table_add_entry(extend_head, &merge_table[i]);
            continue;
        }
        prev_track = track - 1;
        if (track && storage_is_in_use(d, prev_track)) {
            if (!(bb_table_get_last_pba(extend_head, &pba) &&
                  (pba == prev_track))) {
                bb_table_add(extend_head, track - 1, true);
                count++;
            }
        }
        bb_table_add_entry(extend_head, &merge_head->table[i]);
        next_track = track + 1;
        if ((track != (d->report.max_track_num - 1)) &&
            (storage_is_in_use(d, next_track))) {
            bb_table_add(extend_head, track + 1, true);
        }
    }
    //return bb_table_get_size(extend_head);
}
*/

//int batch_extend(struct disk *d, b_table_head_t *h) {
void batch_extend(struct disk *d, b_table_head_t *h) {
    bb_head_t *merge_head, *extend_head;
    bb_entry_t *merge_table;
    unsigned long track, bandNum, whp;
    size_t i;

    merge_head = &h->block_head;
    extend_head = &h->extend_head;
    merge_table = merge_head->table;

    if (d->cacheFlag == true) {
        for (i = 0; i < merge_head->size; i++) {
            bb_table_add_entry(extend_head, &merge_table[i]);
        }
    }
    else if (d->cacheFlag == false) {
        for (i = 0; i < merge_head->size; i++) {
            track = merge_table[i].pba;
            bandNum = band_count(track);
            whp = band[bandNum].write_head_p/TRACK_SIZE;
            if (band[bandNum].flagRMW == false) {
                if (track >= whp) {
                    bb_table_add_entry(extend_head, &merge_table[i]);
                    band[bandNum].write_head_p = track * TRACK_SIZE;
                }
                else {
                    band[bandNum].flagRMW = true;
                    band[bandNum].RMW_start = track;
                    band[bandNum].RMW_end = track;
                    bb_table_add_entry(extend_head, &merge_table[i]);
                }
            }
            else {
                band[bandNum].RMW_end = track;
                if (track >= whp) {
                    //bb_table_add(extend_head, track, true);
                    band[bandNum].write_head_p = track * TRACK_SIZE;
                }
                bb_table_add_entry(extend_head, &merge_table[i]);
            }
        }     
        for(size_t bc = 0; bc < total_bands; bc++) {

            if(band[bc].flagRMW == true) {
#ifdef NATIVE_A
                for(size_t rmw = band[bc].band_head/TRACK_SIZE; rmw < band[bc].RMW_start; rmw++) {
                    //bb_table_add(extend_head, rmw, true);
                    if((storage_is_in_use(d, rmw))) {
                        bb_table_add(extend_head, rmw, true);
                    }
                }
#endif                
                for(size_t rmw = band[bc].RMW_end + 1; rmw <= band[bc].write_head_p/TRACK_SIZE; rmw++) {
                    //bb_table_add(extend_head, track, true);
                    if((storage_is_in_use(d, rmw))) {
                        bb_table_add(extend_head, rmw, true);
                    }
                }
            }

        }
    }
}
/*
void batch_extend_hybrid(struct disk *d, b_table_head_t *h) {
    bb_head_t *merge_head, *extend_head;
    bb_entry_t *merge_table;
    unsigned long track, bandNum, whp, track_ED;
    size_t i;

    merge_head = &h->block_head;
    extend_head = &h->extend_head;
    merge_table = merge_head->table;

    for(i = 0; i < merge_head->size; i++) {
        track = merge_table[i].pba;
        bandNum = band_count(track);
        whp = band[bandNum].write_head_p/TRACK_SIZE;
        if(band[bandNum].flagCMR) {
            track_ED = band[bandNum].band_CMR_end/TRACK_SIZE;
        }
        else {
            track_ED = band[bandNum].band_end/TRACK_SIZE;
        }
        if(track < track_ED) {
            if(!band[bandNum].flagRMW) {
                if(track >= whp) {
                    bb_table_add_entry(extend_head, &merge_table[i]);
                    band[bandNum].write_head_p = track * TRACK_SIZE;
                }
                else {
                    band[bandNum].flagRMW = true;
                    band[bandNum].RMW_start = track;
                    band[bandNum].RMW_end = track;
                    bb_table_add_entry(extend_head, &merge_table[i]);
                }
            }
            else {
                band[bandNum].RMW_end = track;
                if (track >= whp) {
                    band[bandNum].write_head_p = track * TRACK_SIZE;
                }
                bb_table_add_entry(extend_head, &merge_table[i]);
            }
        }
    }
    for(size_t bc = 0; bc < total_bands; bc++) {

        if(band[bc].flagRMW == true) {
#ifdef NATIVE_A
            for(size_t rmw = band[bc].band_head/TRACK_SIZE; rmw < band[bc].RMW_start; rmw++) {
                    if((storage_is_in_use(d, rmw))) {
                        bb_table_add(extend_head, rmw, true);
                    }
                }
#endif                
            for(size_t rmw = band[bc].RMW_end + 1; rmw <= band[bc].write_head_p/TRACK_SIZE; rmw++) {
                if((storage_is_in_use(d, rmw))) {
                    bb_table_add(extend_head, rmw, true);
                }
            }
        }
    }
}
*/
void batch_extend_fluidsmr(struct disk *d, b_table_head_t *h) {
    bb_head_t *merge_head, *extend_head;
    bb_entry_t *merge_table;
    unsigned long track, bandNum, whp;
    size_t i;

    merge_head = &h->block_head;
    extend_head = &h->extend_head;
    merge_table = merge_head->table;

    for(i = 0; i < merge_head->size; i++) {
        track = merge_table[i].pba;
        bandNum = band_count(track);
        if(band[bandNum].flagCMR) {
            bb_table_add_entry(extend_head, &merge_table[i]);
        }
        else {
            whp = band[bandNum].write_head_p/TRACK_SIZE;
            if (band[bandNum].flagRMW == false) {
                if (track >= whp) {
                    bb_table_add_entry(extend_head, &merge_table[i]);
                    band[bandNum].write_head_p = track * TRACK_SIZE;
                }
                else {
                    band[bandNum].flagRMW = true;
                    band[bandNum].RMW_start = track;
                    band[bandNum].RMW_end = track;
                    bb_table_add_entry(extend_head, &merge_table[i]);
                }
            }
            else {
                band[bandNum].RMW_end = track;
                if (track >= whp) {
                    //bb_table_add(extend_head, track, true);
                    band[bandNum].write_head_p = track * TRACK_SIZE;
                }
                bb_table_add_entry(extend_head, &merge_table[i]);
            }
        }
        for(size_t bc = 0; bc < total_bands; bc++) {
            if(band[bc].flagRMW == true) {
#ifdef NATIVE_A
                for(size_t rmw = band[bc].band_head/TRACK_SIZE; rmw < band[bc].RMW_start; rmw++) {
                    //bb_table_add(extend_head, rmw, true);
                    if((storage_is_in_use(d, rmw))) {
                        bb_table_add(extend_head, rmw, true);
                    }
                }
#endif
                for(size_t rmw = band[bc].RMW_end + 1; rmw <= band[bc].write_head_p/TRACK_SIZE; rmw++) {
                    //bb_table_add(extend_head, track, true);
                    if((storage_is_in_use(d, rmw))) {
                        bb_table_add(extend_head, rmw, true);
                    }
                }
            }
        }
    }
}
/*
void batch_extend_FluidSMR(struct disk *d, b_table_head_t *h) {
    bb_head_t *merge_head, *extend_head;
    bb_entry_t *merge_table;
    unsigned long track, zone_num, whp;
    size_t i;

    merge_head = &h->block_head;
    extend_head = &h->extend_head;
    merge_table = merge_head->table;

    for (i = 0; i < merge_head->size; i++) {
        track = merge_table[i].pba;
        zone_num = FLUID_ZONE_COUNT(track);
        if (fluid_zone[zone_num].zone_mode == 1) {
            bb_table_add_entry(extend_head, &merge_table[i]);
        }
        else if (fluid_zone[zone_num].zone_mode == 1) {
            whp = fluid_zone[zone_num].write_head_p/TRACK_SIZE;
            if (!fluid_zone[zone_num].flagRMW) {
                if (track >= whp) {
                    bb_table_add_entry(extend_head, &merge_table[i]);
                    fluid_zone[zone_num].write_head_p = track * TRACK_SIZE;
                }
                else {
                    fluid_zone[zone_num].flagRMW = true;
                    fluid_zone[zone_num].RMW_start = track;
                    fluid_zone[zone_num].RMW_end = track;
                    bb_table_add_entry(extend_head, &merge_table[i]);
                }
            }
            else {
                fluid_zone[zone_num].RMW_end = track;
                if (track >= whp) {
                    fluid_zone[zone_num].write_head_p = track * TRACK_SIZE;
                }
                bb_table_add_entry(extend_head, &merge_table[i]);
            }
        }
    }

    for (size_t zone_count = 0; zone_count < MAX_zone_num; zone_count++) {
        if ((fluid_zone[zone_count].zone_mode == 2) && (fluid_zone[zone_count].flagRMW)) {
            for (size_t rmw = fluid_zone[zone_count].zone_head/TRACK_SIZE; rmw < fluid_zone[zone_count].RMW_start; rmw++) {
                if ((storage_is_in_use(d, rmw))) {
                    bb_table_add(extend_head, rmw, true);
                }
            }
        }
    }
}
*/
int batch_sync(struct disk *d, b_table_head_t *h) {
    if (h->block_head.size == 0) {
        return 0;
    }
    bb_head_t *bbh = &h->block_head;
    bb_entry_t *p = bbh->table;
    qsort((void *) p, bbh->size, sizeof(*p), batch_cmp);
    return 1;
}

extern bool is_csv_flag;
int _batch_read(struct disk *d, b_table_head_t *t) {
    if (0 == batch_sync(d, t)) {
        if (is_csv_flag) {
            batch_clear(t);
            return 0;
        } else {
            fprintf(stderr, "Error: sync batch with 0 block in %s()\n", __FUNCTION__);
            exit(EXIT_FAILURE);
        }
    }
    int count = pba_read(d, t);
    record_read(d, t);
    return count;
}

int batch_read(struct disk *d, b_table_head_t *t) {
    int count = _batch_read(d, t);
    batch_clear(t);
    return count;
}

int batch_write(struct disk *d, b_table_head_t *t) {
    int count, merge_count;

    assert(0 != (merge_count = batch_sync(d, t)));


#ifndef CMR
    //int extend_count = batch_extend(d, t);
#ifdef FLUIDSMR
    batch_extend_fluidsmr(d, t);
#else    
    batch_extend(d, t);
#endif
    //extend_count = merge_count;
    //assert(extend_count >= merge_count);
#endif

    count = pba_write(d, t);
    record_write(d, t);
    batch_clear(t);
    return count;
}

int batch_delete(struct disk *d, b_table_head_t *t) {
    assert(0 != batch_sync(d, t));
    batch_extend(d, t);
    record_delete(d, t);
    int count = pba_delete(d, t);
    batch_clear(t);
    return count;
}

int batch_invalid(struct disk *d, b_table_head_t *t) {
    assert(0 != batch_sync(d, t));
    int count = pba_delete(d, t);
    batch_clear(t);
    return count;
}