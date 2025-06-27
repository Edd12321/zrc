#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <unordered_map>

// Complains when messed up syntax is encountered
#define SYNTAX_ERROR {                                             \
  std::cerr << "syntax error: " << argv[0] << ' ' << help << '\n'; \
  return "1";                                                      \
}

// Extra control flow
#define until(x) while (!(x))
#define unless(x) if (!(x))

#undef END
// Easier command declararions
#if defined(__GLIBC__) && !defined(__UCLIBC__)
	#define OPTIND_RESET 0
#else
	#define OPTIND_RESET 1
#endif

std::unordered_map<std::string, std::string> help_strs;
#define COMMAND(x, help_str) { (help_strs[#x] = #help_str, #x),  [](int argc, char *argv[]) -> zrc_obj {\
	int opt; optind = OPTIND_RESET; auto const& help = #help_str; \

#define END ; return vars::status;} },

#define SIGEXIT 0
const std::map<std::string, int> txt2sig = {
	{ "sighup"  , SIGHUP   }, { "sigint"   , SIGINT    }, { "sigquit", SIGQUIT },
	{ "sigill"  , SIGILL   }, { "sigtrap"  , SIGTRAP   }, { "sigabrt", SIGABRT },
	{ "sigbus"  , SIGBUS   }, { "sigfpe"   , SIGFPE    }, { "sigkill", SIGKILL },
	{ "sigusr1" , SIGUSR1  }, { "sigsegv"  , SIGSEGV   }, { "sigusr2", SIGUSR2 },
	{ "sigpipe" , SIGPIPE  }, { "sigalrm"  , SIGALRM   }, { "sigterm", SIGTERM },
	/* #16: unused         */ { "sigchld"  , SIGCHLD   }, { "sigcont", SIGCONT },
	{ "sigstop" , SIGSTOP  }, { "sigtstp"  , SIGTSTP   }, { "sigttin", SIGTTIN },
	{ "sigttou" , SIGTTOU  }, { "sigurg"   , SIGURG    }, { "sigxcpu", SIGXCPU },
	{ "sigxfsz" , SIGXFSZ  }, { "sigvtalrm", SIGVTALRM }, { "sigprof", SIGPROF },
	{ "sigwinch", SIGWINCH }, { "sigio"    , SIGPOLL   }, { "sigpoll", SIGPOLL },
	                          // (both SIGIO and SIGPOLL are the same)
	{ "sigsys"  , SIGSYS   },


#ifdef SIGPWR
	{ "sigpwr"  , SIGPWR   },
#endif
#ifdef SIGRTMIN
	{ "sigrtmin", SIGRTMIN },
#endif
#ifdef SIGRTMAX
	{ "sigrtmax", SIGRTMAX },
#endif
	{ "sigexit" , SIGEXIT }
	//Special zrc "pseudo-signal"
};

// Helps us to see if we can change control flow
#define EXCEPTION_CLASS(x)                   \
  class x##_handler                          \
  {                                          \
  public:                                    \
    virtual const char *what() const throw() \
    {                                        \
      return "Caught " #x;                   \
    }                                        \
  };
EXCEPTION_CLASS(fallthrough)
EXCEPTION_CLASS(break)
EXCEPTION_CLASS(continue)
EXCEPTION_CLASS(return)
EXCEPTION_CLASS(regex)

bool in_loop;
bool in_switch;
bool in_func;

class block_handler
{
private:
	bool ok = false, *ref = &in_loop;
public:
	block_handler(bool *ref)
	{
		this->ref = ref;
		if (!*ref)
			ok = true;
		*ref = true;
	}
	~block_handler()
	{
		if (ok)
			*ref = false;
	}
};

static inline std::string concat(int argc, char *argv[], int i)
{
	std::string ret;
	for (; i < argc; ++i) {
		ret += argv[i];
		if (i < argc-1)
			ret += ' ';
	}
	return ret;
}

static inline zrc_num expr(std::string const& str)
{
	return expr(str.c_str());
}

// Eval-or-exec behaviour on arguments
static inline void eoe(int argc, char *argv[], int i)
{
	if (argc == i+1) 
		eval(argv[i]);
	else
		exec(argc-i, argv+i);
}

// Match ERE
bool regex_match(std::string const& txt, std::string const& reg, int cflags = REG_NOSUB | REG_EXTENDED)
{
	regex_t regex;

	if (regcomp(&regex, reg.c_str(), cflags))
		throw regex_handler();
	auto ret = regexec(&regex, txt.c_str(), 0, NULL, 0);
	regfree(&regex);
	return !ret;
}

// Dir stack
std::stack<std::string> pstack;
// Path hashing
std::unordered_map<std::string, std::string> hctable;

static inline void prints(std::stack<std::string> sp)
{
	if (!sp.empty())
		chdir(sp.top().c_str());
	while (!sp.empty()) {
		std::cout << sp.top() << ' ';
		sp.pop();
	}
	std::cout << '\n';
}

// User functions
struct zrc_fun {
	std::string body;

	zrc_fun()=default;
	zrc_fun(std::string const& b) : body(b) {}

	zrc_obj operator()(int argc, char *argv[])
	{
		zrc_obj argc_old = vars::argc;
		zrc_arr argv_old = vars::argv;
		vars::argv = copy_argv(argc, argv);
		vars::argc = numtos(argc);
		block_handler fh(&in_func);
		try { eval(body); } catch (return_handler ex) {}
		vars::argc = argc_old;
		vars::argv = argv_old;

		// Don't forget
		return vars::status;
	}
};

/*****************
 *               *
 * Command table *
 *               *
 *****************/
CMD_TBL builtins = {

// Commands that only work in certain contexts
#define CTRLFLOW_HELPER(x, y, help_str, z)         \
  COMMAND(y, help_str)                             \
    if (!in_##x) {                                 \
      std::cerr << "can't " #y ", not in " #x "\n";\
      return vars::status;                         \
    }                                              \
    z;                                             \
    throw y##_handler();                           \
  END
CTRLFLOW_HELPER(switch, fallthrough,,)
CTRLFLOW_HELPER(loop,   break,,)
CTRLFLOW_HELPER(loop,   continue,,)
CTRLFLOW_HELPER(func,   return,[<val>],
	if (argc > 1) {
		if (argc > 2) SYNTAX_ERROR
		vars::status = std::string(argv[1]);
	}
)

// Unless and while use a similar command.
#define WHILE_HELPER(x)             \
  COMMAND(x, <expr> <eoe>)          \
    if (argc < 3) SYNTAX_ERROR      \
    block_handler lh(&in_loop);     \
  _repeat_while:                    \
    try {                           \
      if (argc == 3)                \
        x (expr(argv[1]))           \
          eval(argv[2]);            \
      else                          \
        x (expr(argv[1]))           \
          exec(argc-2, argv+2);     \
    } catch (break_handler ex) {    \
    } catch (continue_handler ex) { \
      goto _repeat_while;           \
    }                               \
  END
WHILE_HELPER(while)
WHILE_HELPER(until)

// Substitute arguments
COMMAND(subst,  [<w1> <w2>...])  return subst(concat(argc, argv, 1))         END
// Evaluate arguments
COMMAND(eval,   [<w1> <w2>...])  return eval(concat(argc, argv, 1))          END
// Evaluate as an expression
COMMAND(expr,   [<w1> <w2>...])  return numtos(expr(concat(argc, argv, 1)))  END
// Concatenate arguments
COMMAND(concat, [<w1> <w2>...])  return concat(argc, argv, 1)                END
// Replace currently running process
COMMAND(exec,   <w1> [<w2>...])  if (argc > 1) execvp(*(argv+1), argv+1)     END
// Wait for child processes to finish execution
COMMAND(wait,                 )  while (wait(NULL) > 0)                      END
// Source a script
COMMAND(.,      [<w1> <w2>...])  source(concat(argc, argv, 1))               END
// Disable internal hash table
COMMAND(unhash,               )  hctable.clear()                             END
// Display internal job table
COMMAND(jobs,                 )  show_jobs()                                 END

// Foreground/background tasks
#define FGBG(z, x, y)                            \
  COMMAND(x, <n>)                                \
    if (argc != 2) SYNTAX_ERROR                  \
    auto n = expr(argv[1]);                      \
    if (isnan(n)) SYNTAX_ERROR                   \
    auto p = pid2job(n);                         \
    if (p < 0) {                                 \
      std::cerr << "Bad PID\n";                  \
      return "2";                                \
    }                                            \
    kill(-getpgid(n), SIGCONT);                  \
    jobstate(p, z); y;                           \
    if (getpid() == tty_pid)                     \
       tcsetpgrp(tty_fd, tty_pid)                \
  END
FGBG(1, fg, tcsetpgrp(tty_fd, getpgid(n)); reaper(n, WUNTRACED))
FGBG(0, bg,)

// JID to PID
COMMAND(job, <n>)
	if (argc != 2) SYNTAX_ERROR
	auto x = expr(argv[1]);
	if (isnan(x)) SYNTAX_ERROR
	auto pid = job2pid(x);
	if (pid < 0) {
		std::cerr << "Expected a valid JID" << std::endl;
		return "-1";
	}
	return numtos(pid)
END

// Refresh internal hash table
COMMAND(rehash,)
	std::istringstream iss{getvar(PATH)};
	std::string tmp;
	hctable.clear();

	while (getline(iss, tmp, ':')) {
		struct dirent *entry;
		struct stat sb;

		DIR *d = opendir(tmp.c_str());
		if (d) {
			while ((entry = readdir(d))) {
				char *nm = entry->d_name;
				char nm2[PATH_MAX];
				sprintf(nm2, "%s/%s", tmp.c_str(), nm);
				if (hctable.find(nm) == hctable.end() && !stat(nm2, &sb) && sb.st_mode & S_IXUSR) {
					hctable[nm] = nm2;
					std::cout << "Added " << nm << " (" << nm2 << ")\n";
				}
			}
		}
		closedir(d);
	}
END

// Prioritise builtins
COMMAND(builtin, <arg1> <arg2>...)
	if (argc < 2) SYNTAX_ERROR
	--argc, ++argv;
	if (builtins.find(*argv) != builtins.end())
		builtins.at(*argv)(argc, argv);
	else
		exec(argc, argv)
END

// Add/remove a new function
COMMAND(fn, <name> [<w1> <w2>...])
	if (argc >= 3) {
		// Function body
		auto b = concat(argc, argv, 2);
		functions[argv[1]] = zrc_fun(concat(argc, argv, 2));
	} else if (argc == 2) {
		functions.erase(argv[1]);
	} else SYNTAX_ERROR	
END

COMMAND(die, [<w1> <w2>...])
	std::cerr << concat(argc, argv, 1) << std::endl;
	exit(EXIT_FAILURE)
END

// List all commands
COMMAND(help, [<cmd1> <cmd2>...])
	bool handled_args = false;
	for (int i = 1; i < argc; ++i) {
		if (functions.find(argv[i]) != functions.end()) {
			std::cout << "# function\n";
			std::cout << "fn " << list(argv[i]) << " {" << functions.at(argv[i]).body << "}\n";
			handled_args = true;
		}
		if (builtins.find(argv[i]) != builtins.end()) {
			if (handled_args)
				std::cout << '\n';
			std::cout << "# builtin " << '\n' << "# " << argv[i] << ' ';
			std::stringstream ss{help_strs[argv[i]]};
			std::string buf;
			getline(ss, buf);
			std::cout << buf << '\n';
			while (getline(ss, buf))
				std::cout << '#' << buf << '\n';
			handled_args = true;
		}
		if (hctable.find(argv[i]) != hctable.end()) {
			if (handled_args)
				std::cout << '\n';
			std::cout << "# external\n";
			std::cout << "# " << hctable[argv[i]] << '\n';
			handled_args = true;
		}
		if (!handled_args) SYNTAX_ERROR
	}

	if (handled_args)
		return "0";

#define DISPLAY_COMMANDS_MAP(MAP)            \
	std::vector<std::string> v##MAP;         \
	for (auto const& it : MAP)               \
		v##MAP.push_back(it.first);          \
	std::sort(v##MAP.begin(), v##MAP.end()); \
	for (auto const& it : v##MAP)            \
		std::cout << it << ' '; 
	DISPLAY_COMMANDS_MAP(builtins)
	std::cout << "\n\n";
	DISPLAY_COMMANDS_MAP(functions)
	std::cout << '\n';
END

// If/else command
COMMAND(if, <expr> <eval> [else <eoe>])
	if (argc < 3) SYNTAX_ERROR
	if (expr(argv[1]))
		eval(argv[2]);
	else if (argc > 3) {
		if (argc == 4 || strcmp(argv[3], "else")) SYNTAX_ERROR
		eoe(argc, argv, 4);
	}
END

// Unless command
COMMAND(unless, <expr> <eoe>)
	if (argc < 3) SYNTAX_ERROR
		unless (expr(argv[1]))
			eoe(argc, argv, 2)
END

// For loops
COMMAND(for, <eval> <expr> <eval> <eoe>)
	if (argc < 5) SYNTAX_ERROR
	block_handler bh(&in_loop);
	auto old_stmt = argv[1];
_repeat_for:
	try {
		for (eval(argv[1]); expr(argv[2]); eval(argv[3]))
			eoe(argc, argv, 4);
	} catch (break_handler ex) {
	} catch (continue_handler ex) {
		argv[1] = argv[3];
		goto _repeat_for;
	}
	argv[1] = old_stmt
END

// Do/while and do/until
COMMAND(do, <eoe> while|until <expr>...)
	if (argc < 4) SYNTAX_ERROR
	bool w = !strcmp(argv[argc-2], "while");
	bool u = !strcmp(argv[argc-2], "until");
	if (!w && !u) SYNTAX_ERROR

	block_handler bh(&in_loop);
_repeat_do:
	try {
		if (w) do { eoe(argc-2, argv, 1); } while (expr(argv[argc-1]));
		if (u) do { eoe(argc-2, argv, 1); } until (expr(argv[argc-1]));
	} catch (break_handler ex) {
	} catch (continue_handler ex) {
		if (w &&  expr(argv[argc-1])) goto _repeat_do;
		if (u && !expr(argv[argc-1])) goto _repeat_do;
	}
END

COMMAND(repeat, <expr> <eoe>)
	if (argc < 3) SYNTAX_ERROR

	zrc_num exp = floor(expr(argv[1]));
	if (isnan(exp)) SYNTAX_ERROR

	while (exp--)
		eoe(argc, argv, 2);
END

// Switch instruction
COMMAND(switch, <val> {< <case|regex|default> <eval>...>})
	if (argc != 3) SYNTAX_ERROR
	
	auto wlst = lex(argv[2], SPLIT_WORDS).elems;
	auto txt = argv[1];
	ssize_t i, len, def = -1;

	struct switch_case {
		enum switch_type {
			CASE, DEFAULT, REGEX
		} type;
		std::string txt;
		std::string block;
	};
	std::vector<switch_case> vec;
	using SW = switch_case::switch_type;

	len = wlst.size();
	// Parse
	for (i = 0; i < len; ++i) {
		std::string conv = wlst[i];
		if (conv == "case" && i < len-2) {
			vec.push_back({ SW::CASE, wlst[i+1], wlst[i+2] });
			i += 2;
			continue;
		}
		if (conv == "regex" && i < len-2) {
			vec.push_back({ SW::REGEX, wlst[i+1], wlst[i+2] });
			i += 2;
			continue;
		}
		if (conv == "default" && i < len-1) {
			if (def != -1) {
				std::cerr << "syntax error: Expected only one default block" << std::endl;
				return "1";
			}
			vec.push_back({ SW::DEFAULT, std::string(), wlst[++i] });
			def = vec.size()-1;
			continue;
		}
		std::cerr << "syntax error: Invalid case type " << list(conv) << std::endl;
		return "1";
	}

	block_handler sh(&in_switch);
	len = vec.size();
	bool fell = false, ran_once = true;
	// Try to evaluate
	for (i = 0; i < len; ++i) {
_repeat_switch:
		try {
			switch (vec[i].type) {
				// case {word} {script}
				case SW::CASE:
					if (fell || vec[i].txt == txt) {
						ran_once = true;
						return eval(vec[i].block);
					}
					break;

				// regex {regex} {script}
				case SW::REGEX:
					try {
						if (fell || regex_match(txt, vec[i].txt)) {
							ran_once = true;
							return eval(vec[i].block);
						}
					} catch (regex_handler ex) {
						std::cerr << "syntax error: Invalid regex " << list(vec[i].txt) << std::endl;
						return "1";
					}
					break;

				// default {script}
				case SW::DEFAULT:;
					if (fell || def != -1) {
						ran_once = true;
						return eval(vec[i].block);
					}
			}
			fell = false;
		} catch (fallthrough_handler ex) {
			fell = true;
			continue;
		}
	}
	if (!ran_once) {
		i = def;
		goto _repeat_switch;
	}
END

// For-each loop
COMMAND(foreach, <var> <var-list> <eoe>)
	if (argc < 4) SYNTAX_ERROR
	block_handler lh(&in_loop);
	auto vlst = lex(argv[2], SPLIT_WORDS).elems;
	auto it = vlst.begin();
_repeat_foreach:
	try {
		for (auto it = vlst.begin(); it != vlst.end(); ++it) {
			setvar(argv[1], *it);
			eoe(argc, argv, 3);
		}
	} catch (break_handler ex) {
	} catch (continue_handler ex) {
		++it; goto _repeat_foreach;
	}
END

// Negation
COMMAND(!, [<eoe>])
	if (argc == 1) return vars::status;

	eoe(argc, argv, 1);
	return numtos(!(stonum(vars::status)))
END

// Subshell
COMMAND(@, [<eoe>])
	if (argc == 1) return vars::status;

	std::string ret_str;
	int pd[2];
	pipe(pd);
	pid_t pid = fork();
	if (pid == 0) {
		close(pd[0]);
		eoe(argc, argv, 1);
		fflush(stdout);
		dup2(pd[1], STDOUT_FILENO);
		close(pd[1]);
		std::cout << vars::status << std::flush;
		_exit(0);
	} else {
		close(pd[1]);
		char c;
		while (read(pd[0], &c, 1) >= 1)
			ret_str += c;
		close(pd[0]);
	}
	return ret_str
END

// Fork off a new process, C-style
COMMAND(fork,)
	return numtos(fork())
END

// Get pid of self or job
COMMAND(pid,)
	return numtos(getpid())
END

// Read from stdin
COMMAND(read, [-d <delim>|-n <nchars>] [-p <prompt>] [-f <fd>] [<var1> <var2>...])
	auto delim = (char*)"\n";
	int status = 2, n = -1, fd = STDIN_FILENO;

	while ((opt = getopt(argc, argv, "d:n:p:f:")) != -1) {
		switch (opt) {
			case 'd':
				delim = optarg;
				break;
			case 'n':
				n = expr(optarg);
				break;
			case 'p':
				std::cout << optarg << std::flush;
				break;
			case 'f':
				fd = expr(optarg);
				break;
			case '?':
				SYNTAX_ERROR
		}
	}
	auto read_str = [&]()
	{
		std::string ret_val;
		char c;
		status = 2;
		if (n < 0) {
			char c;
			while (read(fd, &c, 1) == 1) {
				status = 0;
				if (strchr(delim, c))
					break;
				ret_val += c;
			}
		} else {
			char *s = new char[n+1];
			if (read(fd, s, n) > 0)
				status = 0;
			s[n] = '\0';
			ret_val += s;
			delete [] s;
		}
		return ret_val;
	};

	if (optind >= argc)
		std::cout << read_str() << std::endl;
	else for (; optind < argc; ++optind)
		setvar(argv[optind], read_str());
	return numtos(status);
END

// Aliases
COMMAND(alias, [<name> < <w1> <w2>...>])
	if (argc == 1)
		for (auto& it : kv_alias)
			std::cout << "alias " << list(it.first) << ' ' << list(it.second) << '\n';
	else if (argc >= 3)
		kv_alias[argv[1]] = concat(argc, argv, 2);
	else SYNTAX_ERROR
END

COMMAND(unalias, <a1> <a2>...)
	if (argc < 2) SYNTAX_ERROR
	for (int i = 1; i < argc; ++i)
		kv_alias.erase(argv[i]);
END

// Key bindings
COMMAND(bindkey, [-c] [<seq> < <w1> <w2>...>])
	bind b;
	if (argc == 2)
		SYNTAX_ERROR
	if (argc == 1) {
		for (auto const& it : kv_bindkey) {
			std::cout << "bindkey ";
			if (it.second.zrc_cmd)
				std::cout << "-c ";
			std::cout << list(it.first) << ' ' << list(it.second.cmd) << '\n';
		}
		return vars::status;
	}
	if (argc >= 4 && !strcmp(argv[1], "-c")) {
		b.zrc_cmd = true;
		--argc, ++argv;
	} else
		b.zrc_cmd = false;
	b.cmd = concat(argc, argv, 2);
	kv_bindkey[argv[1]] = std::move(b);
END
	
COMMAND(unbindkey, <b1> <b2>...)
	if (argc < 2) SYNTAX_ERROR;
	for (int i = 1; i < argc; ++i)
		kv_bindkey.erase(argv[i]);
END

// Change directory
COMMAND(cd, [<dir>])
	struct stat sb;

	if (argc == 1) {
		struct passwd *pw = getpwuid(getuid());
		chdir(pw->pw_dir);
	} else {
		if (argc != 2) SYNTAX_ERROR
		if (!stat(argv[1], &sb) && S_ISDIR(sb.st_mode))
			chdir(argv[1]);
		else {
			/* empty */ {
				std::istringstream iss{getvar(CDPATH)};
				std::string tmp;
				while (getline(iss, tmp, ':')) {
					char t[PATH_MAX];
					sprintf(t, "%s/%s", tmp.data(), argv[1]);
					if (!stat(t, &sb) && S_ISDIR(sb.st_mode)) {
						chdir(t);
						return vars::status;
					}
				}
			}
			perror(argv[1]);
			return "1";
		}
	}
END

// Print to stdout (inspired by suckless.org's implementation)
COMMAND(echo, [<w1> <w2>...])
	bool nflag = false;
	--argc, ++argv;
	if (*argv && !strcmp(*argv, "-n"))
		--argc, ++argv,
		nflag = true;
	for (; *argv; --argc, ++argv) {
		std::cout << *argv;
		if (argc > 1)
			std::cout << ' ';
	}
	if (!nflag)
		std::cout << '\n';
END

// Assign to a list of variables
COMMAND(set, < <var> [bin-op]= <val> >...)
	if ((argc-1) % 3 != 0) SYNTAX_ERROR
	zrc_obj lret;
	for (int i = 2; i < argc; i += 3) {
		auto len = strlen(argv[i])-1;
		if (argv[i][len] != '=')
			SYNTAX_ERROR
		if (argv[i][1] == '\0')
			lret = setvar(argv[i-1], argv[i+1]);
		else {
			argv[i][len] = '\0';
			lret = setvar(argv[i-1], numtos(expr("("+getvar(argv[i-1])+")"+argv[i]+"("+argv[i+1]+")")));
		}
	}
	return lret
END

// Unset a list of variables
COMMAND(unset, <var1> <var2>...)
	if (argc < 2) SYNTAX_ERROR
	for (int i = 1; i < argc; ++i)
		unsetvar(argv[i])
END

// Export one or more variables
COMMAND(export, [-n] < <var1> <var2>...>)
	bool nflag = false;
	while ((opt = getopt(argc, argv, "n")) != -1) {
		switch (opt) {
			case 'n': nflag = true; break;
			case '?': SYNTAX_ERROR
		}
	}
	if (optind >= argc) SYNTAX_ERROR
	for (; optind < argc; ++optind) {
		std::string val = getvar(argv[optind]);
		unsetvar(argv[optind]);
		if (!nflag)
			setenv(argv[optind], val.c_str(), true);
		else
			setvar(argv[optind], val.c_str());
	}
END

// Filename globbing
COMMAND(glob, [-sb?t?] <patterns...>)
	char flags[4] = "s";
	int g_flags = GLOB_NOSORT;
	// If your stdlib doesn't use GNU extensions,
	// then I guess that means no tilde exp for you.
#ifdef GLOB_BRACE
	strcat(flags, "b");
#endif
#ifdef GLOB_TILDE
	strcat(flags, "t");
#endif
	/* different help str gets generated here */ {
	std::string help = "[-"; help += flags; help += "] <patterns...>";
	while ((opt = getopt(argc, argv, flags)) != -1) {
		switch (opt) {
			case '?': SYNTAX_ERROR
			case 's': g_flags &= ~GLOB_NOSORT; break;
#ifdef GLOB_BRACE
			case 'b': g_flags |= GLOB_BRACE; break;
#endif
#ifdef GLOB_TILDE
			case 't': g_flags |= GLOB_TILDE; break;
#endif
		}
	}
	if (optind >= argc)
		SYNTAX_ERROR
	}
	
	std::string lst;
	for (; optind < argc; ++optind) {
		auto ret = glob(argv[optind], g_flags);
		for (size_t i = 0; i < ret.size(); ++i) {
			lst += ret[i];
			if (i < ret.size()-1)
				lst += ' ';
		}
		if (optind < argc-1)
			lst += '\n';
	}
	return lst
END

// Increment a variable
COMMAND(inc, <var> [<amount>])
	if (argc < 2 || argc > 3) SYNTAX_ERROR
	std::string val = (argc == 2) ? "1" : argv[2];
	return numtos(setvar(argv[1], numtos(expr(getvar(argv[1]))+expr(val))))
END

// PHP chr/ord
COMMAND(chr, <expr1> <expr2>...)
	if (argc != 2) SYNTAX_ERROR
	std::string ret;
	auto e = expr(concat(argc, argv, 1));
	if (isnan(e))
		ret = "error";
	else
		ret += (char)e;
	return ret
END

COMMAND(ord, <c>)
	if (argc != 2) SYNTAX_ERROR
	return std::to_string(argv[1][0])
END

// Lexical scoping
COMMAND(let, <var-list> <eoe>)
	if (argc < 3) SYNTAX_ERROR

	auto wlst = lex(argv[1], SPLIT_WORDS).elems;
	
	bool ret = false, brk = false, con = false, fal = false;
	std::unordered_map<std::string, zrc_obj> vmap;
	std::unordered_map<std::string, zrc_arr> amap;
	std::unordered_map<std::string, bool> didnt_exist;

	for (auto const& it : wlst) {
		if (vars::amap.find(it) != vars::amap.end())
			amap[it] = vars::amap[it];
		else if (vars::vmap.find(it) != vars::vmap.end())
			vmap[it] = vars::vmap[it];
		else
			didnt_exist[it] = true;
	}

	try {
		eoe(argc, argv, 2);
	} catch (return_handler ex)      { ret = true; }
	  catch (break_handler ex)       { brk = true; }
	  catch (continue_handler ex)    { con = true; }
	  catch (fallthrough_handler ex) { fal = true; }
	
	for (auto const& it : amap)
		vars::amap[it.first] = it.second;
	for (auto const& it : vmap) {
		unsetvar(it.first);
		setvar(it.first, it.second);
	}
	for (auto const& it : didnt_exist)
		unsetvar(it.first);

	if (ret) throw return_handler();
	if (brk) throw break_handler();
	if (con) throw continue_handler();
	if (fal) throw fallthrough_handler();
END

// Close shell
COMMAND(exit, [<val>])
	if (argc > 2) SYNTAX_ERROR
	if (argc < 2) exit(expr(vars::status));
	else exit(atoi(argv[1]))
END

// Directory stack
COMMAND(pushd, [<dir>])
	if (argc > 2) SYNTAX_ERROR

	if (pstack.empty()) {
		char wd[PATH_MAX];
		getcwd(wd, sizeof wd);
		pstack.push(wd);
	}
	if (argc == 2) {
		struct stat strat;
		if (stat(argv[1], &strat) != 0) {
			perror(argv[1]);
			return "2";
		}
		if (!S_ISDIR(strat.st_mode)) {
			std::cerr << argv[1] << " is not a directory\n";
			return "3";
		}
		char *rp = realpath(argv[1], NULL);
		pstack.push(rp);
		free(rp);
	} else {
		if (pstack.size() == 1) {
			std::cerr << "No other directory\n";
			return "4";
		}
		auto p = pstack.top();
		pstack.pop();
		std::swap(p, pstack.top());
		pstack.push(p);
	}
	prints(pstack)
END

COMMAND(popd,)
	if (pstack.empty()) {
		std::cerr << "Directory stack empty\n";
		return "1";
	}
	pstack.pop();
	prints(pstack)
END

// Increase memory amount
COMMAND(rlimit, <n>BKMGTPEZYg)
	auto cptr = help+3;
	if (argc != 2)
		SYNTAX_ERROR

	auto len = strlen(argv[1])-1;
	char lc = argv[1][len];
	auto fd = strchr(cptr, lc);
	argv[1][len] = '\0';

	rlim_t memory;
	struct rlimit rlm;

	if (!fd || isnan(memory = expr(argv[1])))
		SYNTAX_ERROR
	
	memory <<= (fd-cptr)*10;
	if (!getrlimit(RLIMIT_STACK, &rlm))
		if (rlm.rlim_cur < memory) {
			rlm.rlim_cur = memory;
			if (setrlimit(RLIMIT_STACK, &rlm)) {
				std::cerr << "setrlimit() failed!\n";
				return "2";
			}
		}
END

/****************************************
 *                                      *
 * Special data type manipulation cmds  *
 *                                      *
 ****************************************/

// Strings
COMMAND(str, <s> > | >= | == | != | <=> | <= | < <p> \n
             <s> <ind> \n
             <s> + <ptr> \n
             <s> <r1> <r2> \n
             <s> <r1> <r2> \n
             <s> <ind> = <c> \n
             <s> - <ind1> <ind2>...)

#define STROP(x, op) if (argc == 4 && !strcmp(argv[2], #x)) return numtos(strcmp(argv[1], argv[3]) op);
	// String comparisons
	STROP(>, > 1) STROP(==, == 0) STROP(<=, <= 0)
	STROP(<, < 0) STROP(!=, != 0) STROP(>=, >= 0)
	STROP(<=>,)

	// Return string length
	if (argc == 3 && !strcmp(argv[2], "len")) return numtos(strlen(argv[1]));
	// Return string char at index
	if (argc == 3) {
		zrc_num i = expr(argv[2]);
		if (!isnan(i) && i >= 0 && i < strlen(argv[1]))
			return std::string(1, argv[1][size_t(i)]);
		else SYNTAX_ERROR
	}
	// Return string starting from index
	if (argc == 4 && !strcmp(argv[2], "+")) {
		zrc_num i = expr(argv[3]);
		if (!isnan(i) && i >= 0 && i <= strlen(argv[1]))
			return std::string(argv[1]+size_t(i));
		else SYNTAX_ERROR
	}
	// Return string minus chars from indexes removed
	if (argc >= 4 && !strcmp(argv[2], "-")) {
		std::string str = argv[1];
		for (int i = 3, k = 0; i < argc; ++i) {
			zrc_num j = expr(argv[i]);
			if (!isnan(j) && j >= 0 && j < str.length())
				str.erase(j-k++, 1);
			else SYNTAX_ERROR
		}
		return str;
	}
	// Return string range
	if (argc == 4) {
		zrc_num i = expr(argv[2]);
		zrc_num j = expr(argv[3]);
		if (!isnan(i) && i >= 0 && j >= 0 && !isnan(j) && i <= j && j < strlen(argv[1])) {
			size_t li = i, lj = j+1;
			char c_old = argv[1][lj];
			argv[1][lj] = '\0';
			std::string ret_val = argv[1]+li;
			argv[1][lj] = c_old;
			return ret_val;
		} else SYNTAX_ERROR
	}
	// Return string with replaced char
	if (argc == 5 && !strcmp(argv[3], "=")) {
		zrc_num i = expr(argv[2]);
		if (!isnan(i) && i >= 0 && i < strlen(argv[1])) {
			argv[1][size_t(i)] = argv[4][0];
			return argv[1];
		} else SYNTAX_ERROR
	}
	SYNTAX_ERROR
END

// Arrays
COMMAND(arr, <a> := <list> \n
             <a> = <even-list> \n
             <a> -= <elem1> <elem2>... \n
             <a> len \n
             <a> keys \n
             <a> vals \n
             <a> destroy \n
             <a>)

	if (argc < 2) SYNTAX_ERROR
	auto& arr = vars::amap[argv[1]];

	auto get_sorted_keys = [&]() {
		auto sort = [](std::string const& lhs, std::string const& rhs) {
			zrc_num x, y;
			bool x_num = true, y_num = true;
			try { x = std::stold(lhs); } catch (...) { x_num = false; }
			try { y = std::stold(rhs); } catch (...) { y_num = false; }
			if (x_num != y_num)
				return x_num;
			if (x_num && x != y)
				return x < y;
			return lhs < rhs;
		};
		std::vector<std::string> keys;
		for (auto const& it : arr)
			keys.push_back(it.first);
		std::sort(keys.begin(), keys.end(), sort);
		return keys;
	};

	// Return array as list (k v)
	if (argc == 2) {
		zrc_obj ret_val;
		for (auto const& it : get_sorted_keys())
			ret_val += list(it) + ' ' + list(arr[it]) + '\n';
		if (!ret_val.empty()) ret_val.pop_back();
		return ret_val;
	}
	// Destroy array
	if (argc == 3 && !strcmp(argv[2], "destroy")) {
		vars::amap.erase(argv[1]);
		return vars::status;
	}
	// Return array length
	if (argc == 3 && !strcmp(argv[2], "len")) return numtos(arr.size());
	// Return array as list (k)
	if (argc == 3 && !strcmp(argv[2], "keys")) {
		zrc_obj ret_val;
		for (auto const& it : get_sorted_keys())
			ret_val += list(it) + ' ';
		if (!ret_val.empty()) ret_val.pop_back();
		return ret_val;	
	}
	// Return array as list (v)
	if (argc == 3 && !strcmp(argv[2], "vals")) {
		zrc_obj ret_val;
		for (auto const& it : get_sorted_keys())
			ret_val += list(arr[it]) + ' ';
		if (!ret_val.empty()) ret_val.pop_back();
		return ret_val;	
	}
	// Assign to array as numbered list
	if (argc == 4 && !strcmp(argv[2], ":=")) {
		auto wlst = lex(argv[3], SPLIT_WORDS).elems;
		for (size_t i = 0; i < wlst.size(); ++i)
			arr[numtos(i)] = wlst[i];
		return vars::status;
	}
	// Assign to array as key/value pair
	if (argc == 4 && !strcmp(argv[2], "=")) {
		auto wlst = lex(argv[3], SPLIT_WORDS).elems;
		if (wlst.size() % 2 != 0) SYNTAX_ERROR
		for (size_t i = 0; i < wlst.size()-1; i += 2)
			arr[wlst[i]] = wlst[i+1];
		return vars::status;
	}
	// Remove keys from array
	if (argc >= 4 && !strcmp(argv[2], "-=")) {
		for (int i = 3; i < argc; ++i)
			arr.erase(argv[i]);
		return vars::status;
	}
	SYNTAX_ERROR
END


// Tcl-style lists
COMMAND(list, new <w1> <w2> ... \n
              len <l> \n
              <i> <l> \n
              <i> = <val> <l> \n
              <i> += <val> <l> \n
              += <val> <l>)

	if (argc < 3) SYNTAX_ERROR
	
	// Create new list
	if (!strcmp(argv[1], "new")) return list(argc-2, argv+2);
	
	auto wlst = lex(argv[argc-1], SPLIT_WORDS).elems;
	
	// Append element
	if (argc == 3 && !strcmp(argv[1], "+=")) { wlst.push_back(argv[2]); return list(wlst); }
	// Get list length
	if (argc == 3 && !strcmp(argv[1], "len")) return numtos(wlst.size());
	
	auto i = expr(argv[1]);
	if (isnan(i) || i < 0 || i >= wlst.size())
		SYNTAX_ERROR
	
	// Get element at index
	if (argc == 3) return wlst[i];
	// Set element at inde
	if (argc == 5 && !strcmp(argv[2], "=")) { wlst[i] = argv[3]; return list(wlst); }
	// Insert element at index
	if (argc == 5 && !strcmp(argv[2], "+=")) { wlst.insert(wlst.begin()+i, argv[3]); return list(wlst); }
	SYNTAX_ERROR
END

/**************************
 *                        *
 * Shell I/O redirections *
 *                        *
 **************************/
#define REDIR(x, ...) COMMAND(x, [<fd>?] <file> <eoe>) return numtos(redir(argc, argv, __VA_ARGS__)) END
// Stdout/other descriptors
REDIR(>   , STDOUT_FILENO, DO_CLOBBER | OVERWR | OPTFD_Y)
REDIR(>>  , STDOUT_FILENO, DO_CLOBBER | APPEND | OPTFD_Y)
REDIR(>?  , STDOUT_FILENO, NO_CLOBBER | OVERWR | OPTFD_Y)
REDIR(>>? , STDOUT_FILENO, NO_CLOBBER | APPEND | OPTFD_Y)
// Stderr
REDIR(^   , STDERR_FILENO, DO_CLOBBER | OVERWR | OPTFD_N)
REDIR(^^  , STDERR_FILENO, DO_CLOBBER | APPEND | OPTFD_N)
REDIR(^?  , STDERR_FILENO, NO_CLOBBER | OVERWR | OPTFD_N)
REDIR(^^? , STDERR_FILENO, NO_CLOBBER | APPEND | OPTFD_N)
// Stdin
REDIR(<   ,  STDIN_FILENO, DO_CLOBBER | READFL | OPTFD_N)

// Duplicate fds
COMMAND(>&, [<fd1>] <fd2> <eoe>)
	if (argc < 3) SYNTAX_ERROR
	
	auto fd1 = stonum(argv[1]), fd2 = stonum(argv[2]);

	bool is_valid = good_fd(fd1);
	if (isnan(fd1) || fd1 < 0 || fd1 > FD_MAX) {
		std::cerr << "error: Bad file descriptor " << fd1 << '\n';
		return "2";
	}
	if (!isnan(fd2)) {
		if (argc < 4) SYNTAX_ERROR
		--argc, ++argv;
	} else
		fd2 = fd1, fd1 = STDOUT_FILENO;
	if (fd2 < 0 || fd2 > FD_MAX || !good_fd(fd2)) {
		std::cerr << "error: Bad file descriptor " << fd2 << '\n';
		return "3";
	}
	new_fd fd(fd1);
	dup2(fd2, fd1);
	eoe(argc, argv, 2);
	dup2(fd, fd1);
	if (!is_valid)
		close(fd1);
END

// Close fds
COMMAND(>&-, [<fd>] <eoe>)
	if (argc < 2) SYNTAX_ERROR

	auto fd = stonum(argv[1]);
	if (!isnan(fd)) {
		if (argc < 3) SYNTAX_ERROR
		--argc, ++argv;
	} else
		fd = STDOUT_FILENO;
	bool is_valid = good_fd(fd);
	if (fd < 0 || fd > FD_MAX || !is_valid) {
		std::cerr << "error: Bad file descriptor " << fd << '\n';
		return "2";
	}

	new_fd nfd(fd);
	close(fd);
	eoe(argc, argv, 1);
	if (is_valid)
		dup2(nfd, fd);
END

};
