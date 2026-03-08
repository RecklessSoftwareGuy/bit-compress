# Binary Run-Length Encoder (v1.1.0: The Entropy Update)

A highly optimized data compression pipeline written in C and Python. This project demonstrates stream processing, constant-space $O(1)$ memory management, and the practical application of Shannon's Information Theory through Run-Length Encoding (RLE).

## 1. Project Overview
This toolset reads raw binary files, extracts data bit-by-bit using bitwise shifting and masking, and compresses the stream using an RLE algorithm. 

The system consists of two highly optimized components:
* **The Generator (`test.py`):** An $O(N)$ Python script that bypasses standard string immutability overhead. It generates localized test data using raw bitwise shifts (`<<`, `|`) and packs them directly into a C-style `bytearray`.
* **The Compressor (`compressor.c`):** A C program utilizing `fread` and `fwrite` to stream raw bytes into a localized 8-bit accumulator. It evaluates the bits, counts consecutive runs, and safely handles 8-bit hardware limits (255 overflow) while maintaining strict RLE alternating sequences.

## 2. What's New in v1.1.0
* **Algorithmic Benchmarking:** The C compressor now natively tracks CPU clock cycles via `<time.h>` and utilizes file pointer offsets (`ftell()`) to calculate exact byte differentials, generating a real-time performance report upon execution.
* **Controllable Entropy:** The Python generator now utilizes weighted probabilities (`random.choices`) rather than strict 50/50 randomization. This simulates the low-entropy data clustering found in real-world binary files (like flat-color bitmaps or formatted text).
* **$O(N)$ Execution Speed:** Removed $O(N^2)$ string concatenation bottlenecks in the Python generator, resulting in near-instantaneous test file generation.

## 3. Resolving "Negative Compression"
In version 1.0, the system processed purely random (50/50) high-entropy data, resulting in "negative compression." Because RLE requires an 8-bit integer to store the count of a run, compressing a 1-bit run resulted in an 800% overhead footprint.

By implementing the controllable entropy dial in v1.1.0 (e.g., heavily weighting the probability of generating `0`s over `1`s), the data now "clusters." The RLE engine can successfully process unbroken stretches of dozens or hundreds of identical bits, effectively packing large blocks of raw data into single 8-bit count bytes and achieving highly efficient, positive compression ratios.

## 4. Architecture & Complexity
* **Time Complexity:** $O(N)$ for both data generation and compression. The pipeline operates in a single, highly efficient forward pass.
* **Space Complexity:** $O(1)$ (Constant Space). By using a stream-processing architecture, the compressor only consumes a few bytes of RAM for the accumulator and file pointers, regardless of whether the input file is 10 Kilobytes or 10 Terabytes.