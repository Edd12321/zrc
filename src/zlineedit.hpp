#define CURSOR_FWD(X) std::cerr << "\033[" << X << 'C' << std::flush
#define CURSOR_BWD(X) std::cerr << "\033[" << X << 'D' << std::flush

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
	long cursor_pos, histpos, histmax;
	bool dp_list, first_word, start_bind;
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
		filename += "/" ZHISTFILE; 
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

#define ARROW_MACRO(X) {           \
    size_t len = buf.length();     \
    if (!histmax)                  \
        return 1;                  \
    histpos = X;                   \
    buf = (histpos == histmax)     \
        ? ""                       \
        : vec[histpos];            \
    if (cursor_pos)                \
        CURSOR_BWD(cursor_pos);    \
    for (long i = 0; i < len; ++i) \
      std::cerr << ' ';            \
    if (len)                       \
        CURSOR_BWD(len);           \
    std::cerr << buf;              \
    cursor_pos = buf.length();     \
    return 0;                      \
}

	sub tab(std::string&);
	sub cmd(std::string&);

	DispatchTable<std::string, std::function<bool(std::string&, char c)> > actions = {
		/** Histfile navigation **/
		{ "hist-go-down"     , [](std::string& buf, char c)
				ARROW_MACRO((histpos < histmax) ? histpos+1 : 0) },
		{ "hist-go-up"       , [](std::string& buf, char c)
				ARROW_MACRO((histpos > 0) ? histpos-1 : histmax) },
		/** Line editor cursor navigation **/
		{ "cursor-move-left" , [](std::string& buf, char c) {
				if (cursor_pos) {
					--cursor_pos;
					CURSOR_BWD(1);
				}
				return 0;
			}
		},
		{ "cursor-move-right", [](std::string& buf, char c) {
				if (cursor_pos < buf.length()) {
					++cursor_pos;
					CURSOR_FWD(1);
				}
				return 0;
			}
		},
		{ "cursor-move-begin", [](std::string& buf, char c) {
				while (cursor_pos) {
					--cursor_pos;
					CURSOR_BWD(1);
				}
				return 0;
			}
		},
		{ "cursor-move-end"  , [](std::string& buf, char c) {
				while (cursor_pos < buf.length()) {
					++cursor_pos;
					CURSOR_FWD(1);
				}
				return 0;
			}
		},
		/* Modify input buffer */
		{ "cursor-erase"     , [](std::string& buf, char c) {
				size_t len = buf.length();
				if (cursor_pos && cursor_pos <= len) {
					CURSOR_BWD(cursor_pos);
					buf.erase(--cursor_pos, 1);
					std::cerr << buf << ' ';
					CURSOR_BWD(len-cursor_pos);
				}
				return 0;
			}
		},
		{ "cursor-insert"    , [](std::string& buf, char c) {
				size_t len;
				if (isspace(c))
					first_word = true;
				if (cursor_pos)
					CURSOR_BWD(cursor_pos);
				buf.insert(cursor_pos++, 1, c);
				std::cerr << buf;
				len = buf.length();
				if (cursor_pos != len)
					CURSOR_BWD(len-cursor_pos);
				return 0;
			}
		},
		{ "expand-word"      , [](std::string& buf, char c) {
				if (first_word)
					tab(buf);
				else
					cmd(buf);
				if (dp_list) {
					SHOW_PROMPT;
					std::cerr << buf;
				}
				dp_list = 0;
				return 0;
			}
		},
		{ "key-return"      , [](std::string& buf, char c) {
				std::cerr << std::endl;
				W(buf);
				std::cin.clear();
				return 1;
			}
		},
	};

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
					int tmp;
					while ((tmp = getchar()) != EOF) {
						if (tmp == 'q') {
							std::cerr << std::endl;
							return (dp_list = i);
						} else if (tmp == KEY_RET) {
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

/** Check if the keyboard was hit.
 * 
 * @param  none
 * @return bool
 */
static inline bool
kbhit()
{
	struct timeval time = { 0, ZRC_BIND_TIMEOUT };
	fd_set val;

	FD_ZERO(&val);
	FD_SET(0, &val);
	return select(1, &val, NULL, NULL, &time) > 0;
}

/** Execute a keybinding.
 * 
 * @param {string&}cmd,{char}mod,{string&}buf
 * @return bool
 */
static inline bool
exec_act(std::string cmd, char mod, std::string& buf)
{
	using namespace zlineshort;

	if (cmd.empty())
		return 0;
	if (keybinds.find(cmd) == keybinds.end())
		cmd.clear();
	if (keybinds[cmd].zcmd)
		eval(keybinds[cmd].cmd);
	else if (actions.find(keybinds[cmd].cmd) != actions.end())
		return actions[keybinds[cmd].cmd](buf, mod);
	else
		std::cerr << errmsg << "Invalid line editor command\n";
	return 0;
}

/** Gets a line from stdin using the line editor.
 * 
 * @param {string&}buf
 * @return bool
 */
bool
zlineedit(std::string& buf)
{
	/* Shortcut support */
	using namespace zlineshort;
	dp_list = first_word = cursor_pos = start_bind = 0;
	R(); buf.clear();
	actions["hist-go-down"](buf, 0);

	RawInputMode mode;
	std::string s, cmd;
	char c;
	for ever {
		int k_found = 0, k_eq = 0;
		if (kbhit()) {
			if (read(STDIN_FILENO, &c, 1) == -1)
				return 0;
			s += c;
			for (auto const& it : keybinds) {
				if (it.first.find(s) == 0) {
					++k_found;
					if (it.first == s)
						++k_eq;
				}
			}
			if (!k_found || (k_found == 1 && k_eq == 1)) {
				if (exec_act(s, c, buf))
					return 1;
				s.clear();
			}
		} else {
			if (exec_act(s, c, buf)) 
				return 1;
			s.clear();
		}
	}
	std::cin.clear();
	return 1;
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
