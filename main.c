#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "./include/lba.h"
#include "./include/op_mode.h"
#include "./include/pba.h"
#include "./include/cache.h"
#include "./include/journaling.h"
#include "./include/FluidSMR.h"
#include "./include/Hybrid.h"

bool is_csv_flag = false;
op_mode_t recording_mode = normal_op_mode;

unsigned long long bytes;
unsigned long long bytes_cache;
void parsing_csv(struct disk *disk, FILE *stream)
{
    unsigned long lba, n, val;
    char *line = NULL;
    ssize_t nread;
    size_t len;
    // unsigned long fid, remain, remainder, num_bytes, left, num_traces, percent, ten_percent, total_traces;
    unsigned long fid, remain, remainder, num_bytes, left, num_traces, percent, ten_percent;
    int c;
    struct report *report = &disk->report;
    num_traces = 4500000;
    percent = num_traces / 100;
    ten_percent = num_traces / 10;
    // total_traces = 0;

    while ((num_traces--) && ((nread = getline(&line, &len, stream)) != -1))
    {
        char *p = line;
        while (*p == ' ')
        {
            p++;
        }
        if (*p == '#')
        {
            continue;
        }
        if ((val = sscanf(p, "%d,%lu,%llu,%lu\n", &c, &fid, &bytes, &num_bytes)) == 4)
        {
            report->ins_count++;

            lba = bytes / BLOCK_SIZE;
            remainder = bytes % BLOCK_SIZE;
            remain = (remainder == 0 ? 0 : BLOCK_SIZE - remainder);
            n = 0;
            if (num_bytes == 0)
            {
                n = 0;
            }
            else if (remain < num_bytes)
            {
                n = !!remain;
                left = num_bytes - remain;
                n += (left / BLOCK_SIZE) + !!(left % BLOCK_SIZE);
            }
            else
            {
                n = 1;
            }

            // total_traces++;

            switch (c)
            {
            case 1:
                report->read_ins_count++;
                disk->d_op->read(disk, lba, n, fid);
                break;

            case 2:
                report->write_ins_count++;
                disk->d_op->write(disk, lba, n, fid);
                break;

            case 3:
                report->delete_ins_count++;
                disk->d_op->remove(disk, lba, n, fid);
                break;

            default:
                fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized mode. mode: %d\n", c);
                break;
            }
        }
        else
        {
            fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized format.\n");
            exit(EXIT_FAILURE);
        }
        if (!(num_traces % percent))
        {
            putchar('#');
            fflush(stdout);
        }
        if (!(num_traces % ten_percent))
        {
            putchar('\n');
            fflush(stdout);
        }
    }
    free(line);
}

void parsing_csv_cache(struct disk *cache, struct disk *disk, FILE *stream)
{
    unsigned long lba, n, val;
    char *line = NULL;
    ssize_t nread;
    size_t len;
    unsigned long fid, remain, remainder, num_bytes, left, num_traces, percent, ten_percent, total_traces;
    int c;
    struct report *report = &disk->report;
    struct report *cacheReport = &cache->report;
    num_traces = 4500000;
    /* Invalid read */
    // num_traces = 333;
    percent = num_traces / 100;
    ten_percent = num_traces / 10;
    total_traces = 0;
    unsigned long cachePointer = 0;
    unsigned long buffer_remain;
    unsigned long *diskAddress;
    diskAddress = malloc((num_traces + 1) * sizeof(unsigned long));
    while ((num_traces--) && ((nread = getline(&line, &len, stream)) != -1))
    {
        char *p = line;
        while (*p == ' ')
        {
            p++;
        }
        if (*p == '#')
        {
            continue;
        }
        if ((val = sscanf(p, "%d,%lu,%llu,%lu\n", &c, &fid, &bytes, &num_bytes)) == 4)
        {
            report->ins_count++;
            cacheReport->ins_count++;

            lba = bytes / BLOCK_SIZE;
            remainder = bytes % BLOCK_SIZE;
            remain = (remainder == 0 ? 0 : BLOCK_SIZE - remainder);
            n = 0;
            if (num_bytes == 0)
            {
                n = 0;
            }
            else if (remain < num_bytes)
            {
                n = !!remain;
                left = num_bytes - remain;
                n += (left / BLOCK_SIZE) + !!(left % BLOCK_SIZE);
            }
            else
            {
                n = 1;
            }

            total_traces++;

            switch (c)
            {
            case 1:
            {
                unsigned long cache_n = 0;
                unsigned long cache_ptr;
                for (size_t cc = 0; cc < buffer_size; cc++)
                {
                    if (cache_buffer[cc].inUsed == true)
                    {
                        if (cache_buffer[cc].diskAddress == lba)
                        {
                            if (cache_n == 0)
                            {
                                cache_n = 1;
                                cache_ptr = cc;
                            }
                            else
                            {
                                cache_n++;
                            }
                        }
                        if (cache_n == n)
                        {
                            cacheReport->read_ins_count++;
                            cache->d_op->read(cache, cache_ptr, cache_n, fid);
                            break;
                        }
                    }
                }
                if (cache_n < n)
                {
                    if (cache_n == 0)
                    {
                        report->read_ins_count++;
                        disk->d_op->read(disk, lba, n, fid);
                    }
                    else
                    {
                        cacheReport->read_ins_count++;
                        cache->d_op->read(cache, cache_ptr, cache_n, fid);
                        report->read_ins_count++;
                        disk->d_op->read(disk, lba, n - cache_n, fid);
                    }
                }
                break;
            }
            case 2:
            {
                diskAddress[total_traces] = lba;
                while ((buffer_remain = buffer_size - cachePointer) < n)
                {
                    if (buffer_remain == 0)
                    {
                        unsigned long ins_dic_count = 0;
                        unsigned long cache_ptr;
                        unsigned long cache_fid;
                        unsigned long cache_n;
                        bool isEmpty = true;
                        for (size_t cc = 0; cc < buffer_size; cc++)
                        {
                            if (cache_buffer[cc].inUsed == true)
                            {
                                isEmpty = false;
                                if (ins_dic_count == 0)
                                {
                                    ins_dic_count = cache_buffer[cc].insDic;
                                    cache_fid = cache_buffer[cc].fid;
                                    cache_ptr = cc;
                                    cache_n = 1;
                                    cache_buffer[cc].inUsed = false;
                                }
                                else if (ins_dic_count == cache_buffer[cc].insDic)
                                {
                                    cache_n++;
                                    cache_buffer[cc].inUsed = false;
                                }
                                else if (ins_dic_count != cache_buffer[cc].insDic)
                                {
                                    report->write_ins_count++;
                                    disk->d_op->write(disk, diskAddress[ins_dic_count], cache_n, cache_fid);
                                    cacheReport->delete_ins_count++;
                                    cache->d_op->remove(cache, cache_ptr, cache_n, cache_fid);

                                    ins_dic_count = cache_buffer[cc].insDic;
                                    cache_fid = cache_buffer[cc].fid;
                                    cache_ptr = cc;
                                    cache_n = 1;
                                    cache_buffer[cc].inUsed = false;
                                }
                            }
                        }
                        if (isEmpty == false)
                        {
                            report->write_ins_count++;
                            disk->d_op->write(disk, diskAddress[ins_dic_count], cache_n, cache_fid);

                            cacheReport->delete_ins_count++;
                            cache->d_op->remove(cache, cache_ptr, cache_n, cache_fid);
                        }

                        cachePointer = 0;
                    }
                    else
                    {
                        cacheReport->write_ins_count++;
                        cache->d_op->write(cache, cachePointer, buffer_remain, fid);

                        for (size_t cc = 0; cc < buffer_remain; cc++)
                        {
                            cache_buffer[cachePointer + cc].inUsed = true;
                            cache_buffer[cachePointer + cc].diskAddress = lba;
                            cache_buffer[cachePointer + cc].fid = fid;
                            cache_buffer[cachePointer + cc].insDic = total_traces;
                        }

                        n -= buffer_remain;
                        cachePointer += buffer_remain;
                    }
                }
                cacheReport->write_ins_count++;
                cache->d_op->write(cache, cachePointer, n, fid);

                for (size_t cc = 0; cc < n; cc++)
                {
                    cache_buffer[cachePointer + cc].inUsed = true;
                    cache_buffer[cachePointer + cc].diskAddress = lba;
                    cache_buffer[cachePointer + cc].fid = fid;
                    cache_buffer[cachePointer + cc].insDic = total_traces;
                }
                cachePointer += n;
                break;
            }
            case 3:
            {
                unsigned long cache_n = 0;
                unsigned long cache_ptr;
                for (size_t cc = 0; cc < buffer_size; cc++)
                {
                    if (cache_buffer[cc].inUsed == true)
                    {
                        if (cache_buffer[cc].diskAddress == lba)
                        {
                            cache_buffer[cc].inUsed = false;
                            if (cache_n == 0)
                            {
                                cache_n = 1;
                                cache_ptr = cc;
                                // cache_buffer[cc].inUsed = false;
                            }
                            else
                            {
                                cache_n++;
                            }
                            // cache_buffer[cc].inUsed = false;
                        }
                        if (cache_n == n)
                        {
                            cacheReport->delete_ins_count++;
                            cache->d_op->remove(cache, cache_ptr, cache_n, fid);
                            break;
                        }
                    }
                }

                if (cache_n < n)
                {
                    if (cache_n == 0)
                    {
                        report->delete_ins_count++;
                        disk->d_op->remove(disk, lba, n, fid);
                    }
                    else
                    {
                        cacheReport->delete_ins_count++;
                        cache->d_op->remove(cache, cache_ptr, cache_n, fid);

                        report->delete_ins_count++;
                        disk->d_op->remove(disk, lba, n - cache_n, fid);
                    }
                }
                break;
            }
            default:
                fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized mode. mode: %d\n", c);
                break;
            }
        }
        else
        {
            fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized format.\n");
            exit(EXIT_FAILURE);
        }
        if (!(num_traces % percent))
        {
            putchar('#');
            fflush(stdout);
        }
        if (!(num_traces % ten_percent))
        {
            putchar('\n');
            fflush(stdout);
        }
    }
    free(line);
    free(diskAddress);
}

void parsing_postmark(struct disk *disk, FILE *stream)
{
    unsigned long lba, n, val;
    char *line;
    ssize_t nread;
    size_t len;
    unsigned long fid;
    int count, c;
    struct report *report = &disk->report;
    line = NULL;

    while ((nread = getline(&line, &len, stream)) != -1)
    {
        char *p = line;
        while (*p == ' ')
        {
            p++;
        }
        if (*p == '#')
        {
            continue;
        }
        if ((val = sscanf(p, "%d,%lu,%lu,%lu\n", &c, &fid, &lba, &n)) == 4)
        {
            report->ins_count++;
            switch (c)
            {
            case 1:
                report->read_ins_count++;
                count = disk->d_op->read(disk, lba, n, fid);
                if (count != n)
                {
                    fprintf(stderr, "Error: size of input != size of output while reading\n");
                    exit(EXIT_FAILURE);
                }
                break;

            case 2:
                report->write_ins_count++;
                count = disk->d_op->write(disk, lba, n, fid);
                if (count != n)
                {
                    fprintf(stderr, "Error: size of input != size of output while writing\n");
                    exit(EXIT_FAILURE);
                }
                break;

            case 3:
                report->delete_ins_count++;
                disk->d_op->remove(disk, lba, n, fid);
                break;

            default:
                fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized mode.\n");
                break;
            }
        }
        else
        {
            fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized format.\n");
            exit(EXIT_FAILURE);
        }
    }
    free(line);
}

void parsing_postmark_cache(struct disk *cache, struct disk *disk, FILE *stream)
{
    unsigned long lba, n, val;
    char *line;
    ssize_t nread;
    size_t len;
    unsigned long fid;
    // int count, c;
    int c;
    struct report *report = &disk->report;

    struct report *cacheReport = &cache->report;

    line = NULL;

    unsigned long num_traces = 4500000;

    unsigned long cachePointer = 0;
    unsigned long buffer_remain;
    unsigned long *diskAddress;

    unsigned long total_traces = 0;
    diskAddress = malloc((num_traces + 1) * sizeof(unsigned long));

    while ((nread = getline(&line, &len, stream)) != -1)
    {
        char *p = line;
        while (*p == ' ')
        {
            p++;
        }
        if (*p == '#')
        {
            continue;
        }
        if ((val = sscanf(p, "%d,%lu,%lu,%lu\n", &c, &fid, &lba, &n)) == 4)
        {
            report->ins_count++;
            cacheReport->ins_count++;

            total_traces++;

            switch (c)
            {
            case 1:
            {
                unsigned long cache_n = 0;
                unsigned long cache_ptr;
                for (size_t cc = 0; cc < buffer_size; cc++)
                {
                    if (cache_buffer[cc].inUsed == true)
                    {
                        if (cache_buffer[cc].diskAddress == lba)
                        {
                            if (cache_n == 0)
                            {
                                cache_n = 1;
                                cache_ptr = cc;
                            }
                            else
                            {
                                cache_n++;
                            }
                        }
                        if (cache_n == n)
                        {
                            cacheReport->read_ins_count++;
                            cache->d_op->read(cache, cache_ptr, cache_n, fid);
                            break;
                        }
                    }
                }
                if (cache_n < n)
                {
                    if (cache_n == 0)
                    {
                        report->read_ins_count++;
                        disk->d_op->read(disk, lba, n, fid);
                    }
                    else
                    {
                        cacheReport->read_ins_count++;
                        cache->d_op->read(cache, cache_ptr, cache_n, fid);
                        report->read_ins_count++;
                        disk->d_op->read(disk, lba, n - cache_n, fid);
                    }
                }
                break;
            }
            case 2:
            {
                diskAddress[total_traces] = lba;
                while ((buffer_remain = buffer_size - cachePointer) < n)
                {
                    if (buffer_remain == 0)
                    {
                        unsigned long ins_dic_count = 0;
                        unsigned long cache_ptr;
                        unsigned long cache_fid;
                        unsigned long cache_n;
                        bool isEmpty = true;
                        for (size_t cc = 0; cc < buffer_size; cc++)
                        {
                            if (cache_buffer[cc].inUsed == true)
                            {
                                isEmpty = false;
                                if (ins_dic_count == 0)
                                {
                                    ins_dic_count = cache_buffer[cc].insDic;
                                    cache_fid = cache_buffer[cc].fid;
                                    cache_ptr = cc;
                                    cache_n = 1;
                                    cache_buffer[cc].inUsed = false;
                                }
                                else if (ins_dic_count == cache_buffer[cc].insDic)
                                {
                                    cache_n++;
                                    cache_buffer[cc].inUsed = false;
                                }
                                else if (ins_dic_count != cache_buffer[cc].insDic)
                                {
                                    report->write_ins_count++;
                                    disk->d_op->write(disk, diskAddress[ins_dic_count], cache_n, cache_fid);
                                    cacheReport->delete_ins_count++;
                                    cache->d_op->remove(cache, cache_ptr, cache_n, cache_fid);

                                    ins_dic_count = cache_buffer[cc].insDic;
                                    cache_fid = cache_buffer[cc].fid;
                                    cache_ptr = cc;
                                    cache_n = 1;
                                    cache_buffer[cc].inUsed = false;
                                }
                            }
                        }
                        if (isEmpty == false)
                        {
                            report->write_ins_count++;
                            disk->d_op->write(disk, diskAddress[ins_dic_count], cache_n, cache_fid);

                            cacheReport->delete_ins_count++;
                            cache->d_op->remove(cache, cache_ptr, cache_n, cache_fid);
                        }

                        cachePointer = 0;
                    }
                    else
                    {
                        cacheReport->write_ins_count++;
                        cache->d_op->write(cache, cachePointer, buffer_remain, fid);

                        for (size_t cc = 0; cc < buffer_remain; cc++)
                        {
                            cache_buffer[cachePointer + cc].inUsed = true;
                            cache_buffer[cachePointer + cc].diskAddress = lba;
                            cache_buffer[cachePointer + cc].fid = fid;
                            cache_buffer[cachePointer + cc].insDic = total_traces;
                        }

                        n -= buffer_remain;
                        cachePointer += buffer_remain;
                    }
                }
                cacheReport->write_ins_count++;
                cache->d_op->write(cache, cachePointer, n, fid);

                for (size_t cc = 0; cc < n; cc++)
                {
                    cache_buffer[cachePointer + cc].inUsed = true;
                    cache_buffer[cachePointer + cc].diskAddress = lba;
                    cache_buffer[cachePointer + cc].fid = fid;
                    cache_buffer[cachePointer + cc].insDic = total_traces;
                }
                cachePointer += n;
                break;
            }
            case 3:
            {
                unsigned long cache_n = 0;
                unsigned long cache_ptr;
                for (size_t cc = 0; cc < buffer_size; cc++)
                {
                    if (cache_buffer[cc].inUsed == true)
                    {
                        if (cache_buffer[cc].diskAddress == lba)
                        {
                            cache_buffer[cc].inUsed = false;
                            if (cache_n == 0)
                            {
                                cache_n = 1;
                                cache_ptr = cc;
                            }
                            else
                            {
                                cache_n++;
                            }
                        }
                        if (cache_n == n)
                        {
                            cacheReport->delete_ins_count++;
                            cache->d_op->remove(cache, cache_ptr, cache_n, fid);
                            break;
                        }
                    }
                }

                if (cache_n < n)
                {
                    if (cache_n == 0)
                    {
                        report->delete_ins_count++;
                        disk->d_op->remove(disk, lba, n, fid);
                    }
                    else
                    {
                        cacheReport->delete_ins_count++;
                        cache->d_op->remove(cache, cache_ptr, cache_n, fid);

                        report->delete_ins_count++;
                        disk->d_op->remove(disk, lba, n - cache_n, fid);
                    }
                }
                break;
            }
            default:
                fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized mode.\n");
                break;
            }
        }
        else
        {
            fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized format.\n");
            exit(EXIT_FAILURE);
        }
    }
    free(line);
    free(diskAddress);
}

void parsing_hybrid_csv(struct disk *disk, FILE *stream)
{
    unsigned long lba, n, val;
    char *line = NULL;
    ssize_t nread;
    size_t len;

    unsigned long fid, remain, remainder, num_bytes, left, num_traces, percent, ten_percent;
    int c;
    struct report *report = &disk->report;
    num_traces = 4500000UL;
    percent = num_traces / 100UL;
    ten_percent = num_traces / 10UL;

    unsigned long commit_count = 0UL;
    unsigned long commit_accumulate = 0UL;

    unsigned long checkpoint_count = 0UL;
    unsigned long checkpoint_accumulate = 0UL;

    hybrid_block_remain = report->max_block_num;
    hybrid_used_zone_count = 0UL;
    hybrid_zone = malloc((total_bands) * sizeof(struct HYBRID_ZONE));

    for (unsigned long zoneNum = 0UL; zoneNum < total_bands; zoneNum++)
    {
        hybrid_zone[zoneNum].isOffline = true;
        hybrid_zone[zoneNum].write_head_pointer = 0UL;
        hybrid_zone[zoneNum].isFull = false;
        hybrid_zone[zoneNum].isJournaling = false;
        hybrid_zone[zoneNum].logical_disk_LBA = zoneNum * 256UL;
    }

    hybrid_journaling_percent = 10UL; // 10%
    hybrid_journaling_zone_limit = total_bands * hybrid_journaling_percent / 100UL;
    hybrid_journaling_zone_count = 0UL;
    hybrid_journaling_hotness_bound = (hybrid_journaling_zone_limit / 2UL) * 256UL;

    hybrid_hotness_bound = ((total_bands - hybrid_journaling_hotness_bound) / 2UL) * 256UL;

    fprintf(stderr, "Journaling Limit: %lu\n", hybrid_journaling_zone_limit);

    for (unsigned long zoneNum = 0UL; zoneNum < total_bands; zoneNum++)
    {
        fprintf(stderr, "Hybrid Zone %lu, LBA: %lu\n", zoneNum, hybrid_zone[zoneNum].logical_disk_LBA);
    }

    while ((num_traces--) && ((nread = getline(&line, &len, stream)) != -1))
    {
        char *p = line;
        while (*p == ' ')
        {
            p++;
        }
        if (*p == '#')
        {
            continue;
        }
        if ((val = sscanf(p, "%d,%lu,%llu,%lu\n", &c, &fid, &bytes, &num_bytes)) == 4)
        {
            report->ins_count++;

            lba = bytes / BLOCK_SIZE;
            remainder = bytes % BLOCK_SIZE;
            remain = (remainder == 0 ? 0 : BLOCK_SIZE - remainder);
            n = 0;
            if (num_bytes == 0)
            {
                n = 0;
            }
            else if (remain < num_bytes)
            {
                n = !!remain;
                left = num_bytes - remain;
                n += (left / BLOCK_SIZE) + !!(left % BLOCK_SIZE);
            }
            else
            {
                n = 1;
            }

            switch (c)
            {
            case 1:
                report->read_ins_count++;
                disk->d_op->read(disk, lba, n, fid);
                break;

            case 2:
                report->write_ins_count++;
                int commit_mode;
                if (commit_count == 1000UL)
                {
                    hybrid_journaling_hotness_bound = commit_accumulate / 1000UL;
                    commit_count = 0UL;
                    commit_accumulate = 0UL;
                }
                else
                {
                    commit_count++;
                    commit_accumulate += n;
                }

                if (n < hybrid_journaling_hotness_bound)
                {
                    commit_mode = 0; // CMR
                }
                else
                {
                    commit_mode = 1; // SMR
                }

                // Commit
                // Check and write update data
                unsigned long *hybrid_journaling_update_list = malloc((n) * sizeof(unsigned long));
                unsigned long hybrid_journaling_update_count = 0UL;
                for (unsigned long check_lba = lba; check_lba < lba + n; check_lba++)
                {
                    bool found = false;
                    for (unsigned long check_update_zone = 0UL; check_update_zone < hybrid_used_zone_count; check_update_zone++)
                    {
                        // hybrid_used_zone_count 不是 0 嗎 ==
                        if (!hybrid_zone[check_update_zone].isOffline && hybrid_zone[check_update_zone].isJournaling)
                        {
                            for (unsigned long check_update = 0UL; check_update < hybrid_zone[check_update_zone].write_head_pointer; check_update++)
                            {
                                if (hybrid_zone[check_update_zone].blocks[check_update].fileID == fid && hybrid_zone[check_update_zone].blocks[check_update].original_lba == check_lba)
                                {
                                    hybrid_journaling_update_list[hybrid_journaling_update_count] = check_lba;
                                    hybrid_journaling_update_count++;
                                    disk->d_op->write(disk, hybrid_zone[check_update_zone].logical_disk_LBA + check_update, 1, fid);
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if (found)
                        {
                            break;
                        }
                    }
                    if (found)
                    {
                        break;
                    }
                }

                unsigned long lba_ptr = lba;
                unsigned long n_ptr = n;
                unsigned long update_start_ptr = 0;
                // unsigned long hybrid_journaling_ptr;
                while (n_ptr > 0UL)
                {
                    for (unsigned long check_free_zone = 0UL; check_free_zone < hybrid_used_zone_count; check_free_zone++)
                    {
                        // 這邊甚至因為不會跑 for 迴圈的內容，造成 n_ptr 永遠大於 0，所以一直在跑無窮迴圈
                        if (!hybrid_zone[check_free_zone].isOffline && hybrid_zone[check_free_zone].isJournaling && !hybrid_zone[check_free_zone].isFull && hybrid_zone[check_free_zone].zone_type == commit_mode)
                        {
                            unsigned long whp = hybrid_zone[check_free_zone].write_head_pointer;
                            unsigned long zone_n = 0;
                            while (whp < HYBRID_ZONE_TRACK_COUNT && n_ptr > 0UL)
                            {
                                if (hybrid_zone[check_free_zone].blocks[whp].isSEALED)
                                {
                                    hybrid_zone[check_free_zone].isFull = true;
                                    break;
                                }
                                if (lba_ptr != hybrid_journaling_update_list[update_start_ptr])
                                {
                                    hybrid_zone[check_free_zone].blocks[whp].fileID = fid;
                                    hybrid_zone[check_free_zone].blocks[whp].original_lba = lba_ptr;
                                    lba_ptr++;
                                    zone_n++;
                                    n_ptr--;
                                    whp++;
                                }
                                else
                                {
                                    lba_ptr++;
                                    n_ptr--;
                                    if (update_start_ptr < hybrid_journaling_update_count - 1UL)
                                    {
                                        update_start_ptr++;
                                    }
                                }

                                if (whp == HYBRID_ZONE_TRACK_COUNT)
                                {
                                    hybrid_zone[check_free_zone].isFull = true;
                                }
                            }

                            disk->d_op->write(disk, hybrid_zone[check_free_zone].logical_disk_LBA + hybrid_zone[check_free_zone].write_head_pointer, zone_n, fid);
                            hybrid_zone[check_free_zone].write_head_pointer = whp;
                            hybrid_journaling_block_remain -= zone_n;
                        }
                        if (n_ptr <= 0)
                        {
                            break;
                        }
                    }
                    /*
                    int checkpoint_mode;
                    if(checkpoint_count == 1000UL) {
                        hybrid_hotness_bound = checkpoint_accumulate/1000UL;
                        checkpoint_count = 0UL;
                        checkpoint_accumulate = 0UL;
                    }
                    else {
                        checkpoint_count++;
                        checkpoint_accumulate += n;
                    }

                    if(n < hybrid_hotness_bound) {
                        checkpoint_mode = 0; //CMR
                    }
                    else {
                        checkpoint_mode = 1; //SMR
                    }
                    */
                    if (n_ptr > 0 && hybrid_journaling_zone_count < hybrid_journaling_zone_limit)
                    {
                        for (unsigned long check_offline_zone = 0UL; check_offline_zone < total_bands; check_offline_zone++)
                        {
                            if (hybrid_zone[check_offline_zone].isOffline)
                            {
                                hybrid_zone[check_offline_zone].isOffline = false;
                                hybrid_zone[check_offline_zone].isJournaling = true;
                                hybrid_zone[check_offline_zone].zone_type = commit_mode;

                                for (unsigned long i = 0UL; i < HYBRID_ZONE_TRACK_COUNT; i++)
                                {
                                    if (commit_mode == 0 && i >= HYBRID_ZONE_CMR_TRACK_COUNT)
                                    {
                                        hybrid_zone[check_offline_zone].blocks[i].isSEALED = true;
                                    }
                                    else
                                    {
                                        hybrid_zone[check_offline_zone].blocks[i].isSEALED = false;
                                        hybrid_journaling_block_remain++;
                                    }
                                }
                                hybrid_journaling_zone_count++;
                                break;
                            }
                        }
                    }
                    else if (n_ptr > 0 && hybrid_journaling_zone_count >= hybrid_journaling_zone_limit)
                    {
                        // Checkpoint
                    }
                }
                // disk->d_op->write(disk, lba, n, fid);
                break;

            case 3:
                report->delete_ins_count++;
                disk->d_op->remove(disk, lba, n, fid);
                break;

            default:
                fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized mode. mode: %d\n", c);
                break;
            }
        }
        else
        {
            fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized format.\n");
            exit(EXIT_FAILURE);
        }
        if (!(num_traces % percent))
        {
            putchar('#');
            fflush(stdout);
        }
        if (!(num_traces % ten_percent))
        {
            putchar('\n');
            fflush(stdout);
        }
    }
    free(line);
}

/*
void parsing_hybrid_csv(struct disk *disk, FILE *stream) {
    unsigned long lba, n, val;
    char *line = NULL;
    ssize_t nread;
    size_t len;
    unsigned long fid, remain, remainder, num_bytes, left, num_traces, percent, ten_percent, total_traces;
    int c;
    struct report *report = &disk->report;
    num_traces = 4500000;
    percent = num_traces / 100;
    ten_percent = num_traces / 10;
    total_traces = 0;

    size_t jLimit = 10; //10%
    jareaZoneCount = (total_bands * jLimit) / 100;
    jareaSize = jareaZoneCount * BAND_BLOCK_COUNT;
    jareaSMRCount = jareaZoneCount;
    jareaCMRCount = jareaZoneCount - jareaSMRCount;

    unsigned long jArea_block_used = 0;
    unsigned long jZonePointer = 0;
    unsigned long jBlockPointer = 0;
    unsigned long jArea_block_remain;
    unsigned long jArea_offset = jareaSize;
    //jH_B = malloc(jareaSize * sizeof(struct jHybrid_Block));
    //jH_Z = malloc(jareaZoneCount * sizeof(struct jHybrid_Zone));
    //init_jarea_hybrid(jH_Z, jareaSize, jareaZoneCount);
    jHybrid_Area = malloc(jareaSize * sizeof(struct jHybrid));
    init_jHybrid(jHybrid_Area, jareaSize);

    unsigned long *trace_ins_list;
    trace_ins_list = malloc((num_traces + 1) * sizeof(unsigned long));
    //unsigned long request_counter = 0;
    unsigned long avg_request_size = 8;

    while((num_traces--) && ((nread = getline(&line, &len, stream)) != -1)) {
        char *p = line;
        while(*p == ' ') {
            p++;
        }
        if(*p == '#') {
            continue;
        }
        if((val = sscanf(p, "%d,%lu,%llu,%lu\n", &c, &fid, &bytes, &num_bytes)) == 4) {
            report->ins_count++;

            lba = bytes / BLOCK_SIZE;
            remainder = bytes % BLOCK_SIZE;
            remain = (remainder == 0 ? 0 : BLOCK_SIZE - remainder);
            n = 0;
            if(num_bytes == 0) {
                n = 0;
            }
            else if(remain < num_bytes) {
                n = !!remain;
                left = num_bytes - remain;
                n += (left / BLOCK_SIZE) + !!(left % BLOCK_SIZE);
            }
            else {
                n = 1;
            }

            total_traces++;

            switch(c) {
                case 1:
                    //printf("Read Case\n");
                    break;

                case 2:
                {
                    //bool isCheckpoint = false;
                    trace_ins_list[total_traces] = lba;
                    while((jArea_block_remain = jareaSize - jArea_block_used) < n) {
                        if(jArea_block_remain == 0) {
                            //printf("Hi\n\n\n");
                            //disk->d_op->remove(disk, 0, jareaSize, 1000);
                            unsigned long req_count = 0;
                            unsigned long check_point_fid;
                            unsigned long check_point_n;
                            bool isEmpty = true;
                            for(size_t jArea_count = 0; jArea_count < jareaSize; jArea_count++) {
                                if(jHybrid_Area[jArea_count].inUsed) {
                                    isEmpty = false;
                                    if(req_count == 0) {
                                        req_count = jHybrid_Area[jArea_count].reqNum;
                                        check_point_fid = jHybrid_Area[jArea_count].fid;
                                        check_point_n = 1;
                                        jHybrid_Area[jArea_count].inUsed = false;
                                    }
                                    else if(req_count == jHybrid_Area[jArea_count].reqNum) {
                                        check_point_n++;
                                        jHybrid_Area[jArea_count].inUsed = false;
                                    }
                                    else if(req_count != jHybrid_Area[jArea_count].reqNum) {
                                        //request_counter++;
                                        unsigned long check_point_lba = trace_ins_list[req_count] + jArea_offset;
                                        unsigned long check_point_zone = check_point_lba/BAND_BLOCK_COUNT;
                                        bool changeMark = false; //default
                                        if(check_point_n < avg_request_size) {
                                            changeMark = true; //CMR
                                        }

                                        unsigned long zone_across = 1;
                                        //if(!band[check_point_zone].inUsed) {
                                        if(band[check_point_zone].write_head_p/TRACK_SIZE == band[check_point_zone].band_head/TRACK_SIZE) {
                                            zone_across = check_point_n/BAND_BLOCK_COUNT;
                                            if(check_point_n%BAND_BLOCK_COUNT > BAND_BLOCK_COUNT_CMR) {
                                                zone_across++;
                                            }
                                        }
                                        else {
                                            unsigned long zone_remain;
                                            if(!band[check_point_zone].flagCMR) {
                                                zone_remain = band[check_point_zone].band_end - band[check_point_zone].write_head_p;
                                            }
                                            else {
                                                zone_remain = band[check_point_zone].band_CMR_end - band[check_point_zone].write_head_p;
                                            }
                                            zone_across += (check_point_n - zone_remain)/BAND_BLOCK_COUNT;
                                            if(check_point_n%BAND_BLOCK_COUNT > BAND_BLOCK_COUNT_CMR) {
                                                zone_across++;
                                            }
                                        }
                                        trace_ins_list[req_count] += check_point_n;

                                        for(size_t i = 0; i < zone_across; i++) {
                                            //if(!band[check_point_zone + zone_across].inUsed) {
                                            if(band[check_point_zone].write_head_p/TRACK_SIZE == band[check_point_zone].band_head/TRACK_SIZE) {
                                                band[check_point_zone + zone_across].flagCMR = changeMark;
                                            }
                                            if(band[check_point_zone + zone_across].flagCMR) {
                                                check_point_n += BAND_BLOCK_COUNT - BAND_BLOCK_COUNT_CMR;
                                            }
                                        }
                                        printf("\n******\n");
                                        report->write_ins_count++;
                                        disk->d_op->write(disk, check_point_lba + jArea_offset, check_point_n, check_point_fid);

                                        req_count = jHybrid_Area[jArea_count].reqNum;
                                        check_point_fid = jHybrid_Area[jArea_count].fid;
                                        check_point_n = 1;
                                        jHybrid_Area[jArea_count].inUsed = false;
                                    }
                                }
                            }
                            if(isEmpty == false) {
                                //request_counter++;
                                unsigned long check_point_lba = trace_ins_list[req_count] + jArea_offset;
                                unsigned long check_point_zone = check_point_lba/BAND_BLOCK_COUNT;
                                bool changeMark = false; //default
                                if(check_point_n < avg_request_size) {
                                    changeMark = true; //CMR
                                }

                                unsigned long zone_across = 1;
                                //if(!band[check_point_zone].inUsed) {
                                if(band[check_point_zone].write_head_p/TRACK_SIZE == band[check_point_zone].band_head/TRACK_SIZE) {
                                    zone_across = check_point_n/BAND_BLOCK_COUNT;
                                    if(check_point_n%BAND_BLOCK_COUNT > BAND_BLOCK_COUNT_CMR) {
                                        zone_across++;
                                    }
                                    //band[check_point_zone].inUsed = true;
                                }
                                else {
                                    unsigned long zone_remain;
                                    if(!band[check_point_zone].flagCMR) {
                                        zone_remain = band[check_point_zone].band_end - band[check_point_zone].write_head_p;
                                    }
                                    else {
                                        zone_remain = band[check_point_zone].band_CMR_end - band[check_point_zone].write_head_p;
                                    }
                                    zone_across += (check_point_n - zone_remain)/BAND_BLOCK_COUNT;
                                    if(check_point_n%BAND_BLOCK_COUNT > BAND_BLOCK_COUNT_CMR) {
                                        zone_across++;
                                    }
                                }
                                trace_ins_list[req_count] += check_point_n;

                                for(size_t i = 0; i < zone_across; i++) {
                                    //if(!band[check_point_zone + zone_across].inUsed) {
                                    if(band[check_point_zone + zone_across].write_head_p/TRACK_SIZE == band[check_point_zone + zone_across].band_head/TRACK_SIZE) {
                                        band[check_point_zone + zone_across].flagCMR = changeMark;
                                    }
                                    if(band[check_point_zone + zone_across].flagCMR) {
                                        check_point_n += BAND_BLOCK_COUNT - BAND_BLOCK_COUNT_CMR;
                                    }
                                }
                                printf("\n+++++++++\n");
                                report->write_ins_count++;
                                disk->d_op->write(disk, check_point_lba + jArea_offset, check_point_n, check_point_fid);
                            }

                            printf("Finish Check Point\n\n");
                            disk->d_op->remove(disk, 0, jareaSize, 666);
                            jArea_block_used = 0;
                            jBlockPointer = 0;
                            jZonePointer = 0;
                        }
                        else {
                            printf("\nAlmost fulllll!!\n");
                            unsigned long zone_across = 1;
                            //if(!band[jZonePointer].inUsed) {
                            if(band[jZonePointer].write_head_p/TRACK_SIZE == band[jZonePointer].band_head/TRACK_SIZE) {
                                zone_across = n/BAND_BLOCK_COUNT;
                                if(n%BAND_BLOCK_COUNT > BAND_BLOCK_COUNT_CMR) {
                                    zone_across++;
                                }
                            }
                            else {
                                unsigned long zone_remain;
                                if(!band[jZonePointer].flagCMR) {
                                    zone_remain = band[jZonePointer].band_end - band[jZonePointer].write_head_p;
                                }
                                else {
                                    zone_remain = band[jZonePointer].band_CMR_end - band[jZonePointer].write_head_p;
                                }
                                zone_across += (n - zone_remain)/BAND_BLOCK_COUNT;
                                if(n%BAND_BLOCK_COUNT > BAND_BLOCK_COUNT_CMR) {
                                    zone_across++;
                                }
                            }
                            jArea_block_used += n;
                            for(size_t i = 0; i < zone_across; i++) {
                                if(band[jZonePointer + zone_across].flagCMR) {
                                    n += BAND_BLOCK_COUNT - BAND_BLOCK_COUNT_CMR;
                                }
                            }
                            //report->write_ins_count++;
                            //disk->d_op->write(disk, jBlockPointer, n, fid);
                            jBlockPointer += n;
                            jZonePointer = jBlockPointer/BAND_BLOCK_COUNT;
                        }
                        //break;
                    }
                    unsigned long zone_across = 1;
                    //if(!band[jZonePointer].inUsed) {
                    if(band[jZonePointer].write_head_p/TRACK_SIZE == band[jZonePointer].band_head/TRACK_SIZE) {
                        zone_across = n/BAND_BLOCK_COUNT;
                        if(n%BAND_BLOCK_COUNT > BAND_BLOCK_COUNT_CMR) {
                            zone_across++;
                        }
                    }
                    else {
                        unsigned long zone_remain;
                        if(!band[jZonePointer].flagCMR) {
                            zone_remain = band[jZonePointer].band_end - band[jZonePointer].write_head_p;
                        }
                        else {
                            zone_remain = band[jZonePointer].band_CMR_end - band[jZonePointer].write_head_p;
                        }
                        zone_across += (n - zone_remain)/BAND_BLOCK_COUNT;
                        if(n%BAND_BLOCK_COUNT > BAND_BLOCK_COUNT_CMR) {
                            zone_across++;
                        }
                    }
                    jArea_block_used += n;
                    for(size_t i = 0; i < zone_across; i++) {
                        if(band[jZonePointer + zone_across].flagCMR) {
                            n += BAND_BLOCK_COUNT - BAND_BLOCK_COUNT_CMR;
                        }
                    }
                    report->write_ins_count++;
                    //disk->d_op->write(disk, jBlockPointer, n, fid);
                    disk->d_op->write(disk, jBlockPointer, n, 666);
                    jBlockPointer += n;
                    jZonePointer = jBlockPointer/BAND_BLOCK_COUNT;

                    //printf()

                    break;
                }
                case 3:
                    //printf("Delete Case\n");
                    break;

                default:
                    fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized mode. mode: %d\n", c);
                    break;
            }
        }
        else {
            fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized format.\n");
            exit(EXIT_FAILURE);
        }
        if(!(num_traces % percent)) {
            putchar('#');
            fflush(stdout);
        }
        if(!(num_traces % ten_percent)) {
            putchar('\n');
            fflush(stdout);
        }
    }
    free(line);
}
*/
void fluidsmr_parsing_csv(struct disk *disk, FILE *stream)
{
    unsigned long lba, n, val;
    char *line = NULL;
    ssize_t nread;
    size_t len;
    // unsigned long fid, remain, remainder, num_bytes, left, num_traces, percent, ten_percent, total_traces;
    unsigned long fid, remain, remainder, num_bytes, left, num_traces, percent, ten_percent;
    int c, hint;
    struct report *report = &disk->report;
    num_traces = 4500000;
    percent = num_traces / 100;
    ten_percent = num_traces / 10;
    // total_traces = 0;

    block_remain = report->max_block_num;
    used_zone_count = 0;
    extendable_zone = malloc((total_bands) * sizeof(struct EXTENDABLE_SPACE_ZONE));
    unsigned long rrr = 3;

    unsigned long R_total = 0;
    unsigned long C_total = 0;

    for (int zone = 0; zone < total_bands; zone++)
    {
        extendable_zone[zone].zone_type = 2;
        extendable_zone[zone].isUsed = false;
        extendable_zone[zone].NSW_amount = 0;
        extendable_zone[zone].write_head_pointer = 0;
        extendable_zone[zone].isFull = false;
        extendable_zone[zone].isCache = false;
    }

    cmr_boundary = 0;
    smr_boundary = total_bands - 1;

    while ((num_traces--) && ((nread = getline(&line, &len, stream)) != -1))
    {
        char *p = line;
        while (*p == ' ')
        {
            p++;
        }
        if (*p == '#')
        {
            continue;
        }
        // if((val = sscanf(p, "%d,%lu,%llu,%lu\n", &c, &fid, &bytes, &num_bytes)) == 4) {
        if ((val = sscanf(p, "%d,%lu,%llu,%lu,%d\n", &c, &fid, &bytes, &num_bytes, &hint)) == 5)
        {
            report->ins_count++;

            lba = bytes / BLOCK_SIZE;
            remainder = bytes % BLOCK_SIZE;
            remain = (remainder == 0 ? 0 : BLOCK_SIZE - remainder);
            n = 0;
            if (num_bytes == 0)
            {
                n = 0;
            }
            else if (remain < num_bytes)
            {
                n = !!remain;
                left = num_bytes - remain;
                n += (left / BLOCK_SIZE) + !!(left % BLOCK_SIZE);
            }
            else
            {
                n = 1;
            }

            // fprintf(stderr, "**%lu**%d**\n", total_traces, c);

            // total_traces++;

            switch (c)
            {
            case 1:
                report->read_ins_count++;
                for (size_t i = 0; i < used_zone_count; i++)
                {
                    unsigned long fid_found = 0;
                    for (size_t j = 0; j < extendable_zone[i].write_head_pointer; j++)
                    {
                        if (extendable_zone[i].blocks[j].isSEALED)
                        {
                            break;
                        }
                        if (extendable_zone[i].blocks[j].fileID == fid && (extendable_zone[i].blocks[j].original_lba >= lba && extendable_zone[i].blocks[j].original_lba < lba + n))
                        {
                            // fprintf(stderr, "********************\n\n");
                            // fprintf(stderr, "Found: %lu\n", extendable_zone[i].blocks[j].original_lba);
                            fid_found++;
                        }
                    }
                    if (fid_found > 0)
                    {
                        disk->d_op->read(disk, extendable_zone[i].logical_disk_LBA, fid_found, fid);
                        n -= fid_found;
                        if (n <= 0)
                        {
                            break;
                        }
                    }
                }
                break;

            case 2:
                // disk->d_op->write(disk, lba, n, fid);
                report->write_ins_count++;
                unsigned long *update_list = malloc((n) * sizeof(unsigned long));
                unsigned long update_count = 0;

                for (unsigned long check_lba = lba; check_lba < lba + n; check_lba++)
                {
                    bool found = false;
                    for (int check_update_zone = 0; check_update_zone < used_zone_count; check_update_zone++)
                    {
                        if (extendable_zone[check_update_zone].isCache)
                        {
                            for (int check_update = 0; check_update < extendable_zone[check_update_zone].write_head_pointer; check_update++)
                            {
                                if (extendable_zone[check_update_zone].blocks[check_update].fileID == fid && extendable_zone[check_update_zone].blocks[check_update].original_lba == check_lba)
                                {
                                    update_list[update_count] = check_lba;
                                    update_count++;
                                    disk->d_op->write(disk, extendable_zone[check_update_zone].logical_disk_LBA + check_update, 1, fid);
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if (found)
                        {
                            break;
                        }
                    }
                    if (found)
                    {
                        break;
                    }

                    for (int check_update_zone = 0; check_update_zone < used_zone_count; check_update_zone++)
                    {
                        if (!extendable_zone[check_update_zone].isCache)
                        {
                            for (int check_update = 0; check_update < extendable_zone[check_update_zone].write_head_pointer; check_update++)
                            {
                                if (extendable_zone[check_update_zone].blocks[check_update].fileID == fid && extendable_zone[check_update_zone].blocks[check_update].original_lba == check_lba)
                                {
                                    update_list[update_count] = check_lba;
                                    update_count++;

                                    if (extendable_zone[check_update_zone].zone_type == 0 || C_total < 1)
                                    {
                                        disk->d_op->write(disk, extendable_zone[check_update_zone].logical_disk_LBA + check_update, 1, fid);
                                    }
                                    else if (extendable_zone[check_update_zone].zone_type == 1)
                                    {
                                        for (int zone_num = 0; zone_num < used_zone_count; zone_num++)
                                        {
                                            if (extendable_zone[zone_num].isCache && !extendable_zone[zone_num].isFull)
                                            {
                                                extendable_zone[zone_num].blocks[extendable_zone[zone_num].write_head_pointer].fileID = fid;
                                                extendable_zone[zone_num].blocks[extendable_zone[zone_num].write_head_pointer].original_lba = check_lba;
                                                disk->d_op->write(disk, extendable_zone[zone_num].logical_disk_LBA + extendable_zone[zone_num].write_head_pointer, 1, fid);
                                                extendable_zone[zone_num].write_head_pointer++;
                                                C_total--;
                                                if (extendable_zone[zone_num].write_head_pointer >= FLUID_ZONE_CMR_TRACK_COUNT)
                                                {
                                                    extendable_zone[zone_num].isFull = true;
                                                }
                                            }
                                        }
                                    }

                                    found = true;
                                    break;
                                }
                            }
                        }
                        if (found)
                        {
                            break;
                        }
                    }
                    if (found)
                    {
                        break;
                    }
                }
                /*
                for(int www = 0; www < update_count; www++) {
                    fprintf(stderr, "Need update:%lu\n", update_list[www]);
                }
                */

                if (block_remain < n)
                {
                    unsigned long *q_cmr = malloc((cmr_count) * sizeof(unsigned long));
                    unsigned long *q_smr = malloc((smr_count) * sizeof(unsigned long));
                    size_t c_ptr = 0;
                    size_t s_ptr = 0;
                    for (int z = 0; z < used_zone_count; z++)
                    {
                        if (!extendable_zone[z].isCache)
                        {
                            R_total += extendable_zone[z].NSW_amount;
                            if (extendable_zone[z].zone_type == 0)
                            {
                                q_cmr[c_ptr] = z;
                                c_ptr++;
                            }
                            else
                            {
                                q_smr[s_ptr] = z;
                                s_ptr++;
                            }
                        }
                    }

                    if (R_total > C_total)
                    {
                        c_ptr = cmr_count;
                        s_ptr = smr_count;
                        unsigned long min_cmr_zone;
                        unsigned long z_cmr;
                        unsigned long z_cmr_NSW_amount = FLUID_ZONE_TRACK_COUNT;

                        unsigned long MAX_smr_zone;
                        unsigned long z_smr;
                        unsigned long z_smr_NSW_amount = 0;

                        while ((c_ptr > 0 && s_ptr > 0) && (C_total < R_total))
                        { // zone_swapping
                            // fprintf(stderr, "\nSWAP\n");
                            for (int i = 0; i < c_ptr; i++)
                            {
                                if (extendable_zone[q_cmr[i]].NSW_amount <= z_cmr_NSW_amount)
                                {
                                    min_cmr_zone = i;
                                    z_cmr_NSW_amount = extendable_zone[q_cmr[i]].NSW_amount;
                                }
                            }
                            z_cmr = q_cmr[min_cmr_zone];
                            q_cmr[min_cmr_zone] = q_cmr[c_ptr - 1];
                            c_ptr--;
                            if (extendable_zone[z_cmr].NSW_amount / extendable_zone[z_cmr].write_head_pointer > rrr)
                            {
                                break;
                            }

                            for (int i = 0; i < s_ptr; i++)
                            {
                                if (extendable_zone[q_smr[i]].NSW_amount >= z_smr_NSW_amount)
                                {
                                    MAX_smr_zone = i;
                                    z_smr_NSW_amount = extendable_zone[q_smr[i]].NSW_amount;
                                }
                            }
                            z_smr = q_smr[MAX_smr_zone];
                            q_smr[MAX_smr_zone] = q_smr[s_ptr - 1];
                            s_ptr--;
                            if (extendable_zone[z_smr].NSW_amount / extendable_zone[z_smr].write_head_pointer < rrr)
                            {
                                break;
                            }

                            R_total -= extendable_zone[z_smr].NSW_amount;
                            R_total -= extendable_zone[z_cmr].NSW_amount;
                            extendable_zone[z_smr].NSW_amount = 0;
                            extendable_zone[z_cmr].NSW_amount = 0;

                            unsigned long clear_ptr = 0;
                            unsigned long clear_fid = extendable_zone[z_cmr].blocks[0].fileID;
                            unsigned long clear_n = 1;
                            for (size_t i = 1; i < extendable_zone[z_cmr].write_head_pointer; i++)
                            {
                                if (extendable_zone[z_cmr].blocks[i].fileID == clear_fid)
                                {
                                    clear_n++;
                                }

                                else
                                {
                                    disk->d_op->remove(disk, extendable_zone[z_cmr].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    clear_ptr += clear_n;
                                    clear_fid = extendable_zone[z_cmr].blocks[i].fileID;
                                    clear_n = 1;
                                }
                            }
                            disk->d_op->remove(disk, extendable_zone[z_cmr].logical_disk_LBA + clear_ptr, clear_n, clear_fid);

                            clear_ptr = 0;
                            clear_fid = extendable_zone[z_smr].blocks[0].fileID;
                            clear_n = 1;
                            for (size_t i = 1; i < extendable_zone[z_smr].write_head_pointer; i++)
                            {
                                if (extendable_zone[z_smr].blocks[i].fileID == clear_fid)
                                {
                                    clear_n++;
                                }

                                else
                                {
                                    disk->d_op->remove(disk, extendable_zone[z_smr].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    clear_ptr += clear_n;
                                    clear_fid = extendable_zone[z_smr].blocks[i].fileID;
                                    clear_n = 1;
                                }
                            }
                            disk->d_op->remove(disk, extendable_zone[z_smr].logical_disk_LBA + clear_ptr, clear_n, clear_fid);

                            struct EXTENDABLE_SPACE_ZONE swap_buffer_zone;
                            swap_buffer_zone = extendable_zone[z_smr];
                            swap_buffer_zone.zone_type = 0;
                            swap_buffer_zone.logical_disk_LBA = extendable_zone[z_cmr].logical_disk_LBA;

                            unsigned long swap_buffer_lba = extendable_zone[z_smr].logical_disk_LBA;
                            extendable_zone[z_cmr] = extendable_zone[z_smr];
                            extendable_zone[z_cmr].zone_type = 1;
                            extendable_zone[z_cmr].logical_disk_LBA = swap_buffer_lba;

                            extendable_zone[z_smr] = swap_buffer_zone;

                            R_total -= (extendable_zone[z_smr].NSW_amount + extendable_zone[z_cmr].NSW_amount);
                            extendable_zone[z_smr].NSW_amount = 0;
                            extendable_zone[z_cmr].NSW_amount = 0;

                            for (size_t i = FLUID_ZONE_CMR_TRACK_COUNT; i < FLUID_ZONE_TRACK_COUNT; i++)
                            {
                                extendable_zone[z_cmr].blocks[i].isSEALED = false;
                                extendable_zone[z_smr].blocks[i].isSEALED = true;
                            }
                            if (extendable_zone[z_cmr].write_head_pointer == FLUID_ZONE_CMR_TRACK_COUNT)
                            {
                                extendable_zone[z_cmr].isFull = false;
                            }
                            if (extendable_zone[z_smr].write_head_pointer == FLUID_ZONE_CMR_TRACK_COUNT)
                            {
                                extendable_zone[z_cmr].isFull = true;
                            }

                            band[extendable_zone[z_cmr].logical_disk_LBA / FLUID_ZONE_TRACK_COUNT].flagCMR = false;
                            band[extendable_zone[z_smr].logical_disk_LBA / FLUID_ZONE_TRACK_COUNT].flagCMR = true;

                            clear_ptr = 0;
                            clear_fid = extendable_zone[z_cmr].blocks[0].fileID;
                            clear_n = 1;
                            for (size_t i = 1; i < extendable_zone[z_cmr].write_head_pointer; i++)
                            {
                                if (extendable_zone[z_cmr].blocks[i].fileID == clear_fid)
                                {
                                    clear_n++;
                                }

                                else
                                {
                                    disk->d_op->write(disk, extendable_zone[z_cmr].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    clear_ptr += clear_n;
                                    clear_fid = extendable_zone[z_cmr].blocks[i].fileID;
                                    clear_n = 1;
                                }
                            }
                            disk->d_op->write(disk, extendable_zone[z_cmr].logical_disk_LBA + clear_ptr, clear_n, clear_fid);

                            clear_ptr = 0;
                            clear_fid = extendable_zone[z_smr].blocks[0].fileID;
                            clear_n = 1;
                            for (size_t i = 1; i < extendable_zone[z_smr].write_head_pointer; i++)
                            {
                                if (extendable_zone[z_smr].blocks[i].fileID == clear_fid)
                                {
                                    clear_n++;
                                }

                                else
                                {
                                    disk->d_op->write(disk, extendable_zone[z_smr].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    clear_ptr += clear_n;
                                    clear_fid = extendable_zone[z_smr].blocks[i].fileID;
                                    clear_n = 1;
                                }
                            }
                            disk->d_op->write(disk, extendable_zone[z_smr].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                        }

                        size_t bubble_i, bubble_j, bubble_temp;

                        for (bubble_i = 0; bubble_i < c_ptr - 1; ++bubble_i)
                        {
                            for (bubble_j = 0; bubble_j < c_ptr - 1 - bubble_i; ++bubble_j)
                            {
                                if (extendable_zone[q_cmr[bubble_j]].NSW_amount < extendable_zone[q_cmr[bubble_j + 1]].NSW_amount)
                                {
                                    bubble_temp = q_cmr[bubble_j + 1];
                                    q_cmr[bubble_j + 1] = q_cmr[bubble_j];
                                    q_cmr[bubble_j] = bubble_temp;
                                }
                            }
                        }

                        int C_conv = total_bands * 30 / 100;
                        while ((c_ptr > C_conv) && (C_total < R_total))
                        {
                            // fprintf(stderr, "\nConver CMR -> SMR\n");
                            unsigned long avg_r = 0;
                            for (int i = c_ptr - 1 - C_conv; i < c_ptr; i++)
                            {
                                avg_r += extendable_zone[q_cmr[i]].NSW_amount / extendable_zone[q_cmr[i]].write_head_pointer;
                            }
                            avg_r /= C_conv;
                            if (avg_r > rrr)
                            {
                                break;
                            }

                            struct EXTENDABLE_SPACE_ZONE *conv_buffer_zone = malloc((3) * sizeof(struct EXTENDABLE_SPACE_ZONE));
                            for (size_t conv_count = 0; conv_count < (C_conv / 3); conv_count++)
                            {
                                conv_buffer_zone[0] = extendable_zone[q_cmr[c_ptr - 1]];
                                conv_buffer_zone[1] = extendable_zone[q_cmr[c_ptr - 2]];
                                conv_buffer_zone[2] = extendable_zone[q_cmr[c_ptr - 3]];
                                R_total -= (extendable_zone[q_cmr[c_ptr - 1]].NSW_amount + extendable_zone[q_cmr[c_ptr - 2]].NSW_amount + extendable_zone[q_cmr[c_ptr - 3]].NSW_amount);

                                for (size_t cc = 1; cc < 4; cc++)
                                {
                                    unsigned long clear_ptr = 0;
                                    unsigned long clear_fid = extendable_zone[q_cmr[c_ptr - cc]].blocks[0].fileID;
                                    unsigned long clear_n = 1;
                                    for (size_t i = 1; i < extendable_zone[q_cmr[c_ptr - cc]].write_head_pointer; i++)
                                    {
                                        if (extendable_zone[q_cmr[c_ptr - cc]].blocks[i].fileID == clear_fid)
                                        {
                                            clear_n++;
                                        }
                                        else
                                        {
                                            disk->d_op->remove(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                            block_remain += clear_n;
                                            clear_ptr += clear_n;
                                            clear_fid = extendable_zone[q_cmr[c_ptr - cc]].blocks[i].fileID;
                                            clear_n = 1;
                                        }
                                    }
                                    disk->d_op->remove(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    block_remain += clear_n;
                                    // extendable_zone[q_cmr[ccc]].logical_disk_LBA = smr_boundary*FLUID_ZONE_TRACK_COUNT;
                                }
                                for (size_t cc = 1; cc < 4; cc++)
                                {
                                    extendable_zone[q_cmr[c_ptr - cc]].isUsed = true;
                                    extendable_zone[q_cmr[c_ptr - cc]].zone_type = 1;
                                    extendable_zone[q_cmr[c_ptr - cc]].isFull = false;
                                    extendable_zone[q_cmr[c_ptr - cc]].NSW_amount = 0;
                                    extendable_zone[q_cmr[c_ptr - cc]].write_head_pointer = 0;
                                    band[extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA / FLUID_ZONE_TRACK_COUNT].flagCMR = false;

                                    for (size_t i = FLUID_ZONE_CMR_TRACK_COUNT; i < FLUID_ZONE_TRACK_COUNT; i++)
                                    {
                                        extendable_zone[q_cmr[c_ptr - cc]].blocks[i].isSEALED = false;
                                        block_remain++;
                                    }
                                    cmr_count--;
                                    smr_count++;
                                }

                                for (size_t cc = 1; cc < 4; cc++)
                                {
                                    for (unsigned long i = 0; i < FLUID_ZONE_TRACK_COUNT; i++)
                                    {
                                        disk->d_op->write(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + i, 1UL, 0UL);
                                    }
                                    for (unsigned long i = 0; i < FLUID_ZONE_TRACK_COUNT; i++)
                                    {
                                        disk->d_op->remove(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + i, 1UL, 0UL);
                                    }
                                }

                                extendable_zone[used_zone_count].isUsed = true;
                                extendable_zone[used_zone_count].zone_type = 0;
                                extendable_zone[used_zone_count].isCache = true;
                                band[extendable_zone[used_zone_count].logical_disk_LBA / FLUID_ZONE_TRACK_COUNT].flagCACHE = true;
                                for (size_t i = 0; i < FLUID_ZONE_TRACK_COUNT; i++)
                                {
                                    extendable_zone[used_zone_count].blocks[i].isSEALED = false;
                                }
                                for (size_t i = FLUID_ZONE_CMR_TRACK_COUNT; i < FLUID_ZONE_TRACK_COUNT; i++)
                                {
                                    extendable_zone[used_zone_count].blocks[i].isSEALED = true;
                                    block_remain--;
                                }
                                cmr_count++;
                                extendable_zone[used_zone_count].logical_disk_LBA = cmr_boundary * FLUID_ZONE_TRACK_COUNT;
                                band[cmr_boundary].flagCMR = true;
                                cmr_boundary++;

                                C_total += FLUID_ZONE_CMR_TRACK_COUNT;

                                size_t b_i = 0;
                                size_t s_i = 1;
                                size_t bi_ptr = 0;
                                size_t si_ptr = 0;
                                while (b_i < 3 && s_i < 4)
                                {
                                    extendable_zone[q_cmr[c_ptr - s_i]].blocks[si_ptr].fileID = conv_buffer_zone[b_i].blocks[bi_ptr].fileID;
                                    extendable_zone[q_cmr[c_ptr - s_i]].blocks[si_ptr].original_lba = conv_buffer_zone[b_i].blocks[bi_ptr].original_lba;
                                    extendable_zone[q_cmr[c_ptr - s_i]].write_head_pointer++;
                                    si_ptr++;
                                    bi_ptr++;
                                    if (si_ptr == FLUID_ZONE_TRACK_COUNT)
                                    {
                                        s_i++;
                                        si_ptr = 0;
                                    }
                                    if (bi_ptr == conv_buffer_zone[b_i].write_head_pointer)
                                    {
                                        b_i++;
                                        bi_ptr = 0;
                                    }
                                }

                                for (size_t cc = 1; cc < 4; cc++)
                                {
                                    unsigned long clear_ptr = 0;
                                    unsigned long clear_fid = extendable_zone[q_cmr[c_ptr - cc]].blocks[0].fileID;
                                    unsigned long clear_n = 1;
                                    for (size_t i = 1; i < extendable_zone[q_cmr[c_ptr - cc]].write_head_pointer; i++)
                                    {
                                        if (extendable_zone[q_cmr[c_ptr - cc]].blocks[i].fileID == clear_fid)
                                        {
                                            clear_n++;
                                        }

                                        else
                                        {
                                            disk->d_op->write(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                            disk->d_op->remove(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                            disk->d_op->write(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                            clear_ptr += clear_n;
                                            clear_fid = extendable_zone[q_cmr[c_ptr - cc]].blocks[i].fileID;
                                            clear_n = 1;
                                        }
                                    }
                                    disk->d_op->write(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    disk->d_op->remove(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    disk->d_op->write(disk, extendable_zone[q_cmr[c_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    block_remain -= clear_n;
                                }
                                c_ptr -= 3;
                            }
                        }

                        for (bubble_i = 0; bubble_i < s_ptr - 1; ++bubble_i)
                        {
                            for (bubble_j = 0; bubble_j < s_ptr - 1 - bubble_i; ++bubble_j)
                            {
                                if (extendable_zone[q_smr[bubble_j]].NSW_amount > extendable_zone[q_smr[bubble_j + 1]].NSW_amount)
                                {
                                    bubble_temp = q_smr[bubble_j + 1];
                                    q_smr[bubble_j + 1] = q_smr[bubble_j];
                                    q_smr[bubble_j] = bubble_temp;
                                }
                            }
                        }

                        while ((s_ptr > C_conv) && (C_total < R_total))
                        {
                            // fprintf(stderr, "\nConver SMR -> CMR\n");
                            unsigned long avg_r = 0;

                            for (int i = s_ptr - 1 - C_conv; i < s_ptr; i++)
                            {
                                avg_r += extendable_zone[q_smr[i]].NSW_amount / extendable_zone[q_smr[i]].write_head_pointer;
                            }
                            avg_r /= C_conv;
                            if (avg_r > rrr)
                            {
                                break;
                            }

                            struct EXTENDABLE_SPACE_ZONE *conv_buffer_zone = malloc((2) * sizeof(struct EXTENDABLE_SPACE_ZONE));
                            for (size_t conv_count = 0; conv_count < (C_conv / 2); conv_count++)
                            {
                                // fprintf(stderr, "%lu\n", C_total);
                                if (C_total < 256)
                                {
                                    break;
                                }
                                // fprintf(stderr, "%zu\n", s_ptr);
                                conv_buffer_zone[0] = extendable_zone[q_smr[s_ptr - 1]];
                                conv_buffer_zone[1] = extendable_zone[q_smr[s_ptr - 2]];

                                R_total -= (extendable_zone[q_smr[s_ptr - 1]].NSW_amount + extendable_zone[q_smr[s_ptr - 2]].NSW_amount);

                                // fprintf(stderr, "???????\n");
                                bool found = false;
                                size_t free_cache = -1;
                                for (size_t find_cache = 0; find_cache < used_zone_count; find_cache++)
                                {
                                    // fprintf(stderr, "\nClean Cache\n");
                                    if (extendable_zone[find_cache].isCache)
                                    {
                                        free_cache = find_cache;
                                        for (size_t i = 0; i < extendable_zone[find_cache].write_head_pointer; i++)
                                        {

                                            unsigned long check_lba = extendable_zone[find_cache].blocks[i].original_lba;
                                            unsigned long check_fid = extendable_zone[find_cache].blocks[i].fileID;
                                            // fprintf(stderr, "\nClean Cache\n");

                                            for (int check_update_zone = 0; check_update_zone < used_zone_count; check_update_zone++)
                                            {
                                                if (!extendable_zone[check_update_zone].isCache && check_update_zone != find_cache)
                                                {
                                                    for (int check_update = 0; check_update < extendable_zone[check_update_zone].write_head_pointer; check_update++)
                                                    {
                                                        if (extendable_zone[check_update_zone].blocks[check_update].fileID == check_fid && extendable_zone[check_update_zone].blocks[check_update].original_lba == check_lba)
                                                        {
                                                            disk->d_op->write(disk, extendable_zone[check_update_zone].logical_disk_LBA + check_update, 1, fid);
                                                            found = true;
                                                            break;
                                                        }
                                                    }
                                                }
                                                if (found)
                                                {
                                                    break;
                                                }
                                            }
                                        }
                                        // fprintf(stderr, "\nClean Cache\n");
                                        extendable_zone[find_cache].isCache = false;
                                        extendable_zone[find_cache].write_head_pointer = 0;
                                        extendable_zone[find_cache].isFull = false;
                                        band[extendable_zone[find_cache].logical_disk_LBA / FLUID_ZONE_TRACK_COUNT].flagCACHE = false;
                                    }
                                }
                                if (free_cache == -1)
                                {
                                    break;
                                }
                                C_total -= FLUID_ZONE_CMR_TRACK_COUNT;

                                for (size_t cc = 1; cc < 3; cc++)
                                {
                                    unsigned long clear_ptr = 0;
                                    unsigned long clear_fid = extendable_zone[q_smr[s_ptr - cc]].blocks[0].fileID;
                                    unsigned long clear_n = 1;
                                    for (size_t i = 1; i < extendable_zone[q_smr[s_ptr - cc]].write_head_pointer; i++)
                                    {
                                        if (extendable_zone[q_smr[s_ptr - cc]].blocks[i].fileID == clear_fid)
                                        {
                                            clear_n++;
                                        }
                                        else
                                        {
                                            disk->d_op->remove(disk, extendable_zone[q_smr[s_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                            block_remain += clear_n;
                                            clear_ptr += clear_n;
                                            clear_fid = extendable_zone[q_smr[s_ptr - cc]].blocks[i].fileID;
                                            clear_n = 1;
                                        }
                                    }
                                    disk->d_op->remove(disk, extendable_zone[q_smr[s_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    block_remain += clear_n;
                                    // extendable_zone[q_cmr[ccc]].logical_disk_LBA = smr_boundary*FLUID_ZONE_TRACK_COUNT;
                                }

                                for (size_t cc = 1; cc < 3; cc++)
                                {
                                    extendable_zone[q_smr[s_ptr - cc]].isUsed = true;
                                    extendable_zone[q_smr[s_ptr - cc]].zone_type = 0;
                                    extendable_zone[q_smr[s_ptr - cc]].isFull = false;
                                    extendable_zone[q_smr[s_ptr - cc]].NSW_amount = 0;
                                    extendable_zone[q_smr[s_ptr - cc]].write_head_pointer = 0;
                                    band[extendable_zone[q_smr[s_ptr - cc]].logical_disk_LBA / FLUID_ZONE_TRACK_COUNT].flagCMR = true;

                                    for (size_t i = FLUID_ZONE_CMR_TRACK_COUNT; i < FLUID_ZONE_TRACK_COUNT; i++)
                                    {
                                        extendable_zone[q_smr[s_ptr - cc]].blocks[i].isSEALED = true;
                                        block_remain--;
                                    }
                                    cmr_count++;
                                    smr_count--;
                                }

                                for (size_t cc = 1; cc < 3; cc++)
                                {
                                    for (unsigned long i = 0; i < FLUID_ZONE_CMR_TRACK_COUNT; i++)
                                    {
                                        disk->d_op->write(disk, extendable_zone[free_cache].logical_disk_LBA + i, 1UL, 0UL);
                                    }
                                    for (unsigned long i = 0; i < FLUID_ZONE_CMR_TRACK_COUNT; i++)
                                    {
                                        disk->d_op->remove(disk, extendable_zone[free_cache].logical_disk_LBA + i, 1UL, 0UL);
                                    }
                                }

                                // fprintf(stderr, "*** %lu ***\n", extendable_zone[free_cache].blocks[0].fileID);
                                // fprintf(stderr, "*** bug ***\n");
                                for (size_t i = 0; i < 2; i++)
                                {

                                    for (unsigned long j = FLUID_ZONE_CMR_TRACK_COUNT; j < FLUID_ZONE_TRACK_COUNT; j++)
                                    {

                                        extendable_zone[free_cache].blocks[extendable_zone[free_cache].write_head_pointer].fileID = conv_buffer_zone[i].blocks[j].fileID;
                                        extendable_zone[free_cache].blocks[extendable_zone[free_cache].write_head_pointer].original_lba = conv_buffer_zone[i].blocks[j].original_lba;
                                        extendable_zone[free_cache].write_head_pointer++;
                                        if (extendable_zone[free_cache].write_head_pointer == FLUID_ZONE_CMR_TRACK_COUNT)
                                        {
                                            extendable_zone[free_cache].isFull = true;
                                        }
                                    }
                                }
                                if (conv_buffer_zone[0].write_head_pointer > FLUID_ZONE_CMR_TRACK_COUNT)
                                {
                                    conv_buffer_zone[0].write_head_pointer = FLUID_ZONE_CMR_TRACK_COUNT;
                                }
                                if (conv_buffer_zone[1].write_head_pointer > FLUID_ZONE_CMR_TRACK_COUNT)
                                {
                                    conv_buffer_zone[1].write_head_pointer = FLUID_ZONE_CMR_TRACK_COUNT;
                                }

                                unsigned long clear_ptr = 0;
                                unsigned long clear_fid = extendable_zone[free_cache].blocks[0].fileID;
                                unsigned long clear_n = 1;
                                for (size_t i = 1; i < extendable_zone[free_cache].write_head_pointer; i++)
                                {
                                    if (extendable_zone[free_cache].blocks[i].fileID == clear_fid)
                                    {
                                        clear_n++;
                                    }

                                    else
                                    {
                                        disk->d_op->write(disk, extendable_zone[free_cache].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                        disk->d_op->remove(disk, extendable_zone[free_cache].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                        disk->d_op->write(disk, extendable_zone[free_cache].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                        clear_ptr += clear_n;
                                        clear_fid = extendable_zone[free_cache].blocks[i].fileID;
                                        clear_n = 1;
                                    }
                                }
                                disk->d_op->write(disk, extendable_zone[free_cache].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                disk->d_op->remove(disk, extendable_zone[free_cache].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                disk->d_op->write(disk, extendable_zone[free_cache].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                block_remain -= clear_n;

                                for (size_t i = 0; i < 2; i++)
                                {
                                    for (size_t j = 0; j < conv_buffer_zone[i].write_head_pointer; j++)
                                    {
                                        extendable_zone[q_smr[s_ptr - i + 1]].blocks[j].fileID = conv_buffer_zone[i].blocks[j].fileID;
                                        extendable_zone[q_smr[s_ptr - i + 1]].blocks[j].original_lba = conv_buffer_zone[i].blocks[j].original_lba;
                                        extendable_zone[q_smr[s_ptr - i + 1]].write_head_pointer++;
                                        if (extendable_zone[free_cache].write_head_pointer == FLUID_ZONE_CMR_TRACK_COUNT)
                                        {
                                            extendable_zone[free_cache].isFull = true;
                                        }
                                    }
                                }
                                for (size_t cc = 1; cc < 3; cc++)
                                {
                                    unsigned long clear_ptr = 0;
                                    unsigned long clear_fid = extendable_zone[q_smr[s_ptr - cc]].blocks[0].fileID;
                                    unsigned long clear_n = 1;
                                    for (size_t i = 1; i < extendable_zone[q_smr[s_ptr - cc]].write_head_pointer; i++)
                                    {
                                        if (extendable_zone[q_smr[s_ptr - cc]].blocks[i].fileID == clear_fid)
                                        {
                                            clear_n++;
                                        }

                                        else
                                        {
                                            disk->d_op->write(disk, extendable_zone[q_smr[s_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                            disk->d_op->remove(disk, extendable_zone[q_smr[s_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                            disk->d_op->write(disk, extendable_zone[q_smr[s_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                            clear_ptr += clear_n;
                                            clear_fid = extendable_zone[q_smr[s_ptr - cc]].blocks[i].fileID;
                                            clear_n = 1;
                                        }
                                    }
                                    disk->d_op->write(disk, extendable_zone[q_smr[s_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    disk->d_op->remove(disk, extendable_zone[q_smr[s_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    disk->d_op->write(disk, extendable_zone[q_smr[s_ptr - cc]].logical_disk_LBA + clear_ptr, clear_n, clear_fid);
                                    block_remain -= clear_n;
                                }
                                s_ptr -= 2;
                            }
                        }
                    }
                }

                unsigned long n_written = 0;
                unsigned long update_start_ptr = 0;
                for (int z = 0; z < used_zone_count; z++)
                {
                    if (!extendable_zone[z].isCache && !extendable_zone[z].isFull && extendable_zone[z].zone_type == hint)
                    {
                        unsigned long whp = extendable_zone[z].write_head_pointer;
                        unsigned long zone_n = 0;
                        while (whp < FLUID_ZONE_TRACK_COUNT && n > 0)
                        {
                            if (extendable_zone[z].blocks[whp].isSEALED)
                            {
                                extendable_zone[z].isFull = true;
                                break;
                            }
                            if (lba + n_written != update_list[update_start_ptr])
                            {
                                extendable_zone[z].blocks[whp].fileID = fid;
                                extendable_zone[z].blocks[whp].original_lba = lba + n_written;
                                n_written++;
                                zone_n++;
                                n--;
                                whp++;
                            }
                            else
                            {
                                n_written++;
                                n--;
                                if (update_start_ptr < update_count - 1)
                                {
                                    update_start_ptr++;
                                }
                            }

                            if (whp == FLUID_ZONE_TRACK_COUNT)
                            {
                                extendable_zone[z].isFull = true;
                            }
                        }

                        disk->d_op->write(disk, extendable_zone[z].logical_disk_LBA + extendable_zone[z].write_head_pointer, zone_n, fid);
                        extendable_zone[z].write_head_pointer = whp;
                        extendable_zone[z].NSW_amount += zone_n;
                        block_remain -= zone_n;
                    }
                    if (n <= 0)
                    {
                        break;
                    }
                }

                while (n > 0 && used_zone_count < total_bands)
                {
                    // init fluid zone
                    extendable_zone[used_zone_count].isUsed = true;
                    extendable_zone[used_zone_count].zone_type = hint;
                    for (size_t i = 0; i < FLUID_ZONE_TRACK_COUNT; i++)
                    {
                        extendable_zone[used_zone_count].blocks[i].isSEALED = false;
                    }
                    // init CMR zone
                    if (hint == 0)
                    {
                        for (size_t i = FLUID_ZONE_CMR_TRACK_COUNT; i < FLUID_ZONE_TRACK_COUNT; i++)
                        {
                            extendable_zone[used_zone_count].blocks[i].isSEALED = true;
                            block_remain--;
                        }
                        cmr_count++;
                        extendable_zone[used_zone_count].logical_disk_LBA = cmr_boundary * FLUID_ZONE_TRACK_COUNT;
                        band[cmr_boundary].flagCMR = true;
                        cmr_boundary++;
                    }
                    // init SMR zone
                    else
                    {
                        smr_count++;
                        extendable_zone[used_zone_count].logical_disk_LBA = smr_boundary * FLUID_ZONE_TRACK_COUNT;
                        band[smr_boundary].flagCMR = false;
                        smr_boundary--;
                    }
                    // write data
                    unsigned long whp = 0;
                    unsigned long zone_n = 0;
                    while (whp < FLUID_ZONE_TRACK_COUNT && n > 0)
                    {
                        if (extendable_zone[used_zone_count].blocks[whp].isSEALED)
                        {
                            extendable_zone[used_zone_count].isFull = true;
                            break;
                        }
                        if (lba + n_written != update_list[update_start_ptr])
                        {
                            extendable_zone[used_zone_count].blocks[whp].fileID = fid;
                            extendable_zone[used_zone_count].blocks[whp].original_lba = lba + n_written;
                            n_written++;
                            zone_n++;
                            n--;
                            whp++;
                        }
                        else
                        {
                            n_written++;
                            n--;
                            if (update_start_ptr < update_count - 1)
                            {
                                update_start_ptr++;
                            }
                        }

                        if (whp == FLUID_ZONE_TRACK_COUNT || (extendable_zone[used_zone_count].zone_type == 0 && whp == FLUID_ZONE_CMR_TRACK_COUNT))
                        {
                            extendable_zone[used_zone_count].isFull = true;
                        }
                    }
                    disk->d_op->write(disk, extendable_zone[used_zone_count].logical_disk_LBA + extendable_zone[used_zone_count].write_head_pointer, zone_n, fid);
                    extendable_zone[used_zone_count].write_head_pointer = whp;
                    block_remain -= zone_n;
                    used_zone_count++;
                }

                /*Print Extendable Space

                for(int bug = 0; bug < used_zone_count; bug++) {
                    fprintf(stderr, "\nZone:%d\n", bug);
                    for(int ii = 0; ii < extendable_zone[bug].write_head_pointer; ii++) {
                        fprintf(stderr, "block%d: %lu\n", ii, extendable_zone[bug].blocks[ii].original_lba);
                    }
                    fprintf(stderr, "Write Head Pointer:%lu\n", extendable_zone[bug].write_head_pointer);
                    if(extendable_zone[bug].isFull) {
                        fprintf(stderr, "Is Full!!\n");
                    }
                    else {
                        fprintf(stderr, "Not Full!!\n");
                    }
                }
                */
                // fprintf(stderr, "%d\n", hint);
                break;

            case 3:
                report->delete_ins_count++;
                for (size_t i = 0; i < used_zone_count; i++)
                {
                    unsigned long fid_found = 0;
                    for (size_t j = 0; j < extendable_zone[i].write_head_pointer; j++)
                    {
                        if (extendable_zone[i].blocks[j].isSEALED)
                        {
                            break;
                        }
                        if (extendable_zone[i].blocks[j].fileID == fid)
                        {
                            fid_found++;
                        }
                    }
                    if (fid_found > 0)
                    {
                        disk->d_op->read(disk, extendable_zone[i].logical_disk_LBA, fid_found, fid);
                        n -= fid_found;
                        if (extendable_zone[i].isFull)
                        {
                            extendable_zone[i].isFull = false;
                        }
                        if (n <= 0)
                        {
                            break;
                        }
                    }
                }
                // disk->d_op->remove(disk, lba, n, fid);
                break;

            default:
                fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized mode. mode: %d\n", c);
                break;
            }
        }
        else
        {
            fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized format.\n");
            exit(EXIT_FAILURE);
        }
        if (!(num_traces % percent))
        {
            putchar('#');
            fflush(stdout);
        }
        if (!(num_traces % ten_percent))
        {
            putchar('\n');
            fflush(stdout);
        }
    }
    free(line);

    size_t Free_Zone = 0;
    for (size_t i = 0; i < total_bands; i++)
    {
        if (!extendable_zone[i].isUsed)
        {
            Free_Zone++;
        }
    }
    data_usage = (total_bands - Free_Zone) / total_bands;
}

void start_parsing(struct disk *disk, char *file_name)
{
    FILE *stream;

    stream = fopen(file_name, "r");
    if (!stream)
    {
        fprintf(stderr, "ERROR: open file failed. %s\n", strerror(errno));
        return;
    }
    if (is_csv_flag)
    {
        parsing_csv(disk, stream);
    }
    else
    {
        parsing_postmark(disk, stream);
    }
    fclose(stream);
}

void start_parsing_cache(struct disk *cache, struct disk *disk, char *file_name)
{
    FILE *stream;

    stream = fopen(file_name, "r");
    if (!stream)
    {
        fprintf(stderr, "ERROR: open file failed. %s\n", strerror(errno));
        return;
    }
    if (is_csv_flag)
    {
        parsing_csv_cache(cache, disk, stream);
    }
    else
    {
        parsing_postmark_cache(cache, disk, stream);
        // printf("Postmark is not ready!\n\n");
    }
    fclose(stream);
}

void hybrid_start_parsing(struct disk *disk, char *file_name)
{
    FILE *stream;

    stream = fopen(file_name, "r");
    if (!stream)
    {
        fprintf(stderr, "ERROR: open file failed. %s\n", strerror(errno));
        return;
    }
    if (is_csv_flag)
    {
        // parsing_csv(disk, stream);
        parsing_hybrid_csv(disk, stream);
        printf("Parsing hybrid csv\n");
    }
    else
    {
        // parsing_postmark(disk, stream);
        printf("Parsing hybrid postmark\n");
    }
    fclose(stream);
}

void fluidsmr_start_parsing(struct disk *disk, char *file_name)
{
    FILE *stream;

    stream = fopen(file_name, "r");
    if (!stream)
    {
        fprintf(stderr, "ERROR: open file failed. %s\n", strerror(errno));
        return;
    }
    if (is_csv_flag)
    {
        fluidsmr_parsing_csv(disk, stream);
    }
    else
    {
        parsing_postmark(disk, stream);
    }
    fclose(stream);
}

int main(int argc, char **argv)
{
    int size;
    int opt = 0;
    size_t len;
    char input_file[MAX_LENS + 1];
    time_t start_time, end_time; /* elapsed timers */

    /* parse arguments */
    while ((opt = getopt(argc, argv, "cs:i:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            is_csv_flag = true;
            break;

        case 's':
            size = atoi(optarg);
            break;

        case 'i':
            len = strlen(optarg);
            if (len > MAX_LENS)
            {
                len = MAX_LENS;
            }
            strncpy(input_file, optarg, len);
            input_file[len] = '\0';
            break;

        default:
            fprintf(stderr, "Usage: %s (-c) [-s size(GB)] [-i input_file_name]\n", argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    /* create virtual disk */
    struct disk disk = {0};
    struct report *report = &disk.report;

    printf("Init disk...\n");
    if (init_disk(&disk, size, false))
    {
        fprintf(stderr, "ERROR: init_disk failed\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Init disk [OK]\n\n");
    }

#ifdef CACHE
    /* create virtual cmr cache */
    int cacheSize = size / 10;
    struct disk cache = {0};
    struct report *cacheReport = &cache.report;
    printf("Init cmr cache...\n");
    if (init_disk(&cache, cacheSize, true))
    {
        fprintf(stderr, "ERROR: init_cache failed\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Init cache [OK]\n\n");
    }
#endif
    /* parse operations file with cache*/
    printf("Parse instructions...\n");
    time(&start_time);
#ifdef CACHE
    start_parsing_cache(&cache, &disk, input_file);
#else
#ifdef HYBRID
    hybrid_start_parsing(&disk, input_file);
#else
#ifdef FLUIDSMR
    fluidsmr_start_parsing(&disk, input_file);
#else
    start_parsing(&disk, input_file);
#endif
#endif
#endif
    time(&end_time);
    printf("\n\nParse instructions [OK]\n\n");
    double elapsed = difftime(end_time, start_time);

#ifdef CACHE
    printf("*************************CACHE************************\n");
    printf("-------------------------\n");
    printf("Time information:\n\n");
    printf("%f seconds total\n", elapsed);

    printf("-------------------------\n");
    printf("Disk information:\n\n");
    printf("Size of cache = %d GB\n", size / 10);
    printf("Number of blocks = %llu\n", cacheReport->max_block_num);

    printf("-------------------------\n");
    printf("Transaction information:\n\n");
    printf("Total number of instructions        = %16llu instructions\n", cacheReport->ins_count);
    printf("Total number of read instructions   = %16llu instructions\n", cacheReport->read_ins_count);
    printf("Total number of write instructions  = %16llu instructions\n", cacheReport->write_ins_count);
    printf("Total number of invalid read        = %16llu blocks\n", cacheReport->num_invalid_read);
    printf("Total number of invalid write       = %16llu blocks\n", cacheReport->num_invalid_write);
    printf("\n");

    printf("#########################\n");
    printf("######## Latency ########\n");
    printf("#########################\n");
    printf("Data Write Latency                  = %19llu ns\n", cacheReport->normal.total_write_time);
    printf("Data Read Latency                   = %19llu ns\n", cacheReport->normal.total_read_time);
    printf("#########################\n");
    printf("######### Size ##########\n");
    printf("#########################\n");
    printf("Accumulated Write Size              = %19llu B\n", cacheReport->total_write_size);
    printf("#########################\n");
    printf("Total Write Block Size              = %19llu B\n", cacheReport->normal.total_write_block_size);
    printf("Total Read Block Size               = %19llu B\n", cacheReport->normal.total_read_block_size);
    printf("Total Delete Block Size             = %19llu B\n", cacheReport->total_delete_write_block_size);
    end_disk(&cache);
    printf("*************************CACHE************************\n\n");
#endif

    printf("*************************DISK*************************\n");
    printf("-------------------------\n");
    printf("Time information:\n\n");
    printf("%f seconds total\n", elapsed);

    printf("-------------------------\n");
    printf("Disk information:\n\n");
    printf("Size of disk = %d GB\n", size);
    printf("Number of blocks = %llu\n", report->max_block_num);

    printf("-------------------------\n");
    printf("Transaction information:\n\n");
    printf("Total number of instructions        = %16llu instructions\n", report->ins_count);
    printf("Total number of read instructions   = %16llu instructions\n", report->read_ins_count);
    printf("Total number of write instructions  = %16llu instructions\n", report->write_ins_count);
    printf("Total number of invalid read        = %16llu blocks\n", report->num_invalid_read);
    printf("Total number of invalid write       = %16llu blocks\n", report->num_invalid_write);
    printf("\n");

    printf("#########################\n");
    printf("######## Latency ########\n");
    printf("#########################\n");
    printf("Data Write Latency                  = %19llu ns\n", report->normal.total_write_time);
    printf("Data Read Latency                   = %19llu ns\n", report->normal.total_read_time);
    printf("Zone Conversion Latency             = ns\n");
    printf("Data Movement Latency               = ns\n");
    printf("#########################\n");
    printf("######### Size ##########\n");
    printf("#########################\n");
    printf("Accumulated Write Size              = %19llu B\n", report->total_write_size);
    printf("Accumulated Rewrite Size            = %19llu B\n", report->total_rewrite_size);
    printf("Accumulated Reread Size             = %19llu B\n", report->total_reread_size);
    printf("#########################\n");
    printf("Total Write Block Size              = %19llu B\n", report->normal.total_write_block_size);
    printf("Total Read Block Size               = %19llu B\n", report->normal.total_read_block_size);
    printf("Total Delete Block Size             = %19llu B\n", report->total_delete_write_block_size);
    end_disk(&disk);
    printf("*************************DISK*************************\n\n");
    printf("\nData Usage Percentage             = %lu %%\n", data_usage * 100UL);
    return 0;
}