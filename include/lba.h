#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KILOBYTE 1024UL
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * GIGABYTE)
#define MAX_LENS 80UL
#define SECTOR_PER_TRACK 512UL
#define SECTOR_SIZE 4096UL
#define TRACK_SIZE (SECTOR_SIZE * SECTOR_PER_TRACK)
//TRACK_SIZE 2097152UL
#define BLOCK_SIZE TRACK_SIZE

#define GB_TO_BYTE(n) ((uint64_t) (n) << 30)

#define SECTOR_BITS (64 - __builtin_clzl(SECTOR_PER_TRACK) - 1)
#define SECTOR_MASK ((0x1ULL << SECTOR_BITS) - 1)
#define TRACK_MASK (~SECTOR_MASK)
#define CLEAR_SECTOR(x) ((x) & ~SECTOR_MASK)

#define lba_to_track(lba) (lba)



typedef enum { lbaread, lbawrite, lbadelete } lba_command;

typedef enum { status_free, status_booked, status_in_use, status_invalid, status_end } block_status_t;

typedef enum { normal_type, top_buffer_type, buffered_type, block_swap_type, end_type } block_type_t;

struct disk;
struct track;
struct block;
struct disk_operations;
struct ltp_table_head;
struct ltp_entry;

struct time_size {
    uint64_t total_read_time;
    uint64_t total_write_time;
    uint64_t total_write_size;
    uint64_t total_read_block_size;
    uint64_t total_write_block_size;
};

struct report {
    struct time_size normal, journaling;
    uint64_t max_track_num;
    uint64_t max_block_num;
    uint64_t current_use_block_num;
    uint64_t total_access_time;
    /* delete information */
    uint64_t total_delete_time;
    uint64_t total_delete_write_block_size;
    uint64_t total_delete_write_size;
    uint64_t total_delete_rewrite_size;
    uint64_t total_delete_reread_size;
    uint64_t total_read_size;
    uint64_t total_write_size;
    uint64_t total_rewrite_size;
    uint64_t total_reread_size;
    uint64_t ins_count;
    uint64_t read_ins_count;
    uint64_t write_ins_count;
    uint64_t delete_ins_count;
    uint64_t num_invalid_read;
    uint64_t num_invalid_write;
};

struct disk {
    struct block *storage;
    struct disk_operations *d_op;
    struct ltp_table_head *ltp_table_head;
    /* top-buffer */
    //struct ptt_table_head *ptt_table_head;
    struct report report;
    bool cacheFlag;
};

struct block {
    block_status_t status; /* block status */
    unsigned long lba;
    unsigned count;
};

static inline bool storage_is_free(struct disk *d, unsigned long pba) {
    return (d->storage[pba].status == status_invalid) || (d->storage[pba].status == status_free);
}

static inline bool storage_is_in_use(struct disk *d, unsigned long pba) {
    return (d->storage[pba].status == status_booked) || (d->storage[pba].status == status_in_use);
}

struct disk_operations {
    int (*read)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
    int (*write)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
    int (*remove)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
    int (*journaling_write)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
    int (*invalid)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
};

struct ltp_entry {
    unsigned long pba;
    unsigned long fid;
    bool valid;
};

struct ltp_table_head {
    struct ltp_entry *table;
    size_t count;
};

bool is_block_data_valid(struct disk *d, unsigned long lba, unsigned long fid);
bool is_ltp_mapping_valid(struct disk *d, unsigned long lba, unsigned long fid);
void clear_info(struct disk *d);
int init_disk(struct disk *disk, int size, bool cacheFlag);
void end_disk(struct disk *disk);

int lba_read(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
int lba_write(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
int lba_delete(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
int journaling_write(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
int lba_invalid(struct disk *d, unsigned long lba, size_t n, unsigned long fid);