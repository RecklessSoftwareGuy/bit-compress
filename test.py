import random
import os
import time

def create_binary_test() -> bytearray:
    length = random.randint(1000000, 3000000)
    rem = length % 8
    length = length - rem
    print(f"Target number of bits: {length}")
    
    byte_arr = bytearray()
    
    current_byte = 0
    bit_count = 0
    
    for _ in range(length):
        # THE WEIGHTS: 95% chance of generating a 0, 5% chance of a 1
        new_bit = random.choices([0, 1], weights=[95, 5])[0]
        current_byte = (current_byte << 1) | new_bit
        bit_count += 1
        if bit_count == 8:
            byte_arr.append(current_byte)
            current_byte = 0
            bit_count = 0
            
    print(f"Total file size will be: {len(byte_arr)} bytes")
    return byte_arr

os.makedirs('assets', exist_ok=True)

print("Starting binary generation...")
start_time = time.time()

with open('assets/test.bin', 'wb') as f:
    b_data = create_binary_test()
    f.write(b_data)

end_time = time.time()

print("Binary file created successfully at assets/test.bin")
print(f"Python Execution Time: {end_time - start_time:.4f} seconds")