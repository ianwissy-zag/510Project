Task 1.

a) In dense MVM, the number of flops is the number of flops for GEMM divided by N, as only one column is multiplied, for a computed value of 2N^2 = 500KFLOPS. 

b) Dense memory for once matrix is just the size of the matrix times the number of bytes per element, or 4N^2 = 1MB.

c) Each non-zero element in the sparse matrix is multiplied and added by one vector element. This results in 2 computations per non-zero element. Since there are (1-s)N^2 nonzero elements in the matrix, there are 2(1-s)N^2 = (1-s) * 500KFLOPS computations.

d) The sparse matrix consists of three tables, two of size (1-s)N^2 and one of size N+1, for a  total of 2(1-s)N^2 + N + 1 elements. Since each element is 4 bytes, the total size is then 4* (2(1-s)N^2 + N + 1) = 2MB * (1-s) + 2KB ~= 2MB * (1-s).

Task 2.

The compute for a dense MVM is 2N^2, and the compute for a sparse matrix is 2(1-s)N^2, so the speedup of sparse over dense is just 2N^2/(2(1-s)N^2) = 1/(1-s). This results in a speedup of 2x when s=.5.

Task 3.

The size of the sparse matrix representation is ~2MB * (1-s). The size of the dense matrix is 1MB. Therefore the break even point is approximately s=.5.

Task 4.

Dense Matrix-Vector Multiplication: 1MB/320GB/s = 3.125 microseconds.
Sparse Matrix-Vector Multiplication: 2MB* (1-.9)/320GB/s = 625 nanoseconds.
Speedup = 3.125 microseconds/625 nanoseconds = 5x speedup. 
