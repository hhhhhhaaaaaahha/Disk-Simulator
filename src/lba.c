#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "batch.h"
#include "lba.h"
#include "pba.h"
#include "record_op.h"
#include "band.h"
#include "FluidSMR.h"
#include "cache.h"

struct disk_operations ops = {
    .read = lba_read,
    .write = lba_write,
    .remove = lba_delete,
    .journaling_write = journaling_write,
    .invalid = lba_invalid,
};

int init_disk(struct disk *disk, int size, bool cacheFlag) {
    void *p;
    struct report *report = &disk->report;
    unsigned long block_num = 0;

    assert(disk);
    assert(size > 0);

    disk->cacheFlag = cacheFlag;

    /* Initialize struct report */
    memset(&disk->report, 0, sizeof(disk->report));
    
    /* Initialize track info */
    unsigned long total_tracks;
    uint64_t total_byte = GB_TO_BYTE(size);
    total_tracks = total_byte / TRACK_SIZE;
    report->max_track_num = total_tracks;
    block_num = total_tracks;

    /* Initialize disk storage */
    p = malloc(block_num * sizeof(*disk->storage));
    if (!p) {
        goto done_disk_storage;
    }
    memset(p, 0, block_num * sizeof(*disk->storage));
    disk->storage = (struct block *) p;

    /* Initialize lba to pba table head*/
    p = malloc(sizeof(*disk->ltp_table_head));
    if (!p) {
        goto done_ltp_table_head;
    }
    memset(p, 0, sizeof(*disk->ltp_table_head));
    disk->ltp_table_head = (struct ltp_table_head *) p;

    /* Initialize lba to pba table */
    p = malloc(block_num * sizeof(struct ltp_entry));
    if (!p) {
        goto done_ltp_table;
    }
    memset(p, 0, block_num * sizeof(struct ltp_entry));
    disk->ltp_table_head->table = (struct ltp_entry *) p;
    report->max_block_num = block_num;

    /* Initialize btable info */
    if (disk->cacheFlag == false) {
        if (init_batch_table(&gbtable)) {
            goto done_gbtable;
        }
        if (init_batch_table(&mbtable)) {
            goto done_mbtable;
        }
        if (init_batch_table(&bbtable)) {
            goto done_bbtable;
        }
    }
    
    /* Initialize fluid_zone storage */
    //MAX_zone_num = total_byte / FLUID_ZONE_SIZE;
    //min_zone_num = MAX_zone_num / FLUID_DENSITY_GAIN;
    //fluid_cmr_zone = malloc(MAX_zone_num * sizeof(struct FLUID_ZONE));
    //fluid_smr_zone = malloc(MAX_zone_num * sizeof(struct FLUID_ZONE));
    //init_fluid_zone(fluid_cmr_zone, MAX_zone_num);
    //init_fluid_zone(fluid_smr_zone, MAX_zone_num);
    //fluid_zone = malloc(MAX_zone_num * sizeof(struct FLUID_ZONE));
    //init_fluid_zone(fluid_zone, MAX_zone_num);
    /*
    if (!fluid_zone) {
        goto done_bbtable;
    }
    */
    /* Initialize band storage */
    if (disk->cacheFlag == false) {
        total_bands = total_byte / BAND_SIZE;
        band = malloc(total_bands * sizeof(struct Band));
        init_band(band, total_bands);
        if (!band) {
            goto done_bbtable;
        }
    }

    /* Initialize band storage with cache */
    if (disk->cacheFlag == true) {
        buffer_size = report->max_block_num;
        cache_buffer = malloc(buffer_size * sizeof(struct Cache));
        init_cache_buffer(cache_buffer, buffer_size);
        if (!cache_buffer) {
            goto done_band;
        }
    }

    disk->d_op = &ops;

    return 0;

done_band:
    if (disk->cacheFlag == false) {
        free(band);
    }
done_bbtable:
    if (disk->cacheFlag == false) {
        end_batch_table(&mbtable);
    }
done_mbtable:
    if (disk->cacheFlag == false) {
        end_batch_table(&gbtable);
    }
done_gbtable:
    if (disk->cacheFlag == false) {
        free(disk->ltp_table_head->table);
    }
done_ltp_table:
    free(disk->ltp_table_head);
done_ltp_table_head:
    free(disk->storage);
done_disk_storage:
    return -1;
}

void end_disk(struct disk *disk) {
    if (disk->cacheFlag == false) {
        free(band);
    }
    free(disk->ltp_table_head->table);
    free(disk->ltp_table_head);
    free(disk->storage);
    if (disk->cacheFlag == false) {
        end_batch_table(&gbtable);
    }
    clear_info(disk);
    memset(&disk, 0, sizeof(disk));
    
    //enable_block_swap = false;
    //enable_top_buffer = false;
}

bool is_ltp_mapping_valid(struct disk *d, unsigned long lba, unsigned long fid) {
    struct ltp_entry *e = &d->ltp_table_head->table[lba];
    return ((e->valid) && e->fid == fid);
}

bool is_block_data_valid(struct disk *d, unsigned long lba, unsigned long fid) {
    if (!is_ltp_mapping_valid(d, lba, fid))
        return false;

    return d->storage[lba_to_pba(d, lba)].status == status_in_use;
}

void add_pba(struct disk *d, unsigned long lba, b_table_head_t *table) {
    unsigned long pba = lba_to_pba(d, lba);
    batch_add(d, pba, table);
}

int lba_invalid(struct disk *d, unsigned long lba, size_t n, unsigned long fid) {
    if (n == 0)
        return 0;
    for (size_t i = 0; i < n; i++) {
        if (is_block_data_valid(d, lba + i, fid)) {

            add_pba(d, lba + i, &gbtable);

        } else {
            fprintf(stderr, "Error: Invalid a block whick is not valid. lba: %lu\n", lba + i);
            exit(EXIT_FAILURE);
        }
    }
    return batch_invalid(d, &gbtable);
}

int lba_read(struct disk *d, unsigned long lba, size_t n, unsigned long fid) {
    size_t num_invalid = 0;
    struct report *report = &d->report;
    if (n == 0)
        return 0;
    if (!(lba < report->max_block_num) || !((lba + (n - 1)) < report->max_block_num)) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        if (is_block_data_valid(d, lba + i, fid)) {
            batch_add(d, lba_to_pba(d, lba + i), &gbtable);
        } else {
            num_invalid++;
            d->report.num_invalid_read++;
        }
    }
    if (num_invalid == n)
        return 0;
    return batch_read(d, &gbtable);
}

bool is_invalid_write(struct disk *d, unsigned long lba, unsigned long fid) {
    struct ltp_entry *e = &d->ltp_table_head->table[lba];
    return e->valid && (fid != e->fid);
}

int lba_write(struct disk *d, unsigned long lba, size_t n, unsigned long fid) {
    size_t num_invalid = 0;
    struct report *report = &d->report;
    if(n == 0)
        return 0;
    if(!(lba < report->max_block_num) || !((lba + (n - 1)) < report->max_block_num)) {
        return 0;
    }

    for(size_t i = 0; i < n; i++) {
        if(is_invalid_write(d, lba + i, fid)) {
            num_invalid++;
            report->num_invalid_write++;
            continue;
        }
        unsigned long pba = pba_search(d, lba + i, fid);
        assert(pba < d->report.max_block_num);
        /* link lba and pba */
        d->storage[pba].lba = lba + i;
        batch_add(d, pba, &gbtable);
    }
    if(num_invalid == n)
        return 0;
    return batch_write(d, &gbtable);
}

int native_lba_delete(struct disk *d, unsigned long lba, size_t n, unsigned long fid) {
    struct report *report = &d->report;
    size_t len = report->max_block_num;
    struct ltp_entry *entry = d->ltp_table_head->table;
    int count = 0;
    for (size_t i = 0; i < len; i++) {
        if (entry[i].valid && (fid == entry[i].fid)) {
            unsigned long pba = lba_to_pba(d, i);
            batch_add(d, pba, &gbtable);
            count++;
        }
    }
    if (count == 0)
        return 0;
    return batch_delete(d, &gbtable);
}

int lba_delete(struct disk *d, unsigned long lba, size_t n, unsigned long fid) {
    return native_lba_delete(d, lba, n, fid);
}

void clear_info(struct disk *d) {
    struct report *p = &d->report;
    p->total_access_time = 0;
    memset(&d->report.normal, 0, sizeof(struct time_size));
    memset(&d->report.journaling, 0, sizeof(struct time_size));
}

int journaling_write(struct disk *d, unsigned long lba, size_t n, unsigned long fid) {
    struct report *report = &d->report;
    if (n == 0)
        return 0;
    assert((lba < report->max_block_num) ||
           ((lba + (n - 1)) < report->max_block_num));

    for (size_t i = 0; i < n; i++) {
        unsigned long pba = pba_search_jfs(d, lba + i, fid);
        /* link lba and pba */
        d->storage[pba].lba = lba + i;
        batch_add(d, pba, &gbtable);
    }
    return batch_write(d, &gbtable);
}
