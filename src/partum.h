/*
 * PartumTestKit - a cross-platform CPU benchmark
 *
 * Measures single-threaded and multi-threaded performance across
 * integer, floating point and render-style workloads and reports
 * Cinebench-like scores.
 */

#ifndef PARTUM_H
#define PARTUM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#define PARTUM_NAME    "PartumTestKit"
#define PARTUM_VERSION "1.0.0"

struct parallel_task {
    void (*fn)(void *);
    void *arg;
};

struct workload {
    const char *name;
    const char *detail;
    double (*run)(int threads, int quick, unsigned long long *checksum);
    double (*work)(int quick);
    double ops_per_point;
};

/* platform helpers (func.c) */
int run_parallel(struct parallel_task *tasks, int count);
int get_processor_count(void);
void get_cpu_name(char *buf, size_t len);
double now_seconds(void);
void die(const char *msg);

/* workloads (workloads.c) */
double run_integer_workload(int threads, int quick, unsigned long long *checksum);
double run_float_workload(int threads, int quick, unsigned long long *checksum);
double run_render_workload(int threads, int quick, unsigned long long *checksum);

double integer_work_units(int quick);
double float_work_units(int quick);
double render_work_units(int quick);

#endif
