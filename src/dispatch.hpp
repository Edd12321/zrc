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

#define MakeOp(X)\
	else if (!strcmp(argv[i], X "="))\
		setvar(argv[i-1], expr((std::string)"("+getvar(argv[i-1])+")" X "("+argv[i+1]+")"))

typedef std::string FunctionName;
typedef std::string CodeBlock;
typedef std::string AliasName;
typedef std::string Path;
#define DispatchTable std::map

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
/** Evaluates an arithmetic expression **/
Command(expr)   { return expr(combine(argc, argv, 1)); }
/** Evalues its arguments as a script **/
Command(eval)   { return eval(combine(argc, argv, 1)); }
/** Variable type commands **/
Command(array)  { return array(argc, argv); }
Command(string) { return string(argc, argv); }

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

/** Executes a block while an expression evaluates zero **/
Command(while) {
	if (argc != 3)
		syntax_error("<expr> <block>");
	while (OK(argv[1]))
		eval(argv[2]);
	NoReturn;
}

/** !ditto **/
Command(until) {
	if (argc != 3)
		syntax_error("<expr> <block>");
	until(OK(argv[1]))
		eval(argv[2]);
	NoReturn;
}

/** C-style for statement **/
Command(for) {
	if (argc != 5)
		syntax_error("<block> <expr> <block> <block>");
	for (eval(argv[1]); OK(argv[2]); eval(argv[3]))
		eval(argv[4]);
	NoReturn;
}

/** Iterates through a list of words **/
Command(foreach) {
	if (argc < 4)
		syntax_error("<var> <list1> <list2...> <block>");
	for (int i = 2; i < argc-1; ++i) {
		setvar(argv[1], argv[i]);
		eval(argv[argc-1]);
	}
	NoReturn;
}

/** Do-While implementation **/
Command(do) {
	bool w = !strcmp(argv[2], "while");
	bool u = !strcmp(argv[2], "until");

	if (argc != 4 || (!w && !u))
		syntax_error("<block> while|until <expr>");

	if (w)
		do eval(argv[1]);
		while (OK(argv[3]));
	else
		do eval(argv[1]);
		until(OK(argv[3]));
	NoReturn;
}

/** Switch statement implementation **/
Command(switch) {
	std::string      def_cmd;
	WordList         args;
	std::ifstream    fin("/dev/null");
	if (argc != 3)
		syntax_error("<value> {<case <c> cmd|reg <r>|default <block>...}");
	
	args = tokenize(argv[2], fin);
	fin.close();
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
	if (argc < 2)
		syntax_error("<val>");
	return combine(argc, argv, 1);
}

/** Defines a new function **/
Command(fn) {
	if (argc != 3)
		syntax_error("<name> <block>");
	if (+FOUND_FN(1))
		other_error("Function exists", 2);
	funcs[argv[1]] = argv[2];
	if (txt2sig.find(argv[1]) != txt2sig.end()) {
		if (!strcmp(argv[1], "sigexit")) {
			atexit([](){eval(funcs["sigexit"]);});
		} else {
			signal2(txt2sig.at(argv[1]), [](int sig){
				// We have to re-traverse the hashmap, because lambdas can't be passed as
				// function pointers if they capture argv[]...
				for (auto const& it : txt2sig)
					if (it.second == sig)
						eval(it.first);
			});
		}
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
	
		// This is the `SIGEXIT` pseudosig:
		else if (!strcmp(argv[1], "sigexit"))
			atexit([](){});

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
		atexit([](){});
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
	const char* se = "[-p <prompt>][-d <delim>][-q][-w] str|char <n>|line";
	
	int opt;
	bool q = false, w = false;
	char d = '\n';

	optind = 1;
	while ((opt = getopt(argc, argv, "p:d:qw")) != -1) {
		switch (opt) {
		case 'p':
			std::cout << optarg << std::flush;
			break;
		case 'd': d = optarg[0]; break;
		case 'q': q = true;      break;
		case 'w': w = true;      break;
		case '?':
			//optind = -1;
			break;
		}
	}
	if (optind >= argc)
		syntax_error(se);
	if (!strcmp(argv[optind], "str")) {
		if (optind != argc-1)
			syntax_error(se);
		std::string str;
		if (q) {
			if (!(std::cin >> std::quoted(str)))
				return "1";
		} else {
			if (!(std::cin >> str))
				return "1";
		}
		std::cout << str;
		
	} else if (!strcmp(argv[optind], "line")) {
		if (optind != argc-1)
			syntax_error(se);
		std::string str;
		if (!std::getline(std::cin, str, d))
			return "1";
		std::cout << str;
		
	} else if (!strcmp(argv[optind], "char")) {
		if (optind != argc-2)
			syntax_error(se);
		char ch;
		if (w) {
			if (!(std::cin >> std::noskipws >> ch))
				return "1";
		} else {
			if (!(std::cin >> ch))
				return "1";
		}
		std::cout << ch;
	} else {
		syntax_error(se);
	}
	std::cout << std::endl;
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
		}
		// expr shortcuts
		MakeOp("+"); MakeOp("<<");
		MakeOp("-"); MakeOp(">>");
		MakeOp("*"); MakeOp("**");
		MakeOp("/"); MakeOp("&&");
		MakeOp("%"); MakeOp("||");
		MakeOp("|"); MakeOp("//");
		MakeOp("^");
		MakeOp("&");
		else syntax_error(se);
	}
	NoReturn;
}

/** PHP chr/ord **/
Command(chr) { 
	if (argc != 2)
		syntax_error("o");
	std::string t;
	t += (char)std::stoi(expr(combine(argc, argv, 1)));
	return t;
}
Command(ord) { 
	if (argc != 2)
		syntax_error("c");
	return std::to_string((int)argv[1][0]);
}

/** Add/remove alias **/
Command(alias) {
	if (argc == 3) {
		std::ifstream fin("/dev/null");
		aliases[argv[1]] = tokenize(argv[2], fin);
		std::for_each(aliases[argv[1]].wl.begin(), aliases[argv[1]].wl.end(), &str_subst);
		fin.close();
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
	std::map<std::string, Variable> hm;

	std::ifstream fin("/dev/null");
	vars = tokenize(argv[1], fin);
	fin.close();
	for (std::string const& str : vars.wl)
		hm[str] = getvar(str);
	eval(argv[2]);
	for (std::string const& str : vars.wl)
		setvar(str, hm[str]);
	NoReturn;
	
}

/** Sourcing scripts into current session **/
Command(source) {
	int i;

	for (i = 1; i < argc; ++i) {
		if (access(argv[i], F_OK) == 0) {
			std::ifstream fin(argv[i]);
			eval_stream(fin);
			fin.close();
		} else {
			perror(argv[1]);
			return "1";
		}
	}
	return "0";
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
	setvar("?", ret_val);
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

Command(help);

DispatchTable<std::string, std::function<std::string(int, char**)>> dispatch_table = {
	de(exit),  de(return), de(fn),      de(nf),     de(jobs),    de(wait),  de(cd),
	de(die),   ce(@,fork), de(echo),    de(expr),   de(eval),    de(if),    de(unless),
	de(while), de(for),    de(foreach), de(do),     de(switch),  de(set),   de(inc),
	de(array), de(string), de(read),    de(chr),    de(ord),     de(alias), de(unalias),
	de(let),   de(until),  de(source),  de(unset),  de(help),    ce(!,not), de(bg),
	de(fg),    de(pushd),  de(popd),    de(regexp), ce(.,source)
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
