# Binary Run-Length Encoder (v1.2.0: Automated Integration Testing)

A highly optimized data compression pipeline written in C and Python. This project demonstrates stream processing, constant-space $O(1)$ memory management, and automated integration testing to verify lossless data compression via Run-Length Encoding (RLE).

## 1. Project Overview
This toolset reads raw binary files, extracts data bit-by-bit using bitwise shifting and masking, and compresses the stream using an RLE algorithm. 

The system consists of three core components:
* **The Generator (`test.py`):** An $O(N)$ Python script generating localized, low-entropy test data using raw bitwise shifts (`<<`, `|`).
* **The Codec (`compressor.c` & `decompressor.c`):** C programs utilizing `fread` and `fwrite` to stream raw bytes into localized accumulators. They evaluate bits, count consecutive runs, and safely handle hardware limits (255 overflow) while maintaining strict RLE alternating sequences.
* **The Orchestrator (`main.py`):** An automated Python pipeline that coordinates the compilation, execution, and mathematical verification of the entire process.

## 2. What's New in v1.2.0
* **Automated Integration Pipeline:** Introduced `main.py` to orchestrate the entire data lifecycle. It automatically generates the binary file, triggers the C compressor, triggers the C decompressor, and evaluates the output.
* **Lossless Verification:** Integrated Python's `filecmp` module (`shallow=False`) to perform deep byte-by-byte comparisons between the original `test.bin` and the final `decompressed.bin`. This algorithmically guarantees zero data corruption during the round trip.
* **Subprocess Orchestration & Error Handling:** The pipeline uses `subprocess.run` with strict error catching to isolate segmentation faults or execution failures in the C binaries, preventing cascading errors in the testing environment.

## 3. Resolving "Negative Compression"
Early iterations processed purely random (50/50) high-entropy data, resulting in "negative compression" due to the 8-bit overhead required to store RLE counts. By implementing weighted probabilities, the data now "clusters," allowing the RLE engine to pack large blocks of identical bits into single bytes, achieving highly efficient, positive compression ratios.

## 4. Architecture & Complexity
* **Time Complexity:** $O(N)$ for data generation, compression, and decompression. The pipeline operates in a single, highly efficient forward pass.
* **Space Complexity:** $O(1)$ (Constant Space). By using a stream-processing architecture, the C tools only consume a few bytes of RAM for accumulators and file pointers, regardless of input file size.