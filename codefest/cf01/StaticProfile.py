import os
import torch
from torchvision.models import resnet18
from torchinfo import summary

# 0. Setup output directory and file path
output_dir = "./profiling"
os.makedirs(output_dir, exist_ok=True)
output_file = os.path.join(output_dir, "resnet18_profile.txt")

# 1. Instantiate the model
model = resnet18().float()
model.eval()

# 2. Define the exact input specifications
batch_size = 1
channels = 3
height = 224
width = 224
input_shape = (batch_size, channels, height, width)

# ==========================================
# PART 1: Static Profiling with torchinfo
# ==========================================
# verbose=0 suppresses console output so we can capture the string
model_stats = summary(
    model, 
    input_size=input_shape,
    dtypes=[torch.float32],
    col_names=["input_size", "output_size", "num_params", "mult_adds"],
    depth=3,
    verbose=0 
)
summary_str = str(model_stats)

# ==========================================
# PART 2: Write out to file
# ==========================================
with open(output_file, "w") as f:
    f.write("--- Architectural Profiling (torchinfo) ---\n")
    f.write(summary_str)

print(f"Profiling complete. Output successfully written to {output_file}")
