#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void
usage()
{
	fprintf(stderr, "usage: basename <path> [<suffix>]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	char *str, *ptr;
	if (argc > 3)
		usage();
	str = basename(argv[1]);
	if (argc == 3 && (ptr = strstr(str, argv[2]))) {
		if (!strcmp(ptr, argv[2]))
			*ptr = '\0';
	}
	puts(str);
}
