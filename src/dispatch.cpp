#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <glob.h>

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

// To reuse getopt inside a builtin, without affecting external state
class getopt_guard {
private:
	int saved_optind = 1, saved_opterr = 1, saved_optopt = 0;
	char *saved_optarg = nullptr;
#ifdef HAVE_OPTRESET
	int saved_optreset = 0;
#endif
public:
	getopt_guard(int new_opterr = 1) {
		// Get all the original ones
		saved_optind = optind;
		saved_opterr = opterr;
		saved_optopt = optopt;
		saved_optarg = optarg;
#ifdef HAVE_OPTRESET
		saved_optreset = optreset;
		optreset = 1;
#endif

#ifdef __GLIBC__
		optind = 0;
#else
		optind = 1;
#endif
		optarg = nullptr;
		opterr = new_opterr;
		optopt = 0;
	}

	~getopt_guard() {
		optind = saved_optind;
		opterr = saved_opterr;
		optopt = saved_optopt;
		optarg = saved_optarg;
#ifdef HAVE_OPTRESET
		optreset = saved_optreset;
#endif
	}
};

std::unordered_map<std::string, std::string> help_strs;
#define COMMAND(x, help_str) { (help_strs[#x] = #help_str, #x),  [](int argc, char *argv[]) -> zrc_obj {\
	auto const& help = #help_str; \

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
#define EXCEPTION_CLASS(x)                      \
  class x##_handler : std::exception {          \
  public:                                       \
    virtual const char *what() const throw () { \
      return "Caught " #x;                      \
    }                                           \
  };
EXCEPTION_CLASS(fallthrough)
EXCEPTION_CLASS(break)
EXCEPTION_CLASS(continue)
EXCEPTION_CLASS(return)
EXCEPTION_CLASS(regex)

bool in_loop;
bool in_switch;
bool in_func;

volatile sig_atomic_t chld_notif;

class block_handler {
private:
	bool ok = false, *ref = &in_loop;
public:
	block_handler(bool *ref) {
		this->ref = ref;
		if (!*ref)
			ok = true;
		*ref = true;
	}
	~block_handler() {
		if (ok)
			*ref = false;
	}
};

static inline std::string concat(int argc, char *argv[], int i) {
	std::string ret;
	for (; i < argc; ++i) {
		ret += argv[i];
		if (i < argc-1)
			ret += ' ';
	}
	return ret;
}

static inline zrc_num expr(std::string const& str) {
	return expr(str.c_str());
}

// Eval-or-exec behaviour on arguments
static inline void eoe(int argc, char *argv[], int i) {
	if (argc == i+1) 
		eval(argv[i]);
	else
		exec(argc-i, argv+i);
}

// Match ERE
bool regex_match(std::string const& txt, std::string const& reg, int cflags = REG_NOSUB | REG_EXTENDED) {
	regex_t regex;

	if (regcomp(&regex, reg.c_str(), cflags))
		throw regex_handler();
	auto ret = regexec(&regex, txt.c_str(), 0, nullptr, 0);
	regfree(&regex);
	return !ret;
}

// Dir stack
std::stack<std::string> pstack;
// Path hashing
std::unordered_map<std::string, std::string> hctable;

static inline void prints(std::stack<std::string> sp) {
	if (!sp.empty())
		chdir(sp.top().c_str());
	while (!sp.empty()) {
		std::cout << sp.top() << ' ';
		sp.pop();
	}
	std::cout << '\n';
}

// User functions
struct zrc_frame {
	bool is_fun;
	std::string fun;
	bool is_script;
	std::string script;
};
std::vector<zrc_frame> callstack;

struct zrc_fun {
	std::string body;

	zrc_fun() = default;
	zrc_fun(std::string const& b) : body(b) {}

	inline zrc_obj operator()(int argc, char *argv[]) const {
		zrc_obj zargc_old = vars::argc; int argc_old = ::argc;
		zrc_arr zargv_old = vars::argv; char **argv_old = ::argv;
		vars::argv = copy_argv(argc, argv); ::argv = argv;
		vars::argc = numtos(argc); ::argc = argc;
		// Break, continue and fallthrough dont work inside functions
		bool in_switch = ::in_switch, in_loop = ::in_loop;
		::in_switch = ::in_loop = false;
		// For the caller command
		bool is_fun_old = is_fun;
		std::string fun_name_old = fun_name;
		callstack.push_back({is_fun, fun_name, is_script, script_name});
		is_fun = true; fun_name = argv[0];

		SCOPE_EXIT {
			vars::argc = zargc_old; ::argc = argc_old;
			vars::argv = zargv_old; ::argv = argv_old;
			
			::in_switch = in_switch, ::in_loop = in_loop;

			is_fun = is_fun_old; fun_name = fun_name_old;
			callstack.pop_back();
		};
		block_handler fh(&in_func);
		try { eval(body); } catch (return_handler ex) {}

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
#define WHILE_HELPER(x)               \
  COMMAND(x, <expr> <eoe>)            \
    if(argc < 3) SYNTAX_ERROR         \
    block_handler lh(&in_loop);       \
		x (expr(argv[1])) try { eoe(argc, argv, 2); } catch (break_handler ex) { break; } catch (continue_handler ex) { continue; } \
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
COMMAND(wait,                 )  while (wait(nullptr) > 0)                   END
// Source a script
COMMAND(source, [<w1> <w2>...])  source(concat(argc, argv, 1))               END
// Disable internal hash table
COMMAND(unhash,               )  hctable.clear()                             END
// Display internal job table
COMMAND(jobs,                 )  show_jobs()                                 END

// Bash-style getopts
COMMAND(getopts, <opt> <var>)
	if (argc != 3) SYNTAX_ERROR

	zrc_obj s_opterr = vars::opterr;
	if (s_opterr.empty())
		vars::opterr = std::to_string(opterr);
	else opterr = stonum(s_opterr);
	
	zrc_obj s_optind = vars::optind;
	if (s_optind.empty())
		vars::optind = std::to_string(optind);
	else optind = stonum(s_optind);
	
	int opt = getopt(::argc, ::argv, argv[1]);
	setvar(argv[2], opt != -1 ? std::string(1, opt) : "");
	vars::optarg = optarg ? optarg : "";
	vars::optind = std::to_string(optind);

	return opt != -1 ? std::to_string(opt) : "false";
END

// Foreground/background tasks
#define FGBG(z, x, y)                            \
  COMMAND(x, <n>)                                \
    if (argc != 2) SYNTAX_ERROR                  \
    auto n = expr(argv[1]);                      \
    if (isnan(n)) SYNTAX_ERROR                   \
	if (!interactive_sesh) {                     \
	  std::cerr << "Can't FG/BG in a script\n";  \
	  return "2";                                \
	}                                            \
    auto p = pid2job(n);                         \
    if (p < 0) {                                 \
      std::cerr << "Bad PID\n";                  \
      return "3";                                \
    }                                            \
    kill(-getpgid(n), SIGCONT);                  \
    jobstate(p, z); y;                           \
    if (getpid() == tty_pid && interactive_sesh) \
       tcsetpgrp(tty_fd, tty_pid)                \
  END
FGBG(1, fg, tcsetpgrp(tty_fd, getpgid(n)); reaper(n, WUNTRACED))
FGBG(0, bg,)

// JID to PID
COMMAND(job, <n>)
	if (argc != 2) SYNTAX_ERROR
	auto x = expr(argv[1]);
	if (isnan(x)) SYNTAX_ERROR
	if (!interactive_sesh) {
		std::cerr << "Can't get JID in a script\n";
		return "-1";
	}
	auto pid = job2pid(x);
	if (pid < 0) {
		std::cerr << "Expected a valid JID" << std::endl;
		return "-2";
	}
	return numtos(pid)
END

// Remove jobs from table
COMMAND(disown, <n>)
	if (argc != 2) SYNTAX_ERROR
	auto x = expr(argv[1]);
	if (isnan(x)) SYNTAX_ERROR
	if (!interactive_sesh) {
		std::cerr << "Can't disown a job in a script\n";
		return "2";
	}
	disown_job(x)
END

// Refresh internal hash table
COMMAND(rehash,)
	for (auto const& file : pathwalk()) {
		std::string path = basename(file.first), full = file.second;
		hctable[path] = full;
		std::cout << "Added " << path << " (" << full << ")\n";
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

// Add/remove a new function + do signal trapping
COMMAND(fn, <name> [<w1> <w2>...])
	if (argc >= 3) {
		// Function body
		auto b = concat(argc, argv, 2);
		functions[argv[1]] = zrc_fun(concat(argc, argv, 2));
		if (txt2sig.find(argv[1]) != txt2sig.end()) {
			std::string sig = argv[1]+3;
			if (sig == "exit" || sig == "chld"
			|| interactive_sesh && (sig == "int" || sig == "tstp" || sig == "hup"))
				return vars::status; // this gets set in main()
			signal(txt2sig.at(argv[1]), [](int sig) {
				for (auto const& it : txt2sig) // signal(2) can't capture fun name
					if (it.second == sig)
						run_function(it.first);
			});
		}
	} else if (argc == 2) {
		functions.erase(argv[1]);
		if (txt2sig.find(argv[1]) != txt2sig.end()) {
			std::string sig = argv[1]+3;
			if (sig == "exit" || sig == "chld"
			|| interactive_sesh && (sig == "int" || sig == "tstp" || sig == "hup"))
				return vars::status; // this gets set in main()
			else if (sig == "ttou" && interactive_sesh)
				signal(SIGTTOU, SIG_IGN); // ignore only sometimes
			else if (sig == "ttin" && interactive_sesh)
				signal(SIGTTIN, SIG_IGN); // ditto above
			else signal(txt2sig.at(argv[1]), SIG_DFL);
			return vars::status;
		}
	} else SYNTAX_ERROR	
END

// Lambda functions
COMMAND(apply, <eval> [<w1> <w2>...])
	if (argc < 2) SYNTAX_ERROR
	auto f = zrc_fun(argv[1]);
	char *old_arg = argv[1];
	argv[1] = LAM_STR;
	try { f(argc-1, argv+1); } catch (...) { argv[1] = old_arg; throw; }
	argv[1] = old_arg;
END

// Close shell with message and exit status 1
COMMAND(die, [<w1> <w2>...])
	std::cerr << concat(argc, argv, 1) << std::endl;
	exit(EXIT_FAILURE)
END

// List all commands
COMMAND(help, [<cmd1> <cmd2>...])
	bool handled_args = false;
	for (int i = 1; i < argc; ++i) {
		if (kv_alias.find(argv[i]) != kv_alias.end()) {
			std::cout << "# alias\n";
			std::cout << "alias " << list(argv[i]) << ' ' << list(kv_alias.at(argv[i])) << '\n';
			handled_args = true;
		}
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

	std::cout << ZVERSION << '\n';
	std::vector<std::string> vbuiltins, vfunctions, valiases;
	for (auto const& it : functions) vfunctions.push_back(it.first);
	for (auto const& it : builtins) vbuiltins.push_back(it.first);
	for (auto const& it : kv_alias) valiases.push_back(it.first);
	std::sort(vfunctions.begin(), vfunctions.end());
	std::sort(valiases.begin(), valiases.end());
	std::sort(vbuiltins.begin(), vbuiltins.end());

	std::cout << valiases.size() << " aliases" << (valiases.empty() ? "." : ":\n");
	for (auto const& it : valiases) std::cout << it << ' ';
	std::cout << "\n\n";

	std::cout << vfunctions.size() << " functions" << (vfunctions.empty() ? "." : ":\n");
	for (auto const& it : vfunctions) std::cout << it << ' ';
	std::cout << "\n\n";
	
	std::cout << vbuiltins.size() << " builtins:\n";
	size_t row, col, hc;
	bool newl = false, tty = isatty(STDOUT_FILENO);
	if (tty) {
		line_edit::init_term(row, col);
		hc = col / 2 - 1;
	}
	for (auto& cmd : vbuiltins) {
		cmd += ' ' + help_strs[cmd];
		if (tty) {
			if (cmd.length() > hc)
				cmd = cmd.substr(0, hc - 3) + "...";
			if (cmd.length() < hc)
				cmd += std::string(hc - cmd.length(), ' ');
		}
		std::replace(cmd.begin(), cmd.end(), '\n', '|');
		std::cout << cmd << ((newl || !tty) ? "\n" : " ");
		newl = !newl;
	}
	if (newl) std::cout << '\n';
END

// Command correction
COMMAND(fc, [-e <editor>] [-lnr] [<num>])
	int opt;
	getopt_guard gg;
	bool lflag = false, nflag = false, rflag = false;
	zrc_obj editor = vars::editor;
	if (editor.empty())
		editor = vars::EDITOR;
	if (editor.empty())
		editor = FC_EDITOR;
	while ((opt = getopt(argc, argv, "e:lnr")) != -1) {
		switch (opt) {
			case 'e':
				editor = optarg;
				break;
			case 'l':
				lflag = true;
				break;
			case 'n':
				nflag = true;
				break;
			case 'r':
				rflag = true;
				break;
			case '?': SYNTAX_ERROR
		}
	}
	auto& hist = line_edit::histfile;
	ssize_t num = lflag ? 15 : 1, i, len = hist.size();
	if (optind == argc-1) {
		zrc_num e = expr(argv[optind]);
		if (isnan(e) || e < 0) SYNTAX_ERROR
		num = std::min(len, (ssize_t)e);
	}
	signed char dir;
	std::function<bool(ssize_t)> cond;

	if (rflag)
		dir = -1, i = len-1, cond = [&](ssize_t i) { return i >= len-num; };
	else
		dir = +1, i = len-num, cond = [&](ssize_t i) { return i < len; };

	std::ofstream fout;
	std::string fc_file, fc_name;
	if (!lflag)  {
		char temp[] = FIFO_DIRNAME;
		fc_name = mkdtemp(temp);
		fc_file = fc_name + "/" + FC_FILNAME;
		fout.open(fc_file);
	}
	for (; cond(i); i += dir) {
		if (lflag) {
			if (!nflag)
				std::cout << std::setw(8) << i+1 << ' ';
			std::cout << hist[i] << '\n';
		} else {
			fout << hist[i] << '\n';
		}
	}
	if (!lflag) {
		fout.close();
		invoke_void(exec, {editor.c_str(), fc_file.c_str()});
		source(fc_file);

		unlink(fc_file.c_str());
		rmdir(fc_name.c_str());
	}
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
	for (eval(argv[1]); expr(argv[2]); eval(argv[3]))
		try { eoe(argc, argv, 4); } catch (break_handler ex) { break; } catch (continue_handler ex) { continue; }
END

// Do/while and do/until
COMMAND(do, <eoe> while|until <expr>...)
	if (argc < 4) SYNTAX_ERROR
	bool w = !strcmp(argv[argc-2], "while");
	bool u = !strcmp(argv[argc-2], "until");
	if (!w && !u) SYNTAX_ERROR

	block_handler bh(&in_loop);
	if (w) {
		do {
			try { eoe(argc-2, argv, 1); } catch (break_handler ex) { break; } catch (continue_handler ex) { continue; }
		} while (expr(argv[argc-1]));
	} else {
		do {
			try { eoe(argc-2, argv, 1); } catch (break_handler ex) { break; } catch (continue_handler ex) { continue; }
		} until (expr(argv[argc-1]));
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
	ssize_t i, def = -1;

	struct switch_case {
		enum switch_type {
			CASE, DEFAULT, REGEX
		} type;
		std::string txt;
		std::string block;
	};
	std::vector<switch_case> vec;
	using SW = switch_case::switch_type;

	size_t len = wlst.size();
	// Parse
	for (i = 0; i < len; ++i) {
		std::string conv = wlst[i];
		if (conv == "case" && i+2 < len) {
			vec.push_back({ SW::CASE, wlst[i+1], wlst[i+2] });
			i += 2;
			continue;
		}
		if (conv == "regex" && i+2 < len) {
			vec.push_back({ SW::REGEX, wlst[i+1], wlst[i+2] });
			i += 2;
			continue;
		}
		if (conv == "default" && i+1 < len) {
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
	bool fell = false;
	// Try to evaluate
	for (auto const& it : vec) {
		switch (it.type) {
			case SW::CASE:
				if (fell || txt == it.txt)
					try { return eval(it.block); } catch (fallthrough_handler ex) { fell = true; continue; }
				break;

			case SW::REGEX:
				try {
					if (fell || regex_match(txt, it.txt))
						try { return eval(it.block); } catch (fallthrough_handler ex) { fell = true; continue; }
				} catch (regex_handler ex) {
					std::cerr << "syntax error: Invalid regex " << list(it.txt) << std::endl;
					return "1";
				}
				break;

			case SW::DEFAULT:
				if (fell) {
					def = -1;
					try { return eval(it.block); } catch (fallthrough_handler ex) { fell = true; continue; }
				}
				break;
		}
		fell = false;
	}
	// We made it this far, so nothing matched
	if (def != -1)
		for (i = def; i < vec.size(); ++i)
			try { return eval(vec[i].block); } catch (fallthrough_handler ex) {}
END

// Exceptions
COMMAND(throw, [<w1> <w2>...<wn>])
	throw zrc_obj(concat(argc, argv, 1));
END

COMMAND(try, <eval> catch <name> <eoe>)
	if (argc < 5 || strcmp(argv[2], "catch")) SYNTAX_ERROR
	try {
		eval(argv[1]);
	} catch (zrc_obj const& e) {
		setvar(argv[3], e);
		eoe(argc, argv, 4);
	}
END

// Stack trace
COMMAND(caller, [<expr>])
	zrc_num e = expr(concat(argc, argv, 1));
	if (isnan(e)) SYNTAX_ERROR
	if (e >= 0 && e < callstack.size()) {
		zrc_frame f = callstack[callstack.size()-1-e];
		if (f.is_fun)
			std::cout << "fn " << list(f.fun);
		else std::cout << "repl";
		std::cout << " @ ";
		if (f.is_script)
			std::cout << "script " << f.script;
		else std::cout << "interactive";
		std::cout << '\n';
		return "0";
	}
	return "1";
END

// For-each loop
COMMAND(foreach, <var> <var-list> <eoe>)
	if (argc < 4) SYNTAX_ERROR
	block_handler lh(&in_loop);
	auto vlst = lex(argv[2], SPLIT_WORDS).elems;
	for (auto const& it : vlst) {
		try {
			setvar(argv[1], it);
			eoe(argc, argv, 3);
		} catch (break_handler ex) {
			break;
		} catch (continue_handler ex) {
			continue;
		}
	}
END

// Bash-style select (similar to foreach)
COMMAND(select, <var> <list> <eoe>)
	if (argc < 4) SYNTAX_ERROR
	block_handler lh(&in_loop);
	auto vlst = lex(argv[2], SPLIT_WORDS).elems;
	std::map<std::string, int> ind;
	for (size_t i = 0; i < vlst.size(); ++i) {
		auto evald = list(vlst[i]);
		ind[evald] = i + 1;
		std::cout << i + 1 << ") " << list(vlst[i]) << '\n';
	}
	for (;;) {
		try {
			std::string str;
			invoke_void(builtins.at("read"), {"read", argv[1]});
			auto var = getvar(argv[1]);
			if (ind.find(var) != ind.end())
				vars::reply = std::to_string(ind[var]);
			eoe(argc, argv, 3);
		} catch (break_handler ex) {
			break;
		} catch (continue_handler ex) {
			continue;
		}
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
		reset_sigs();
		close(pd[0]);
		fcntl(pd[1], F_SETFD, O_CLOEXEC);
		SCOPE_EXIT {
			fflush(stdout);
			dup2(pd[1], STDOUT_FILENO);
			close(pd[1]);
			std::cout << vars::status << std::flush;
			_exit(0);
		};
		eoe(argc, argv, 1);
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
	if (interactive_sesh && callstack.empty()) {
		std::cerr << "can't fork from a shell prompt\n";
		return "-1";
	}
	return numtos(fork())
END

// Get pid of self or job
COMMAND(pid,)
	return numtos(getpid())
END

// Read from stdin
COMMAND(read, [-d <delim>|-n <nchars>] [-p <prompt>] [-f <fd>] [<var1> <var2>...])
	std::string delim = vars::ifs;
	if (delim.empty())
		delim = vars::IFS;
	if (delim.empty())
		delim = "\n";
	int status = 2, n = -1, fd = STDIN_FILENO;
	std::string prompt;

	int opt;
	getopt_guard gg;
	while ((opt = getopt(argc, argv, "d:n:p:f:")) != -1) {
		switch (opt) {
			case 'd':
				delim = optarg;
				break;
			case 'n':
				n = expr(optarg);
				break;
			case 'p':
				prompt = optarg;
				break;
			case 'f':
				fd = expr(optarg);
				break;
			case '?':
				SYNTAX_ERROR
		}
	}
	auto read_str = [&]() {
		status = 2;
		if (n < 0) {
			std::string ret_val;
			char c;
			for (;;) {
				ssize_t r = read(fd, &c, 1);
				if (r == 1) {
					status = 0;
					if (strchr(delim.c_str(), (unsigned char)c))
						break;
					ret_val += c;
				} else if (r == 0) {
					break;
				} else{ // r == -1
					if (errno == EINTR) continue;
					perror("read");
					break;
				}
			}
			return ret_val;
		} else {
			if (n == 0)
				return std::string();

			std::string s;
			s.resize(n);
			ssize_t r = read(fd, &s[0], n);
			if (r > 0) {
				s.resize(r);
				status = 0;
			} else {
				s.clear();
				if (r == -1)
					perror("read");
			}
			return s;
		}
	};

	if (optind >= argc)
		std::cout << prompt << read_str() << std::endl;
	else for (; optind < argc; ++optind) {
		std::cout << prompt;
		setvar(argv[optind], read_str());
	}
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
		if (pw)
			chdir(pw->pw_dir);
		else {
			std::cerr << argv[0] << ": could not find home dir!\n";
			return "1";
		}
	} else {
		if (argc != 2) SYNTAX_ERROR
		if (!stat(argv[1], &sb) && S_ISDIR(sb.st_mode))
			chdir(argv[1]);
		else {
			/* empty */ {
				std::istringstream iss{vars::CDPATH};
				std::string tmp;
				while (getline(iss, tmp, ':')) {
					tmp += "/";
					tmp += argv[1];
					if (!stat(tmp.c_str(), &sb) && S_ISDIR(sb.st_mode)) {
						chdir(tmp.c_str());
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
COMMAND(set, < <var> [<expr-binop>|.]= <val> >...)
	if ((argc-1) % 3 != 0) SYNTAX_ERROR
	zrc_obj lret;
	for (int i = 2; i < argc; i += 3) {
		auto len = strlen(argv[i])-1;
		if (argv[i][len] != '=') // All of these must have = at the end.
			SYNTAX_ERROR
		if (argv[i][1] == '\0') {// Plain =
			lret = setvar(argv[i-1], argv[i+1]);
		} else if (len == 1 && argv[i][0] == '.') {
			lret = setvar(argv[i-1], getvar(argv[i-1]) + argv[i+1]);
		} else { // [op]=
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
	int opt;
	getopt_guard gg;
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
	getopt_guard gg;
	char flags[4] = "s";
	int opt, g_flags = GLOB_NOSORT;
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
	if (isnan(e) || e < 0 || e > 255)
		ret = "error";
	else
		ret += static_cast<unsigned char>(e);
	return ret
END

COMMAND(ord, <c>)
	if (argc != 2) SYNTAX_ERROR
	return std::to_string(static_cast<unsigned char>(argv[1][0]))
END

// Lexical scoping
COMMAND(let, <var-list> <eoe>)
	if (argc < 3) SYNTAX_ERROR
	std::unordered_map<std::string, zrc_obj> vmap;
	std::unordered_map<std::string, zrc_arr> amap;
	std::unordered_map<std::string, bool> didnt_exist;
	auto wlst = lex(argv[1], SPLIT_WORDS).elems;
	for (auto const& it : wlst) {
		if (vars::amap.find(it) != vars::amap.end())
			amap[it] = vars::amap[it];
		else if (vars::vmap.find(it) != vars::vmap.end())
			vmap[it] = vars::vmap[it];
		else
			didnt_exist[it] = true;
	}
	SCOPE_EXIT {
		for (auto const& it : amap)
			vars::amap[it.first] = it.second;
		for (auto const& it : vmap) {
			unsetvar(it.first);
			setvar(it.first, it.second);
		}
		for (auto const& it : didnt_exist)
			unsetvar(it.first);
	};
	eoe(argc, argv, 2);
END

// Close shell
COMMAND(exit, [<val>])
	if (argc > 2) SYNTAX_ERROR
	if (argc < 2) exit(expr(vars::status));
	else exit(atoi(argv[1]))
END

// Exit but fancier
COMMAND(logout, [<val>])
	if (::argv[0][0] != '-') {
		std::cerr << argv[0] << ": not a login shell: use `exit'\n";
		return "1";
	}
	builtins.at("exit")(argc, argv)
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
		char *rp = realpath(argv[1], nullptr);
		if (!rp) {
			perror("realpath");
			return "2";
		}
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
COMMAND(rlimit, <n>[BKMGTPEZYg])
	if (argc != 2 || !argv[1][0]) SYNTAX_ERROR
	static const char *prefixes = help + 4;

	size_t len = strlen(argv[1]);
	auto found = (const char*)memchr(prefixes, argv[1][len-1], strlen(prefixes)-1); // get rid of ]

	unsigned long long mul = 1;
	if (found) {
		mul = 1ULL << (10 * (found - prefixes));
		argv[1][len-1] = '\0';
	}
	zrc_num x = expr(argv[1]);
	if (isnan(x) || x < 0) SYNTAX_ERROR
	rlim_t memory = x * mul;
	struct rlimit rlm;

	if (!getrlimit(RLIMIT_STACK, &rlm))
		if (rlm.rlim_cur < memory) {
			rlm.rlim_cur = memory;
			if (setrlimit(RLIMIT_STACK, &rlm)) {
				std::cerr << "setrlimit() failed!\n";
				return "2";
			}
		}
END

// Shift argv to the left
COMMAND(shift, [<n>])
	long howmuch, len = vars::argv.size(), i;
	zrc_num x = 1;
	if (argc > 2) SYNTAX_ERROR
	if (argc == 2 && isnan(x = expr(argv[1]))) SYNTAX_ERROR
	howmuch = x;

	if (howmuch >= len) {
		vars::argv.clear();
		::argv += argc, ::argc = 0;
		return vars::argc = "0";
	}
	::argv += howmuch, ::argc -= howmuch;
	for (i = 0; i < len - howmuch; ++i) {
		setvar("argv " + std::to_string(i),
		getvar("argv " + std::to_string(i + howmuch)));
		unsetvar("argv " + std::to_string(i + howmuch));
	}
	return vars::argc = std::to_string(len - howmuch);
END

// regex
COMMAND(regexp, <reg> <txt> <var1> <var2...>)
	if (argc < 4) SYNTAX_ERROR
	regex_t rexp;
	const char *pattern = argv[1], *txt = argv[2], *it = txt;
	regmatch_t pmatch;
	int k = 3;
	if (regcomp(&rexp, pattern, REG_EXTENDED))
		SYNTAX_ERROR
	std::string ret_val = "2", match;
	while (!regexec(&rexp, it, 1, &pmatch, 0)) {
		ret_val = "0";
		if (k >= argc) {
			regfree(&rexp);
			return vars::status;
		}
		int start = pmatch.rm_so, end = pmatch.rm_eo, len = end - start;
		if (end > start)
			match = std::string(it + start, len);
		else
			match.clear();
		setvar(argv[k++], match);
		if (end == start) {
			if (it[start] == '\0')
				break;
			++it;
		} else it += end;
	}
	regfree(&rexp);
	return ret_val;
END

/****************************************
 *                                      *
 * Special data type manipulation cmds  *
 *                                      *
 ****************************************/

// Strings
COMMAND(str, <s> > | >= | == | != | =~ | <\x3d> | <= | < <p> \n
             <s> len \n
             <s> <ind> \n
             <s> + <ptr> \n
             <s> <r1> <r2> \n
             <s> <ind> = <c> \n
             <s> - <ind1> <ind2>...)

#define STROP(x, op) if (argc == 4 && !strcmp(argv[2], #x)) return numtos(strcmp(argv[1], argv[3]) op);
	// String comparisons
	STROP(>, > 0) STROP(==, == 0) STROP(<=, <= 0)
	STROP(<, < 0) STROP(!=, != 0) STROP(>=, >= 0)
	STROP(<\x3d>,) // <=>, to make clang++ shut up
	
	// Regex comparison
	if (argc == 4 && !strcmp(argv[2], "=~")) {
		regex_t rexp;
		const char *pattern = argv[3], *txt = argv[1];
		regmatch_t pmatch;
		if (regcomp(&rexp, pattern, REG_EXTENDED))
			SYNTAX_ERROR
		auto ret_val = !regexec(&rexp, txt, 1, &pmatch, 0) ? "true" : "false";
		regfree(&rexp);
		return ret_val;
	}

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
              += <val> <l> \n
              map <lambda> <l> \n
              filter <lambda> <l> \n
              reduce <lambda> <l>)

	if (argc < 3) SYNTAX_ERROR
	
	// Create new list
	if (!strcmp(argv[1], "new")) return list(argc-2, argv+2);
	
	auto wlst = lex(argv[argc-1], SPLIT_WORDS).elems;
	
	// Append element
	if (argc == 4 && !strcmp(argv[1], "+=")) { wlst.push_back(argv[2]); return list(wlst); }
	// Get list length
	if (argc == 3 && !strcmp(argv[1], "len")) return numtos(wlst.size());
	
	// Lambda stuf
	if (argc == 4 && !strcmp(argv[1], "map")) {
		zrc_fun f(argv[2]);
		for (auto& it : wlst) {
			// Very very careful
			std::string str = it;
			it = invoke(f, {LAM_STR, str.c_str()});
		}
		return list(wlst);
	}
	if (argc == 4 && !strcmp(argv[1], "filter")) {
		zrc_fun f(argv[2]);
		for (size_t i = 0; i < wlst.size(); ++i) {
			std::string str = wlst[i];
			auto e = expr(invoke(f, {LAM_STR, str.c_str()}));
			if (isnan(e) || !e)
				wlst.erase(wlst.begin() + i--);
		}
		return list(wlst);
	}
	if (argc == 4 && !strcmp(argv[1], "reduce")) {
		while (wlst.size() < 2) {
			token tok;
			tok.parts.push_back({substit::type::PLAIN_TEXT, ""});
			wlst.push_back(tok);
		}
		zrc_fun f(argv[2]);
		std::string s1 = wlst[0], s2 = wlst[1];
		zrc_obj ret_val = invoke(f, {LAM_STR, s1.c_str(), s2.c_str()});
		for (size_t i = 2; i < wlst.size(); ++i) {
			std::string str = wlst[i];
			ret_val = invoke(f, {LAM_STR, ret_val.c_str(), str.c_str()});
		}
		return ret_val;
	}

	auto i = expr(argv[1]);
	if (isnan(i) || i < 0 || i >= wlst.size())
		SYNTAX_ERROR

	// Get element at index
	if (argc == 3) return wlst[i];
	// Set element at index
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
#define REDIR(x, ...) COMMAND(x, [<fd>?] <file> <eoe>) return redir(argc, argv, __VA_ARGS__) END
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
	SCOPE_EXIT {
		dup2(fd, fd1);
		if (!is_valid)
			close(fd1);
	};
	eoe(argc, argv, 2);
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
	SCOPE_EXIT {
		if (is_valid)
			dup2(nfd, fd);
	};
	eoe(argc, argv, 1);
END

};
