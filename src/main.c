/*
 * PartumTestKit - a cross-platform CPU benchmark
 *
 * Usage: partumtestkit [options]
 */

#include "partum.h"

/* Score calibration: one point is this many work units per second.
 * These keep scores in a Cinebench-like range on current hardware. */
#define INT_OPS_PER_POINT 31500000.0
#define FLT_OPS_PER_POINT 7000000.0
#define RND_OPS_PER_POINT 1340000.0

struct workload_result {
    const char *name;
    const char *detail;
    double seconds;
    double work;
    double score;
    unsigned long long checksum;
};

struct phase_result {
    int ran;
    int threads;
    double total_score;
    struct workload_result items[3];
};

static const struct workload WORKLOADS[3] = {
    { "Integer", "prime counting", run_integer_workload, integer_work_units, INT_OPS_PER_POINT },
    { "Float",   "matrix multiply", run_float_workload, float_work_units,   FLT_OPS_PER_POINT },
    { "Render",  "mandelbrot",      run_render_workload, render_work_units, RND_OPS_PER_POINT }
};

static void print_usage(FILE *out)
{
    fprintf(out,
        "%s %s - CPU benchmark\n\n"
        "Usage: partumtestkit [options]\n\n"
        "Options:\n"
        "  -q, --quick          Run a shorter test\n"
        "  -t, --threads <n>    Worker threads for the multi-core test (default: all)\n"
        "  -1, --single         Run only the single-threaded test\n"
        "  -m, --multi          Run only the multi-threaded test\n"
        "  -r, --runs <n>       Runs per workload, best time kept (default: 3)\n"
        "      --json           Print machine-readable JSON results\n"
        "  -h, --help           Show this help\n"
        "  -v, --version        Show the version\n",
        PARTUM_NAME, PARTUM_VERSION);
}

static int parse_positive_int(const char *text, const char *what)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (end == text || *end != '\0' || value < 1 || value > 1000000) {
        fprintf(stderr, "Invalid %s: %s\n", what, text);
        exit(1);
    }

    return (int)value;
}

static void run_phase(struct phase_result *phase, int threads, int quick, int runs)
{
    phase->ran = 1;
    phase->threads = threads;
    phase->total_score = 0.0;

    for (int i = 0; i < 3; i++) {
        const struct workload *w = &WORKLOADS[i];
        double best = 0.0;
        unsigned long long checksum = 0;

        for (int r = 0; r < runs; r++) {
            unsigned long long run_checksum = 0;
            double elapsed = w->run(threads, quick, &run_checksum);
            if (r == 0 || elapsed < best) {
                best = elapsed;
                checksum = run_checksum;
            }
        }

        double work = w->work(quick);
        double score = (work / best) / w->ops_per_point;

        phase->items[i].name = w->name;
        phase->items[i].detail = w->detail;
        phase->items[i].seconds = best;
        phase->items[i].work = work;
        phase->items[i].score = score;
        phase->items[i].checksum = checksum;
        phase->total_score += score;
    }
}

static void print_phase(const struct phase_result *phase, const char *label)
{
    printf("\n%s (%d thread%s)\n", label, phase->threads, phase->threads == 1 ? "" : "s");

    for (int i = 0; i < 3; i++) {
        const struct workload_result *item = &phase->items[i];
        printf("  %-9s %-18s %8.3f s   %8.0f pts\n",
               item->name, item->detail, item->seconds, item->score);
    }

    printf("  %-28s %8s   %8.0f pts\n", "Total", "", phase->total_score);
}

static void json_string(const char *s)
{
    putchar('"');
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            default:   putchar(*p); break;
        }
    }
    putchar('"');
}

static void print_json(const char *cpu, int cores, int threads, int quick,
                       const struct phase_result *single,
                       const struct phase_result *multi,
                       double partum_score, double scaling)
{
    printf("{\n");
    printf("  \"tool\": \"%s\",\n", PARTUM_NAME);
    printf("  \"version\": \"%s\",\n", PARTUM_VERSION);
    printf("  \"cpu\": ");
    json_string(cpu);
    printf(",\n");
    printf("  \"cores\": %d,\n", cores);
    if (multi->ran)
        printf("  \"threads\": %d,\n", threads);
    else
        printf("  \"threads\": null,\n");
    printf("  \"quick\": %s,\n", quick ? "true" : "false");
    printf("  \"partum_score\": %.0f,\n", partum_score);
    if (single->ran && multi->ran)
        printf("  \"scaling\": %.2f,\n", scaling);
    else
        printf("  \"scaling\": null,\n");

    const struct phase_result *phases[2] = { single, multi };
    const char *keys[2] = { "single", "multi" };

    printf("  \"results\": {\n");
    for (int p = 0; p < 2; p++) {
        if (!phases[p]->ran)
            continue;

        printf("    \"%s\": {\n", keys[p]);
        printf("      \"threads\": %d,\n", phases[p]->threads);
        printf("      \"score\": %.0f,\n", phases[p]->total_score);
        printf("      \"workloads\": [\n");

        for (int i = 0; i < 3; i++) {
            const struct workload_result *item = &phases[p]->items[i];
            printf("        { \"name\": \"%s\", \"seconds\": %.4f, \"work\": %.0f, "
                   "\"score\": %.0f, \"checksum\": %llu }%s\n",
                   item->name, item->seconds, item->work, item->score,
                   item->checksum, i < 2 ? "," : "");
        }

        printf("      ]\n");
        printf("    }%s\n", (p == 0 && multi->ran) ? "," : "");
    }
    printf("  }\n");
    printf("}\n");
}

int main(int argc, char *argv[])
{
    int quick = 0;
    int only_single = 0;
    int only_multi = 0;
    int json = 0;
    int runs = 3;
    int requested_threads = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(stdout);
            return 0;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            printf("%s %s\n", PARTUM_NAME, PARTUM_VERSION);
            return 0;
        } else if (strcmp(arg, "-q") == 0 || strcmp(arg, "--quick") == 0) {
            quick = 1;
        } else if (strcmp(arg, "-1") == 0 || strcmp(arg, "--single") == 0) {
            only_single = 1;
        } else if (strcmp(arg, "-m") == 0 || strcmp(arg, "--multi") == 0) {
            only_multi = 1;
        } else if (strcmp(arg, "--json") == 0) {
            json = 1;
        } else if (strcmp(arg, "-t") == 0 || strcmp(arg, "--threads") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for %s\n", arg);
                return 1;
            }
            requested_threads = parse_positive_int(argv[++i], "thread count");
        } else if (strcmp(arg, "-r") == 0 || strcmp(arg, "--runs") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for %s\n", arg);
                return 1;
            }
            runs = parse_positive_int(argv[++i], "run count");
        } else {
            fprintf(stderr, "Unknown option: %s\n\n", arg);
            print_usage(stderr);
            return 1;
        }
    }

    if (only_single && only_multi) {
        fprintf(stderr, "--single and --multi cannot be used together\n");
        return 1;
    }

    int cores = get_processor_count();
    int threads = requested_threads > 0 ? requested_threads : cores;
    if (threads > 1024)
        threads = 1024;

    char cpu[256];
    get_cpu_name(cpu, sizeof(cpu));

    struct phase_result single;
    struct phase_result multi;
    memset(&single, 0, sizeof(single));
    memset(&multi, 0, sizeof(multi));

    if (!json) {
        printf("\n%s %s\n", PARTUM_NAME, PARTUM_VERSION);
        printf("CPU    %s\n", cpu);
        printf("Cores  %d logical\n", cores);
        printf("Mode   %s%s\n", quick ? "quick" : "standard",
               runs > 1 ? " (best of runs)" : "");
        fflush(stdout);
    }

    if (!only_multi) {
        if (!json) {
            printf("\nRunning single-threaded test...\n");
            fflush(stdout);
        }
        run_phase(&single, 1, quick, runs);
        if (!json) {
            print_phase(&single, "Single-threaded");
            fflush(stdout);
        }
    }

    if (!only_single) {
        if (!json) {
            printf("\nRunning multi-threaded test with %d threads...\n", threads);
            fflush(stdout);
        }
        run_phase(&multi, threads, quick, runs);
        if (!json) {
            print_phase(&multi, "Multi-threaded");
            fflush(stdout);
        }
    }

    double partum_score = multi.ran ? multi.total_score : single.total_score;
    double scaling = 0.0;
    if (single.ran && multi.ran && single.total_score > 0.0)
        scaling = multi.total_score / single.total_score;

    if (json) {
        print_json(cpu, cores, threads, quick, &single, &multi, partum_score, scaling);
        return 0;
    }

    printf("\n");
    if (single.ran && multi.ran)
        printf("Scaling          %.2fx\n", scaling);
    printf("%-16s %8.0f pts\n", "Partum Score", partum_score);
    printf("\n");

    return 0;
}
