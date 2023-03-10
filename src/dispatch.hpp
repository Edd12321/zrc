// Macro for laziness
#define FOUND_FN(X) (funcs.find(argv[X]) != funcs.end())
#define itoa ldtoa
#define OK(X) atof(expr(X))
#define de(X) { #X, zrc_builtin_##X }
#define unless(X) if (!(X))

#define Command(X) \
	static inline std::string \
	zrc_builtin_##X(int argc, char *argv[])

#define NoReturn return ret_val
#define syntax_error(X) {\
	std::cerr << errmsg << "(" << argv[0] << ") " << X << '\n';\
	return "1";\
}

#define MakeOp(X)\
	else if (!strcmp(argv[i], #X "="))\
		setvar(argv[i-1], expr((std::string)"("+getvar(argv[i-1])+")"#X"("+argv[i+1]+")"))

typedef std::string FunctionName;
typedef std::string CodeBlock;
typedef std::string AliasName;
#define DispatchTable std::map

DispatchTable<FunctionName, CodeBlock> funcs;
DispatchTable<AliasName, WordList> aliases;

/** Closes the Zrc session **/
Command(exit)   { exit(EXIT_SUCCESS); }
/** Displays a job table **/
Command(jobs)   { jobs(); return "0"; }
/** Waits for child processes to finish **/
Command(wait)   { wait(NULL); return "0"; }
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
	if (argc != 4 || strcmp(argv[2], "while"))
		syntax_error("<block> while <expr>");
	do
		eval(argv[1]);
	while (OK(argv[3]));
	NoReturn;
}

/** Switch statement implementation **/
Command(switch) {
	std::string      def_cmd;
	WordList         args;
	std::ifstream    fin("/dev/null");
	if (argc != 3)
		syntax_error("<value> {<case <c> cmd|default...>}");
	
	args = tokenize(argv[2], fin);
	fin.close();
	std::for_each(args.wl.begin(), args.wl.end(), &str_subst);
	for (int i = 0, argc = args.size(); i < argc; i += 3) {
		if (args.wl[i] == "case") {
			if (args.wl[i+1] == argv[1]) {
				eval(args.wl[i+2]);
				NoReturn;
			}
		} else if (args.wl[i] == "default") {
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
		syntax_error("Function exists");
	funcs[argv[1]] = argv[2];
	return "0";
}

/** Undefines a function**/
Command(nf) {
	if (argc != 2)
		syntax_error("<name>");
	if (!FOUND_FN(1))
		syntax_error("Function not found");
	funcs.erase(argv[1]);
	return "0";
}

/** Changes the current dir **/
Command(cd) {
	struct stat sb;
	if (argc != 2)
		syntax_error("<dir>");
	if (stat(argv[1], &sb) == 0 && S_ISDIR(sb.st_mode))
		chdir(argv[1]);
	else {
		perror(argv[1]);
		return "1";
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
		die("fork()");

	if (pid == 0) {
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
	char delim = '\n', s;
	int n_mode = -1, i, opt;
	std::string prompt, res;
	while ((opt = getopt(argc, argv, "d:n:p:")) != -1) {
		switch(opt) {
		case 'd':
			delim = optarg[0];
			break;
		case 'n':
			n_mode = atoi(optarg);
			break;
		case 'p':
			prompt = optarg;
			break;
		case ':': [[fallthrough]];
		case '?':
			syntax_error("[-d delim ][-n nchars][-p prompt]");
		}
	}
	std::cout << prompt << std::flush;
	if (n_mode > 0) {
		for (i = 0; i < n_mode; ++i)
			if (std::cin >> std::noskipws >> s)
				res += s;
			else
				break;
	} else {
		std::getline(std::cin, res, delim);
	}
	std::cout << res << std::flush;
	return std::to_string(res != "");
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
	constexpr char se[] = "<var> [+-*/%|^&<<>>**]|[:]= <val>";
	if (argc < 4)
		syntax_error(se);

	for (int i = 2; i < argc; i += 3) {
		if (!strcmp(argv[i], "=")) {
			setvar(argv[i-1], argv[i+1]);
		} else if (!strcmp(argv[i], ":=")) {
			setvar(argv[i-1], argv[i+1]);
			ret_val = argv[i+1];
		}
		// expr shortcuts
		MakeOp(+); MakeOp(<<);
		MakeOp(-); MakeOp(>>);
		MakeOp(*); MakeOp(**);
		MakeOp(/);
		MakeOp(%);
		MakeOp(|);
		MakeOp(^);
		MakeOp(&);
		else syntax_error(se);
	}
	NoReturn;
}

/** PHP chr/ord **/
Command(chr) { 
	std::string t;
	t += (char)std::stoi(expr(combine(argc, argv, 1)));
	return t;
}
Command(ord) { return std::to_string((int)argv[1][0]); }

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
	NoReturn;
}

Command(unalias) {
	if (argc != 2)
		syntax_error("<name>");
	if (aliases.find(argv[1]) == aliases.end())
		syntax_error("Alias not found");
	aliases.erase(argv[1]);
	NoReturn;
}

DispatchTable<std::string, std::function<std::string(int, char**)>> dispatch_table = {
	de(exit),  de(return), de(fn),      de(nf),   de(jobs),   de(wait),  de(cd),
	de(die),   de(fork),   de(echo),    de(expr), de(eval),   de(if),    de(unless),
	de(while), de(for),    de(foreach), de(do),   de(switch), de(set),   de(inc),
	de(array), de(string), de(read),    de(chr),  de(ord),    de(alias), de(unalias)
};


extern inline bool
builtin_check(int argc, char *argv[])
{
	if (dispatch_table.find(argv[0]) == dispatch_table.end())
		return false;
	ret_val = dispatch_table.at(argv[0])(argc, argv);
	return true;
}
