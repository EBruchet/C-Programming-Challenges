//#define EXIT_SUCCESS 0

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int main()
{
	int i;
	long sz = 100000;
	mlockall( MCL_FUTURE | MCL_ONFAULT );
	// Lock virtual memory from being swapped to disk
	for (i = 0; i < 10000; i++)
	{
		printf("Allocating block %d \n", i);
		fflush(stdout);
		char *p = malloc(sz);
	if (p == NULL) {
		printf("Allocation error.. \n");
		fflush(stdout);
	} else {
		printf("Success.. \n");
		fflush(stdout);
	}
		memset(p, 0, sz);
		// malloc'ed memory must be used
		usleep(10000);
	}
	return 0;
}
