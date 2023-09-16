// Use path hashing
#if defined USE_HASHCACHE && USE_HASHCACHE == 1
    #define RUNCMD {                    \
        if (hctable.find(*argv) != hctable.end())\
            *argv = &hctable[*argv][0]; \
        int rv;                         \
        if (!access(*argv, F_OK))       \
            rv = execv(*argv, argv);    \
        else                            \
            rv = execvp(*argv, argv);   \
        if (rv < 0) {                   \
          perror(*argv);                \
          exit(127);                    \
        } else {                        \
          exit(rv);                     \
        }                               \
    }
// Don't use path hashing
#else
    #define RUNCMD {                    \
        int rv = execvp(*argv, argv);   \
        if (rv < 0) {                   \
            perror(*argv);              \
            exit(127);                  \
        } else {                        \
            exit(rv);                   \
        }                               \
    }
#endif

/** Utility class to undo redirections **/
class FdHelper
{
private:
	enum FdAct : signed int {
		FD_ACT_NOP = -1, FD_ACT_CLOSE = -2
	};

	int baks[10];
public:
	FdHelper()
		{ std::fill_n(baks, 10, FD_ACT_NOP); }
	bool find(int fd)
		{ return baks[fd] != FD_ACT_NOP; }

	int
	add_fd(int fd)
	{
		if (baks[fd] == FD_ACT_NOP) {
			baks[fd] = (fcntl(fd, F_GETFD) != -1 || errno != EBADF)
				? dup2(fd, fd+fd_offset)
				: FdAct::FD_ACT_CLOSE;
		}
		return baks[fd];
	}

	~FdHelper()
	{
		for (int i = 0; i <= 9; ++i) {
			if (baks[i] == FdAct::FD_ACT_CLOSE) {
				close(i);
			} else if (baks[i] != FdAct::FD_ACT_NOP) {
				dup2(baks[i], i);
				close(baks[i]);
			}
			baks[i] = FD_ACT_NOP;
		}
		if (tcgetpgrp(STDIN_FILENO) != zrcpid)
			tcsetpgrp(STDIN_FILENO, zrcpid);
	}
};

/** Perform fd redirections one at a time **/
class Redirector
{
private:
	enum RedirTo : signed int {
		TO_FILE = -1, FROM_FILE = -2, TO_FD = -3, TO_CLOSE = -4
	};

	struct RedirAct {
		int fd;

		struct Target {
			RedirTo where;
			int fd;
			bool app;
			bool noclob;
			bool hedoc;
			std::string filename;
		} target;
	};
	std::vector<RedirAct> acts;
public:
	// Destructor gets magically called and fds get reset
	FdHelper fdh;

	/** Add redirections **/
	void redir_tofile(int fd, bool app, bool noclob, std::string filename)
		{ fdh.add_fd(fd); acts.push_back({fd, {TO_FILE, 0, app, noclob,0, filename}}); }

	void redir_tofd(int fd1, int fd2)
		{ fdh.add_fd(fd2); acts.push_back({fd1, {TO_FD, fd2, 0, 0, 0, ""}}); }
	
	void redir_close(int fd)
		{ fdh.add_fd(fd); acts.push_back({fd, {TO_CLOSE, 0, 0, 0, 0, ""}}); }
	
	void redir_left(int fd, std::string filename, bool hedoc=false)
		{ fdh.add_fd(fd); acts.push_back({fd, {FROM_FILE, 0, 0, 0, hedoc, filename}}); }

	void
	do_redirs()
	{
		int ffd;
		for (auto it = acts.begin(); it != acts.end(); ++it) {
			auto fd = it->fd; auto target = it->target;

			switch (target.where) {
			/* (x)>/>> y... */
			case TO_FILE:
				if (target.app)
					ffd = open(target.filename.data(), O_CREAT|O_APPEND|O_WRONLY, 0644);
				else if (!(target.noclob && !access(target.filename.data(), F_OK)))
					ffd = open(target.filename.data(), O_CREAT|O_TRUNC|O_WRONLY, 0600);
				else
					std::cerr << warnmsg 
						<< "The file "
						<< target.filename.data()
						<< " already exists\n";
				if (ffd != fd) {
					dup2(ffd, fd);
					close(ffd);
				}
				break;
		
			/* </<</<<< x ... */
			case FROM_FILE:
				ffd = open(target.filename.data(), O_RDONLY);
				if (ffd != fd) {
					dup2(ffd, fd);
					close(ffd);
				}
				if (target.hedoc)
					unlink(target.filename.data());
				break;

			/* (x)> &y ...*/
			case TO_FD:
				dup2(fd, target.fd);
				break;

			/* (x)> &- ... */
			case TO_CLOSE:
				close(fd);
			}
		}
		acts.clear();
	}
};

/** Converts an array into a space-separated string.
 * 
 * @param {int}c,{char**}v,{int}i
 * @return void
 */
template<typename T> std::string
combine(int c, T v, int i)
{
    std::string buf = "";
    for (int k = i; k < c-1; ++k) {
        buf += v[k];
				buf += ' ';
    }
		buf += v[c-1];
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
	try
		{ eval(funcs[cmd]); }
	catch (ZrcReturnHandler ex)
		{ }
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
				std::string oa = getvar($ARGC);

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
	fifos.remove_if([](std::unique_ptr<Fifo> const& f) {
		return f->eval_level == fd_offset;
	});	
	for (i = 0; i < argc; ++i) {
		free(argv[i]);
		argv[i] = NULL;
	}
	make_new_jobs = false;
	setvar($RETURN, ret_val);
}

/** Captures a program fragment's standard output.
 *
 * @param {string}frag
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

/** Perform process substitution/pipeline branching.
 * 
 * @param {string}frag
 * @return string
 */
std::string
io_proc(std::string frag)
{
	char temp[PATH_MAX];
#ifdef __ANDROID__
	strcpy( temp, (geteuid()) != 0
			? "/data/data/com.termux/files/usr" PTMP
			: PTMP );
#else
	strcpy(temp, PTMP);
#endif
	char *dir = mkdtemp(temp);
	char fifo[PATH_MAX];
	pid_t pid;
	if (!dir) {
		perror("mkdtemp");
		return "";
	}
	sprintf(fifo, "%s/fifo", dir);
	mkdir(dir, 0755);
	mkfifo(fifo, 0666);
	pid = fork();
	if (pid == 0) {
		NO_SIGEXIT;
		chk_exit = true;
		dup2(open(fifo, O_WRONLY), STDOUT_FILENO);
		eval(frag);
		exit(0);
	}
	fifos.emplace_back(std::make_unique<Fifo>(fd_offset, pid, fifo));
	return fifo;
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
 * @param {string}fn,{Redirector&}red,{bool}hedoc=false
 * @return bool
 */
bool
io_left(std::string fn, Redirector& red, bool hedoc=false)
{
	int fd;

	if (!str_subst_expect1(fn))
		return 0;
	if (access(fn.c_str(), F_OK)) {
		perror(fn.c_str());
		return 0;
	}
	red.redir_left(STDIN_FILENO, fn, hedoc);
	return 1;
}

/** Redirects Fd #n (can also append to a file).
 *
 * @param {string}exp,{int}fd,{bool}app,{bool}noclob,{Redirector&}red
 * @return bool
 */
bool
io_right(std::string exp, int fd, bool app, bool noclob, Redirector& red)
{
	auto len = exp.length();
	int fd2;

	/* close file descriptor (...> &-) */
	if (exp == "&-") {
		red.redir_close(fd);
		return 1;
	}

	/* duplicate file descriptor fd to n (...> &<n>) */
	if (len == 2 && exp[0] == '&') {
		if (!isdigit(exp[1])) {
			std::cerr << errmsg << "...&<n>\n";
			return 0;
		}
		exp[1] -= '0';
		/*
		if (fcntl(exp[1], F_GETFL) < 0 && errno == EBADF) {
			char s[2];
			s[0] = exp[1]+'0', s[1] = '\0';
			perror(s);
			return 0;
		}
		*/
		red.redir_tofd(exp[1], fd);
		return 1;
	}

	if (!str_subst_expect1(exp))
		return 0;
	red.redir_tofile(fd, app, noclob, exp.data());
	return 1;
}

/** Converts stdout to stdin.
 * 
 * @param {int}argc,{char**}argv,{Redirector&}fdh
 * @return void
 */
void
io_pipe(int argc, char *argv[], Redirector& red)
{
	int pd[2];
	pipe(pd);
	dup2(pd[1], STDOUT_FILENO);
	close(pd[1]);

	// Run
	red.do_redirs();
	exec(argc, argv);
	red.fdh.~FdHelper();
	dup2(pd[0], STDIN_FILENO);
	close(pd[0]);
}

/** Heredocs/herestrings support
 *
 * @param {string}hs,{istream&}in,bool mode,{Redirector&}red
 * @return bool
 */
bool
io_hedoc(std::string hs, std::istream& in, bool mode, Redirector& red)
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
	io_left(temp, red, true);
	return 1;
}
