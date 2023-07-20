enum Mode {
	GET,
	SET,
	UNSET
};

/** Scalars **/
typedef std::string Scalar;
typedef std::string Variable;

/* convert ints to string*/
template<typename T>
std::string S(T const& t)
{
	if constexpr (std::is_integral<T>::value)
		return std::to_string(t);
	else
		return static_cast<std::string>(t);
}

/** Associative array objects **/
class Array
{
public:
	std::unordered_map<Variable, std::string> hm;
public:
	//array length...
	size_t size()
		{ return hm.size(); }

	//$...
	template<typename T>
	std::string get(T const& key)
		{ return (hm.find(S(key)) != hm.end()) ? hm.at(S(key)) : ""; }
	//set ... = ...
	template<typename A, typename B>
	void set(A const& key, B const& value)
		{ hm[S(key)] = S(value); }
	//unset ...
	template<typename T>
	void destroy(T const& key)
		{ if (hm.find(S(key)) != hm.end()) { hm.erase(S(key)); } }
};

std::unordered_map<Variable, Scalar> s_hm;
std::unordered_map<Variable, Array>  a_hm;

/** Gets a variable's value
 * 
 * @param {std::string}a1,{std::string}a2
 * @return std::string
 */

std::string
variable_magic(std::string const& a1, std::string const& a2, Mode mode)
{
	auto it = a1.find('(');

	// Array
	if (it != std::string::npos && a1.back() == ')') {
		std::string nam = a1;
		std::string key = a1;
		nam.erase(nam.begin()+it, nam.end());
		key.erase(key.begin(), key.begin()+it+1);
		key.pop_back();

		switch(mode) {
		case GET:
			return a_hm[nam].get(key);
		case SET:
			if (nam == $ENV)
				setenv(key.data(), a2.data(), 1);
			a_hm[nam].set(key, a2);
			break;
		case UNSET:
			if (nam == $ENV)
				unsetenv(key.data());
			a_hm[nam].destroy(key);
		}

	// Scalar
	} else {
		switch(mode) {
		case GET:
			return s_hm[a1];
		case SET:
			s_hm[a1] = a2;
			break;
		case UNSET:
			s_hm.erase(a1);
		}
	}
	return "";
}

template<typename A, typename B>
std::string variable_magic(A const& a1, B const& a2, Mode mode)
	{ return variable_magic(S(a1), S(a2), mode); }

#define   setvar(X, Y) variable_magic(X,  Y,   SET)
#define unsetvar(X)    variable_magic(X, "", UNSET)
#define   getvar(X)    variable_magic(X, "",   GET)
#define INIT_ZRC_ARGS {                               \
    for (int i = 0; i < argc; ++i)                    \
        a_hm[$ARGV].set(std::to_string(i), argv[i]);  \
    setvar($ARGC, itoa(argc));                        \
}

extern char **environ;
#define INIT_ZRC_ENVVARS {                            \
	char **s = environ;                                 \
  for (; *s; ++s) {                                   \
    std::string ss = *s;                              \
    auto x = ss.find("=");                            \
    a_hm[$ENV].set(                                   \
        ss.substr(0, x),                              \
        ss.substr(x+1)                                \
    );                                                \
  }                                                   \
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

	if (argc < 2)
		USAGE2;

	// % array index <a> <i>
	if (!strcmp(argv[1], "index")) {
		if (argc != 4)
			USAGE2;
		return a_hm[argv[2]].get(argv[3]);
	}
	// % array length <a>
	if (!strcmp(argv[1], "length")) return std::to_string(a_hm[argv[2]].size());
	// % array delete <a>
	if (!strcmp(argv[1], "delete")) a_hm.erase(argv[2]);
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
		NullFin;
		args = tokenize(argv[4], fin);
		a_hm.erase(argv[2]);
		std::for_each(args.wl.begin(), args.wl.end(), &str_subst);
		for (i = 0, argc = args.size(); i < argc; ++i)
			a_hm[argv[2]].set(std::to_string(i), args.wl[i]);
	}
	// % array setkey <a> = {<k1> <v1> <k2> <v2>...}
	if (!strcmp(argv[1], "setkey")) {
		if (argc < 5 || strcmp(argv[3], "="))
			USAGE2;

		NullFin;
		args = tokenize(argv[4], fin);
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
