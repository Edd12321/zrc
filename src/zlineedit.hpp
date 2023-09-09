/** Keyboard macros **/
//KEY_ESC and CTRL(X) defined in subst.hpp
#define KEY_BACKSPACE 127
#define KEY_ALTBSPACE 8
#if defined(_WIN32)
	#define KEY_RETURN 13
#else
	#define KEY_RETURN '\n'
#endif

/** Moving cursor **/
#define CURSOR_FWD(X) std::cerr << "\033[" << X << "C"
#define CURSOR_BWD(X) std::cerr << "\033[" << X << "D"

/* Display $PS1 */
#define SHOW_PROMPT \
	if (s_hm.find($PS1) != s_hm.end()) {\
		char *argv[2];\
		argv[0] = strdup("@");\
		argv[1] = strdup((S("echo -n 1> &2 ")+getvar($PS1)).c_str());\
		exec(2, argv);\
	} else {\
		std::cerr << default_prompt << std::flush;\
	}

#if defined USE_ZLINEEDIT && USE_ZLINEEDIT == 1
/** Returns a terminal's width and height
 * 
 * @param {int&}row,{int&}col
 * @return void
 */
sub init_term(size_t& row, size_t& col)
{
	struct winsize term;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term);
	row = term.ws_row;
	col = term.ws_col;
}

namespace zlineshort
{
	long cursor_pos = 0, histpos, histmax;
	bool dp_list = false;
	std::fstream f;
	std::vector<std::string> vec;
	std::string filename;

	/* Histfile procedures */
	sub R()
	{
		struct passwd *pw = getpwuid(getuid());
		std::string tmp;
		filename.clear();
		filename += pw->pw_dir;
		filename += "/.zrc_history";
		f.open(filename, std::fstream::in);

		histmax = 0;
		vec.clear();

		while (std::getline(f, tmp))
			vec.emplace_back(tmp);
		f.close();
		histmax = vec.size();
		histpos = histmax-1;
	}
	sub W(std::string_view sv)
	{
		if (!std::all_of(sv.begin(), sv.end(), isspace)) {
			f.open(filename, std::fstream::out|std::fstream::app);
			f << sv << std::endl;
			f.close();
		}
	}

	/* Arrow key history browsing */

#define ARROW_MACRO(X) {\
	size_t len = buf.length();\
	if (!histmax)\
		return;\
	histpos = (X);\
	buf = (histpos == histmax)\
		? ""\
		: vec[histpos];\
	if (cursor_pos)\
		CURSOR_BWD(cursor_pos);\
	for (long i = 0; i < len; ++i)\
		std::cerr << ' ';\
	if (len)\
		CURSOR_BWD(len);\
	std::cerr << buf;\
	cursor_pos = buf.length();\
}
	sub dn(std::string& buf)
		ARROW_MACRO((histpos<histmax) ? histpos+1 : 0)

	sub up(std::string& buf)
		ARROW_MACRO((histpos>0) ? histpos-1 : histmax)

	/* Moving the cursor around */
	sub lt(std::string& buf)
	{
		if (cursor_pos) {
			--cursor_pos;
			CURSOR_BWD(1);
		}
	}
	sub rt(std::string& buf)
	{
		if (cursor_pos < buf.length()) {
			++cursor_pos;
			CURSOR_FWD(1);
		}
	}
	/* Modify input buffer */
	sub del(std::string& buf)
	{
		size_t i, len = buf.length();
		if (cursor_pos && cursor_pos <= len) {
			CURSOR_BWD(cursor_pos);
			buf.erase(--cursor_pos, 1);
			std::cerr << buf << ' ';
			CURSOR_BWD(len-cursor_pos);
		}
	}
	sub insert(std::string& buf, char const& c)
	{
		size_t len;
		if (cursor_pos)
			CURSOR_BWD(cursor_pos);
		buf.insert(cursor_pos++, 1, c);
		std::cerr << buf;
		len = buf.length();
		if (cursor_pos != len)
			CURSOR_BWD(len-cursor_pos);
	}

	/* Completion */
	static inline bool
	list(std::vector<std::string> const& vec)
	{
		size_t i = 0, len;
		size_t term_hi, term_wd;
		/* init terminal screen size vars */
		init_term(term_hi, term_wd);
		std::cerr << '\n';
		for (len = vec.size(); i < len; ++i) {
			std::cerr << std::setw(term_wd/3) << vec[i];
			if ((i+1) % 3 == 0) {
				std::cerr << '\n';
				if (term_hi*3-6 <= i) {
					std::cerr << "--More--";
					char tmp;
					while ((tmp = getchar()) != EOF) {
						if (tmp == 'q') {
							std::cerr << std::endl;
							return (dp_list = i);
						} else if (tmp == KEY_RETURN) {
							std::cerr << "\b \b\b \b\b \b\b \b\b \b\b \b\b \b\b \b";
							break;
						}
					}
				}
			}
		}
		std::cerr << std::endl;
		return (dp_list = i);
	}
	sub tab(std::string& buf)
	{
		NullIOSink ns;
		std::istream fin(&ns);
		WordList wlist = tokenize(buf, fin);
		WordList globbed = glob(wlist.back()+"*");
		if (globbed.size() == 1) {
			wlist.wl.back() = globbed.wl[0];
			buf = combine(wlist.size(), wlist.wl, 0);
			CURSOR_BWD(cursor_pos);
			std::cerr << buf;
			cursor_pos = buf.length();
		} else if (globbed.size()) {
			list(globbed.wl);
		}
	}
	sub cmd(std::string& buf)
	{
		std::vector<std::string> vec;
#if defined USED_HASHCACHE && USE_HASHCACHE == 1
		for (auto const& it : hctable)
			vec.emplace_back(it.first);
#else
		std::istringstream iss(getvar($PATH));
		std::string tmp;
		struct dirent *entry;
		DIR *d = NULL;
		while (getline(iss, tmp, ':')) {
			d = opendir(tmp.c_str());
			if (d != NULL) {
				while ((entry = readdir(d))) {
					std::string s{entry->d_name};
					if (s != "." && s != ".." && !s.rfind(buf, 0))
						vec.emplace_back(s);
				}
				closedir(d);
			}
		}
#endif
		for (auto const& it : dispatch_table)
			if (it.first.rfind(buf, 0) == 0)
				vec.emplace_back(it.first);
		if (vec.size() == 1) {
			buf = vec[0];
			CURSOR_BWD(cursor_pos);
			std::cerr << buf;
			cursor_pos = buf.length();
		} else if (!vec.size()) {
			tab(buf);
		} else {
			list(vec);
		}
	}
}

class RawInputMode
{
private:
	struct termios term;
public:
	RawInputMode()
	{
		tcgetattr(STDIN_FILENO, &term);
		term.c_lflag &= ~ICANON;
		term.c_lflag &= ~ECHO;
		tcsetattr(STDIN_FILENO, TCSANOW, &term);
	}

	~RawInputMode()
	{
		term.c_lflag |= ICANON;
		term.c_lflag |= ECHO;
		tcsetattr(STDIN_FILENO, TCSADRAIN, &term);
	}
};

bool
zlineedit(std::string& buf)
{
	/* Shortcut support */
	using namespace zlineshort;
	RawInputMode rm;
	char c;
	bool first_word = false;
	dp_list = cursor_pos = 0;
	R();
	buf.clear();
	dn(buf);

	while ((c = getchar()) != EOF) {
		switch (c) {
		/* The line editing part */
#if defined(_WIN32)
		case 224:
			c = getchar();
			if (c == 'H') up(buf);
			if (c == 'P') dn(buf);
			if (c == 'K') lt(buf);
			if (c == 'M') rt(buf);
			break;
#else
		case KEY_ESC:
			c = getchar();
			if (c == '[') {
				c = getchar();
				if (c == 'A') up(buf);
				if (c == 'B') dn(buf);
				if (c == 'C') rt(buf);
				if (c == 'D') lt(buf);
			}
			break;
#endif
		
		case KEY_RETURN:
			std::cerr << std::endl;
			W(buf);
			std::cin.clear();
			return true;

		case KEY_BACKSPACE: /* FALLTHROUGH */
		case KEY_ALTBSPACE:
			del(buf);
			break;

		case '\t': /* NOT FALLTHROUGH */
			if (first_word)
				tab(buf);
			else
				cmd(buf);
			if (dp_list) {
				SHOW_PROMPT;
				std::cerr << buf;
			}
			dp_list = false;
			break;

		default:
			if (isspace(c))
				first_word = true;
			insert(buf, c);
		}
	}
	std::cin.clear();
	return true;
}
#else
inline bool zlineedit(std::string& buf)
	{ return !std::getline(std::cin, buf).fail(); }
#endif

template<typename T> inline bool
zrc_read_line(std::istream& in, std::string& buf, T const& prompt)
{
	//std::cin.clear();
	if (TERMINAL) {
		std::cerr << prompt << std::flush;
		return zlineedit(buf);
	} else {
		return !std::getline(in, buf).fail();
	}
}

inline bool
zrc_read_line(std::istream& in, std::string& buf)
{
	//std::cin.clear();
	if (TERMINAL) {
		SHOW_PROMPT;
		return zlineedit(buf);
	} else {
		return !std::getline(in, buf).fail();
	}
}
