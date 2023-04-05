#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static inline void
usage()
{
	fprintf(stderr, "sponge <file>\n");
	exit(EXIT_FAILURE);
}

void
passthru(FILE *f1, FILE *f2)
{
	char   buf[BUFSIZ];
	size_t len;
	while ((len = fread(buf, 1, sizeof buf, f1)) > 0) {
		if (fwrite(buf, 1, len, f2) != len) {
			perror("fwrite");
			exit(EXIT_FAILURE);
		}
		if (feof(f1))
			break;
	}
}

int
main(int argc, char *argv[])
{
	char buf[BUFSIZ], tmpf[PATH_MAX];
	FILE *fp, *fn;
	size_t len;

	if (argc != 2)
		usage();
	tmpnam(tmpf);
	fp = fopen(tmpf, "w");
	passthru(stdin, fp);
	fclose(fp);
	fclose(stdin);
	fp = fopen(tmpf,    "r");
	fn = fopen(argv[1], "w");
	passthru(fp, fn);
	unlink(tmpf);
	return EXIT_SUCCESS;
}
