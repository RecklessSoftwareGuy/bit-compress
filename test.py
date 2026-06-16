import random
import os
import time

def create_binary_test() -> bytearray:
    length = random.randint(10000000, 30000000)
    rem = length % 8
    length = length - rem
    print(f"Target number of bits: {length}")

    num_bytes = length // 8
    num_ones = int(length * 0.05)
    if num_ones > length:
        num_ones = length

    # Fast path: sample bit positions for the 1s, set them directly in a pre-zeroed bytearray.
    # This replaces the O(N) per-bit Python loop + random.choices with O(N/8 + num_ones) work.
    ones_positions = random.sample(range(length), num_ones)

    byte_arr = bytearray(num_bytes)
    for pos in ones_positions:
        b_idx = pos // 8
        bit_in_b = 7 - (pos % 8)  # MSB-first within each byte (matches compressor bit order)
        byte_arr[b_idx] |= (1 << bit_in_b)

    print(f"Total file size will be: {len(byte_arr)} bytes")
    return byte_arr


def generate_test_file(path="assets/test.bin"):
    """Generate the biased binary test file and write it to *path*.
    Safe to call from other modules (unlike the old top-level side effect).
    """
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    print("Starting binary generation...")
    start_time = time.time()
    b_data = create_binary_test()
    with open(path, "wb") as f:
        f.write(b_data)
    end_time = time.time()
    print(f"Binary file created successfully at {path}")
    print(f"Python Execution Time: {end_time - start_time:.4f} seconds")
    return path


if __name__ == "__main__":
    generate_test_file("assets/test.bin")