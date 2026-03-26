/*
 * page_cache_models.c
 *
 * Compare Linux page cache behaviour across four execution models:
 *   1) single     -> one process, one thread
 *   2) threads    -> one process, many threads
 *   3) processes  -> many processes
 *   4) hybrid     -> many processes, each with many threads
 *
 * The workers read the same file sequentially. The first run is usually cold-ish.
 * The second run is warmer because the file's pages may already be in the page cache.
 *
 * Build:
 *   gcc -O2 -Wall -pthread -o page_cache_models page_cache_models.c
 *
 * Usage examples:
 *   dd if=/dev/urandom of=testdata.bin bs=1M count=128 status=progress
 *   ./page_cache_models testdata.bin single 1 1 2
 *   ./page_cache_models testdata.bin threads 1 4 2
 *   ./page_cache_models testdata.bin processes 4 1 2
 *   ./page_cache_models testdata.bin hybrid 2 2 2
 *
 * Author: Arun Singh | arunsingh.in@gmail.com
 * OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
*/

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE (1024 * 1024)

typedef struct {
    const char *path;
    off_t start;
    off_t end;
    int id;
} worker_arg_t;

typedef struct {
    long minflt;
    long majflt;
    double seconds;
    unsigned long long checksum;
} worker_result_t;

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void get_faults(long *minflt, long *majflt)
{
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) {
        perror("getrusage");
        exit(1);
    }
    *minflt = ru.ru_minflt;
    *majflt = ru.ru_majflt;
}

static worker_result_t read_range(const char *path, off_t start, off_t end)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    if (lseek(fd, start, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        close(fd);
        exit(1);
    }

    char *buf = malloc(BUF_SIZE);
    if (!buf) {
        perror("malloc");
        close(fd);
        exit(1);
    }

    long min0, maj0, min1, maj1;
    get_faults(&min0, &maj0);
    double t0 = now_seconds();

    unsigned long long checksum = 0;
    off_t remaining = end - start;
    while (remaining > 0) {
        size_t chunk = remaining > BUF_SIZE ? BUF_SIZE : (size_t)remaining;
        ssize_t n = read(fd, buf, chunk);
        if (n < 0) {
            perror("read");
            free(buf);
            close(fd);
            exit(1);
        }
        if (n == 0) {
            break;
        }
        for (ssize_t i = 0; i < n; i += 4096) {
            checksum += (unsigned char)buf[i];
        }
        remaining -= n;
    }

    double t1 = now_seconds();
    get_faults(&min1, &maj1);

    free(buf);
    close(fd);

    worker_result_t r;
    r.minflt = min1 - min0;
    r.majflt = maj1 - maj0;
    r.seconds = t1 - t0;
    r.checksum = checksum;
    return r;
}

static void *thread_worker(void *arg)
{
    worker_arg_t *wa = (worker_arg_t *)arg;
    worker_result_t *res = malloc(sizeof(*res));
    if (!res) {
        perror("malloc");
        pthread_exit(NULL);
    }
    *res = read_range(wa->path, wa->start, wa->end);
    pthread_exit(res);
}

static off_t file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        perror("stat");
        exit(1);
    }
    return st.st_size;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <file> <mode:single|threads|processes|hybrid> <proc_count> <thread_count> <rounds>\n",
            prog);
}

int main(int argc, char **argv)
{
    if (argc != 6) {
        usage(argv[0]);
        return 1;
    }

    const char *path = argv[1];
    const char *mode = argv[2];
    int proc_count = (int)strtol(argv[3], NULL, 10);
    int thread_count = (int)strtol(argv[4], NULL, 10);
    int rounds = (int)strtol(argv[5], NULL, 10);
    if (proc_count <= 0 || proc_count > 1024 ||
        thread_count <= 0 || thread_count > 1024 ||
        rounds <= 0) {
        usage(argv[0]);
        return 1;
    }

    off_t size = file_size(path);
    int total_workers = 1;
    if (strcmp(mode, "single") == 0) {
        proc_count = 1;
        thread_count = 1;
        total_workers = 1;
    } else if (strcmp(mode, "threads") == 0) {
        proc_count = 1;
        total_workers = thread_count;
    } else if (strcmp(mode, "processes") == 0) {
        thread_count = 1;
        total_workers = proc_count;
    } else if (strcmp(mode, "hybrid") == 0) {
        total_workers = proc_count * thread_count;
    } else {
        usage(argv[0]);
        return 1;
    }

    printf("=== Page Cache / Process vs Thread Lab ===\n");
    printf("File             : %s\n", path);
    printf("File size        : %lld bytes\n", (long long)size);
    printf("Mode             : %s\n", mode);
    printf("Processes        : %d\n", proc_count);
    printf("Threads/process  : %d\n", thread_count);
    printf("Total workers    : %d\n", total_workers);
    printf("Rounds           : %d\n\n", rounds);

    for (int round = 1; round <= rounds; round++) {
        printf("--- Round %d ---\n", round);
        printf("Hint: later rounds are often faster because data may already be in the Linux page cache.\n");

        off_t slice = size / total_workers;
        double round_start = now_seconds();
        unsigned long long checksum = 0;

        if (strcmp(mode, "single") == 0 || strcmp(mode, "threads") == 0) {
            pthread_t *ths = calloc((size_t)total_workers, sizeof(*ths));
            worker_arg_t *args = calloc((size_t)total_workers, sizeof(*args));
            if (!ths || !args) {
                perror("calloc");
                return 1;
            }

            for (int i = 0; i < total_workers; i++) {
                args[i].path = path;
                args[i].start = i * slice;
                args[i].end = (i == total_workers - 1) ? size : (i + 1) * slice;
                args[i].id = i;
                if (pthread_create(&ths[i], NULL, thread_worker, &args[i]) != 0) {
                    perror("pthread_create");
                    return 1;
                }
            }

            long total_min = 0, total_maj = 0;
            for (int i = 0; i < total_workers; i++) {
                worker_result_t *res = NULL;
                pthread_join(ths[i], (void **)&res);
                if (!res) {
                    fprintf(stderr, "thread join failed\n");
                    return 1;
                }
                total_min += res->minflt;
                total_maj += res->majflt;
                checksum += res->checksum;
                printf("worker=%d minflt=%ld majflt=%ld sec=%.3f\n",
                       i, res->minflt, res->majflt, res->seconds);
                free(res);
            }
            printf("summary minflt=%ld majflt=%ld checksum=%llu elapsed=%.3f\n\n",
                   total_min, total_maj, checksum, now_seconds() - round_start);
            free(ths);
            free(args);
        } else {
            int (*pipes)[2] = calloc((size_t)proc_count, sizeof(int[2]));
            if (!pipes) {
                perror("calloc");
                return 1;
            }

            for (int p = 0; p < proc_count; p++) {
                if (pipe(pipes[p]) != 0) {
                    perror("pipe");
                    return 1;
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork");
                    return 1;
                }
                if (pid == 0) {
                    close(pipes[p][0]);

                    long total_min = 0, total_maj = 0;
                    unsigned long long proc_checksum = 0;
                    pthread_t *ths = calloc((size_t)thread_count, sizeof(*ths));
                    worker_arg_t *args = calloc((size_t)thread_count, sizeof(*args));
                    if (!ths || !args) {
                        perror("calloc");
                        _exit(1);
                    }

                    for (int t = 0; t < thread_count; t++) {
                        int global_id = p * thread_count + t;
                        args[t].path = path;
                        args[t].start = global_id * slice;
                        args[t].end = (global_id == total_workers - 1) ? size : (global_id + 1) * slice;
                        args[t].id = global_id;
                        if (pthread_create(&ths[t], NULL, thread_worker, &args[t]) != 0) {
                            perror("pthread_create");
                            _exit(1);
                        }
                    }

                    for (int t = 0; t < thread_count; t++) {
                        worker_result_t *res = NULL;
                        pthread_join(ths[t], (void **)&res);
                        if (!res) {
                            _exit(1);
                        }
                        total_min += res->minflt;
                        total_maj += res->majflt;
                        proc_checksum += res->checksum;
                        free(res);
                    }

                    dprintf(pipes[p][1], "%d %ld %ld %llu\n", p, total_min, total_maj, proc_checksum);
                    close(pipes[p][1]);
                    free(ths);
                    free(args);
                    _exit(0);
                }
                close(pipes[p][1]);
            }

            long total_min = 0, total_maj = 0;
            for (int p = 0; p < proc_count; p++) {
                int idx;
                long minf, majf;
                unsigned long long part_sum;
                FILE *fp = fdopen(pipes[p][0], "r");
                if (!fp) {
                    perror("fdopen");
                    return 1;
                }
                if (fscanf(fp, "%d %ld %ld %llu", &idx, &minf, &majf, &part_sum) == 4) {
                    printf("proc=%d minflt=%ld majflt=%ld\n", idx, minf, majf);
                    total_min += minf;
                    total_maj += majf;
                    checksum += part_sum;
                }
                fclose(fp);
            }
            for (int p = 0; p < proc_count; p++) {
                wait(NULL);
            }
            printf("summary minflt=%ld majflt=%ld checksum=%llu elapsed=%.3f\n\n",
                   total_min, total_maj, checksum, now_seconds() - round_start);
            free(pipes);
        }
    }

    printf("Quiz:\n");
    printf("  1. Why can round 2 be faster even when the code is unchanged?\n");
    printf("  2. Which resources are shared by threads but not by separate processes?\n");
    printf("  3. Why might process mode show higher total overhead than thread mode?\n");
    printf("  4. Why are file reads often served from page cache without major faults after warm-up?\n");
    return 0;
}
