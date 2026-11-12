/*
 * PartumTestKit - platform helpers, timing and threading
 */

#include "partum.h"

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifdef _WIN32
#include <process.h>
#include <intrin.h>
#endif

void die(const char *msg)
{
    fprintf(stderr, "\n%s\n", msg);
    exit(1);
}

double now_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

int get_processor_count(void)
{
    int count = 1;

#ifdef _WIN32
    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);
    count = (int)info.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n > 0)
        count = (int)n;
#endif

    if (count < 1)
        count = 1;

    return count;
}

void get_cpu_name(char *buf, size_t len)
{
    if (buf == NULL || len == 0)
        return;

    buf[0] = '\0';

#ifdef __APPLE__
    size_t size = len;
    if (sysctlbyname("machdep.cpu.brand_string", buf, &size, NULL, 0) != 0 || buf[0] == '\0') {
        size = len;
        if (sysctlbyname("hw.model", buf, &size, NULL, 0) != 0)
            snprintf(buf, len, "Unknown CPU");
    }
#elif defined(_WIN32)
    int regs[4];
    unsigned int data[12];
    __cpuid(regs, 0x80000000);
    if ((unsigned int)regs[0] >= 0x80000004u) {
        for (int i = 0; i < 3; i++)
            __cpuid((int *)&data[i * 4], 0x80000002 + i);
        memcpy(buf, data, 48);
        buf[48 < len ? 48 : len - 1] = '\0';
        for (char *p = buf; *p; p++) {
            if (*p == '\n' || *p == '\r')
                *p = ' ';
        }
    } else {
        snprintf(buf, len, "Unknown CPU");
    }
#else
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    colon++;
                    while (*colon == ' ' || *colon == '\t')
                        colon++;
                    snprintf(buf, len, "%s", colon);
                    char *nl = strchr(buf, '\n');
                    if (nl)
                        *nl = '\0';
                }
                break;
            }
        }
        fclose(f);
    }
    if (buf[0] == '\0')
        snprintf(buf, len, "Unknown CPU");
#endif
}

#ifdef _WIN32
static unsigned __stdcall thread_entry(void *p)
{
    struct parallel_task *task = (struct parallel_task *)p;
    task->fn(task->arg);
    return 0;
}
#else
static void *thread_entry(void *p)
{
    struct parallel_task *task = (struct parallel_task *)p;
    task->fn(task->arg);
    return NULL;
}
#endif

int run_parallel(struct parallel_task *tasks, int count)
{
    if (tasks == NULL || count < 1)
        return -1;

    if (count == 1) {
        tasks[0].fn(tasks[0].arg);
        return 0;
    }

#ifdef _WIN32
    HANDLE *threads = (HANDLE *)calloc((size_t)count, sizeof(HANDLE));
    if (threads == NULL)
        return -1;

    int started = 0;
    for (int i = 0; i < count; i++) {
        uintptr_t handle = _beginthreadex(NULL, 0, thread_entry, &tasks[i], 0, NULL);
        if (handle == 0)
            break;
        threads[i] = (HANDLE)handle;
        started++;
    }

    if (started > 0)
        WaitForMultipleObjects((DWORD)started, threads, TRUE, INFINITE);

    for (int i = 0; i < started; i++)
        CloseHandle(threads[i]);

    free(threads);
    return started == count ? 0 : -1;
#else
    pthread_t *threads = (pthread_t *)calloc((size_t)count, sizeof(pthread_t));
    if (threads == NULL)
        return -1;

    int started = 0;
    for (int i = 0; i < count; i++) {
        if (pthread_create(&threads[i], NULL, thread_entry, &tasks[i]) != 0)
            break;
        started++;
    }

    for (int i = 0; i < started; i++)
        pthread_join(threads[i], NULL);

    free(threads);
    return started == count ? 0 : -1;
#endif
}
