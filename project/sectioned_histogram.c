#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#define MAX_CHARS 256

typedef struct {
    unsigned char *file_content;
    long start_index, end_index;
    long char_count[MAX_CHARS];
    struct timeval t_start_count;
    struct timeval t_end_count;
} ThreadData;

/* ---------------- Thread Worker ---------------- */
void* count_characters(void *arg) {
    ThreadData *data = (ThreadData*)arg;

    gettimeofday(&data->t_start_count, NULL);

    for (int i = 0; i < MAX_CHARS; i++)
        data->char_count[i] = 0;

    for (long i = data->start_index; i < data->end_index; i++) {
        unsigned char ch = data->file_content[i];
        data->char_count[ch]++;
    }

    gettimeofday(&data->t_end_count, NULL);
    return NULL;
}

int main(int argc, char *argv[]) {

    if (argc < 3) {
        printf("Usage: %s <textfile> <num_threads>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[2]);

    struct timeval t_start_total, t_end_total;
    struct timeval t1, t2;
    struct timeval t_start_completion, t_end_completion;

    double time_total = 0;
    double time_read = 0;
    double time_thread_create = 0;
    double time_worker = 0;
    double time_thread_wait = 0;
    double time_agg = 0;
    double time_write = 0;
    double time_completion = 0;

    /* ---------------- Total Execution Time ---------------- */
    gettimeofday(&t_start_total, NULL);

    /* ---------------- File Reading ---------------- */
    gettimeofday(&t1, NULL);

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("File error");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    unsigned char *content = malloc(file_size);
    fread(content, 1, file_size, file);
    fclose(file);

    gettimeofday(&t2, NULL);

    time_read = (t2.tv_sec - t1.tv_sec) +
                (t2.tv_usec - t1.tv_usec) / 1e6;

    printf("Total file size: %ld bytes\n", file_size);

    /* -------- Completion Time Starts (I/O excluded) -------- */
    gettimeofday(&t_start_completion, NULL);

    /* ---------------- Thread Creation ---------------- */
    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];

    long chunk_size = file_size / num_threads;

    gettimeofday(&t1, NULL);
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].file_content = content;
        thread_data[i].start_index = i * chunk_size;
        thread_data[i].end_index =
            (i == num_threads - 1) ? file_size : (i + 1) * chunk_size;

        printf("Thread %d processes %ld bytes\n",
               i,
               thread_data[i].end_index - thread_data[i].start_index);

        pthread_create(&threads[i], NULL,
                       count_characters, &thread_data[i]);
    }
    gettimeofday(&t2, NULL);

    time_thread_create = (t2.tv_sec - t1.tv_sec) +
                         (t2.tv_usec - t1.tv_usec) / 1e6;

    /* ---------------- Thread Join ---------------- */
    gettimeofday(&t1, NULL);
    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);
    gettimeofday(&t2, NULL);

    time_thread_wait = (t2.tv_sec - t1.tv_sec) +
                       (t2.tv_usec - t1.tv_usec) / 1e6;

    /* ---------------- Worker Execution (Critical Path) ---------------- */
    for (int i = 0; i < num_threads; i++) {
        double t =
            (thread_data[i].t_end_count.tv_sec -
             thread_data[i].t_start_count.tv_sec) +
            (thread_data[i].t_end_count.tv_usec -
             thread_data[i].t_start_count.tv_usec) / 1e6;

        if (t > time_worker)
            time_worker = t;
    }

    /* ---------------- Aggregation ---------------- */
    gettimeofday(&t1, NULL);

    long global_char_count[MAX_CHARS] = {0};
    for (int i = 0; i < num_threads; i++)
        for (int j = 0; j < MAX_CHARS; j++)
            global_char_count[j] += thread_data[i].char_count[j];

    gettimeofday(&t2, NULL);

    time_agg = (t2.tv_sec - t1.tv_sec) +
               (t2.tv_usec - t1.tv_usec) / 1e6;

    /* ---------------- File Writing ---------------- */
    gettimeofday(&t1, NULL);

    FILE *out_file = fopen("char_frequency.txt", "w");
    if (out_file) {
        for (int i = 0; i < MAX_CHARS; i++) {
            if (global_char_count[i] > 0) {
                if (i == '\n')
                    fprintf(out_file, "\\n,%ld;", global_char_count[i]);
                else if (i == '\r')
                    fprintf(out_file, "\\r,%ld;", global_char_count[i]);
                else if (i == ' ')
                    fprintf(out_file, "' ',%ld;", global_char_count[i]);
                else
                    fprintf(out_file, "%c,%ld;",
                            (unsigned char)i, global_char_count[i]);
            }
        }
        fclose(out_file);
    }

    gettimeofday(&t2, NULL);

    time_write = (t2.tv_sec - t1.tv_sec) +
                 (t2.tv_usec - t1.tv_usec) / 1e6;

    /* -------- Completion Time Ends -------- */
    gettimeofday(&t_end_completion, NULL);

    time_completion =
        (t_end_completion.tv_sec - t_start_completion.tv_sec) +
        (t_end_completion.tv_usec - t_start_completion.tv_usec) / 1e6;

    /* ---------------- Total Execution Ends ---------------- */
    gettimeofday(&t_end_total, NULL);

    time_total =
        (t_end_total.tv_sec - t_start_total.tv_sec) +
        (t_end_total.tv_usec - t_start_total.tv_usec) / 1e6;

    /* ---------------- Timing Report ---------------- */
    printf("\nTiming Report:\n");
    printf("File reading/loading time     : %.6f s\n", time_read);
    printf("Thread creation time          : %.6f s\n", time_thread_create);
    printf("Thread worker execution time  : %.6f s\n", time_worker);
    printf("Thread waiting (join) time    : %.6f s\n", time_thread_wait);
    printf("Aggregation time              : %.6f s\n", time_agg);
    printf("File writing time             : %.6f s\n", time_write);
    printf("Completion time (I/O excluded): %.6f s\n", time_completion);
    printf("Total execution time          : %.6f s\n", time_total);

    free(content);
    return 0;
}

