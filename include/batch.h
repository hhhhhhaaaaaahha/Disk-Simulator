#ifndef BATCH_H
#define BATCH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct batch_block_entry bb_entry_t;

typedef struct batch_block_head bb_head_t;

typedef struct batch_table_head b_table_head_t;

struct batch_block_entry {
    unsigned long pba;
    bool isVirtual;
};

struct batch_block_head {
    bb_entry_t *table;
    size_t capacity;
    size_t size;
};

struct batch_table_head {
    bb_head_t block_head, extend_head;
};

struct disk;

void batch_add(struct disk *d, unsigned long pba, b_table_head_t *table);
void batch_clear(b_table_head_t *table);
int batch_sync(struct disk *d, b_table_head_t *table);
//int batch_extend(struct disk *d, b_table_head_t *table);
void batch_extend_hybrid(struct disk *d, b_table_head_t *table);
void batch_extend(struct disk *d, b_table_head_t *table);
int init_batch_table(b_table_head_t *table);
void end_batch_table(b_table_head_t *table);
int batch_write(struct disk *d, b_table_head_t *t);
int _batch_read(struct disk *d, b_table_head_t *t);
int batch_read(struct disk *d, b_table_head_t *t);
int batch_delete(struct disk *d, b_table_head_t *t);
int batch_invalid(struct disk *d, b_table_head_t *t);

extern b_table_head_t gbtable;
extern b_table_head_t mbtable;
extern b_table_head_t bbtable;
#endif