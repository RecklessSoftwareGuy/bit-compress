#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    FILE *input_file = fopen("assets/compressed.bin", "rb");
    FILE *output_file = fopen("assets/decompressed.bin", "wb");

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
        
        // 2. Loop 'count_byte' times
        for (int i = 0; i < count_byte; i++) {
            
            // 3. Pack the current bit into our 8-bit buffer
            output_buffer = (output_buffer << 1) | current_target;
            buffer_count++;

            // 4. When the buffer hits exactly 8 bits, flush it to the disk
            if (buffer_count == 8) {
                fwrite(&output_buffer, sizeof(unsigned char), 1, output_file);
                output_buffer = 0; // Reset the buffer
                buffer_count = 0;  // Reset the bit tracker
            }
        }
        current_target = 1 - current_target;
    }

    clock_t end_time = clock();
    double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("Execution Time: %f seconds\n", time_taken);
    printf("Data successfully written to assets/decompressed.bin\n");

    fclose(input_file);
    fclose(output_file);
    return EXIT_SUCCESS;
}