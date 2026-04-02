| Rank |   Layer Name  |  Total MACs | Total Parameters |
|:----:|:-------------:|:-----------:|:----------------:|
| 1    | Conv2d: 1-1   | 118,013,952 | 9,408            |
| 2    | Conv2d: 3-42  | 115,605,504 | 2,359,296        |
| 3    | Conv2d: 3-46  | 115,605,504 | 2,359,296        |
| 4    | Conv2d: 3-49  | 115,605,504 | 2,359,296        |
| 5    | Conv2d: 3-29  | 115,605,504 | 589,824          |

**Note:** There are many ties for second in the Total MACs (12). 
I am ranking these results by Total Parameters. There were also
several ties for 5th place. Layers `3-33` and `3-36` have the same
number of parameters and MACs as layer `3-29`.

The input and output shapes for layer `Conv2d: 1-1` are 
`[1, 3, 224, 224]` and `[1, 64, 112, 112]`. Therefore the total
number of memory elements required for them are `1 * 3 * 224 * 224 = 150,528`
and `1 * 64 * 112 * 112 = 802,816`. Since each element is a 32-bit float, 
the total memory requirement is `4 * (150,528 + 802,816) = 3,813,376 Bytes`.
There are 9,408 parameters in this layer, which require `9,408 * 4 = 37,632 Bytes`,
for a total of `3,851,008 Bytes`.

The calculation for arithmetic intensity is given by `(2 * MACs) / (Memory Bytes)`.
Therefore the arithmetic intensity is `(2 * 118,013,952) / 3,851,008 = 61.29 FLOPs/Byte`.
