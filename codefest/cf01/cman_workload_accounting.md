Tasks:
1. Formula: NumMACs(layer) = Nodes(layer) * Nodes(layer-1)
   Results:
   Layer 1: Nodes(layer1) = 256, Nodes(layer0) = 784, so NumMACs(layer1) = 256 * 784 = 200,704
   Layer 2: Nodes(layer2) = 128, Nodes(layer1) = 256, so NumMACs(layer2) = 128 * 256 = 32,768
   Layer 3: Nodes(layer3) = 10,  Nodes(layer2) = 128, so NumMACs(layer3) = 10  * 128 = 1280
   
2. Total MACs = 200,704 + 32,768 + 1280 = 234,752.

3. The total number of trainable parameters is the same as the number of MACs, as each connection between two
layers can be parameterized, and also each connection requires a MAC. Therefore no additional calculation is required.
The total number of trainable parameters is simply 234,752.

4. Each weight is 4-bytes, so the total memory requried for the weights is just 4 times the number of parameters, which is
4 bytes/weight * 234,752 weights = 939,008 bytes.

5. Each node requires 4-bytes. I am assuming that output nodes also require data storage, so they are included in the calculation.
There are 784 + 256 + 128 + 10 = 1178 nodes, so the output storage requires 4 bytes/node * 1178 nodes = 4712 bytes. 

6. The arithmetic intensity is given by (2 * total MACs)/(weight bytes + activation bytes). Plugging in the values from the previous 
sections results in: AI = (2 * 234,752)/(939,008 + 1178) = .4975. 
