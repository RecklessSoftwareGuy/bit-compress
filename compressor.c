#include<stdio.h>
#include<stdlib.h>

int main() {
    FILE *input_file = fopen("assets/test.bin", "rb");
    FILE *output_file = fopen("assets/compressed.bin", "wb");
    if (input_file == NULL || output_file == NULL) {
        perror("Error opening files");
        return EXIT_FAILURE;
    }

    unsigned char current_byte;
    unsigned char count = 0;

    //1. Start the compression by first targetting 0s
    int current_target = 0; 

    printf("Starting Run-Length Encoding compression...\n");

    // 2. Read the file one byte at a time
    while (fread(&current_byte, sizeof(unsigned char), 1, input_file) == 1) {
        
        // 3. Extract and process each of the 8 bits, go backwards & right-shift as C starts reading bytes from LSB
        for (int i = 7; i >= 0; i--) {
            int bit = (current_byte >> i) & 1;
            if (bit == current_target) {
                count++;
                // 4. Handler for 8-bit overflow (max value 255)
                if (count == 255) {
                    // Write the maxed-out count to the file
                    fwrite(&count, sizeof(unsigned char), 1, output_file);
                    // Because RLE strictly alternates, the decoder will expect the next byte to be the count of the opposite bit. Since our run is still going, we tell the decoder the opposite bit appears 0 times.
                    unsigned char zero_count = 0;
                    fwrite(&zero_count, sizeof(unsigned char), 1, output_file);
                    
                    // Reset our counter to keep counting the current bit
                    count = 0;
                }
            } else {
                // 5. The bit flipped either from 0 to 1 or 1 to 0
                // Write the final count of the previous run to the file
                fwrite(&count, sizeof(unsigned char), 1, output_file);
                
                // Reset the count to 1 (because we just saw the first bit of the new run)
                count = 1;
                
                // Flip our target tracker
                current_target = bit; 
            }
        }
    }

    // 6. Write the very last count when the file ends
    fwrite(&count, sizeof(unsigned char), 1, output_file);

    printf("Compression complete! Check assets/compressed.bin\n");

    fclose(input_file);
    fclose(output_file);
    return EXIT_SUCCESS;
}