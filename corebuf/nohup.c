#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#define or ||
#define GetFile(X) open(X, O_APPEND|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR)

static inline bool
die(const char *errstr, int errcode)
{
	fprintf(stderr, "%s\n", errstr);
	exit(errcode);
	return 0;//dummy return
}

int
main(int argc, char *argv[])
{
	const char *fname = "nohup.out";
	const char *usage = "usage: nohup <cmd [<args...>]";
	char dest[PATH_MAX];
	char *home;
	int fd;

	argv++;
	argc > 1
		or die(usage, EXIT_FAILURE);
	signal(SIGHUP, SIG_IGN) != SIG_ERR
		or die("signal(2)", 126);
	if (isatty(STDOUT_FILENO)) {
		if ((fd = GetFile(fname)) == -1) {
			home = getenv("HOME");
			if (home)
				snprintf(dest, sizeof(dest), "%s/%s", home, fname);
			else
				strcpy(dest, fname);
			if ((fd = GetFile(fname)) == -1)
				return 126;
			dup2(fd, STDOUT_FILENO);
		}
	}
	if (isatty(STDERR_FILENO))
		dup2(STDOUT_FILENO, STDERR_FILENO);
	execvp(*argv, argv);
	exit(126+(errno == ENOENT));
}
