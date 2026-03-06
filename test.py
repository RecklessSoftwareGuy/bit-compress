#Program creates test files
import random

def create_test() -> str:
    digits = [0, 1]
    length = random.randint(10000, 30000)
    print(f"Length of file: {length}")
    s = ""
    while length > 0:
        s += f"{random.choice(digits)}"
        length -=1
    return s

with open('assets/test.txt', 'w') as f:
    s = create_test()
    f.write(s)
    print("File created sucessfully")