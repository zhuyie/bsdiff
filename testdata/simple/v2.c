#include <stdio.h>
#include <stdlib.h>

void test(int v)
{
	printf("    decimal = %d\n", v);
	printf("      octal = %o\n", v);
	printf("    is_even = %s\n", (v % 2 == 0) ? "true" : "false");
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
