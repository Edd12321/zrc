#include <stdio.h>
#define ever (;;)

int
main(int argc, char *argv[])
{
	if (argc < 2) for ever puts("y");
	else for ever {
		for (char **ptr = argv+1; *ptr; ++ptr)
			printf("%s ", *ptr);
		putchar('\n');
	}
}
