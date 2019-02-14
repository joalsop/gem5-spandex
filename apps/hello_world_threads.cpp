#include <pthread.h>
#include <stdio.h>
#include <gem5/denovo_region_api.h>

#define NUM_THREADS     3

int counter = 0;
pthread_mutex_t mutex;

void *print_hello(void *threadid) {
	mem_sim_on_this();

	int val = 0;

	pthread_mutex_lock(&mutex);
	val = counter;
	counter += 1;
	pthread_mutex_unlock(&mutex);

	long tid = (long)threadid;

	printf("Thread %ld: Hello World counted %d.\n", tid, val);

	mem_sim_off_this();

	return NULL;
}

int main (int argc, char *argv[]) {
	mem_sim_on_this();

	pthread_t threads[NUM_THREADS];

	mem_sim_off_this();

    for(long t = 0; t < NUM_THREADS; t++) {
        if (pthread_create(&threads[t], NULL, print_hello, (void*)t)) {
            printf("Main: ERROR: pthread_create() failed\n");
			return 1;
        }
    }

	pthread_exit(NULL);
}
