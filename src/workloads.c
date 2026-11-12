/*
 * PartumTestKit - benchmark workloads
 *
 * Each workload does a fixed, deterministic amount of work. The same
 * total work is used for the single-threaded and multi-threaded runs,
 * so a faster/more parallel machine finishes in less wall-clock time.
 */

#include "partum.h"

/* ------------------------------------------------------------------ */
/* Integer: trial-division prime counting                             */
/* ------------------------------------------------------------------ */

#define INTEGER_STANDARD 10000000ULL
#define INTEGER_QUICK     4000000ULL

struct int_args {
    unsigned long long from;
    unsigned long long to;
    unsigned long long count;
};

static int is_prime(unsigned long long n)
{
    if (n < 2)
        return 0;
    if (n % 2 == 0)
        return n == 2;
    if (n % 3 == 0)
        return n == 3;

    for (unsigned long long i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    }
    return 1;
}

static void count_primes(void *p)
{
    struct int_args *a = (struct int_args *)p;
    unsigned long long count = 0;

    for (unsigned long long n = a->from; n <= a->to; n++) {
        if (is_prime(n))
            count++;
    }

    a->count = count;
}

double integer_work_units(int quick)
{
    double n = (double)(quick ? INTEGER_QUICK : INTEGER_STANDARD);
    /* trial division costs roughly the square root of each candidate */
    return (2.0 / 3.0) * pow(n, 1.5);
}

double run_integer_workload(int threads, int quick, unsigned long long *checksum)
{
    unsigned long long n = quick ? INTEGER_QUICK : INTEGER_STANDARD;

    if (threads < 1)
        threads = 1;

    struct int_args *args = (struct int_args *)calloc((size_t)threads, sizeof(struct int_args));
    struct parallel_task *tasks = (struct parallel_task *)calloc((size_t)threads, sizeof(struct parallel_task));

    if (args == NULL || tasks == NULL)
        die("Error allocating memory");

    unsigned long long chunk = (n + (unsigned long long)threads - 1) / (unsigned long long)threads;
    unsigned long long start = 2;

    for (int i = 0; i < threads; i++) {
        unsigned long long end = start + chunk - 1;
        if (i == threads - 1 || end > n)
            end = n;

        args[i].from = start;
        args[i].to = end;
        args[i].count = 0;

        tasks[i].fn = count_primes;
        tasks[i].arg = &args[i];

        start = end + 1;
    }

    double t0 = now_seconds();
    if (run_parallel(tasks, threads) != 0)
        die("Error creating threads");
    double elapsed = now_seconds() - t0;

    unsigned long long total = 0;
    for (int i = 0; i < threads; i++)
        total += args[i].count;

    if (checksum)
        *checksum = total;

    free(args);
    free(tasks);

    return elapsed;
}

/* ------------------------------------------------------------------ */
/* Floating point: naive double-precision matrix multiplication       */
/* ------------------------------------------------------------------ */

#define FLOAT_STANDARD 1024
#define FLOAT_QUICK     1024

struct mm_args {
    const double *a;
    const double *b;
    double *c;
    int n;
    int row_from;
    int row_to;
    double sum;
};

static void matmul_rows(void *p)
{
    struct mm_args *args = (struct mm_args *)p;
    int n = args->n;
    double sum = 0.0;

    for (int i = args->row_from; i < args->row_to; i++) {
        double *ci = args->c + (size_t)i * (size_t)n;

        for (int k = 0; k < n; k++) {
            double a = args->a[(size_t)i * (size_t)n + (size_t)k];
            const double *bk = args->b + (size_t)k * (size_t)n;
            for (int j = 0; j < n; j++)
                ci[j] += a * bk[j];
        }

        for (int j = 0; j < n; j++)
            sum += ci[j];
    }

    args->sum = sum;
}

double float_work_units(int quick)
{
    double n = (double)(quick ? FLOAT_QUICK : FLOAT_STANDARD);
    return 2.0 * n * n * n;
}

double run_float_workload(int threads, int quick, unsigned long long *checksum)
{
    int n = quick ? FLOAT_QUICK : FLOAT_STANDARD;

    if (threads < 1)
        threads = 1;

    size_t elements = (size_t)n * (size_t)n;
    double *a = (double *)malloc(elements * sizeof(double));
    double *b = (double *)malloc(elements * sizeof(double));
    double *c = (double *)malloc(elements * sizeof(double));

    if (a == NULL || b == NULL || c == NULL)
        die("Error allocating memory");

    for (size_t i = 0; i < elements; i++) {
        a[i] = (double)(i % 17) * 0.1 + 1.0;
        b[i] = (double)(i % 13) * 0.1 - 0.5;
        c[i] = 0.0;
    }

    struct mm_args *args = (struct mm_args *)calloc((size_t)threads, sizeof(struct mm_args));
    struct parallel_task *tasks = (struct parallel_task *)calloc((size_t)threads, sizeof(struct parallel_task));

    if (args == NULL || tasks == NULL)
        die("Error allocating memory");

    int rows = (n + threads - 1) / threads;
    int start = 0;

    for (int i = 0; i < threads; i++) {
        int end = start + rows;
        if (i == threads - 1 || end > n)
            end = n;

        args[i].a = a;
        args[i].b = b;
        args[i].c = c;
        args[i].n = n;
        args[i].row_from = start;
        args[i].row_to = end;
        args[i].sum = 0.0;

        tasks[i].fn = matmul_rows;
        tasks[i].arg = &args[i];

        start = end;
    }

    double t0 = now_seconds();
    if (run_parallel(tasks, threads) != 0)
        die("Error creating threads");
    double elapsed = now_seconds() - t0;

    if (checksum) {
        double sum = 0.0;
        for (int i = 0; i < threads; i++)
            sum += args[i].sum;
        *checksum = (unsigned long long)(sum * 1000.0);
    }

    free(args);
    free(tasks);
    free(a);
    free(b);
    free(c);

    return elapsed;
}

/* ------------------------------------------------------------------ */
/* Render: Mandelbrot set at a fixed resolution                       */
/* ------------------------------------------------------------------ */

#define RENDER_STANDARD_W 1600
#define RENDER_STANDARD_H 1000
#define RENDER_STANDARD_I 1024

#define RENDER_QUICK_W     1600
#define RENDER_QUICK_H     1000
#define RENDER_QUICK_I     256

struct render_args {
    int width;
    int height;
    int max_iter;
    int row_from;
    int row_to;
    unsigned long long checksum;
};

static void render_rows(void *p)
{
    struct render_args *args = (struct render_args *)p;
    unsigned long long sum = 0;

    for (int y = args->row_from; y < args->row_to; y++) {
        double ci = -1.25 + (2.5 * (double)y) / (double)args->height;

        for (int x = 0; x < args->width; x++) {
            double cr = -2.2 + (3.0 * (double)x) / (double)args->width;
            double zr = 0.0;
            double zi = 0.0;
            int iter = 0;

            while (iter < args->max_iter && zr * zr + zi * zi <= 4.0) {
                double tmp = zr * zr - zi * zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = tmp;
                iter++;
            }

            sum += (unsigned long long)iter
                 + (unsigned long long)x * 3ULL
                 + (unsigned long long)y * 7ULL;
        }
    }

    args->checksum = sum;
}

double render_work_units(int quick)
{
    double w = (double)(quick ? RENDER_QUICK_W : RENDER_STANDARD_W);
    double h = (double)(quick ? RENDER_QUICK_H : RENDER_STANDARD_H);
    double i = (double)(quick ? RENDER_QUICK_I : RENDER_STANDARD_I);
    return w * h * i;
}

double run_render_workload(int threads, int quick, unsigned long long *checksum)
{
    int width = quick ? RENDER_QUICK_W : RENDER_STANDARD_W;
    int height = quick ? RENDER_QUICK_H : RENDER_STANDARD_H;
    int max_iter = quick ? RENDER_QUICK_I : RENDER_STANDARD_I;

    if (threads < 1)
        threads = 1;

    struct render_args *args = (struct render_args *)calloc((size_t)threads, sizeof(struct render_args));
    struct parallel_task *tasks = (struct parallel_task *)calloc((size_t)threads, sizeof(struct parallel_task));

    if (args == NULL || tasks == NULL)
        die("Error allocating memory");

    int rows = (height + threads - 1) / threads;
    int start = 0;

    for (int i = 0; i < threads; i++) {
        int end = start + rows;
        if (i == threads - 1 || end > height)
            end = height;

        args[i].width = width;
        args[i].height = height;
        args[i].max_iter = max_iter;
        args[i].row_from = start;
        args[i].row_to = end;
        args[i].checksum = 0;

        tasks[i].fn = render_rows;
        tasks[i].arg = &args[i];

        start = end;
    }

    double t0 = now_seconds();
    if (run_parallel(tasks, threads) != 0)
        die("Error creating threads");
    double elapsed = now_seconds() - t0;

    if (checksum) {
        unsigned long long total = 0;
        for (int i = 0; i < threads; i++)
            total += args[i].checksum;
        *checksum = total;
    }

    free(args);
    free(tasks);

    return elapsed;
}
