#include <stdio.h>
#include "memory.h"

int main()
{
	int *array;
	mem_init();

	fprintf(stdout, " ------------------------ Original main ---------------- \n");
	array = mem_alloc(sizeof(int) * 10);
	if (array == NULL)
		perror("mem_alloc()"), exit(-1);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	for (int i = 0; i < 10; i++)
		array[i] = i;

	array = mem_realloc(array, sizeof(int) * 20);
	if (array == NULL)
		perror("mem_realloc()"), exit(-1);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	for (int i = 10; i < 20; i++)
		array[i] = 20 - i;

	for (int i = 20; i-- > 0;)
		printf("%i\n", array[i]);

	mem_free(array);
	mem_dump(stdout);
	fprintf(stdout, "\n");

	// The example from Wikipedia
	fprintf(stdout, " ------------------------ Wikipedia ---------------- \n");
	int *A;
	int *B;
	int *C;
	int *D;
	A = mem_alloc(sizeof(int) * 10);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	B = mem_alloc(sizeof(int) * 20);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	C = mem_alloc(sizeof(int) * 10);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	D = mem_alloc(sizeof(int) * 20);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	mem_free(B);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	mem_free(D);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	mem_free(A);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	mem_free(C);
	mem_dump(stdout);
	fprintf(stdout, "\n");

	fprintf(stdout, " ------------------------ Another example ---------------- \n");
	A = mem_alloc(sizeof(int) * 10);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	B = mem_alloc(sizeof(int) * 20);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	C = mem_alloc(sizeof(int) * 10);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	D = mem_alloc(sizeof(int) * 20);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	B = mem_realloc(B, sizeof(int) * 2000);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	mem_free(D);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	A = mem_realloc(A, sizeof(int) * 5000);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	mem_free(C);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	mem_free(A);
	mem_dump(stdout);
	fprintf(stdout, "\n");
	mem_free(B);
	mem_dump(stdout);
	fprintf(stdout, "\n");

	// Anoter example
	D = mem_alloc(sizeof(int) * 20000);
	// Requests to much, will fail
	if (D == NULL)
		perror("mem_alloc()"), exit(-1);
}
