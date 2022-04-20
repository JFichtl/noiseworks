# Real Time COnvolution

We use the UPOLS algorithm. This acronym stands for Uniformly Partitioned Overlap-Save. The key to making this work is partitioning the impulse response in little uniform chunks and applying the convolution theorem to each to obtain a blazing fast near real-time performance. Because of the way this algorithm is implemented, we'll always have a latency equal to two times the size of the chunks. For example, a 128 sample buffer will induce a 256 sample delay (around 5 mili-seconds for a 48khz sampling rate). [William Gardner](https://cse.hkust.edu.hk/mjg_lib/bibs/DPSu/DPSu.Files/Ga95.PDF) proposed a solution to this problem. This solution, however, is of limited use in systems where the buffer size can't be made smaller than some set number.

We can resume the algorithm in this way:
$$ X_i Y_i$$
