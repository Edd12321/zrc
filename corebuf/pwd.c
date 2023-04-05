#include <sys/stat.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline void
usage()
{
	fprintf(stderr, "usage: pwd [-L|-P]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	char mode = 'L', buf[PATH_MAX];
	const char *ptr;
	struct stat s1, s2;

	if (argc > 2)
		usage();
	if (argc == 2) {
		if (strcmp(argv[1], "-L") && strcmp(argv[1], "-P"))
			usage();
		mode = argv[1][1];
	}
	if (!getcwd(buf, sizeof buf))
		goto _err;
	
	switch (mode) {
	case 'P':
		puts(buf);
		break;
	case 'L':
		ptr = getenv("PWD");
		stat(ptr, &s1);
		stat(buf, &s2);
		if (ptr && s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino)
			puts(ptr);
		else
			puts(buf);
	}

_suc:
	return EXIT_SUCCESS;
_err:
	perror(".");
	return EXIT_FAILURE;
}
