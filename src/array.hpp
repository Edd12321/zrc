#ifndef ARRAY_HPP
#define ARRAY_HPP

extern char **envp;
#define INIT_ZRC_ARGS {                               \
    for (int i = 0; i < argc; ++i)                    \
        a_hm[$ARGV].set(std::to_string(i), argv[i]);  \
    setvar($ARGC, itoa(argc));                        \
}


#define USAGE2 {usage2();return "";}
static inline void
usage2()
{
	std::cerr << "usage: array [index <a> <i>                        ]\n"
	          << "             [length <a>                           ]\n"
	          << "             [delete <a>                           ]\n"
	          << "             [set    <a> = {<v1> <v2...>}          ]\n"
	          << "             [setkey <a> = {<k1> <v1> <k2> <v2>...}]\n"
			  << "             [unset  <a> <k1> <k2...>              ]\n";
}

/** `array` command
 * 
 * @param {int}argc,{char**}argv
 * @return string
 */
std::string
array(int argc, char *argv[])
{
	int i;
	WordList args;
	std::ifstream fin;

	if (argc < 2)
		USAGE2;

	// % array index <a> <i>
	if (!strcmp(argv[1], "index")) {
		if (argc != 4)
			USAGE2;
		return a_hm[argv[2]].get(argv[3]);
	}

	// % array length <a>
	if (!strcmp(argv[1], "length"))
		return std::to_string(a_hm[argv[2]].size);

	// % array delete <a>
	if (!strcmp(argv[1], "delete"))
		a_hm.erase(argv[2]);

	// % array unset <a> <k1> <k2...>
	if (!strcmp(argv[1], "unset")) {
		if (argc < 4)
			USAGE2;
		for (i = 3; i < argc; ++i)
			a_hm[argv[2]].destroy(argv[i]);
	}

	// % array set <a> = {<v1> <v2...>}
	if (!strcmp(argv[1], "set")) {
		if (argc < 5 || strcmp(argv[3], "="))
			USAGE2;
		fin.open("/dev/null", std::ios::in);
		args = tokenize(argv[4], fin);
		fin.close();
		a_hm.erase(argv[2]);
		std::for_each(args.wl.begin(), args.wl.end(), &str_subst);
		for (i = 0, argc = args.size(); i < argc; ++i)
			a_hm[argv[2]].set(std::to_string(i), args.wl[i]);
	}

	// % array setkey <a> = {<k1> <v1> <k2> <v2>...}
	if (!strcmp(argv[1], "setkey")) {
		if (argc < 5 || strcmp(argv[3], "="))
			USAGE2;

		fin.open("/dev/null", std::ios::in);
		args = tokenize(argv[4], fin);
		fin.close();
		a_hm.erase(argv[2]);
		std::for_each(args.wl.begin(), args.wl.end(), &str_subst);
		argc = args.size();
		if (argc & 1)
			USAGE2;
		for (i = 0; i < argc; i += 2)
			a_hm[argv[2]].set(args.wl[i], args.wl[i+1]);
	}

	return "";
}

#endif // defined
