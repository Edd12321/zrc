#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <limits>
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

// Easier command declararions
#define CMD_TBL std::unordered_map<std::string, std::function<zrc_obj(int, char**)> >
#define COMMAND(x) { #x,  [](int argc, char *argv[]) -> zrc_obj {
#define END ;return vars::status;} },

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

bool in_loop;
bool in_swit;
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

// Dir stack
std::stack<std::string> pstack;

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

/*****************
 *               *
 * Command table *
 *               *
 *****************/
CMD_TBL functions, builtins = {

// Commands that only work in certain contexts
#define CTRLFLOW_HELPER(x, y, z)                   \
  COMMAND(y)                                       \
    if (!in_##x) {                                 \
      std::cerr << "can't " #y ", not in " #x "\n";\
      return vars::status;                         \
    }                                              \
    z;                                             \
    throw y##_handler();                           \
  END
CTRLFLOW_HELPER(swit, fallthrough,)
CTRLFLOW_HELPER(loop, break,)
CTRLFLOW_HELPER(loop, continue,)
CTRLFLOW_HELPER(func, return,
	if (argc > 1) {
		auto help = "[<val>]";
		if (argc > 2) SYNTAX_ERROR
		return std::string(argv[1]);
	}
)

// Unless and while use a similar command.
#define WHILE_HELPER(x)             \
  COMMAND(x)                        \
    auto help = "<expr> <eoe>";     \
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
COMMAND(subst)  return subst(concat(argc, argv, 1))         END
// Evaluate arguments
COMMAND(eval)   return eval(concat(argc, argv, 1))          END
// Evaluate as an expression
COMMAND(expr)   return numtos(expr(concat(argc, argv, 1)))  END
// Concatenate arguments
COMMAND(concat) return concat(argc, argv, 1)                END
// Replace currently running process
COMMAND(exec)   if (argc > 1) execvp(*++argv, argv)         END
// Wait for child processes to finish execution
COMMAND(wait)   while (wait(NULL) > 0)                      END
// Source a script
COMMAND(.)      source(concat(argc, argv, 1));              END

// Prioritise builtins
COMMAND(builtin)
	auto help = "<arg1> <arg2>...";
	if (argc < 2) SYNTAX_ERROR
	--argc, ++argv;
	if (builtins.find(*argv) != builtins.end())
		builtins.at(*argv)(argc, argv);
	else
		exec(argc, argv)
END

// Add/remove a new function
COMMAND(fn)
	auto help = "<name> [<w1> <w2>...]";
	if (argc >= 3) {
		// Function body
		auto b = concat(argc, argv, 2);
		functions[argv[1]] = [=](int argc, char *argv[])
		{
			zrc_obj argc_old = vars::argc;
			zrc_arr argv_old = vars::argv;
			vars::argv = copy_argv(argc, argv);
			vars::argc = numtos(argc);
			block_handler fh(&in_func);
			try { eval(b); } catch (return_handler ex) {}
			vars::argc = argc_old;
			vars::argv = argv_old;

			// Don't forget
			return vars::status;
		};
	} else if (argc == 2) {
		functions.erase(argv[1]);
	} else SYNTAX_ERROR
		
END

// List all commands
COMMAND(help)
	for (auto it = builtins.begin(); it != builtins.end(); ++it) {
		std::cout << it->first;
		if (std::next(it) != builtins.end())
			std::cout << ' ';
	}
	std::cout << '\n';
END

// If/else command
COMMAND(if)
	auto help = "<expr> <eval> [else <eoe>]";
	if (argc < 3) SYNTAX_ERROR
	if (expr(argv[1]))
		eval(argv[2]);
	else if (argc > 3) {
		if (argc == 4 || strcmp(argv[3], "else")) SYNTAX_ERROR
		eoe(argc, argv, 4);
	}
END

// Unless command
COMMAND(unless)
	auto help = "<expr> <eoe>";
	if (argc < 3) SYNTAX_ERROR
		unless (expr(argv[1]))
			eoe(argc, argv, 2)
END

// For loops
COMMAND(for)
	auto help = "<eval> <expr> <eval> <eoe>";
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
COMMAND(do)
	auto help = "<eoe> while|until <expr>...";
	if (argc < 4) SYNTAX_ERROR
	bool w = !strcmp(argv[argc-1], "while");
	bool u = !strcmp(argv[argc-1], "until");
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

// Switch instruction
COMMAND(switch)
	// TODO
END

// Negation
COMMAND(!)
	eoe(argc, argv, 1);
	return numtos(!(stonum(vars::status)))
END

// Subshell
COMMAND(@)
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
COMMAND(fork)
	return numtos(fork())
END

// Get pid of self or job
COMMAND(pid)
	return numtos(getpid())
END

// Read from stdin
COMMAND(read)
	optind = 0;
	int opt;

	auto help = "[-d <delim>|-n <nchars>] [-p <prompt>] [<var1> <var2>...]";

	std::string d = "\n", prompt;
	int n = -1;
	bool nflag = false, dflag = false, valid_return = true;

	while ((opt = getopt(argc, argv, "d:n:p:")) != -1) {
		switch (opt) {
			case 'd':
				if (nflag) SYNTAX_ERROR
				d = optarg, dflag = true;
				break;
			
			case 'n':
				if (dflag) SYNTAX_ERROR
				n = expr(optarg);
				if (isnan(n)) SYNTAX_ERROR
				nflag = true;
				break;

			case 'p':
				prompt = optarg;
				break;

			case '?':
				SYNTAX_ERROR
		}
	}

	std::cerr << prompt;
	auto readstr = [&]()
	{
		valid_return &= !feof(stdin);

		std::string str;
		char ch;
		while (std::cin >> std::noskipws >> ch) {
			if (nflag && ch == n)
				break;
			else if (d.find(ch) != std::string::npos)
				break;
			str += ch;
		}
		return str;
	};

	if (optind >= argc)
		std::cout << readstr() << '\n';
	else for (; optind < argc; ++optind)
		setvar(argv[optind], readstr());

	return numtos(valid_return);
END

// Aliases
COMMAND(alias)
	auto help = "[<name> < <w1> <w2>...>]";
	if (argc == 1)
		for (auto& it : kv_alias)
			std::cout << "alias " << list(it.first) << ' ' << list(it.second) << '\n';
	else if (argc >= 3)
		kv_alias[argv[1]] = concat(argc, argv, 2);
	else SYNTAX_ERROR
END

COMMAND(unalias)
	auto help = "<a1> <a2>...";
	if (argc < 2) SYNTAX_ERROR
	for (int i = 1; i < argc; ++i)
		kv_alias.erase(argv[i]);
END

// Key bindings
COMMAND(bindkey)
	auto help = "[-c] [<seq> < <w1> <w2>...>]";
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
	
COMMAND(unbindkey)
	auto help = "<b1> <b2>...";
	if (argc < 2) SYNTAX_ERROR;
	for (int i = 1; i < argc; ++i)
		kv_bindkey.erase(argv[i]);
END

// Change directory
COMMAND(cd)
	struct stat sb;
	auto help = "[<dir>]";

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
COMMAND(echo)
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
COMMAND(set)
	auto help = "< <var> [bin-op]= <val> >...";
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
COMMAND(unset)
	auto help = "<var1> <var2>...";
	if (argc < 2) SYNTAX_ERROR
	for (int i = 1; i < argc; ++i)
		unsetvar(argv[i])
END

// Export one or more variables
COMMAND(export)
	optind = 0;
	int opt;

	auto help = "[-n] < <var1> <var2>...>";
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

// Increment a variable
COMMAND(inc)
	auto help = "<var> [<amount>]";
	if (argc < 2 || argc > 3) SYNTAX_ERROR
	std::string val = (argc == 2) ? "1" : argv[2];
	return numtos(setvar(argv[1], numtos(expr(getvar(argv[1]))+expr(val))))
END

// PHP chr/ord
COMMAND(chr)
	auto help = "<expr1> <expr2>...";
	if (argc != 2) SYNTAX_ERROR
	std::string ret;
	auto e = expr(concat(argc, argv, 1));
	if (isnan(e))
		ret = "error";
	else
		ret += (char)e;
	return ret
END

COMMAND(ord)
	auto help = "<c>";
	if (argc != 2) SYNTAX_ERROR
	return std::to_string(argv[1][0])
END

// Lexical scoping
COMMAND(let)
	auto help = "<var-list> <eoe>";
	if (argc < 3) SYNTAX_ERROR

	auto wlst = lex(argv[1], SPLIT_WORDS).elems;
	
	bool ret = false, brk = false, con = false, fal = false;
	std::unordered_map<std::string, zrc_obj> vmap;
	std::unordered_map<std::string, zrc_arr> amap;

	for (auto const& it : wlst) {
		if (vars::amap.find(it) != vars::amap.end())
			amap[it] = vars::amap[it];
		else if (vars::vmap.find(it) != vars::vmap.end())
			vmap[it] = vars::vmap[it];
		else
			vmap[it] = "";
	}

	try {
		eoe(argc, argv, 2);
	} catch (return_handler ex)      { ret = true; }
	  catch (break_handler ex)       { brk = true; }
	  catch (continue_handler ex)    { con = true; }
	  catch (fallthrough_handler ex) { fal = true; }
	
	for (auto const& it : amap)
		vars::amap[it.first] = it.second;
	for (auto const& it : vmap)
		if (it.first.empty())
			vars::vmap.erase(it.first);
		else
			vars::vmap[it.first] = it.second;
	
	if (ret) throw return_handler();
	if (brk) throw break_handler();
	if (con) throw continue_handler();
	if (fal) throw fallthrough_handler();
END

// Close shell
COMMAND(exit)
	auto help = "[<val>]";
	if (argc > 2) SYNTAX_ERROR
	if (argc < 2) exit(expr(vars::status));
	else exit(atoi(argv[1]))
END

// Directory stack
COMMAND(pushd)
	auto help = "[<dir>]";
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

COMMAND(popd)
	if (pstack.empty()) {
		std::cerr << "Directory stack empty\n";
		return "1";
	}
	pstack.pop();
	prints(pstack)
END

// Increase memory amount
COMMAND(rlimit)
#define RLIMIT_EXP "BKMGTPEZYg"
	auto help = "<n>" RLIMIT_EXP;
	auto cptr = RLIMIT_EXP;
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
COMMAND(str)
	auto help = "<s> > | >= | == | != | <=> | <= | < <p>"
	"\n                  <s> len"
	"\n                  <s> <ind>"
	"\n                  <s> + <ptr>"
	"\n                  <s> <r1> <r2>"
	"\n                  <s> <ind> = <c>"
	"\n                  <s> - <ind1> <ind2>...";

#define STROP(x, op) if (argc == 4 && !strcmp(argv[2], #x)) return numtos(strcmp(argv[1], argv[3]) op);
	STROP(>, > 1) STROP(==, == 0) STROP(<=, <= 0)
	STROP(<, < 0) STROP(!=, != 0) STROP(>=, >= 0)
	STROP(<=>,)

	if (argc == 3 && !strcmp(argv[2], "len")) return numtos(strlen(argv[1]));
	if (argc == 3) {
		zrc_num i = expr(argv[2]);
		if (!isnan(i) && i >= 0 && i < strlen(argv[1]))
			return std::string(1, argv[1][size_t(i)]);
		else SYNTAX_ERROR
	}
	if (argc == 4 && !strcmp(argv[2], "+")) {
		zrc_num i = expr(argv[3]);
		if (!isnan(i) && i >= 0 && i <= strlen(argv[1]))
			return std::string(argv[1]+size_t(i));
		else SYNTAX_ERROR
	}
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
COMMAND(arr)
	auto help = "<a> := <list>"
	"\n                  <a> = <even-list>"
	"\n                  <a> -= <elem1> <elem2>..."
	"\n                  <a> len"
	"\n                  <a> keys"
	"\n                  <a> destroy";

	if (argc < 2) SYNTAX_ERROR
	auto& arr = vars::amap[argv[1]];

	if (argc == 3 && !strcmp(argv[2], "destroy")) {
		vars::amap.erase(argv[1]);
		return vars::status;
	}
	if (argc == 3 && !strcmp(argv[2], "len")) return numtos(arr.size());
	if (argc == 4 && !strcmp(argv[2], ":=")) {
		auto wlst = lex(argv[3], SPLIT_WORDS).elems;
		for (size_t i = 0; i < wlst.size(); ++i)
			arr[numtos(i)] = wlst[i];
		return vars::status;
	}
	if (argc == 4 && !strcmp(argv[2], "=")) {
		auto wlst = lex(argv[3], SPLIT_WORDS).elems;
		if (wlst.size() % 2 != 0) SYNTAX_ERROR
		for (size_t i = 0; i < wlst.size()-1; i += 2)
			arr[wlst[i]] = wlst[i+1];
		return vars::status;
	}
	if (argc >= 4 && !strcmp(argv[2], "-=")) {
		for (int i = 3; i < argc; ++i)
			arr.erase(argv[i]);
		return vars::status;
	}
	SYNTAX_ERROR
END


// Tcl-style lists
COMMAND(list)
	auto help = "create <w1> <w2> ..."
	"\n                  len <l>"
	"\n                  <i> <l>"
	"\n                  <i> = <val> <l>"
	"\n                  <i> += <val> <l>"
	"\n                  += <val> <l>";

	if (argc < 3) SYNTAX_ERROR
	
	// Create new list
	if (!strcmp(argv[1], "create")) return list(argc-2, argv+2);
	
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
#define REDIR(x, ...) COMMAND(x) return numtos(redir(argc, argv, __VA_ARGS__)) END
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
COMMAND(>&)
	auto help = "[<fd1>] <fd2> <eoe>";
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
COMMAND(>&-)
	auto help = "[<fd>] <eoe>";
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
