#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
    const char *input_path = (argc >= 2) ? argv[1] : "assets/compressed.bin";
    const char *output_path = (argc >= 3) ? argv[2] : "assets/decompressed.bin";

    FILE *input_file = fopen(input_path, "rb");
    FILE *output_file = fopen(output_path, "wb");

    if (input_file == NULL || output_file == NULL) {
        perror("Error opening files");
        return EXIT_FAILURE;
    }

    unsigned char count_byte;
    int current_target = 0; 
    unsigned char output_buffer = 0;
    int buffer_count = 0;

    printf("Starting RLE decompression...\n");
    clock_t start_time = clock();

    // 1. Read the compressed file one count at a time
    while (fread(&count_byte, sizeof(unsigned char), 1, input_file) == 1) {
        
        // 2. Expand 'count_byte' bits of current_target, batched for long runs (major perf win)
        int remaining = count_byte;

        // Fill any partial byte in the buffer first
        if (buffer_count > 0 && remaining > 0) {
            int need = 8 - buffer_count;
            int take = (remaining < need) ? remaining : need;
            for (int k = 0; k < take; k++) {
                output_buffer = (output_buffer << 1) | current_target;
            }
            buffer_count += take;
            remaining -= take;
            if (buffer_count == 8) {
                fwrite(&output_buffer, sizeof(unsigned char), 1, output_file);
                output_buffer = 0;
                buffer_count = 0;
            }
        }

        // Emit as many full identical bytes as possible (the key O(L/8) optimization)
        if (remaining > 0) {
            unsigned char fill = current_target ? 0xFF : 0x00;
            int full_bytes = remaining / 8;
            for (int i = 0; i < full_bytes; i++) {
                fwrite(&fill, sizeof(unsigned char), 1, output_file);
            }
            remaining %= 8;

            // Remainder bits (< 8) go into the (now clean) buffer
            if (remaining > 0) {
                for (int k = 0; k < remaining; k++) {
                    output_buffer = (output_buffer << 1) | current_target;
                }
                buffer_count = remaining;
            }
        }

        current_target = 1 - current_target;
    }

    clock_t end_time = clock();
    double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("Execution Time: %f seconds\n", time_taken);
    printf("Data successfully written to %s\n", output_path);

    fclose(input_file);
    fclose(output_file);
    return EXIT_SUCCESS;
}