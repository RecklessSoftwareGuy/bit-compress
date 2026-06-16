#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* Simple but complete byte-oriented Huffman compressor.
   Matches the style of the RLE pair: streaming I/O where practical,
   clock timing, clear messages, fopen error handling, argv support. */

#define MAX_NODES 512

typedef struct {
    int freq;
    unsigned char symbol;
    int left;   /* index in pool, -1 for leaf */
    int right;
} Node;

static Node nodes[MAX_NODES];
static int node_count = 0;

static unsigned int codes[256];
static int code_lengths[256];

/* Build the tree from freq[256] using simple linear scan for mins (reliable, N<=512 is tiny).
   This guarantees identical tree in compressor and decompressor for the same freqs. */
static int build_tree(const unsigned int freq[256]) {
    node_count = 0;

    /* Only create leaves for symbols that actually appear (cleaner) */
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
        /* empty */
        nodes[node_count].freq = 0;
        nodes[node_count].symbol = 0;
        nodes[node_count].left = -1;
        nodes[node_count].right = -1;
        return node_count++;
    }

    if (node_count == 1) {
        /* single symbol - the only leaf we created */
        return 0;  /* the first (only) node we added */
    }

    /* active list of current tree roots (indices into nodes) */
    int active[512];
    int active_count = 0;

    for (int i = 0; i < node_count; i++) {
        active[active_count++] = i;
    }

    while (active_count > 1) {
        /* find the two lowest freq roots, tie break by smaller tie (we use symbol for leaves, creation for internals) */
        int min1 = -1, min2 = -1;
        for (int j = 0; j < active_count; j++) {
            int idx = active[j];
            int t = (nodes[idx].left == -1 && nodes[idx].right == -1) ? nodes[idx].symbol : (256 + idx);
            if (min1 == -1) {
                min1 = j;
            } else if (min2 == -1) {
                min2 = j;
                if (nodes[active[j]].freq < nodes[active[min2]].freq ||
                    (nodes[active[j]].freq == nodes[active[min2]].freq && t < ( (nodes[active[min2]].left==-1 && nodes[active[min2]].right==-1) ? nodes[active[min2]].symbol : (256+active[min2]) ) )) {
                    min2 = j;
                }
                if (nodes[active[min1]].freq > nodes[active[min2]].freq ||
                    (nodes[active[min1]].freq == nodes[active[min2]].freq && 
                     ( (nodes[active[min1]].left==-1&&nodes[active[min1]].right==-1)?nodes[active[min1]].symbol:(256+active[min1]) ) > t )) {
                    int tmp = min1; min1 = min2; min2 = tmp;
                }
            } else {
                int cur_f = nodes[idx].freq;
                int cur_t = (nodes[idx].left == -1 && nodes[idx].right == -1) ? nodes[idx].symbol : (256 + idx);
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

        /* remove min1 and min2 from active (order doesn't matter, swap with last) */
        if (min1 > min2) { int t=min1; min1=min2; min2=t; }
        active[min1] = active[--active_count];
        if (min2 == active_count) min2 = min1;
        active[min2] = active[--active_count];

        /* new internal */
        nodes[node_count].freq = nodes[a_idx].freq + nodes[b_idx].freq;
        nodes[node_count].symbol = 0;
        nodes[node_count].left = a_idx;
        nodes[node_count].right = b_idx;
        active[active_count++] = node_count;
        node_count++;
    }

    return active[0];
}

/* Assign prefix codes by walking the tree (recursive for clarity, depth is tiny) */
static void generate_codes(int node_idx, unsigned int prefix, int depth) {
    if (nodes[node_idx].left == -1 && nodes[node_idx].right == -1) {
        /* leaf */
        codes[ nodes[node_idx].symbol ] = prefix;
        code_lengths[ nodes[node_idx].symbol ] = depth;
        return;
    }
    generate_codes(nodes[node_idx].left,  (prefix << 1)     , depth + 1);
    generate_codes(nodes[node_idx].right, (prefix << 1) | 1 , depth + 1);
}

/* Handle the degenerate single-symbol (or empty) case explicitly */
static void handle_degenerate(const unsigned int freq[256], int root) {
    int only_symbol = -1;
    int nonzero = 0;
    for (int s = 0; s < 256; s++) {
        if (freq[s] > 0) {
            nonzero++;
            only_symbol = s;
        }
    }
    if (nonzero <= 1) {
        /* Everything (or nothing) is this symbol. We will emit 0 coded bits. */
        if (only_symbol < 0) only_symbol = 0;
        codes[only_symbol] = 0;
        code_lengths[only_symbol] = 0;  /* special marker: 0 bits per symbol */
        /* Force root to be that leaf for decode simplicity */
        nodes[root].symbol = (unsigned char)only_symbol;
        nodes[root].left = -1;
        nodes[root].right = -1;
    }
}

int main(int argc, char *argv[]) {
    const char *input_path  = (argc >= 2) ? argv[1] : "assets/test.bin";
    const char *output_path = (argc >= 3) ? argv[2] : "assets/compressed.bin";

    FILE *in = fopen(input_path, "rb");
    FILE *out = fopen(output_path, "wb");
    if (!in || !out) {
        perror("Error opening files");
        return EXIT_FAILURE;
    }

    printf("Starting Huffman compression...\n");
    clock_t start = clock();

    /* 1. Frequency count (one pass) */
    unsigned int freq[256] = {0};
    unsigned char buf[4096];
    size_t n;
    uint64_t orig_size = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        for (size_t i = 0; i < n; i++) {
            freq[buf[i]]++;
        }
        orig_size += n;
    }

    /* 2. Build tree + codes */
    int root = build_tree(freq);
    memset(codes, 0, sizeof(codes));
    memset(code_lengths, 0, sizeof(code_lengths));
    if (nodes[root].left != -1 || nodes[root].right != -1) {
        generate_codes(root, 0, 0);
    }
    handle_degenerate(freq, root);

    /* Rewind input for encode pass */
    fseek(in, 0, SEEK_SET);

    /* 3. Write simple payload header (what the decomp and Python wrapper expect):
       - 256 * uint32 freq table
       - uint64 orig_size
       - uint64 coded_bits (exact number of Huffman bits, not bytes)
       Then the bit-packed data. */
    /* Write header in native byte order (both compressor and decompressor run on same host).
       Python wrapper layer does not parse this inner header. */
    uint32_t freq32[256];
    for (int i = 0; i < 256; i++) freq32[i] = freq[i];
    fwrite(freq32, sizeof(uint32_t), 256, out);
    fwrite(&orig_size, sizeof(uint64_t), 1, out);

    /* Placeholder for coded_bits (patched after encoding) */
    uint64_t coded_bits_placeholder = 0;
    fwrite(&coded_bits_placeholder, sizeof(uint64_t), 1, out);

    /* Encode pass + count exact bits */
    uint64_t coded_bits = 0;
    unsigned char out_byte = 0;
    int bit_pos = 0;  /* 0..7, bits accumulated in out_byte (MSB first) */

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        for (size_t i = 0; i < n; i++) {
            unsigned char sym = buf[i];
            int clen = code_lengths[sym];
            unsigned int c = codes[sym];

            if (clen == 0) {
                /* Degenerate case: 0 bits for this symbol, just count orig symbols */
                continue;
            }

            for (int b = clen - 1; b >= 0; b--) {  /* MSB of code first */
                int bit = (c >> b) & 1;
                out_byte = (out_byte << 1) | bit;
                bit_pos++;
                coded_bits++;
                if (bit_pos == 8) {
                    fwrite(&out_byte, 1, 1, out);
                    out_byte = 0;
                    bit_pos = 0;
                }
            }
        }
    }

    /* Flush last partial byte (if any) */
    if (bit_pos > 0) {
        out_byte <<= (8 - bit_pos);  /* pad with 0s on the right (LSB) */
        fwrite(&out_byte, 1, 1, out);
    }

    uint64_t sum_from_len = 0;
    for (int s = 0; s < 256; s++) {
        sum_from_len += (uint64_t)freq[s] * code_lengths[s];
    }
    fprintf(stderr, "DEBUG comp: coded_bits_during_emit=%llu sum_from_freqXlen=%llu\n",
            (unsigned long long)coded_bits, (unsigned long long)sum_from_len);

    /* Write coded_bits after the data? No — we already wrote a placeholder. Fix: seek back or write header at end.
       Simpler for streaming: write a dummy 0 for coded_bits first, then after encoding seek+patch, or
       collect the packed bytes in memory for small files, or write header *after* data and adjust decomp.
       For demo clarity and small files we collect the payload bytes after the freq+size, then write coded_bits + payload.
       Easiest robust way: write everything except coded_bits, remember position, write coded_bits at the right spot. */

    /* Because we wrote coded_bits too early, the cleanest is to:
       - Write freq + orig_size + (placeholder 0 for coded_bits) + packed bytes
       - Then seek back to the coded_bits field and overwrite it.
       This is what the code above almost did. We wrote coded_bits before the loop with 0 value. */

    /* Patch the coded_bits field (it sits after the 256*4 + 8 bytes) */
    long coded_bits_offset = (long)(256 * sizeof(uint32_t) + sizeof(uint64_t));
    fseek(out, coded_bits_offset, SEEK_SET);
    fwrite(&coded_bits, sizeof(uint64_t), 1, out);

    /* Go to end for correct ftell size */
    fseek(out, 0, SEEK_END);

    clock_t end = clock();
    long out_size = ftell(out);
    double secs = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Execution Time:   %f seconds\n", secs);
    printf("Input File Size:  %llu bytes\n", (unsigned long long)orig_size);
    printf("Output File Size: %ld bytes\n", out_size);

    double ratio = orig_size ? (1.0 - (double)out_size / (double)orig_size) * 100.0 : 0.0;
    if (out_size < (long)orig_size) {
        printf("Space Saved:\t%.2f%%\n", ratio);
    } else {
        printf("Space Saved:\t%.2f%% (Negative Compression)\n", ratio);
    }

    fclose(in);
    fclose(out);
    return EXIT_SUCCESS;
}
