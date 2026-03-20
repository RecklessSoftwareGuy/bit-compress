import test #contains code that create the random binary file
import subprocess
import filecmp
import sys

def run_pipeline():

    # 1. Run the C Compressor
    print("[1/3] Executing C Compressor...")
    try:
        subprocess.run(["./rle_compressor.out"], check=True)

    except subprocess.CalledProcessError:
        print("❌ ERROR: Compressor crashed during execution.")
        sys.exit(1)
        
    # 2. Run the C Decompressor
    print("\n[2/3] Executing C Decompressor...")
    try:
        subprocess.run(["./rle_decompressor.out"], check=True)
    except subprocess.CalledProcessError:
        print("❌ ERROR: Decompressor crashed during execution.")
        sys.exit(1)
        
    # 3. Verify Lossless Compression
    print("\n[3/3] Verifying Lossless Round-Trip...")
    is_identical = filecmp.cmp('assets/test.bin', 'assets/decompressed.bin', shallow=False)
    
    print("\n========================================")
    if is_identical:
        print("✅ SUCCESS: decompressed.bin is a flawless bit-for-bit match of test.bin!")
        print("The Run-Length Encoding algorithm is 100% lossless.")
    else:
        print("❌ FAILURE: Data corruption detected. The files do not match.")
    print("========================================\n")

if __name__ == "__main__":
    run_pipeline()