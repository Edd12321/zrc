#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static inline void
usage()
{
	fprintf(stderr, "usage: nice [-n<val>] <cmd> [<args...>]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int old, adjust;
	int opt;

	old = getpriority(PRIO_PROCESS, 0);
	if (argc == 1) {
		printf("%d\n", old);
		exit(EXIT_SUCCESS);
	}

	adjust = 10;
	if (*argv[1] == '-' && argv[1][1] == 'n') {
		if (argc < 3)
			usage();
		adjust = atoi(argv[1]+1);
		++argv;
	}
	if (setpriority(PRIO_PROCESS, 0, old+adjust) < 0) {
		perror("prio");
		exit(EXIT_FAILURE);
	}
	++argv;
	return execvp(*argv, argv) < 0;
}
