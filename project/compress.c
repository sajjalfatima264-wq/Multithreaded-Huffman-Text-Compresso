#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_CHARS 256
// Struct to store information for each compression thread
typedef struct {
    unsigned char *file_content;      // Pointer to full input file
    long start_index;                  // Start of this thread's section
    long end_index;                    // End of this thread's section
    char *huffman_codes[MAX_CHARS];    // Huffman codes table
    unsigned char *compressed_buffer;  // Output buffer for this thread
    long total_bits_written;           // Total bits written by this thread
} ThreadCompressData;
// Function to write a single bit into the output buffer
void write_bit_to_buffer(unsigned char *buffer, long bit_pos, int bit_value) {
    if (bit_value)
        buffer[bit_pos >> 3] |= (1 << (7 - (bit_pos & 7)));
    else
        buffer[bit_pos >> 3] &= ~(1 << (7 - (bit_pos & 7)));
}
// Thread function: compresses a section of the input file
void* compress_section(void *arg) {
    ThreadCompressData *data = (ThreadCompressData*)arg;
    long bit_position = 0;
    for (long i = data->start_index; i < data->end_index; i++) {
        unsigned char ch = data->file_content[i];
        char *code = data->huffman_codes[ch];
        if (!code) continue;  // Skip if no Huffman code
        for (int j = 0; code[j] != '\0'; j++) {
            write_bit_to_buffer(data->compressed_buffer, bit_position, code[j] - '0');
            bit_position++;
        }
    }
    data->total_bits_written = bit_position;
    return NULL;
}
int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <input_file> <encoding_file> <num_threads> <output_file>\n", argv[0]);
        return 1;
    }
    int num_threads = atoi(argv[3]);
    if (num_threads <= 0) num_threads = 1;
    char *output_filename = argv[4]; // <-- New argument for output file
    // --- 1. Load Huffman Codes ---
    char *huffman_codes[MAX_CHARS] = {0};
    FILE *encoding_file = fopen(argv[2], "r");
    if (!encoding_file) { perror("Encoding file error"); return 1; }
    char line[1024];
    while (fgets(line, sizeof(line), encoding_file)) {
        if (strstr(line, "---") || strstr(line, "Char |")) continue;
        if (strlen(line) < 5) continue;
        unsigned char ch;
        char code_str[256];
        char *last_pipe = strrchr(line, '|');
        if (!last_pipe) continue;
        if (strncmp(line, "\\n", 2) == 0) ch = '\n';
        else if (strncmp(line, "\\r", 2) == 0) ch = '\r';
        else if (strncmp(line, "' '", 3) == 0) ch = ' ';
        else ch = (unsigned char)line[0];

        if (sscanf(last_pipe + 1, "%s", code_str) == 1) {
            huffman_codes[ch] = strdup(code_str);
        }
    }
    fclose(encoding_file);
    // --- 2. Load Input File ---
    FILE *input_file = fopen(argv[1], "rb");
    if (!input_file) { perror("Input file error"); return 1; }
    fseek(input_file, 0, SEEK_END);
    long file_size = ftell(input_file);
    rewind(input_file);
    unsigned char *file_buffer = malloc(file_size);
    if (!file_buffer) return 1;
    fread(file_buffer, 1, file_size, input_file);
    fclose(input_file);
    // --- 3. Set up Threads ---
    pthread_t threads[num_threads];
    ThreadCompressData thread_data[num_threads];
    long chunk_size = file_size / num_threads;
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].file_content = file_buffer;
        thread_data[i].start_index = i * chunk_size;
        thread_data[i].end_index = (i == num_threads - 1) ? file_size : (i + 1) * chunk_size;
        for (int j = 0; j < MAX_CHARS; j++) 
            thread_data[i].huffman_codes[j] = huffman_codes[j];
        long thread_chunk_length = thread_data[i].end_index - thread_data[i].start_index;
        thread_data[i].compressed_buffer = calloc(thread_chunk_length + 64, 1);
        pthread_create(&threads[i], NULL, compress_section, &thread_data[i]);
    }
    // --- 4. Combine Thread Outputs ---
    FILE *output_file = fopen(output_filename, "wb"); // <-- Use argument here
    if (!output_file) { perror("Output file error"); return 1; }
    fwrite(&num_threads, sizeof(int), 1, output_file);
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        // Write total bits of this section
        fwrite(&thread_data[i].total_bits_written, sizeof(long), 1, output_file);
        // Write compressed data
        long bytes_to_write = (thread_data[i].total_bits_written + 7) / 8;
        if (bytes_to_write > 0) fwrite(thread_data[i].compressed_buffer, 1, bytes_to_write, output_file);
        free(thread_data[i].compressed_buffer);
    }
    printf("Success! %s generated using %d threads.\n", output_filename, num_threads);
    fclose(output_file);
    free(file_buffer);
    for (int i = 0; i < MAX_CHARS; i++) if (huffman_codes[i]) free(huffman_codes[i]);

    return 0;
}

