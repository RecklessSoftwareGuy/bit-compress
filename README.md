# Binary Run-Length Encoder + Huffman (v1.3: Auto-Selecting Codec + Stats)

A highly optimized data compression pipeline written in C and Python. When presented with a binary file (or bytes/"binary string"), the Python front-end automatically chooses between **bit-level RLE** and **byte-level Huffman**, applies the winner, and reports clear statistics (Initial size, Final size, compression ratio).

The system preserves the original stream-processing RLE implementation (with practical time improvements) while adding a matching pair of C Huffman codecs. Python remains the orchestrator/decision/stats layer.

## 1. Project Overview
The tool reads raw binary data and can compress it losslessly with the better of two simple classical algorithms:

* **Bit RLE** (existing, highly effective on clustered / low-entropy bit streams such as the built-in generator's 95/5 data)
* **Byte Huffman** (new, effective on skewed byte distributions)

**Core components**
* **Generator (`test.py`):** Fast O(N/8) generator of biased test data (long runs of 0-bits). No longer executes as a side-effect on import.
* **RLE Codec (`compressor.c` / `decompressor.c`):** Unchanged bit-level RLE logic + argv support for arbitrary files + major constant-factor improvement in the decompressor (batched 0x00/0xFF emission for long runs).
* **Huffman Codec (`huffman_compressor.c` / `huffman_decompressor.c`):** New C implementation (freq table, tree via min-heap + node pool, prefix codes, bit-packed payload with freq+size header, identical tree reconstruction on decode).
* **Front-end / Orchestrator (`main.py`):** argparse CLI + public `compress()` API. Decision (by running both and picking the actual smaller output), BCMP self-describing wrapper (magic + algo + orig_size), stats reporting, subprocess dispatch to the right C pair, and lossless round-trip verification.

## 2. Usage & Statistics (the main new feature)

```bash
# Full demo pipeline (generates data, chooses, compresses, verifies, prints stats)
python main.py

# Compress a user-provided file (auto chooses RLE or Huffman)
python main.py myfile.bin myfile.bin.compressed

# From Python (also accepts raw bytes = "binary string")
from main import compress
stats = compress(b"\x00"*5000 + b"\x01"*3, "/tmp/out.bin")
print(stats)   # {'algo': 'RLE', 'initial': ..., 'final': ..., 'ratio': 93.12, ...}
```

Typical stats output:
```
Initial file size:  3605 bytes
Final file size:    2766 bytes
Compression ratio:  23.27% (RLE)
Time taken:         0.0028 s
```

The delivered file begins with a small `BCMP` header (Python layer) so the correct decompressor can be dispatched later. The raw codec payloads remain usable directly with the C binaries.

## 3. Time Complexity Improvements
* Generator: per-bit Python loop + `random.choices` (O(N)) → direct byte construction via `random.sample` of the sparse 1-bits (≈ O(N/8 + #1s)).
* RLE Decompressor: inner per-bit expansion loop replaced by partial-byte fill + bulk emission of full 0x00/0xFF bytes for long runs (work per run of length L goes from O(L) shifts to O(1 + L/8)).
* Overall decision: both C codecs are extremely fast; the Python layer only adds one analysis + header wrap.
* The system stays O(N) with significantly better practical constants on the data the project was designed for.

See the sub-tasks and verification steps in the implementation plan for before/after measurements.

## 4. Architecture & Backward Compatibility
* Python is the front (decision, stats, CLI, bytes support, verification dispatch).
* All heavy lifting for both algorithms stays in C (matching the original project philosophy).
* Existing `compressor.c` / `decompressor.c` logic is untouched except for optional argv paths and the decomp batching optimization.
* Running the C binaries directly (with or without args) continues to work for the RLE path.
* The automated pipeline (`python main.py` with no arguments) still generates, compresses, round-trips, and verifies losslessly — it simply may now pick Huffman on some generations.

## 5. Resolving "Negative Compression" (historical)
Early versions on pure 50/50 data produced negative compression because of RLE count overhead. The 95/5 generator + (now) the ability to fall back to Huffman gives the user the better of the two simple algorithms for any presented binary data.