#define RUNCMD {                    \
    int rv = execvp(argv[0], argv); \
    if (rv < 0) {                   \
        perror(argv[0]);            \
        exit(127);                  \
    } else {                        \
        exit(rv);                   \
    }                               \
}

/** Converts an array into a space-separated string.
 * 
 * @param {int}c,{char**}v,{int}i
 * @return void
 */
template<typename T> static std::string
combine(int c, T v, int i)
{
    std::string buf = "";
    for (int k = i; k < c; ++k) {
        buf += v[k];
        if (k < c-1)
            buf += ' ';
    }
    return buf;
}

/** Executes a list of words.
 *
 * @param {int}argc,{char**}argv
 * @return void
 */

extern void
exec(int argc, char *argv[])
{
	char         *ret_buf[BUFSIZ];
	pid_t         pid;
	int           cs;
	size_t        i, siz;

	w = (bg_or_fg.front() == FG);
	ret_val = "0";

	// Nothing happens...
	if (!argc) return;
	if (!builtin_check(argc, argv)) {
		switch(w) {
		/**********************
		 * Foreground process *
		 **********************/
		case true:
			if (FOUND_FN(0)) {
				// Copy original argc
				std::string oa = getvar("argc");

				// Copy original argv
				Array bak = a_hm["argv"];
				a_hm.erase("argv");

				INIT_ZRC_ARGS;
				eval(funcs[argv[0]]);
				setvar("argc", oa);
				a_hm["argv"] = bak;
			
			} else {
				if ((pid = fork()) < 0)
					die("fork");
				
				if (pid == 0) {
					RUNCMD;
				
				} else {
					Jid index;
					if (make_new_jobs)
						index = addjob(pid, FG, argc, argv);
					waitpid(pid, &cs, 0);
					ret_val = itoa(WEXITSTATUS(cs));
					setvar("!", itoa(pid));
					if (make_new_jobs)
						deljob(index);
				}
			}
			break;

		/**********************
		 * Background process *
		 **********************/
		case false:
			if ((pid = fork()) < 0)
				die("fork");

			if (pid == 0) {
				if (FOUND_FN(0)) {
					a_hm.erase("argv");
					INIT_ZRC_ARGS;
					eval(funcs[argv[0]]);
				} else {
					RUNCMD;
				}
				exit(0);
			
			} else {
				if (make_new_jobs) {
					Jid index = addjob(pid, BG, argc, argv);
					// Only show in interactive mode
					if (TERMINAL)
						std::cerr << FMT << '\n';
				}
				setvar("!", itoa(pid));
			}
			break;
		}
	}
	// Reset all FDs
	dup2(o_in, STDIN_FILENO);
	dup2(o_out, STDOUT_FILENO);
	for (i = 0, siz = baks.size(); i < siz; ++i)
		dup2(baks[i].first, baks[i].second);
	baks.clear();

	setvar("?", ret_val);
	
	cleanup_memory();
	for (i = 0; i < argc; ++i)
		free(argv[i]);
	make_new_jobs = false;
}

/** Captures a program fragment's standard output.
 *
 * @param {string} frag
 * @return string
 */
std::string
io_cap(std::string frag)
{
	int pd[2];
	pid_t pid;
	char c;
	std::string res;

	pipe(pd);
	pid = fork();
	if (pid == 0) {
		dup2(pd[1], STDOUT_FILENO);
		o_out = dup(STDOUT_FILENO);
		close(pd[0]);
		close(pd[1]);
		eval(frag);
		exit(0);
	
	} else {
		close(pd[1]);
		do {
			int b = read(pd[0], &c, 1);
			if (b != 1)	
				break;
			res += c;
		} while (1);
		close(pd[0]);
	}
	do {} while (wait(NULL) != -1);
	return res;
}

/** Redirects standard input.
 *
 * @param {string}fn
 * @return void
 */
void
io_left(std::string fn)
{
	str_subst(fn);
	int fd = open(fn.data(), O_RDONLY);
	dup2(fd, 0);
	close(fd);
}

/** Redirects Fd #n (can also append to a file).
 *
 * @param {string}fn,{bool}app,{int}n
 * @return void
 */
void
io_right(std::string fn, bool app, int n)
{
	int fd;

	str_subst(fn);
	if (app)
		fd = open(fn.data(), O_CREAT|O_APPEND|O_WRONLY, 0644);
	else
		fd = open(fn.data(), O_WRONLY|O_TRUNC|O_CREAT,  0600);
	dup2(fd, n);
	close(fd);
}

/** Converts stdout to stdin.
 * 
 * @param {int}argc,{char**}argv
 * @return void
 */
void
io_pipe(int argc, char *argv[])
{
	int pd[2];
	pipe(pd);
	dup2(pd[1], 1);
	close(pd[1]);

	// Run
	exec(argc, argv);
	dup2(pd[0], 0);
	close(pd[0]);
}


