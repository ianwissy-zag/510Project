import sys
import re

def calculate_average(file_path):
    times = []
    # Regex looks for the literal string "took ", followed by numbers/decimals, followed by " ms"
    pattern = re.compile(r"took\s+([\d.]+)\s+ms")
    
    try:
        with open(file_path, 'r') as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    # Extract the captured time value and convert to float
                    times.append(float(match.group(1)))
                    
        if not times:
            print("No timing data found in the file.")
            return

        avg_time = sum(times) / len(times)
        
        print(f"Parsed {len(times)} steps.")
        print(f"Total time:    {sum(times):.4f} ms")
        print(f"Average time:  {avg_time:.4f} ms per step")
        
    except FileNotFoundError:
        print(f"Error: The file '{file_path}' was not found.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python avg_timing.py <path_to_log_file>")
    else:
        calculate_average(sys.argv[1])
