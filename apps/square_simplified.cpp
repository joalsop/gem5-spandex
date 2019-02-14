/*
Copyright (c) 2015-2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include "hip/hip_runtime.h"
#include "gem5/denovo_region_api.h"

__global__ void
vector_square(hipLaunchParm lp, float *C, const float *A, size_t N) {
    size_t offset = (hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x);
    size_t stride = hipBlockDim_x * hipGridDim_x ;
 
   for (size_t i = offset; i < N; i += stride) {
        C[i] = A[i] * A[i];
    }
}


int main(int argc, char *argv[]) {
	mem_sim_on_this();

    const size_t N = 1;
    const unsigned blocks = 1;
    const unsigned threadsPerBlock = 1;

    float* A = new float[N];
    float* C = new float[N];

    // Fill with Phi + i
    for (size_t i = 0; i < N; i++) {
        A[i] = 1.618f + i;
    }

    printf("info: launching kenrel\n");
    hipLaunchKernel(vector_square, dim3(blocks), dim3(threadsPerBlock), 0, 0, C, A, N);
    printf("info: finished kernel\n");

    for (size_t i=0; i<N; i++)  {
        if (C[i] != (1.618f + i) * (1.618f + i)) {
            printf("info: wrong\n");
        }
    }

	mem_sim_off_this();
    return 0;
}

