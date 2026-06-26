#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_CHARS 256
#define MAX_CODE_LEN 256

// Struct for reverse lookup
typedef struct {
    char code[MAX_CODE_LEN];
    unsigned char character;
} CodeMap;

// Thread data
typedef struct {
    unsigned char *buffer;
    long bits;
    CodeMap *code_map;
    int code_count;
    char *output_filename;
    int append_mode; // 0 = write, 1 = append
} ThreadData;

// Thread function: decompress a section
void* decompress_section(void *arg) {
    ThreadData *data = (ThreadData*)arg;

    FILE *out = fopen(data->output_filename, data->append_mode ? "ab" : "wb");
    if (!out) { perror("Output file error"); return NULL; }

    char bit_string[data->bits + 1];
    bit_string[0] = '\0';

    for (long i = 0; i < data->bits; i++) {
        int bit = (data->buffer[i >> 3] >> (7 - (i & 7))) & 1;
        int len = strlen(bit_string);
        bit_string[len] = bit + '0';
        bit_string[len + 1] = '\0';

        // Match bit_string with code_map
        for (int j = 0; j < data->code_count; j++) {
            if (strcmp(bit_string, data->code_map[j].code) == 0) {
                fputc(data->code_map[j].character, out);
                bit_string[0] = '\0'; // reset for next character
                break;
            }
        }
    }

    fclose(out);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <compressed_file> <encoding_file> <num_threads> <output_file>\n", argv[0]);
        return 1;
    }

    char *compressed_filename = argv[1];
    char *encoding_filename   = argv[2];
    int num_threads           = atoi(argv[3]); // single thread = 1
    char *output_filename     = argv[4];       // <-- Correctly using argv[4]

    if (num_threads <= 0) num_threads = 1; // default to 1

    // --- 1. Load encoding file into reverse lookup table ---
    CodeMap code_map[MAX_CHARS];
    int code_count = 0;

    FILE *ef = fopen(encoding_filename, "r");
    if (!ef) { perror("Encoding file error"); return 1; }

    char line[256];
    while (fgets(line, sizeof(line), ef)) {
        if (strstr(line, "---") || strstr(line, "Char |")) continue;

        unsigned char ch;
        char code[MAX_CODE_LEN];
        char *pipe_pos = strrchr(line, '|');
        if (!pipe_pos) continue;

        if (strncmp(line, "\\n", 2) == 0) ch = '\n';
        else if (strncmp(line, "\\r", 2) == 0) ch = '\r';
        else if (strncmp(line, "' '", 3) == 0) ch = ' ';
        else ch = (unsigned char)line[0];

        if (sscanf(pipe_pos + 1, "%s", code) == 1) {
            strcpy(code_map[code_count].code, code);
            code_map[code_count].character = ch;
            code_count++;
        }
    }
    fclose(ef);

    // --- 2. Open compressed file ---
    FILE *cf = fopen(compressed_filename, "rb");
    if (!cf) { perror("Compressed file error"); return 1; }

    int total_sections;
    fread(&total_sections, sizeof(int), 1, cf);

    printf("Decompression using single thread (via pthread structure)\n");

    // --- 3. Decompress each section ---
    for (int s = 0; s < total_sections; s++) {
        long section_bits;
        fread(&section_bits, sizeof(long), 1, cf);
        long section_bytes = (section_bits + 7) / 8;

        unsigned char *buffer = malloc(section_bytes);
        fread(buffer, 1, section_bytes, cf);

        ThreadData data = {
            .buffer = buffer,
            .bits = section_bits,
            .code_map = code_map,
            .code_count = code_count,
            .output_filename = output_filename,
            .append_mode = (s != 0) // first section writes, others append
        };

        pthread_t t;
        pthread_create(&t, NULL, decompress_section, &data);
        pthread_join(t, NULL); // single-threaded

        free(buffer);
    }

    fclose(cf);
    printf("Decompression complete! Output file: %s\n", output_filename);

    return 0;
}

