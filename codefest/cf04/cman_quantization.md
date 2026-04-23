a)
Original:
0.85	-1.2	0.34	2.1
-0.07	0.91	-1.88	0.12
1.55	0.03	-0.44	-2.31
-0.18	1.03	0.77	0.55

Absolute Value:
0.85	1.2	    0.34	2.1
0.07	0.91	1.88	0.12
1.55	0.03	0.44	2.31
0.18	1.03	0.77	0.55

Max: 2.31

Scale Factor = 2.31 / 127 = 0.01818897638

Quantized:
47	-66	19	    115
-4	50	-103	7
85	2	-24	    -127
-10	57	42	    30

Dequantized:
0.85	-1.2	0.35	2.09
-0.07	0.91	-1.87	0.13
1.55	0.04	-0.44	-2.31
-0.18	1.04	0.76	0.55

Errors:
0	0	    0.01	0.01
0	0	    0.01	0.01
0	0.01	0	    0
0	0.01	0.01	0

Average Error = 0.004375

Bad Scale = .01

Quantized:
85	-120	34	    127
-7	91	    -128	12
127	3	    -44	    -128
-18	103	    77	    55

Dequantized:
0.85	-1.2	0.34	1.27
-0.07	0.91	-1.28	0.12
1.27	0.03	-0.44	-1.28
-0.18	1.03	0.77	0.55

Error:
0	    0	0	    0.83
0	    0	0.6	    0
0.28	0	0	    1.03
0	    0	0	    0

Average Error = 0.17125

When S is too small, values are that are too large (in absolute value) end up outside the range of [-128,127] and therefore are truncated. This results in significant error upon dequantization, as these values end up considerably smaller than they were previously. 

