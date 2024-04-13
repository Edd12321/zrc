// Macro for laziness
#define FOUND_FN(X) (funcs.find(argv[X]) != funcs.end())
#define OK(X) expr(X)
#define de(X)   { #X, zrc_builtin_##X }
#define ce(X,Y) { #X, zrc_builtin_##Y }, { #Y, zrc_builtin_##Y }

#define unless(X) if (!(X))
#define until(X) while (!(X))

#define Command(X) \
	static inline std::string \
	zrc_builtin_##X(int argc, char *argv[])

#define NoReturn return ret_val
#define syntax_error(X) {\
	std::cerr << errmsg << "(" << argv[0] << ") " << X << '\n';\
	return ZRC_ERRONE_RETURN;\
}
#define other_error(X, C) do {\
	std::cerr << argv[0] << ": " << X << "!\n";\
	return #C;\
} while(0)

DispatchTable<FunctionName, CodeBlock> funcs;
DispatchTable<AliasName, WordList> aliases;

#define SIGEXIT 0
const DispatchTable<FunctionName, int> txt2sig = {
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

#define ExceptionClass(X) \
  class Zrc##X##Handler   \
  {                       \
  public:                 \
    virtual const char *  \
    what() const throw()  \
    {                     \
      return "Caught " #X;\
    }                     \
  }
ExceptionClass(Return);
ExceptionClass(Break);
ExceptionClass(Continue);

/****
 * Check if we can break/continue
 ****/
bool in_loop = false;
bool in_func = false;

class BlockHandler
{
private:
	bool ok = false, *ref = &in_loop;
public:
	BlockHandler(bool *ref)
	{
		this->ref = ref;
		if (!*ref)
			ok = true;
		*ref = true;
	}
	~BlockHandler()
	{
		if (ok)
			*ref = false;
	}
};

/** Converts a number to a std::string object.
 *
 * @param  void
 * @return void
 */
Inline std::string
ldtos(long double x)
{
	std::string str;
	size_t len;
	/* empty */ {
		std::stringstream ss;
		ss << std::fixed << x;
		str = ss.str();
	}
	if (str.find('.') != std::string::npos) {
		while (!str.empty() && str.back() == '0')
			str.pop_back();
		if (!str.empty() && str.back() == '.')
			str.pop_back();
	}
	return str;
}

/** Print directory stack contents.
 *
 * @param {stack<Path>}sp
 * @return void
 */
static inline void
prints(std::stack<Path> sp)
{
	if (!sp.empty())
		chdir(sp.top().c_str());
	while (!sp.empty()) {
		std::cout << sp.top() << ' ';
		sp.pop();
	}
	std::cout << std::endl;
}

#define EXIT_SESSION \
	ret_val = getvar($RETURN);\
	exit((is_number(ret_val) && !ret_val.empty())\
			  ? expr(ret_val)\
			  : EXIT_SUCCESS)\


/** Closes the Zrc session **/
Command(exit)   { EXIT_SESSION; }
/** Displays a job table **/
Command(jobs)   { jobs(); NoReturn; }
/** Waits for child processes to finish **/
Command(wait)   { while (wait(NULL) > 0) ; NoReturn; }
/** Closes with an error message **/
Command(die)    { die(argv[1]); return ZRC_ERRONE_RETURN; }
/** Evalues its arguments as a script **/
Command(eval)   { return eval(combine(argc, argv, 1)); }
/** Variable type commands **/
Command(array)  { return array(argc, argv); }
Command(string) { return string(argc, argv); }
/** Merge lists **/
Command(concat) { return combine(argc, argv, 1); }
/** Clear screen **/
Command(clear)  { std::cout << CLRSCR; NoReturn; }
/** Evaluates an arithmetic expression **/
Command(expr) { return ldtos(expr(combine(argc, argv, 1))); }

/** Executes a block if an expression evaluates non-zero **/
Command(if) {
	constexpr char se[] = "<expr> <block> [else < <arg1> <arg2>... >|<block>]";	
	if (argc < 3)
		syntax_error(se);

	if (OK(argv[1])) {
		eval(argv[2]);
	} else if (argc > 3 && !strcmp(argv[3], "else")) {
		if (argc == 4)
			syntax_error(se);
		if (argc == 5)
			eval(argv[4]);
		else
			exec(argc-4, argv+4);
	}
	NoReturn;
}

/** Executes a block if an expression evaluates zero **/
Command(unless) {
	if (argc != 3)
		syntax_error("<expr> <block>");
	unless(OK(argv[1]))
		eval(argv[2]);
	NoReturn;
}

/** Exception handling **/
Command(break) {
	if (!in_loop)
		other_error("Cannot break, not in a loop", 1);
	throw ZrcBreakHandler();
}
Command(continue) {
	if (!in_loop)
		other_error("Cannot continue, not in a loop", 1);
	throw ZrcContinueHandler();
}

/** Executes a block while an expression evaluates zero **/
Command(while) {
	if (argc != 3)
		syntax_error("<expr> <block>");
	BlockHandler lh(&in_loop);
_repeat_while:
	try {
		while (OK(argv[1]))
			eval(argv[2]);
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex)
		{ goto _repeat_while; }
	NoReturn;
}

/** !ditto **/
Command(until) {
	if (argc != 3)
		syntax_error("<expr> <block>");
	BlockHandler lh(&in_loop);
_repeat_until:
	try {
		until (OK(argv[1]))
			eval(argv[2]);
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex)
		{ goto _repeat_until; }
	NoReturn;
}

/** C-style for statement **/
Command(for) {
	if (argc != 5)
		syntax_error("<block> <expr> <block> <block>");
	BlockHandler lh(&in_loop);
	bool freed = false;
_repeat_for:
	try {
		for (eval(argv[1]); OK(argv[2]); eval(argv[3]))
			eval(argv[4]);
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex) {
		if (!freed) {
			freed = true;
			free(argv[1]);
			argv[1] = argv[3];
		}
		goto _repeat_for;
	}
	// Avoid double free(2) in exec
	if (freed)
		argv[1] = nullptr;
	NoReturn;
}

/** Iterates through a list of words **/
Command(foreach) {
	if (argc < 4)
		syntax_error("<var> <list1> <list2...> <block>");
	BlockHandler lh(&in_loop);
	int i = 2;
_repeat_foreach:
	try {
		for (; i < argc-1; ++i) {
			setvar(argv[1], argv[i]);
			eval(argv[argc-1]);
		}
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex)
		{ ++i; goto _repeat_foreach; }
	NoReturn;
}

/** Do-While implementation **/
Command(do) {
	bool w = !strcmp(argv[2], "while");
	bool u = !strcmp(argv[2], "until");

	if (argc != 4 || (!w && !u))
		syntax_error("<block> while|until <expr>");

	BlockHandler lh(&in_loop);
_repeat_do:
	try {
		if (w)
			do eval(argv[1]);
			while (OK(argv[3]));
		else
			do eval(argv[1]);
			until(OK(argv[3]));
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex)
		{ goto _repeat_do; }
	NoReturn;
}

/** Switch statement implementation **/
Command(switch) {
	std::string def_cmd;
	std::string se = "<value> {<case <c> cmd|reg <r>|default <block>...}";
	WordList args;
	if (argc != 3)
		syntax_error(se);
	/* empty */ {
		NullIOSink ns;
		std::istream fin(&ns);
		args = tokenize(argv[2], fin);
	}
	std::for_each(args.wl.begin(), args.wl.end(), &str_subst);

	for (int i = 0; i < args.size(); ) {
		if (args.wl[i] == "case") {
			if (i >= args.size()-2)
				syntax_error(se);
			if (argv[1] == args.wl[i+1]) {
				eval(args.wl[i+2]);
				NoReturn;
			}
			i += 3;
		}
		
		else if (args.wl[i] == "reg") {
			if (i >= args.size()-2)
				syntax_error(se);
			std::regex sr{args.wl[i+1]};
			if (std::regex_match(argv[1], sr)) {
				eval(args.wl[i+2]);
				NoReturn;
			}
			i += 3;
		}
	
		else if (args.wl[i] == "default") {
			if (i >= args.size()-1)
				syntax_error(se);
			def_cmd = args.wl[i+1];
			i += 2;
		}

		else {
			syntax_error(se);
		}
	}
	eval(def_cmd);
	NoReturn;
}

/** Returns a value **/
Command(return) {
	if (argc > 2)
		syntax_error("[<val>]");
	ret_val = (argc == 2)
		? combine(argc, argv, 1)
		: ZRC_DEFAULT_RETURN;
	if (in_func)
		throw ZrcReturnHandler();
	NoReturn;
}

/** Defines a new function **/
Command(fn) {
	if (argc != 3)
		syntax_error("<name> <block>");
	if (+FOUND_FN(1))
		other_error("Function exists", 2);
	funcs[argv[1]] = argv[2];
	if (txt2sig.find(argv[1]) != txt2sig.end() && (strcmp(argv[1], "sigexit")))
		signal2(txt2sig.at(argv[1]), [](int sig){
			// We have to re-traverse the hashmap, because lambdas can't be passed as
			// function pointers if they capture argv[]...
			for (auto const& it : txt2sig)
				if (it.second == sig && funcs.find(it.first) != funcs.end())
					eval(it.first);
		});
	NoReturn;
}

/** Undefines a function**/
Command(nf) {
	bool ok=0;
	if ((argc != 2 && argc != 3) || (argc == 3 && strcmp(argv[1], "-s")))
		syntax_error("[-s] <name>");
	if (argc == 3) {
		++argv, --argc;
		ok = 1;
	}
	if (!FOUND_FN(1)) {
		if (!ok) {
			--argv, ++argc;
			other_error("Function not found", 2);
		}
		NoReturn;
	} else {
		funcs.erase(argv[1]);
	}

	if (txt2sig.find(argv[1]) != txt2sig.end()) {
		// We have default handlers for these:
		if (!strcmp(argv[1], "sigchld"))
			signal2(SIGCHLD, sigchld_handler);
		else if (!strcmp(argv[1], "sigint" ))
			signal2(SIGINT,   sigint_handler);
		else if (!strcmp(argv[1], "sigtstp"))
			signal2(SIGTSTP, sigtstp_handler);

		// These sigs are ignored:
		else if (!strcmp(argv[1], "sigttin") || !strcmp(argv[1], "sigttou"))
			signal2(txt2sig.at(argv[1]), SIG_IGN);
	
		// These sigs use the default handler:
		else
			signal2(txt2sig.at(argv[1]), SIG_DFL);
	}
	--argv, ++argc;
	NoReturn;
}

/** Changes the current dir **/
Command(cd) {
	struct stat sb;
	if (argc == 1) {
		struct passwd *pw = getpwuid(getuid());
		chdir(pw->pw_dir);
	} else {
		if (argc != 2)
			syntax_error("<dir>");
		if (stat(argv[1], &sb) == 0 && S_ISDIR(sb.st_mode))
			chdir(argv[1]);
		else {
			/* empty */ {
				std::istringstream iss{getvar($CDPATH)};
				std::string tmp;
				while (getline(iss, tmp, ':')) {
					char t[PATH_MAX];
					sprintf(t, "%s/%s", tmp.data(), argv[1]);
					if (stat(t, &sb) == 0 && S_ISDIR(sb.st_mode)) {
						chdir(t);
						NoReturn;
					}
				}
			}
			perror(argv[1]);
			return ZRC_ERRONE_RETURN;
		}
	}
	//Placeholder
	NoReturn;
}

/** Forks a new subshell **/
Command(fork) {
	char *message;
	pid_t pid;

	if (argc != 2)
		syntax_error("<cmd>");
	message = (char*)mmap(NULL, BUFSIZ,
			PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if ((pid = fork()) < 0)
		other_error("fork() failed", 2);

	if (pid == 0) {
		NO_SIGEXIT;
		setvar($PID, std::to_string(getpid()));
		strcpy(message, eval(argv[1]).data());
		EXIT_SESSION;

	} else {
		waitpid(pid, NULL, 0);
		return message;
	}
}

/** Outputs a message to stdout
  Based on this very elegant implementation by Suckless.org:
  https://git.suckless.org/sbase/file/echo.c.html
 **/
Command(echo) {
	bool n_flag = false;

	--argc, ++argv;
	if (*argv && !strcmp(*argv, "-n"))
		n_flag = true,
		--argc, ++argv;
	for (; *argv; --argc, ++argv) {
		std::cout << *argv;
		if (argc > 1)
			std::cout << ' ';
	}
	if (!n_flag)
		std::cout << '\n';
	++argc, --argv;
	std::cout << std::flush;
	NoReturn;
}

/** Reads from stdin **/
Command(read) {
	const char *se = "[-d <delim>|-n <nchars>] [-p <prompt>] [-f <fd>] [<var1> <var2>...]";
	std::string buf, d = "\n";
	long n = -1, i;
	bool dflag = false;
	optind = 0;
	int opt;
	int fd = STDIN_FILENO;
	while ((opt = getopt(argc, argv, "d:n:p:f:")) != -1) {
		switch (opt) {
		case 'd':
			if (n != -1)
				syntax_error(se);
			d = optarg, dflag = true;
			break;
		case 'n':
			if (dflag)
				syntax_error(se);
			n = (ull)expr(optarg);
			break;
		case 'f':
			fd = (ull)expr(optarg);
			break;
		case 'p':
			std::cout << optarg << std::flush;
			break;
		case '?':
			syntax_error(se);
		}
	}
	// buffers
	char b[(n > 0) ? (n+1) : 1];
	char c;
#define GET_INPUT                   \
    buf.clear();                    \
    if (n == -1) {                  \
        int ok;                     \
        ok = read(fd, &c, 1);       \
        if (ok != 1)                \
          return ZRC_ERRONE_RETURN; \
        if (d.find(c) == std::string::npos)\
            buf += c;               \
        for ever {                  \
            ok = read(fd, &c, 1);   \
            if (ok != 1 || d.find(c) != std::string::npos)  \
                break;              \
            buf += c;               \
        }                           \
    } else {                        \
        if (read(fd, b, n) != n)    \
          return ZRC_ERRONE_RETURN; \
        b[n] = '\0';                \
        buf += b;                   \
    }

	if (optind >= argc) {
		GET_INPUT;
		std::cout << buf << std::endl;
	} else for (; optind < argc; ++optind) {
		GET_INPUT;
		setvar(argv[optind], buf);
	}
	NoReturn;
}

/** Increment by a value **/
Command(inc) {
	std::string var, val;

	var = "0";
	val = "1";
	if (argc < 2)
		syntax_error("<var> [val]");
	if (argc >= 3)
		val = combine(argc, argv, 2);
	var = getvar(argv[1]);
	ret_val = ldtos(expr(zrc_fmt("(%s)+(%s)", var.data(), val.data())));
	setvar(argv[1], ret_val);
	NoReturn;
}

/** Set a variable **/
Command(set) {
	constexpr char se[] = "<var> [+-*/%|^&:][**][||][&&][//][<<][>>]= <val>...";
	if (argc < 4)
		syntax_error(se);

	for (int i = 2; i < argc; i += 3) {
		if (!strcmp(argv[i], "=")) {
			setvar(argv[i-1], argv[i+1]);
			//ret_val = ZRC_DEFAULT_RETURN;
		} else if (!strcmp(argv[i], ":=")) {
			setvar(argv[i-1], argv[i+1]);
			ret_val = argv[i+1];
		} else {
			size_t len = strlen(argv[i]);
			if (argv[i][len-1] == '=') {
				argv[i][len-1] = '\0';
				setvar(argv[i-1], ldtos(expr(zrc_fmt("(%s)%s(%s)",
						getvar(argv[i-1]).data(),
						argv[i],
						argv[i+1]))));
			} else syntax_error(se);
		}
	}
	NoReturn;
}

/** PHP chr/ord **/
Command(chr) { 
	if (argc != 2)
		syntax_error("<o>");
	std::string t;
	auto ret = expr(combine(argc, argv, 1));
	if (ret != NAN)
		t += (char)ret;
	return t;
}
Command(ord) { 
	if (argc != 2)
		syntax_error("<c>");
	return std::to_string((int)argv[1][0]);
}

/** Add/remove alias **/
Command(alias) {
	if (argc == 3) {
		NullIOSink ns;
		std::istream fin(&ns);
		aliases[argv[1]] = tokenize(argv[2], fin);
	} else if (argc == 1) {
		for (auto const& it : aliases) {
			std::cout << "alias " << it.first << " {";
			for (auto const& s : it.second.wl)
				std::cout << ' ' << s;
			std::cout << " }" << std::endl;
		}
	} else {
		syntax_error("[<name> <alias>]");
	}
	NoReturn;
}

Command(unalias) {
	if (argc != 2)
		syntax_error("<name>");
	if (aliases.find(argv[1]) == aliases.end())
		other_error("Alias not found", 2);
	aliases.erase(argv[1]);
	NoReturn;
}

/** Lexical scoping **/
Command(let) {
	if (argc != 3)
		syntax_error("<var list> <block>");

	WordList vars;
	std::map<std::string, Array> a_hm_bak;
	std::map<std::string, Scalar> s_hm_bak;
	bool ret=0, brk=0, con=0;
	/* empty */ {
		NullIOSink ns;
		std::istream fin(&ns);
		vars = tokenize(argv[1], fin);
	}
	std::for_each(vars.wl.begin(), vars.wl.end(), &str_subst);	
	for (Variable str : vars.wl) {
		if (str[0] == 'A' && str[1] == ',') {
			str.erase(0, 2);
			a_hm_bak[str] = a_hm[str];
		} else {
			s_hm_bak[str] = getvar(str);
		}
	}
	try {
		//BlockHandler xh(&in_func);
		eval(argv[2]);
	} catch   (ZrcReturnHandler ex) { ret = 1; }
	  catch    (ZrcBreakHandler ex) { brk = 1; }
	  catch (ZrcContinueHandler ex) { con = 1; }
	
	for (Variable& str : vars.wl) {
		if (str[0] == 'A' && str[1] == ',') {
			str.erase(0, 2);
			a_hm[str] = a_hm_bak[str];
		} else {
			setvar(str, s_hm_bak[str]);
		}
	}
	if (ret) throw ZrcReturnHandler();
	if (brk) throw ZrcReturnHandler();
	if (con) throw ZrcContinueHandler();
	NoReturn;
}

/** Sourcing scripts into current session **/
Command(source) {
	int i;

	for (i = 1; i < argc; ++i) {
		if (access(argv[i], F_OK) == 0) {
			std::ifstream fin(argv[i]);
			eval_stream(fin);
		} else {
			perror(argv[1]);
			return ZRC_ERRTWO_RETURN;
		}
	}
	NoReturn;
}

/** Shorthand for including headers **/
Command(include) {
	if (argc != 2)
		syntax_error("<library>");
	char **path = new char *[2];
	path[1] = new char[PATH_MAX];
	sprintf(path[1], LIBPATH "/%s", argv[1]);
	if (!strstr(argv[1], LIBEXT))
		strcat(path[1], LIBEXT);

	auto ret = zrc_builtin_source(2, path);
	delete [] path[1];
	delete [] path;
	return ret;
}

/**Unset a var **/
Command(unset) {
	if (argc != 2)
		syntax_error("<var>");
	unsetvar(argv[1]);
	NoReturn;
}

/** Invert a command's return value **/
Command(not) {
	exec(--argc, ++argv);
	ret_val = (ret_val == ZRC_DEFAULT_RETURN)
		? ZRC_ERRONE_RETURN
		: ZRC_ERRTWO_RETURN;
	setvar($RETURN, ret_val);
	++argc, --argv;
	NoReturn;
}

/** Job control **/
Command(bg) { return bg_fg(argc, argv); }
Command(fg) { return bg_fg(argc, argv); }

/** Manipulate the directory stack **/
std::stack<Path> pstack;
Command(pushd) {
	struct stat strat;
	char wd[PATH_MAX];
	if (argc > 2)
		syntax_error("[<dir>]");
	if (pstack.empty()) {
		char wd[PATH_MAX];
		getcwd(wd, sizeof wd);
		pstack.push(wd);
	}
	if (argc == 2) {
		if (stat(argv[1], &strat) != 0)
			other_error("Could not stat "<<argv[1], 2);
		if (!S_ISDIR(strat.st_mode))
			other_error(argv[1]<<" is not a directory", 3);
		
		char *rp = realpath(argv[1], nullptr);
		pstack.push(rp);
		free(rp);
	
	} else {
		if (pstack.size() == 1)
			other_error("No other directory", 4);
		Path p = pstack.top();
		pstack.pop();
		std::swap(p, pstack.top());
		pstack.push(p);
	}
	prints(pstack);
	NoReturn;
}

Command(popd) {
	if (argc > 1)
		syntax_error("");
	if (pstack.empty())
		other_error("Directory stack empty", 2);
	pstack.pop();
	prints(pstack);
	NoReturn;
}

/** regex **/
Command(regexp) {
	if (argc < 4)
		syntax_error("<reg> <txt> <var1> <var2...>");
	int  k = 3;
	std::regex  rexp(argv[1]);
	std::smatch res;
	std::string txt = argv[2];
	std::string::const_iterator it(txt.cbegin());
	ret_val = ZRC_ERRTWO_RETURN;
	while (std::regex_search(it, txt.cend(), res, rexp)) {
		ret_val = ZRC_DEFAULT_RETURN;
		if (k >= argc)
			NoReturn;
		setvar(argv[k++], res[0]);
		it = res.suffix().first;
	}
	NoReturn;
}

/** Shift argv **/
Command(shift) {
	size_t howmuch = 1, i;
	auto *arg = &a_hm[$ARGV];
	size_t len = arg->size();
	if (argc  > 2)
		syntax_error("[<n>]");
	if (argc == 2)
		howmuch = (ull)expr(argv[1]);
	
	//shift to left
	for (i = 0; i < len-howmuch; ++i)
		arg->set(i, arg->get(i+howmuch));
	
	//delete the rest
	for (i = len-howmuch; i < len; ++i)
		arg->destroy(i);
	
	setvar($ARGC, arg->size());
	return getvar($ARGC);
}

/** Replace proc **/
Command(exec) {
	++argv;
	if (argc > 1)
		execvp(*argv, argv);
	NoReturn;
}

/** Perform substitutions **/
Command(subst) {
	if (argc != 2)
		syntax_error("<str>");
	std::string t{argv[1]};
	str_subst(t);
	return t;
}

/** Increase Zrc call stack for recursion**/
Command(rlimit) {
	if (argc != 2)
		syntax_error("<n>");

	rlim_t memory = (ull)expr(argv[1]);
	struct rlimit rlm;
	int err;
	short exp = 0;
	switch (argv[1][strlen(argv[1])-1]) {
	case 'K': exp =  10; break;
	case 'M': exp =  20; break;
	case 'G': exp =  30; break;
	case 'T': exp =  40; break;
	case 'P': exp =  50; break;
	case 'E': exp =  60; break;
	case 'Z': exp =  70; break;
	case 'Y': exp =  80; break;
	case 'B': exp =  90; break;
	case 'g': exp = 100; break;
	}
	memory <<= exp;
	if (!getrlimit(RLIMIT_STACK, &rlm)) {
		if (rlm.rlim_cur < memory) {
			rlm.rlim_cur = memory;
			if (setrlimit(RLIMIT_STACK, &rlm))
				other_error("setrlimit() failed", 2);
		}
	}
	NoReturn;
}

#if defined USE_HASHCACHE && USE_HASHCACHE == 1
/** Internal command hash table **/
DispatchTable<std::string, std::string> hctable;
Command(unhash) {
	hctable.clear();
	NoReturn;
}
Command(rehash) {
	std::istringstream iss{getvar($PATH)};
	std::string tmp;
	struct dirent *entry;
	struct stat sb;
	DIR *d = NULL;
	hctable.clear();
	while (getline(iss, tmp, ':')) {
		d = opendir(tmp.c_str());
		if (d != NULL) {
			while ((entry = readdir(d))) {
				char *nm = entry->d_name;
				char nm2[PATH_MAX];
				sprintf(nm2, "%s/%s", tmp.c_str(), nm);
				if (hctable.find(nm) == hctable.end() && !stat(nm2, &sb) && sb.st_mode & S_IXUSR)
				  hctable[nm] = nm2;
			}
		}
		closedir(d);
	}
	NoReturn;
}
#endif

#if defined USE_ZLINEEDIT && USE_ZLINEEDIT == 1
/** Key bindings **/
Command(bindkey) {
	Bind b;
	if (argc != 1 && argc != 3 && argc != 4) 
		syntax_error("[-c] [<seq> <cmd>]");
	
	if (argc == 1) {
		for (auto const& it : keybinds) {
			std::cout << "bindkey ";
			if (it.second.zcmd)
				std::cout << "-c ";
			std::cout << '{' << it.first << "} {" << it.second.cmd << "}\n";
		}
		std::cout << std::flush;
		NoReturn;
	}

	if (argc == 4 && !strcmp(argv[1], "-c")) {
		b.zcmd = true;
		--argc, ++argv;
	} else {
		b.zcmd = false;
	}
	b.cmd = argv[2];
	keybinds[argv[1]] = std::move(b);
	if (b.zcmd)
		++argc, --argv;
	NoReturn;
}
Command(unbindkey) {
	if (argc != 2)
		syntax_error("<seq>");
	if (keybinds.find(argv[1]) == keybinds.end())
		other_error("Key binding '"<<argv[1]<<"' not found", 2);
	keybinds.erase(argv[1]);
	NoReturn;
}
#endif

Command(help);
Command(builtin);

const OrderedDispatchTable<std::string, std::function<std::string(int, char**)>> dispatch_table = {
	/* Aliased commands */
	ce(!,not)   , ce(.,source), ce(@,fork),

	/* Normal cmds */
	de(alias)   , de(array)  , de(bg),
	de(cd)      , de(chr)    , de(die),
	de(do)      , de(echo)   , de(eval),
	de(exec)    , de(exit)   , de(expr),
	de(fg)      , de(fn)     , de(for),
	de(foreach) , de(help)   , de(if),
	de(inc)     , de(jobs)   , de(let),
	de(nf)      , de(ord)    , de(popd),
	de(pushd)   , de(read)   , de(regexp),
	de(return)  , de(set)    , de(shift),
	de(source)  , de(string) , de(switch),
	de(unalias) , de(unless) , de(unset),
	de(until)   , de(wait)   , de(while),
	de(subst)   , de(break)  , de(continue),
	de(concat)  , de(rlimit) , de(include),
	de(builtin) , de(clear)  ,
	/* $PATH hashing */
#if defined USE_HASHCACHE && USE_HASHCACHE == 1
	de(unhash)  , de(rehash) ,
#endif
	/* Keyboard bindings */
#if defined USE_ZLINEEDIT && USE_ZLINEEDIT == 1
	de(bindkey) , de(unbindkey)
#endif
};

/** Show a list of all BuiltIns **/
Command(help) {
	for (auto& it : dispatch_table)
		std::cout << it.first << ' ';
	std::cout << std::endl;
	NoReturn;
}

/** Execute command, but prioritize builtins **/
extern inline bool
builtin_check(int argc, char *argv[])
{
	if (dispatch_table.find(argv[0]) == dispatch_table.end())
		return false;
	ret_val = dispatch_table.at(argv[0])(argc, argv);
	return true;
}

Command(builtin) {
	--argc, ++argv;
	if (!argc) {
		++argc, --argv;
		syntax_error("<arg1> <arg2>...");
	}
	if (!builtin_check(argc, argv))
		exec(argc, argv);
	++argc, --argv;
	NoReturn;
}
