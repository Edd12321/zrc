#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static inline void
usage()
{
	fprintf(stderr, "usage: unlink <file>\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	if (argc != 2)
		usage();
	if (unlink(argv[1]) < 0) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
