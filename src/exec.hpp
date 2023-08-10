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
template<typename T> std::string
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

/** Execute a proc/function with a `return` handler.
 *
 * @param {string_view}cmd
 * @return void
 */
static inline void
run_function(std::string const& cmd)
{
	BlockHandler fh(&in_func);
	try {
		eval(funcs[cmd]);
	} catch (ZrcReturnHandler ex)
		{}
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
	sigset_t      mask;
	w = (bg_or_fg.front() == FG);
	ret_val = "0";

#define SIGSET_INITIALIZE \
	sigemptyset(&mask);\
	sigaddset(&mask, SIGCHLD);\
	sigprocmask(SIG_BLOCK, &mask, NULL);

	if (argc > 0) {
		if (w) {
		/**********************
		 * Foreground process *
		 **********************/
			if (FOUND_FN(0)) {
				// Copy original argc
				std::string oa = getvar("argc");

				// Copy original argv
				Array bak = a_hm[$ARGV];
				a_hm.erase($ARGV);
				INIT_ZRC_ARGS;
				run_function(*argv);
				setvar($ARGC, oa);
				a_hm[$ARGV] = bak;
			
			} else if (!builtin_check(argc, argv)) {
				SIGSET_INITIALIZE;
				if ((pid = fork()) < 0)
					die("fork");
				
				if (pid == 0) {
					NO_SIGEXIT;
					setpgid(0, 0);
					tcsetpgrp(STDIN_FILENO, getpgrp());
					sigprocmask(SIG_UNBLOCK, &mask, NULL);
					RUNCMD;
				
				} else {
					if (make_new_jobs)
						addjob(pid, FG, argc, argv);
					sigprocmask(SIG_UNBLOCK, &mask, NULL);
					setvar($LPID, itoa(pid));
					waitproc(pid);
				}
			}

		/**********************
		 * Background process *
		 **********************/
		} else {
			SIGSET_INITIALIZE;
			if ((pid = fork()) < 0)
				die("fork");

			if (pid == 0) {
				NO_SIGEXIT;
				setpgid(0, 0);
				sigprocmask(SIG_UNBLOCK, &mask, NULL);
				if (FOUND_FN(0)) {
					a_hm.erase($ARGV);
					INIT_ZRC_ARGS;
					run_function(*argv);
				} else if (!builtin_check(argc, argv)) {
					RUNCMD;
				}
				exit(0);
			
			} else {
				if (make_new_jobs) {
					Jid index = addjob(pid, BG, argc, argv);
					// Only show in interactive mode
					if (TERMINAL)
						async_message(index, pid, "");
				}
				sigprocmask(SIG_UNBLOCK, &mask, NULL);
				setvar($LPID, itoa(pid));
			}
		}
	}
	
	// Reset/cleanup everything for next command
	dup2(o_in, STDIN_FILENO);
	dup2(o_out, STDOUT_FILENO);
	for (i = 0; i < argc; ++i) {
		free(argv[i]);
		argv[i] = NULL;
	}
	make_new_jobs = false;

	// set fg group ID
	if (tcgetpgrp(STDIN_FILENO) != zrcpid)
		tcsetpgrp(STDIN_FILENO, zrcpid);
	setvar($RETURN, ret_val);
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
		NO_SIGEXIT;
		chk_exit = true;
		dup2(pd[1], STDOUT_FILENO);
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

/** Substitute (and optionally glob) a word for use in redirects.
 * 
 * @param {string&}str
 * @return bool
 */
static bool
str_subst_expect1(std::string& str)
{
	WordList gbzwl;
	std::string str1;

	str1 = str;
	str_subst(str);
	if (str1 == str) {
		gbzwl = glob(str);
		if (gbzwl.size() == 1) {
			str = gbzwl.wl[0];
		} else {
			std::cerr << errmsg << "ambiguous redir\n";
			return 0;
		} 
	}
	return 1;
}

/** Redirects standard input.
 *
 * @param {string}fn
 * @return void
 */
bool
io_left(std::string fn)
{
	if (!str_subst_expect1(fn))
		return 0;
	if (access(fn.c_str(), F_OK)) {
		perror(fn.c_str());
		return 0;
	}
	int fd = open(fn.data(), O_RDONLY);
	dup2(fd, STDIN_FILENO);
	close(fd);
	return 1;
}

/** Redirects Fd #n (can also append to a file).
 *
 * @param {string}fn,{bool}app,{int}n
 * @return void
 */
bool
io_right(std::string fn, bool app, int n)
{
	int fd;
	if (!str_subst_expect1(fn))
		return 0;
	if (app)
		fd = open(fn.data(), O_CREAT|O_APPEND|O_WRONLY, 0644);
	else
		fd = open(fn.data(), O_WRONLY|O_TRUNC|O_CREAT,  0600);
	dup2(fd, n);
	close(fd);
	return 1;
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
	dup2(pd[1], STDOUT_FILENO);
	close(pd[1]);

	// Run
	exec(argc, argv);
	dup2(pd[0], STDIN_FILENO);
	close(pd[0]);
}

/** Heredocs/herestrings support
 *
 * @param {string}hs,{istream&}in,bool mode
 * @return void
 */
bool
io_hedoc(std::string hs, std::istream& in, bool mode)
{
	std::string line, hs1;
	int fd;
	char temp[PATH_MAX];

#ifdef __ANDROID__
	strcpy( temp, (geteuid()) != 0
			? "/data/data/com.termux/files/usr" HTMP
			: HTMP );
#else
	strcpy(temp, HTMP);
#endif
	fd = mkstemp(temp);
	str_subst(hs);
	if (mode) {
		/* HERESTRING */
		hs += "\n";
		if (write(fd, hs.data(), hs.length()) == -1) {
			std::cerr << "herestring: write(3) failed\n";
			return 0;
		}
	} else {
		/* HEREDOC */
		while (zrc_read_line(in, line, here_prompt)) {
			if (line == hs)
				break;
			line += "\n";
			if (write(fd, line.data(), line.length()) == -1) {
				std::cerr << "heredoc: write(3) failed\n";
				return 0;
			}
		}
	}
	close(fd);
	io_left(temp);
	unlink(temp);
	return 1;
}
