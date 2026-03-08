#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    FILE *input_file = fopen("assets/test.bin", "rb");
    FILE *output_file = fopen("assets/compressed.bin", "wb");
    if (input_file == NULL || output_file == NULL) {
        perror("Error opening files");
        return EXIT_FAILURE;
    }

    unsigned char current_byte;
    unsigned char count = 0;
    int current_target = 0; 

    printf("Starting Run-Length Encoding compression...\n");
    clock_t start_time = clock();
    while (fread(&current_byte, sizeof(unsigned char), 1, input_file) == 1) {
        for (int i = 7; i >= 0; i--) {
            int bit = (current_byte >> i) & 1;
            if (bit == current_target) {
                count++;
                if (count == 255) {
                    fwrite(&count, sizeof(unsigned char), 1, output_file);
                    unsigned char zero_count = 0;
                    fwrite(&zero_count, sizeof(unsigned char), 1, output_file);
                    count = 0;
                }
            } else {
                fwrite(&count, sizeof(unsigned char), 1, output_file);
                count = 1;
                current_target = bit; 
            }
        }
    }


    //Benchmarking
    fwrite(&count, sizeof(unsigned char), 1, output_file);
    clock_t end_time = clock();
    long input_size = ftell(input_file);
    long output_size = ftell(output_file);
    double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    double ratio = (1.0 - ((double)output_size / (double)input_size)) * 100.0;
    printf("Execution Time:   %f seconds\n", time_taken);
    printf("Input File Size:  %ld bytes\n", input_size);
    printf("Output File Size: %ld bytes\n", output_size);
    
    if (output_size < input_size) {
        printf("Space Saved:\t%.2f%%\n", ratio);
    } else {
        printf("Space Saved:\t%.2f%% (Negative Compression)\n", ratio);
    }

    fclose(input_file);
    fclose(output_file);
    return EXIT_SUCCESS;
}