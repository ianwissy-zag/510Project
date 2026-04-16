import torch
import sys
import torch.nn as nn

# 1. Define the Network Architecture
class SimpleNet(nn.Module):
    def __init__(self):
        super(SimpleNet, self).__init__()
        # Hidden layer: 4 inputs -> 5 hidden neurons
        self.hidden = nn.Linear(4, 5)
        # Activation function
        self.relu = nn.ReLU()
        # Output layer: 5 hidden neurons -> 1 linear output
        self.output = nn.Linear(5, 1)

    def forward(self, x):
        x = self.hidden(x)
        x = self.relu(x)
        x = self.output(x)
        return x

# 2. Set up the GPU Device
# It's best practice to check for CUDA, falling back to CPU if necessary
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
print(f"Target device: {device}\n")
if not torch.cuda.is_available():
    sys.exit(0)

# 3. Instantiate the network and move it to the GPU
model = SimpleNet().to(device)

# 4. Generate a random input batch and move it to the GPU
# Shape: (Batch Size, Number of Inputs) -> (16, 4)
batch_size = 16
num_inputs = 4
x_input = torch.randn(batch_size, num_inputs).to(device)

# 5. Run the forward pass
# We use torch.no_grad() because we are not training (saves memory/compute)
with torch.no_grad():
    predictions = model(x_input)

# 6. Verify the shapes
print(f"Input tensor shape:  {x_input.shape}")   # Expected: torch.Size([16, 4])
print(f"Output tensor shape: {predictions.shape}") # Expected: torch.Size([16, 1])

# Optional: Verify the tensors are actually on the GPU
print(f"\nInput is on GPU:  {x_input.is_cuda}")
print(f"Output is on GPU: {predictions.is_cuda}")
