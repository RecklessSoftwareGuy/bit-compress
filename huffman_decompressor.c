#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* Matching Huffman decompressor for huffman_compressor.c.
   Rebuilds the identical tree from the freq table written in the payload header. */

#define MAX_NODES 512

typedef struct {
    int freq;
    unsigned char symbol;
    int left;
    int right;
} Node;

static Node nodes[MAX_NODES];
static int node_count = 0;

/* Exact copy of the reliable build_tree from compressor for identical results. */
static int build_tree(const unsigned int freq[256]) {
    node_count = 0;

    for (int s = 0; s < 256; s++) {
        if (freq[s] > 0) {
            nodes[node_count].freq = freq[s];
            nodes[node_count].symbol = (unsigned char)s;
            nodes[node_count].left = -1;
            nodes[node_count].right = -1;
            node_count++;
        }
    }

    if (node_count == 0) {
        nodes[node_count].freq = 0;
        nodes[node_count].symbol = 0;
        nodes[node_count].left = -1;
        nodes[node_count].right = -1;
        return node_count++;
    }

    if (node_count == 1) {
        return 0;
    }

    int active[512];
    int active_count = 0;

    for (int i = 0; i < node_count; i++) {
        active[active_count++] = i;
    }

    while (active_count > 1) {
        int min1 = -1, min2 = -1;
        for (int j = 0; j < active_count; j++) {
            int idx = active[j];
            int t = (nodes[idx].left == -1 && nodes[idx].right == -1) ? nodes[idx].symbol : (256 + idx);
            if (min1 == -1) {
                min1 = j;
            } else if (min2 == -1) {
                min2 = j;
                if (nodes[active[j]].freq < nodes[active[min2]].freq ||
                    (nodes[active[j]].freq == nodes[active[min2]].freq && t < ((nodes[active[min2]].left == -1 && nodes[active[min2]].right == -1) ? nodes[active[min2]].symbol : (256 + active[min2])))) {
                    min2 = j;
                }
                if (nodes[active[min1]].freq > nodes[active[min2]].freq ||
                    (nodes[active[min1]].freq == nodes[active[min2]].freq &&
                     ((nodes[active[min1]].left == -1 && nodes[active[min1]].right == -1) ? nodes[active[min1]].symbol : (256 + active[min1])) > t)) {
                    int tmp = min1; min1 = min2; min2 = tmp;
                }
            } else {
                int cur_f = nodes[idx].freq;
                int cur_t = t;
                int m1_f = nodes[active[min1]].freq;
                int m1_t = (nodes[active[min1]].left == -1 && nodes[active[min1]].right == -1) ? nodes[active[min1]].symbol : (256 + active[min1]);
                int m2_f = nodes[active[min2]].freq;
                int m2_t = (nodes[active[min2]].left == -1 && nodes[active[min2]].right == -1) ? nodes[active[min2]].symbol : (256 + active[min2]);
                if (cur_f < m1_f || (cur_f == m1_f && cur_t < m1_t)) {
                    min2 = min1;
                    min1 = j;
                } else if (cur_f < m2_f || (cur_f == m2_f && cur_t < m2_t)) {
                    min2 = j;
                }
            }
        }

        int a_idx = active[min1];
        int b_idx = active[min2];

        if (min1 > min2) { int t = min1; min1 = min2; min2 = t; }
        active[min1] = active[--active_count];
        if (min2 == active_count) min2 = min1;
        active[min2] = active[--active_count];

        nodes[node_count].freq = nodes[a_idx].freq + nodes[b_idx].freq;
        nodes[node_count].symbol = 0;
        nodes[node_count].left = a_idx;
        nodes[node_count].right = b_idx;
        active[active_count++] = node_count;
        node_count++;
    }

    return active[0];
}

static void handle_degenerate(const unsigned int freq[256], int root) {
    int only_symbol = -1;
    int nonzero = 0;
    for (int s = 0; s < 256; s++) {
        if (freq[s] > 0) { nonzero++; only_symbol = s; }
    }
    if (nonzero <= 1) {
        if (only_symbol < 0) only_symbol = 0;
        nodes[root].symbol = (unsigned char)only_symbol;
        nodes[root].left = -1;
        nodes[root].right = -1;
    }
}

int main(int argc, char *argv[]) {
    const char *input_path  = (argc >= 2) ? argv[1] : "assets/compressed.bin";
    const char *output_path = (argc >= 3) ? argv[2] : "assets/decompressed.bin";

    FILE *in = fopen(input_path, "rb");
    FILE *out = fopen(output_path, "wb");
    if (!in || !out) {
        perror("Error opening files");
        return EXIT_FAILURE;
    }

    printf("Starting Huffman decompression...\n");
    clock_t start = clock();

    /* Read header (native byte order, matching compressor) */
    uint32_t freq32[256];
    if (fread(freq32, sizeof(uint32_t), 256, in) != 256) {
        fprintf(stderr, "Truncated Huffman file (freq table)\n");
        return EXIT_FAILURE;
    }
    unsigned int freq[256];
    for (int i = 0; i < 256; i++) freq[i] = freq32[i];

    uint64_t orig_size = 0;
    if (fread(&orig_size, sizeof(uint64_t), 1, in) != 1) {
        fprintf(stderr, "Truncated Huffman file (orig size)\n");
        return EXIT_FAILURE;
    }

    uint64_t coded_bits = 0;
    if (fread(&coded_bits, sizeof(uint64_t), 1, in) != 1) {
        fprintf(stderr, "Truncated Huffman file (coded bits)\n");
        return EXIT_FAILURE;
    }

    int root = build_tree(freq);
    handle_degenerate(freq, root);

    uint64_t symbols_left = orig_size;
    unsigned char in_byte = 0;
    int bits_in_byte = 0;   /* how many bits we have consumed from current in_byte (0-7) */
    int current_node = root;
    int bits_fetched = 0;

    /* Degenerate 0-bit case (single symbol, coded_bits==0) */
    if (nodes[root].left == -1 && nodes[root].right == -1 && symbols_left > 0) {
        unsigned char sym = nodes[root].symbol;
        for (uint64_t i = 0; i < symbols_left; i++) {
            fwrite(&sym, 1, 1, out);
        }
        symbols_left = 0;
    }

    while (symbols_left > 0) {
        /* Need a new byte? */
        if (bits_in_byte == 0) {
            if (fread(&in_byte, 1, 1, in) != 1) {
                fprintf(stderr, "Unexpected EOF while reading Huffman bitstream\n");
                return EXIT_FAILURE;
            }
            bits_in_byte = 8;
        }

        /* Take MSB first within the byte (matches compressor emission order) */
        int bit = (in_byte >> (bits_in_byte - 1)) & 1;
        bits_in_byte--;
        bits_fetched++;

        /* Walk */
        current_node = bit ? nodes[current_node].right : nodes[current_node].left;

        if (nodes[current_node].left == -1 && nodes[current_node].right == -1) {
            /* reached leaf */
            unsigned char sym = nodes[current_node].symbol;
            fwrite(&sym, 1, 1, out);
            symbols_left--;
            current_node = root;
        }
    }

    // DEBUG
    fprintf(stderr, "DEBUG decomp: header_coded_bits=%llu fetched_bits=%d emitted=%llu (orig=%llu)\n",
            (unsigned long long)coded_bits, bits_fetched,
            (unsigned long long)(orig_size - symbols_left), (unsigned long long)orig_size);

    clock_t end = clock();
    double secs = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Execution Time: %f seconds\n", secs);
    printf("Data successfully written to %s\n", output_path);

    fclose(in);
    fclose(out);
    return EXIT_SUCCESS;
}
