# Binary Run-Length Encoder (v1.0)

A foundational data compression tool written entirely in C. This project is designed to read raw binary files, extract data bit-by-bit using bitwise shifting and masking, and process the stream using a classic Run-Length Encoding (RLE) algorithm. 

## 1. Project Overview
The pipeline consists of:
* **Data Generation:** A Python script utilizing `bytearray` to generate randomized, byte-aligned binary test files.
* **Stream Processing:** A C program that uses `fread` to stream the binary data into memory without loading the entire file, ensuring constant space complexity.
* **Bitwise Extraction:** A localized 8-bit accumulator pattern that shifts (`>>`) and masks (`& 1`) the data to process the file exactly as it exists on the hardware.

## 2. The Implementation (v1.0)
The compressor successfully tracks consecutive runs of `0`s and `1`s. To handle hardware limits, the architecture implements an overflow protocol: if a sequence exceeds the maximum value of an 8-bit `unsigned char` (255), the program writes the max count, inserts a `0` count for the alternating bit, and continues counting. This maintains the strict alternating sequence required by RLE. 

Outputs are written directly to disk as raw bytes using `fwrite` to prevent ASCII bloat.

## 3. The Problem: Negative Compression
During the initial benchmarking of Version 1.0, the system exhibited a severe case of **negative compression**. 

Instead of shrinking the test data, the resulting `compressed.bin` file was significantly larger than the original `test.bin` file. The algorithm functioned flawlessly from a logical standpoint, but mathematically expanded the data footprint.

## 4. Root Cause Analysis: Entropy and Information Theory
The negative compression is not a software bug; it is a direct observation of Shannon’s Information Theory regarding **data entropy**.

The Python test generation script utilized `random.choice([0, 1])`, creating a perfectly randomized 50/50 distribution of bits. This created a file with maximum entropy (high unpredictability), meaning the "runs" of consecutive identical bits were incredibly short—often just 1 or 2 bits long.

**The Mathematical Overhead:**
RLE requires 8 bits (1 byte) to store the count of a run.
* If the algorithm encounters a single isolated `1` in the data stream, the original size of that data is **1 bit**.
* To "compress" it, the C program writes the count `1` to the output file as an 8-bit integer (`00000001`).
* Therefore, it costs **8 bits** of overhead to represent **1 bit** of original data, resulting in an 800% size increase for that specific sequence.

Because the data was completely random, the algorithm spent the entire execution writing 8-bit counts for 1-bit and 2-bit runs, causing the file size to heavily inflate.

## 5. Conclusion
Version 1.0 successfully proves the mechanical function of the bitwise RLE engine but demonstrates that Run-Length Encoding cannot compress high-entropy data. RLE relies entirely on predictability. To achieve actual compression, the input data must have low entropy (long, unbroken stretches of identical bits).