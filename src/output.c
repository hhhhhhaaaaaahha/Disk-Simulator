#include <stdio.h>
#include "lba.h"

void output_ltp_table(struct disk *d)
{
    // FILE *output = fopen("output/ltp_table.csv", "w+");
    // struct ltp_entry *entry = d->ltp_table_head->table;
    // fprintf(output, "lba,hash,pba,trim,valid\n");
    // for (uint64_t i = 0; i < d->report.max_logical_block_num; i++)
    // {
    //     fprintf(output, "%ld,%s,%lu,", i, entry[i].hash, entry[i].pba);
    //     if (entry[i].trim == true)
    //         fprintf(output, "True,");
    //     else
    //         fprintf(output, "False,");
    //     if (entry[i].valid == true)
    //         fprintf(output, "True\n");
    //     else
    //         fprintf(output, "False\n");
    // }
    // fclose(output);
}
void output_disk_info(struct disk *d)
{
    FILE *output = fopen("output/block.csv", "w+");
    struct block *track = d->storage;
    fprintf(output, "pba,lba,count,status\n");
    for (uint64_t i = 0; i < d->report.max_block_num; i++)
    {
        fprintf(output, "%llu,%lu,%u", i, track[i].lba, track[i].count);
        switch (track[i].status)
        {
        case status_free:
            fprintf(output, ",status_free\n");
            break;
        case status_booked:
            fprintf(output, ",status_booked\n");
            break;
        case status_in_use:
            fprintf(output, ",status_in_use\n");
            break;
        case status_invalid:
            fprintf(output, ",status_invalid\n");
            break;
        case status_end:
            fprintf(output, ",status_end\n");
            break;
        default:
            fprintf(output, ",no\n");
            break;
        }
    }
    fclose(output);
}