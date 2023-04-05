#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#define pf(X) if (X##_flag) printf("%7zu ", c##X)

bool l_flag, w_flag;
char c_flag;

size_t cc, cl, cw;
size_t tc, tl, tw;

static inline void
out(size_t cc, size_t cl, size_t cw, const char *name)
{
	pf(l);
	pf(w);
	pf(c);
	if (name)
		printf("%s", name);
	putchar('\n');
}

static inline size_t
wlen(char ch)
{
	wchar_t tmp[2];
	tmp[0] = ch;
	tmp[1] = '\0';
	return wcslen(tmp);
}

static inline void
wc(FILE *fp, const char *name)
{
	wchar_t ch;
	bool word = false;
	cc = cl = cw = 0;
	while ((ch = fgetc(fp)) != EOF) {
		cc += c_flag ? wlen(ch) : 1;
		if (ch == '\n')
			++cl;
		if (!iswspace(ch))
			word = true;
		else if (word)
			word = 0,
			++cw;
	}
	if (word)
		++cw;
	tc += cc, tl += cl, tw += cw;
	out(cc, cl, cw, name);
}

static inline void
usage()
{
	fprintf(stderr, "usage: wc [-c|-m][-lw][<file>|-...]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int opt, status;
	while ((opt = getopt(argc, argv, "cmlw")) != -1) {
		switch (opt) {
		case 'c': /* FALLTHROUGH */
		case 'm':
			c_flag = opt;
			break;
		case 'l':
			l_flag = true;
			break;
		case 'w':
			w_flag = true;
			break;
		case '?':
		default: usage();
		}
	}
	if (!c_flag && !l_flag && !w_flag)
		c_flag = 'c',
		l_flag = w_flag = true;

	if (optind >= argc)
		wc(stdin, NULL);

	else {
		for (status = EXIT_SUCCESS; optind < argc; ++optind) {
			if (!strcmp(argv[optind], "-")) {
				wc(stdin, NULL);
			} else if (access(argv[optind], F_OK) == 0) {
				FILE *fin = fopen(argv[optind], "r");
				wc(fin, argv[optind]);
			} else {
				perror(argv[optind]);
				status = EXIT_FAILURE;
				continue;
			}
		}
		if (argc > 2)
			out(tc, tl, tw, "total");
	}
	return status;
}
