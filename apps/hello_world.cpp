#include <cstdio>
#include "gem5/denovo_region_api.h"

int main() {
	mem_sim_on_this();
	// const int max_fib = 100;
	// const int stride = 9999;

	// int* fib = new int[max_fib*stride];

    // denovo_region_set({
	// 	(void*)fib,
	// 	max_fib*stride*sizeof(int),
	// 	MEMPOLICY_SHARED_LINE_GRANULARITY_OWNED,
	// 	MEMPOLICY_LINE_GRANULARITY_OWNED,
	// 	MEMPOLICY_DEFAULT,
	// 	ALL_CONTEXTS,
	// });
	// printf("0x%p -- 0x%p\n", fib, fib + max_fib*stride);

	// fib[0*stride] = 0;
	// fib[1*stride] = 1;
	// for (int i = 2; i < max_fib; ++i) {
	// 	fib[i*stride] = fib[(i - 1)*stride] + fib[(i - 2)*stride];
	// }
	// printf("fib[20] = %d, should be 6765\n", fib[20*stride]);

	printf("Hello world\n");
	mem_sim_off_this();
	return 0;
}
