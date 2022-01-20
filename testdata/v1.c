#include <stdio.h>
#include <stdlib.h>

void test(int v)
{
	printf("    decimal = %d\n", v);
	printf("hexadecimal = %08x\n", v);
	printf("       bool = %s\n", v ? "true" : "false");
}

int main(int argc, char* argv[])
{
	const char *input = "255";
	if (argc > 1)
		input = argv[1];

	printf("input: %s\n\n", input);

	test(atoi(input));

	return 0;
}
