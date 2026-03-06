import random
import os

def create_binary_test() -> bytearray:
    length = random.randint(10000, 30000)
    rem = length % 8
    length = length - rem
    print(f"Length of the binary string: {length}")
    s = ""
    while length > 0:
        s += f"{random.choice([0, 1])}"
        length -= 1
    print(f"Total file size will be: {len(s) // 8} bytes")
    byte_array = bytearray()
    for i in range(0, len(s), 8):
        byte_chunk = s[i:i+8]
        byte_val = int(byte_chunk, 2) 
        byte_array.append(byte_val)
    return byte_array

with open('assets/test.bin', 'wb') as f:
    b_data = create_binary_test()
    f.write(b_data)
    print("Binary file created successfully at assets/test.bin")