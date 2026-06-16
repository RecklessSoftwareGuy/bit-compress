import argparse
import os
import subprocess
import sys
import tempfile
import time
import struct
import filecmp

import test  # provides create_binary_test + generate_test_file (no longer auto-runs on import)

BCMP_MAGIC = b"BCMP"
HEADER_SIZE = 4 + 1 + 8  # magic + algo_u8 + orig_size_u64 (big-endian)

ALGO_RLE = 0
ALGO_HUFF = 1
ALGO_NAMES = {ALGO_RLE: "RLE", ALGO_HUFF: "HUFFMAN"}


def _compile_if_needed(binary_name: str, c_file: str):
    """Compile the C source to the .out binary if the binary is missing.
    Returns True if the binary is now available.
    """
    if os.path.exists(binary_name):
        return True
    if not os.path.exists(c_file):
        print(f"ERROR: {c_file} not found for compilation of {binary_name}")
        return False
    print(f"Compiling {c_file} -> {binary_name} ...")
    try:
        subprocess.run(["gcc", "-Wall", "-o", binary_name, c_file], check=True)
        return True
    except subprocess.CalledProcessError as e:
        print(f"ERROR: gcc failed for {c_file}: {e}")
        return False


def _ensure_codec(algo: int):
    """Make sure the required compressor/decompressor .out for the algo exist."""
    if algo == ALGO_RLE:
        ok1 = _compile_if_needed("./compressor.out", "compressor.c")
        ok2 = _compile_if_needed("./decompressor.out", "decompressor.c")
        return ok1 and ok2
    else:
        ok1 = _compile_if_needed("./huffman_compressor.out", "huffman_compressor.c")
        ok2 = _compile_if_needed("./huffman_decompressor.out", "huffman_decompressor.c")
        return ok1 and ok2


def _wrap_bcmp(algo: int, orig_size: int, raw_payload: bytes) -> bytes:
    """Return the final deliverable file with our thin self-describing header."""
    header = struct.pack(">4s B Q", BCMP_MAGIC, algo, orig_size)
    return header + raw_payload


def _unwrap_bcmp(blob: bytes):
    """Return (algo, orig_size, raw_payload)."""
    if len(blob) < HEADER_SIZE or not blob.startswith(BCMP_MAGIC):
        raise ValueError("Not a valid BCMP compressed file")
    _, algo, orig = struct.unpack(">4s B Q", blob[:HEADER_SIZE])
    return algo, orig, blob[HEADER_SIZE:]


def _run_codec(algo: int, input_path: str, output_path: str) -> bool:
    """Invoke the correct C binary for the algo with the given paths."""
    if not _ensure_codec(algo):
        return False
    if algo == ALGO_RLE:
        comp = ["./compressor.out", input_path, output_path]
    else:
        comp = ["./huffman_compressor.out", input_path, output_path]
    try:
        subprocess.run(comp, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return True
    except subprocess.CalledProcessError as e:
        print(f"ERROR: {ALGO_NAMES[algo]} compressor crashed: {e}")
        return False


def _run_decompress(algo: int, input_raw_path: str, output_path: str) -> bool:
    """Invoke the correct decompressor for a *raw* (no BCMP header) payload."""
    if not _ensure_codec(algo):
        return False
    if algo == ALGO_RLE:
        dec = ["./decompressor.out", input_raw_path, output_path]
    else:
        dec = ["./huffman_decompressor.out", input_raw_path, output_path]
    try:
        subprocess.run(dec, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return True
    except subprocess.CalledProcessError as e:
        print(f"ERROR: {ALGO_NAMES[algo]} decompressor crashed: {e}")
        return False


def analyze_and_choose(input_path: str) -> str:
    """Return 'rle' or 'huffman' by actually producing both (fast) and picking the smaller raw output.
    This guarantees the stats the user sees are exactly for the delivered file.
    """
    # We use temp files for the raw codec outputs
    with tempfile.NamedTemporaryFile(delete=False) as tr:
        rle_raw = tr.name
    with tempfile.NamedTemporaryFile(delete=False) as th:
        huff_raw = th.name

    try:
        # RLE
        if not _run_codec(ALGO_RLE, input_path, rle_raw):
            rle_size = 10**18
        else:
            rle_size = os.path.getsize(rle_raw)

        # Huffman
        if not _run_codec(ALGO_HUFF, input_path, huff_raw):
            huff_size = 10**18
        else:
            huff_size = os.path.getsize(huff_raw)

        if rle_size <= huff_size:
            return "rle", rle_raw, huff_raw   # winner raw path, loser path (for cleanup)
        else:
            return "huffman", huff_raw, rle_raw
    finally:
        # caller is responsible for cleanup of the two temps
        pass


def compress(input_data_or_path, output_path=None) -> dict:
    """High-level Python front-end API.

    Accepts either a filesystem path (str) or raw bytes ("binary string").
    Returns a dict with:
        'algo', 'initial', 'final', 'ratio', 'output_path', 'time'
    The written file (if output_path given or default) contains the BCMP header + chosen raw payload.
    """
    t0 = time.time()

    # Normalize input to a real on-disk file we can hand to the C programs
    cleanup_input = False
    if isinstance(input_data_or_path, (bytes, bytearray)):
        data = bytes(input_data_or_path)
        initial = len(data)
        with tempfile.NamedTemporaryFile(delete=False) as tf:
            tf.write(data)
            input_path = tf.name
        cleanup_input = True
    else:
        input_path = input_data_or_path
        initial = os.path.getsize(input_path)

    if output_path is None:
        # Default location for ad-hoc use
        output_path = "assets/compressed.bin" if not input_path.startswith("/tmp") else input_path + ".compressed"

    # Run both codecs (very fast), pick the real winner by compressed payload size
    choice, winner_raw, loser_raw = analyze_and_choose(input_path)
    algo = ALGO_RLE if choice == "rle" else ALGO_HUFF

    # Read the raw payload of the winner
    with open(winner_raw, "rb") as f:
        raw_payload = f.read()

    # Wrap with our header and write final deliverable
    final_blob = _wrap_bcmp(algo, initial, raw_payload)
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "wb") as f:
        f.write(final_blob)

    final_size = len(final_blob)
    ratio = (1.0 - final_size / initial) * 100.0 if initial else 0.0
    elapsed = time.time() - t0

    # Cleanup temps
    for p in (winner_raw, loser_raw):
        try:
            os.unlink(p)
        except OSError:
            pass
    if cleanup_input:
        try:
            os.unlink(input_path)
        except OSError:
            pass

    stats = {
        "algo": choice.upper(),
        "initial": initial,
        "final": final_size,
        "ratio": ratio,
        "output_path": output_path,
        "time": elapsed,
    }

    # Pretty print for CLI use
    print("\n========================================")
    print(f"Initial file size:  {initial} bytes")
    print(f"Final file size:    {final_size} bytes")
    print(f"Compression ratio:  {ratio:.2f}% ({stats['algo']})")
    print(f"Time taken:         {elapsed:.4f} s")
    print(f"Output written to:  {output_path}")
    print("========================================")

    return stats


def run_pipeline(input_path=None, output_path=None):
    """The automated integration pipeline (used by default when no args or by legacy main.py usage).
    Generates (if needed), chooses best algo, produces headered output, verifies round-trip lossless,
    and prints the required statistics.
    """
    if input_path is None:
        # Legacy / demo path: (re)generate the clustered test data
        input_path = test.generate_test_file("assets/test.bin")

    if output_path is None:
        output_path = "assets/compressed.bin"

    print("[1/3] Choosing algorithm and compressing (running both codecs, picking winner)...")
    stats = compress(input_path, output_path)

    print("\n[2/3] Verifying lossless round-trip for chosen algorithm...")
    # Extract the raw payload from the headered file we just wrote and feed the correct decomp
    with open(output_path, "rb") as f:
        blob = f.read()
    algo, orig_from_header, raw_payload = _unwrap_bcmp(blob)

    # Write raw payload to a temp for the C decompressor
    with tempfile.NamedTemporaryFile(delete=False) as tr:
        tr.write(raw_payload)
        raw_for_decomp = tr.name

    decomp_out = tempfile.NamedTemporaryFile(delete=False).name

    ok = _run_decompress(algo, raw_for_decomp, decomp_out)

    verified = False
    if ok:
        if algo == ALGO_RLE:
            verified = filecmp.cmp(input_path, decomp_out, shallow=False)
        else:
            # For Huffman compare bytes (or sizes + filecmp)
            with open(input_path, "rb") as a, open(decomp_out, "rb") as b:
                verified = a.read() == b.read()

    print("\n[3/3] Verification result + final statistics")
    print("========================================")
    if verified:
        print("✅ SUCCESS: Round-trip is bit-for-bit identical!")
        print(f"   Algorithm used: {ALGO_NAMES[algo]}")
    else:
        print("❌ FAILURE: Data corruption detected after decompression.")
    print(f"Initial file size:  {stats['initial']} bytes")
    print(f"Final file size:    {stats['final']} bytes")
    print(f"Compression ratio:  {stats['ratio']:.2f}% ({stats['algo']})")
    print("========================================")

    # Cleanup
    for p in (raw_for_decomp, decomp_out):
        try:
            os.unlink(p)
        except OSError:
            pass

    if not verified:
        sys.exit(1)


def main():
    ap = argparse.ArgumentParser(
        description="bit-compress: automatically pick RLE or Huffman for a binary file/string and report stats."
    )
    ap.add_argument("input", nargs="?", help="Input file to compress (omit to use the built-in generator + full verified pipeline)")
    ap.add_argument("output", nargs="?", help="Optional output path for the compressed result (default: assets/compressed.bin or input-based)")
    args = ap.parse_args()

    if args.input is None:
        # Full legacy/demo pipeline with generation + verify
        run_pipeline()
    else:
        # One-shot compress of user-provided file (or will also accept bytes via the compress() API)
        compress(args.input, args.output)


if __name__ == "__main__":
    main()