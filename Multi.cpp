#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>

const double glob_start = 1;
const double glob_end = 32;
const double step = 0.00000001;

enum errors
{
    E_ERROR,
};

struct thread_info
{
    double sum;
    double start;
    double end;
    double delt;
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

    int line_size = cache_line_size();
    if (line_size <= 0)
    {
        perror("Bad cache coherency\n");
        exit(EXIT_FAILURE);
    }

    size_t info_size = sizeof(thread_info);
    if (info_size <= line_size)
        info_size = 2 * line_size; // free line if struct will consist 2 lines
    else
        info_size = (info_size / line_size + 1 + 1) * line_size; // free line

    errno = 0;
    void* arr_info = malloc(num_thr * info_size);
    if (arr_info == NULL)
    {
        perror("Bad info alloc\n");
        exit(EXIT_FAILURE);
    }

    errno = 0;
    pthread_t* arr_thread = (pthread_t*) calloc(num_thr, sizeof(pthread_t));
    if (arr_thread == NULL)
    {
        perror("Bad thread alloc\n");
        exit(EXIT_FAILURE);
    }

    double diap_step = (glob_end - glob_start) / num_thr;

    for (long int i = 0; i < num_thr; i++)
    {
        ((thread_info*)(arr_info + i * info_size))->start = glob_start + diap_step * i;
        ((thread_info*)(arr_info + i * info_size))->end = glob_start + diap_step * (i + 1);
        ((thread_info*)(arr_info + i * info_size))->delt = step;
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

        result += ((thread_info*)(arr_info + i * info_size))->sum;
    }

    printf("%lg\n", result);

    return 0;
}

void* integral_thread(void* info)
{
    if (info == NULL)
        exit(EXIT_FAILURE);

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
