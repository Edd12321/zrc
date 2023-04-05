#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static inline void
usage()
{
	fprintf(stderr, "usage: chroot <dir> <cmd> [<args...>]\n");
	exit(EXIT_FAILURE);
}

static inline void
exception()
{
	fprintf(stderr, "%s\n", strerror(errno));
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	char *shell[] = { "/bin/sh", "-i", NULL };

	if (argc == 1)
		usage();

	if (chroot(argv[1]) || chdir("/"))
		exception();

	if (argc == 2)
		execvp(*shell, shell);
	else
		execvp(argv[2], argv+2);
	exception();
}
