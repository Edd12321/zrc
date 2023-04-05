#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

static inline void
usage()
{
	fprintf(stderr, "usage: dirname <dir>\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	if (argc < 2) usage();
	puts(dirname(argv[1]));
}
