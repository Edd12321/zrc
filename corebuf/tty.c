#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	bool silent = false;
	char *tty;

	if (argc > 1 && !strcmp(argv[1], "-s"))
		silent = true;
	tty = ttyname(STDIN_FILENO);
	if (!silent)
		puts(tty ? tty : "not a tty");
	return !tty;
}
