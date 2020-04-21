#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/sysinfo.h>

const double glob_start = 1;
const double glob_end = 330;
const double glob_step = 0.0000002;

enum errors
{
    E_ERROR = -1,
    E_CACHE_INFO = -2,
    E_BADARGS = -3,
};

struct thread_info
{
    double sum;
    double start;
    double end;
    double delt;
    int num_cpu;
};

struct CPU_info
{
    int cache_line;
    int num_cpus;
    int num_threads;
};

double func(double x);
long int give_num(const char* str_num);
void* integral_thread(void* info);
int cache_line_size();
void* alloc_thread_info(size_t num_threads, size_t* size);
int prepare_threads(void* info, size_t info_size, int num_thr, double start, double end, double step);
int prepare_parasites(void* info, size_t info_size, int num_parasites, double start, double end, double step);

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        perror("Bad arguments number!\n");
        exit(EXIT_FAILURE);
    }

    long int num_thr = give_num(argv[1]);
    if (num_thr <= 0)
        exit(EXIT_FAILURE);

    int num_cpus = get_nprocs();
    int num_parasites = 0;
    if (num_cpus > num_thr)
        num_parasites = num_cpus - num_thr;

    size_t info_size = 0;
    errno = 0;
    void* arr_info = alloc_thread_info(num_thr + num_parasites, &info_size);
    if (arr_info == NULL)
    {
        perror("Bad info allloc\n");
        exit(EXIT_FAILURE);
    }

    errno = 0;
    pthread_t* arr_thread = (pthread_t*) calloc(num_thr + num_parasites, sizeof(pthread_t));
    if (arr_thread == NULL)
    {
        perror("Bad thread alloc\n");
        exit(EXIT_FAILURE);
    }

    int ret = prepare_threads(arr_info, info_size,
        num_thr, glob_start, glob_end, glob_step);
    if (ret < 0)
    {
        perror("Prepare calc threads error\n");
        exit(EXIT_FAILURE);
    }

    ret = prepare_parasites(arr_info + num_thr * info_size, info_size,
        num_parasites, glob_start, glob_start + ((glob_end - glob_start) / num_thr), glob_step);
    if (ret < 0)
    {
        perror("Prepare calc threads error\n");
        exit(EXIT_FAILURE);
    }

    for (long int i = 0; i < num_thr + num_parasites; i++)
    {
        if (num_parasites > 0)
            ((thread_info*)(arr_info + i * info_size))->num_cpu = i;

        int ret = pthread_create(arr_thread + i, NULL, integral_thread, (arr_info + i * info_size));
        if (ret < 0)
        {
            errno = 0;
            perror("Bad thread_start\n");
            exit(EXIT_FAILURE);
        }
    }

    double result = 0.0;

    for(long int i = 0; i < num_thr; i++)
    {
        errno = 0;
        int ret = pthread_join(arr_thread[i], NULL);
        if (ret < 0)
        {
            perror("Bad join\n");
            exit(EXIT_FAILURE);
        }

        //if (i < num_thr)
            result += ((thread_info*)(arr_info + i * info_size))->sum;
        //printf("%lg\n", ((thread_info*)(arr_info + i * info_size))->sum);
    }

    printf("%lg\n", result);

    return 0;
}

int prepare_threads(void* info, size_t info_size, int num_thr, double start, double end, double step)
{
    if (info == NULL || num_thr < 0 || start == NAN || end == NAN || step == NAN)
        return E_BADARGS;

    double diap_step = (end - start) / num_thr;

    for (int i = 0; i < num_thr; i++)
    {
        ((thread_info*)(info + i * info_size))->start = start + diap_step * i;
        ((thread_info*)(info + i * info_size))->end = start + diap_step * (i + 1);
        ((thread_info*)(info + i * info_size))->delt = step;
        ((thread_info*)(info + i * info_size))->num_cpu = -1;

        //printf("%lg\n", start + diap_step * (i + 1));
    }

    return 0;
}

int prepare_parasites(void* info, size_t info_size, int num_parasites, double par_start, double par_end, double par_step)
{
    if (info == NULL || num_parasites < 0 || par_start == NAN || par_end == NAN || par_step == NAN)
        return E_BADARGS;

    for (int i = 0; i < num_parasites; i++)
    {
        ((thread_info*)(info + i * info_size))->start = par_start;
        ((thread_info*)(info + i * info_size))->end = par_end;
        ((thread_info*)(info + i * info_size))->delt = par_step;
        ((thread_info*)(info + i * info_size))->num_cpu = -1;
    }

    return 0;
}

void* alloc_thread_info(size_t num_threads, size_t* size)
{
    if (size == NULL)
        return NULL;

    int line_size = cache_line_size();
    if (line_size <= 0)
    {
        perror("Bad cache coherency\n");
        return NULL;
    }

    size_t info_size = sizeof(thread_info);
    if (info_size <= line_size)
        info_size = 2 * line_size; // free line if struct will consist 2 lines
    else
        info_size = (info_size / line_size + 1 + 1) * line_size; // free line

    *size = info_size;

    errno = 0;
    return malloc(num_threads * info_size);
}

void* integral_thread(void* info)
{
    if (info == NULL)
        exit(EXIT_FAILURE);

    cpu_set_t cpu;
    pthread_t thread = pthread_self();
    int num_cpu = ((thread_info*)info)->num_cpu;

    if (num_cpu > 0)
    {
        CPU_ZERO(&cpu);
        CPU_SET(num_cpu, &cpu);

        errno = 0;
        int ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpu);
        if (ret < 0)
        {
            perror("Bad cpu attach!\n");
            exit(EXIT_FAILURE);
        }
    }


    double delta = ((thread_info*)info)->delt;
    double end = ((thread_info*)(info))->end;

    double x = ((thread_info*)(info))->start + delta;

    for (; x < end; x += delta)// Check x and delta in asm version
        ((thread_info*)(info))->sum += func(x) * delta;

    ((thread_info*)(info))->sum += func(((thread_info*)(info))->start) * delta / 2;
    ((thread_info*)(info))->sum += func(((thread_info*)(info))->end) * delta / 2;

    return NULL;
}

double func(double x)
{
    return x * x;
}

int cache_line_size()
{
    errno = 0;
    FILE* cache_info = fopen("/sys/bus/cpu/devices/cpu0/cache/index0/coherency_line_size", "r");
    if (cache_info == NULL)
    {
        perror("Can't open /sys/bus/cpu/devices/cpu0/cache/index0/coherency_line_size\n");
        return E_ERROR;
    }

    int line_size = 0;
    int ret = fscanf(cache_info, "%d", &line_size);
    if (ret != 1)
    {
        perror("Can't scan coherency_line_size\n");
        return E_ERROR;
    }

    return line_size;
}

long int give_num(const char* str_num)
{
    long int in_num = 0;
    char *end_string;

    errno = 0;
    in_num = strtoll(str_num, &end_string, 10);
    if ((errno != 0 && in_num == 0) || (errno == ERANGE && (in_num == LLONG_MAX || in_num == LLONG_MIN))) {
        printf("Bad string");
        return -2;
    }

    if (str_num == end_string) {
        printf("No number");
        return -3;
    }

    if (*end_string != '\0') {
        printf("Garbage after number");
        return -4;
    }

    if (in_num < 0) {
        printf("i want unsigned num");
        return -5;
    }

    return in_num;
}
