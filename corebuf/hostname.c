#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline void
die(const char *errmsg)
{
	fprintf(stderr, "%s\n", errmsg);
	exit(EXIT_FAILURE);
}

static inline void usage() { die("usage: hostname [<name>]"); }

int
main(int argc, char *argv[])
{
	char name[HOST_NAME_MAX];
	if (argc > 2)
		usage();
	if (argc == 1) {
		if (gethostname(name, sizeof name) < 0)
			die("Could not get hostname");
		puts(name);
	} else if (sethostname(argv[1], strlen(argv[1])) < 0)
			die("Could not set hostname");
	return EXIT_SUCCESS;
}
