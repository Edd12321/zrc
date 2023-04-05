#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define die(...) do {\
	fprintf(stderr, ##__VA_ARGS__);\
	exit(EXIT_FAILURE);\
} while (0)

static inline void
concat(FILE *fin, char *s)
{
	char   buf[BUFSIZ];
	size_t n;

	while ((n = fread(buf, 1, sizeof buf, fin)) > 0)
		if (write(STDOUT_FILENO, buf, n) != n)
			die("write error copying %s", s);
	if (n < 0)
		die("error reading %s", s);
}

int
main(int argc, char *argv[])
{
	if (argc == 1)
		concat(0, "stdin");
	else for (int i = 1; i < argc; ++i) {
		if (access(argv[i], F_OK) == 0) {
			FILE *fin = fopen(argv[i], "r");
			concat(fin, argv[i]);
			fclose(fin);
		} else {
			perror(argv[i]);
			exit(EXIT_FAILURE);
		}
	}
	return EXIT_SUCCESS;
}
