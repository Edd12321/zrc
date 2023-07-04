// Macro for laziness
#define FOUND_FN(X) (funcs.find(argv[X]) != funcs.end())
#define itoa ldtoa
#define OK(X) atof(expr(X))
#define de(X)   { #X, zrc_builtin_##X }
#define ce(X,Y) { #X, zrc_builtin_##Y }

#define unless(X) if (!(X))
#define until(X) while (!(X))

#define Command(X) \
	static inline std::string \
	zrc_builtin_##X(int argc, char *argv[])

#define NoReturn return ret_val
#define syntax_error(X) {\
	std::cerr << errmsg << "(" << argv[0] << ") " << X << '\n';\
	return "1";\
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

/** Closes the Zrc session **/
Command(exit)   { exit(EXIT_SUCCESS); }
/** Displays a job table **/
Command(jobs)   { jobs(); return "0"; }
/** Waits for child processes to finish **/
Command(wait)   { while (wait(NULL) > 0){} return "0"; }
/** Closes with an error message **/
Command(die)    { die(argv[1]); return "1"; }
/** Evalues its arguments as a script **/
Command(eval)   { return eval(combine(argc, argv, 1)); }
/** Variable type commands **/
Command(array)  { return array(argc, argv); }
Command(string) { return string(argc, argv); }
/** Merge lists **/
Command(concat) { return combine(argc, argv, 1); }

/** Evaluates an arithmetic expression **/
Command(expr) {
	ExprType et = INFIX;
	if (argc > 1 && !strcmp(argv[1], "-r"))
		et = RPN,
		--argc, ++argv;
	return expr(combine(argc, argv, 1), et);
}

/** Executes a block if an expression evaluates non-zero **/
Command(if) {
	if (argc < 3)
		syntax_error("<expr> <block> [elsif <expr> <block>...|else <block>]");

	if (OK(argv[1])) {
		eval(argv[2]);
	} else for (int i = 3; i < argc; i += 2) {
		if (!strcmp(argv[i], "else")) {
			if (i == argc-2)
				eval(argv[i+1]);
			else
				syntax_error("else");
			break;
		}
		if (!strcmp(argv[i], "elsif")) {
			if (i <= argc-2 && OK(argv[++i])) {
				eval(argv[i+1]);
				break;
			}
		}
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
	try {
		while (OK(argv[1]))
			eval(argv[2]);
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex) {
		//just do again
		while (OK(argv[1]))
			eval(argv[2]);
	}
	NoReturn;
}

/** !ditto **/
Command(until) {
	if (argc != 3)
		syntax_error("<expr> <block>");
	BlockHandler lh(&in_loop);
	try {
		until(OK(argv[1]))
			eval(argv[2]);
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex) {
		until(OK(argv[1]))
			eval(argv[2]);
	}
	NoReturn;
}

/** C-style for statement **/
Command(for) {
	if (argc != 5)
		syntax_error("<block> <expr> <block> <block>");
	BlockHandler lh(&in_loop);
	try {
		for (eval(argv[1]); OK(argv[2]); eval(argv[3]))
			eval(argv[4]);
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex) {
		for (eval(argv[3]); OK(argv[2]); eval(argv[3]))
			eval(argv[4]);
	}
	NoReturn;
}

/** Iterates through a list of words **/
Command(foreach) {
	if (argc < 4)
		syntax_error("<var> <list1> <list2...> <block>");
	BlockHandler lh(&in_loop);
	int i = 2;
	try {
		for (; i < argc-1; ++i) {
			setvar(argv[1], argv[i]);
			eval(argv[argc-1]);
		}
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex) {
		for (++i; i < argc-1; ++i) {
			setvar(argv[1], argv[i]);
			eval(argv[argc-1]);
		}
	}
	NoReturn;
}

/** Do-While implementation **/
Command(do) {
	bool w = !strcmp(argv[2], "while");
	bool u = !strcmp(argv[2], "until");

	if (argc != 4 || (!w && !u))
		syntax_error("<block> while|until <expr>");

	BlockHandler lh(&in_loop);
	try {
		if (w)
			do eval(argv[1]);
			while (OK(argv[3]));
		else
			do eval(argv[1]);
			until(OK(argv[3]));
	} catch (ZrcBreakHandler ex) {
	} catch (ZrcContinueHandler ex) {
		//same thing again
		if (w)
			do eval(argv[1]);
			while (OK(argv[3]));
		else
			do eval(argv[1]);
		until(OK(argv[3]));
	}
	NoReturn;
}

/** Switch statement implementation **/
Command(switch) {
	std::string def_cmd;
	WordList args;

	if (argc != 3)
		syntax_error("<value> {<case <c> cmd|reg <r>|default <block>...}");
	
	/* empty */ {
		NullFin;
		args = tokenize(argv[2], fin);
	}
	std::for_each(args.wl.begin(), args.wl.end(), &str_subst);
	for (int i = 0, argc = args.size(); i < argc; i += 3) {
		if (args.wl[i] == "case") {
			if (i >= argc-2)
				syntax_error("`case` ends too early");
			if (args.wl[i+1] == argv[1]) {
				eval(args.wl[i+2]);
				NoReturn;
			}
		} else if (args.wl[i] == "reg") {
			if (i >= argc-2)
				syntax_error("`reg` ends too early");
			std::regex sr(args.wl[i+1]);
			if (std::regex_search(argv[1], sr)) {
				eval(args.wl[i+2]);
				NoReturn;
			}
		} else if (args.wl[i] == "default") {
			if (i >= argc-1)
				syntax_error("`default` ends too early");
			def_cmd = args.wl[1+i--];
		} else {
			syntax_error("Expected `case`/`default`");
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
		: "0";
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
	if (txt2sig.find(argv[1]) != txt2sig.end()
	&& (strcmp(argv[1], "sigexit")))
	{
		signal2(txt2sig.at(argv[1]), [](int sig){
			// We have to re-traverse the hashmap, because lambdas can't be passed as
			// function pointers if they capture argv[]...
			for (auto const& it : txt2sig)
				if (it.second == sig && funcs.find(it.first) != funcs.end())
					eval(it.first);
		});
	}
	return "0";
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
		if (!ok)
			other_error("Function not found", 2);
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
	return "0";
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
			perror(argv[1]);
			return "1";
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
		exit(0);
	
	} else {
		waitpid(pid, NULL, 0);
		return message;
	}
}

/** Outputs a message to stdout **/
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
	std::cout << std::flush;
	return "0";	
}

/** Reads from stdin **/
Command(read) {
	const char *se = "[-d <delim>|-n <nchars>] [-p <prompt>] [<var1> <var2>...]";
	std::string buf;
	long n = -1, i;
	char d = '\n', b;
	optind = 0;
	int opt;
	while ((opt = getopt(argc, argv, "d:n:p:")) != -1) {
		switch (opt) {
		case 'd':
			if (n != -1)
				syntax_error(se);
			d = *optarg;
			break;
		case 'n':
			if (d != '\n')
				syntax_error(se);
			n = atoi(optarg);
			break;
		case 'p':
			std::cout << optarg << std::flush;
			break;
		case '?':
			syntax_error(se);
		}
	}
#define GET_INPUT \
	buf.clear();\
	if (n == -1) {\
		int ok;\
		ok = read(0, &b, 1);\
		if (ok != 1)\
			return "1";\
		buf += b;\
		for ever {\
			ok = read(0, &b, 1);\
			if (ok != 1 || b == d)\
				break;\
			buf += b;\
		}\
	} else for (i = 0; i < n; ++i) {\
		if (read(0, &b, 1) != 1)\
			return "1";\
		buf += b;\
	}

	if (optind >= argc) {
		GET_INPUT;
		std::cout << buf << std::endl;
	} else for (; optind < argc; ++optind) {
		GET_INPUT;
		setvar(argv[optind], buf);
	}
	return "0";
}

/** Increment by a value **/
Command(inc) {
	std::string var, val;

	var = "0";
	val = "1";
	if (argc < 2)
		syntax_error("<var> [val]");
	if (argc >= 3)
		val = expr(combine(argc, argv, 2));
	var = getvar(argv[1]);
	ret_val = expr((std::string)"("+var+")+("+val+")");
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
			//ret_val = "0";
		} else if (!strcmp(argv[i], ":=")) {
			setvar(argv[i-1], argv[i+1]);
			ret_val = argv[i+1];
		} else {
			size_t len = strlen(argv[i]);
			if (argv[i][len-1] == '=') {
				argv[i][len-1] = '\0';
				setvar(argv[i-1], expr(
					(std::string)"("
					 + getvar(argv[i-1])
					 + ")"
					 + argv[i]
					 + "("
					 + argv[i+1]
					 + ")"));
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
	if (is_number(ret))
		t += (char)std::stoi(ret);
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
		NullFin;
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
	return "0";
}

Command(unalias) {
	if (argc != 2)
		syntax_error("<name>");
	if (aliases.find(argv[1]) == aliases.end())
		other_error("Alias not found", 2);
	aliases.erase(argv[1]);
	return "0";
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
		NullFin;
		vars = tokenize(argv[1], fin);
	}
	std::for_each(vars.wl.begin(), vars.wl.end(), &str_subst);	
	for (std::string str : vars.wl) {
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
	
	for (std::string& str : vars.wl) {
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
			return "2";
		}
	}
	return "0";
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
	return "0";
}

/** Invert a command's return value **/
Command(not) {
	exec(--argc, ++argv);
	// Avoid double free(1)
	for (int i = 0; i < argc; ++i)
		argv[i] = NULL;
	argv = NULL;
	ret_val = (ret_val=="0")?"1":"0";
	setvar($RETURN, ret_val);
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
		pstack.push(argv[1]);
	
	} else {
		if (pstack.size() == 1)
			other_error("No other directory", 4);
		Path p = pstack.top();
		pstack.pop();
		std::swap(p, pstack.top());
		pstack.push(p);
	}
	prints(pstack);
	return "0";
}

Command(popd) {
	if (argc > 1)
		syntax_error("");
	if (pstack.empty())
		other_error("Directory stack empty", 2);
	pstack.pop();
	prints(pstack);
	return "0";
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
	ret_val = "2";
	while (std::regex_search(it, txt.cend(), res, rexp)) {
		ret_val = "0";
		if (k >= argc)
			NoReturn;
		setvar(argv[k++], res[0]);
		it = res.suffix().first;
	}
	NoReturn;
}

/** Shift argv **/
Command(shift) {
	if (argc > 2)
		syntax_error("[<n>]");
	long len = a_hm[$ARGV].size;
	long howmuch = 1;

	if (argc > 2)
		syntax_error("[<n>]");
	if (argc == 2)
		howmuch = atoi(argv[1]);
	if (len) {
		len -= howmuch;
		for (auto i = 0; i < len-howmuch; ++i) {
			a_hm[$ARGV].set(
				std::to_string(i),
				a_hm[$ARGV].get(std::to_string(i+howmuch))
			);
		}
		for (auto i = len-howmuch; i < len; ++i)
			a_hm[$ARGV].destroy(std::to_string(i));
	}
	return std::to_string(len);
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

	rlim_t memory = std::stoull(argv[1]);
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

Command(help);

const DispatchTable<std::string, std::function<std::string(int, char**)>> dispatch_table = {
	/* Aliased commands */
	ce(!,not)   , ce(.,source), ce(@,fork),
	ce(%include , include),

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
	de(concat)  , de(rlimit)
};

/** Show a list of all BuiltIns **/
Command(help) {
	for (auto& it : dispatch_table)
		std::cout << it.first << ' ';
	std::cout << std::endl;
	return "0";
}

extern inline bool
builtin_check(int argc, char *argv[])
{
	if (dispatch_table.find(argv[0]) == dispatch_table.end())
		return false;
	ret_val = dispatch_table.at(argv[0])(argc, argv);
	return true;
}
