#include <sys/ioctl.h>
#include <termios.h>
#include <poll.h>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

void display_prompt(bool show_secondary_prompt) {
	zrc_obj old_status = vars::status;
	auto cleanup = make_scope_exit([&]() { vars::status = old_status; });

	if (show_secondary_prompt)
		std::cerr << DEFAULT_SPROMPT;
	else if (!run_function("prompt"))
		std::cerr << DEFAULT_PPROMPT;
}

namespace line_edit {
	long cursor_pos, histmax, histpos;
	bool dp_list, first_word, start_bind;
	// defined in globals.hpp
	//std::vector<std::string> histfile;
	std::string filename;
	std::fstream io;

	static inline void init_term(size_t& row, size_t& col) {
		struct winsize term;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &term);
		row = term.ws_row;
		col = term.ws_col;
	}

	static inline bool list(std::vector<std::string> const& vec) {
		size_t i = 0, term_hi, term_wd;
		init_term(term_hi, term_wd);
		std::cerr << '\n';
		for (; i < vec.size(); ++i) {
			std::cerr << std::setw(term_wd / 3) << vec[i];
			if ((i + 1) % 3 == 0) {
				std::cerr << '\n';
				if (term_hi * 3 - 6 <= i) {
					std::cerr << "--More--";
					int tmp;
					while ((tmp = getchar()) != EOF) {
						if (tmp == 'q' || tmp == 'Q') {
							std::cerr << std::endl;
							return dp_list = i;
						} else if (tmp == KEY_RET) {
							std::cerr << "\b \b\b \b\b \b\b \b\b \b\b \b\b \b\b \b";
							break;
						}
					}
				}
			}
		}
		std::cerr << std::endl;
		return dp_list = i;
	}

	static inline void tab(std::string& buf) {
		auto wlist = lex(buf.c_str(), SPLIT_WORDS | SEMICOLON).elems;

#ifndef GLOB_TILDE
	#define GLOB_TILDE 0 /* what kinda computer is blud on */
#endif
		auto globbed = glob((std::string(wlist.back())+"*").c_str(), GLOB_TILDE);
		if (globbed.size() == 1) {
			wlist.back() = globbed[0];
			// Replace last word
			buf.clear();
			for (size_t i = 0; i < wlist.size()-1; ++i)
				buf += wlist[i], buf += ' ';
			buf += wlist.back();
// ANSI terminal cursor
#define CURSOR_FWD(X) std::cerr << "\033[" << X << 'C' << std::flush
#define CURSOR_BWD(X) std::cerr << "\033[" << X << 'D' << std::flush
			CURSOR_BWD(cursor_pos);
			std::cerr << buf;
			cursor_pos = buf.length();	
		} else if (!globbed.empty()) {
			list(globbed);
		}
	}

	static inline void cmd(std::string& buf) {
		std::vector<std::string> vec;
		for (auto const& it : !hctable.empty() ? hctable : pathwalk())
			if (!it.first.rfind(buf, 0))
				vec.emplace_back(basename(it.first));
					
		for (auto const& j : functions)
			if (!j.first.rfind(buf, 0))
				vec.emplace_back(j.first);
		for (auto const& j : builtins)
			if (!j.first.rfind(buf, 0))
				vec.emplace_back(j.first);
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

	static inline void R_histfile() {
		struct passwd *pw = getpwuid(getuid());
		std::string tmp;
		
		filename = pw->pw_dir;
		filename += "/" ZHIST;

		io.open(filename, std::fstream::in);
		histfile.clear();
		while (std::getline(io, tmp))
			histfile.push_back(tmp);
		io.close();
		histmax = histfile.size();
		histpos = histmax-1;
	}

	static inline void W_histfile(std::string const& str) {
		if (!std::all_of(str.begin(), str.end(), isspace)) {
			io.open(filename, std::fstream::out | std::fstream::app);
			io << str << std::endl;
			io.close();
		}
	}

// Utility macros
#define ACTION(x) { #x, [](std::string& buf, char c) {
#undef END
#define END ;return false; } },
// Arrow key history browsing
#define ARROW_MACRO(x)            \
  size_t len = buf.length();      \
  if (!histmax) return true;      \
  histpos = x;                    \
  if (histpos == histmax)         \
    buf.clear();                  \
  else                            \
    buf = histfile[histpos];      \
  if (cursor_pos)                 \
    CURSOR_BWD(cursor_pos);       \
  for (long i = 0; i < len; ++i)  \
    std::cerr << ' ';             \
  if (len)                        \
    CURSOR_BWD(len);              \
  std::cerr << buf;               \
  cursor_pos = buf.length()       \


	std::unordered_map<std::string, std::function<bool(std::string&, char c)> > actions = {
		// Moving through command history
		ACTION(hist-go-down) ARROW_MACRO((histpos < histmax) ? histpos+1 : 0) END
		ACTION(hist-go-up)   ARROW_MACRO((histpos > 0) ? histpos-1 : histmax) END

		// Line editor cursor navigation
		ACTION(cursor-move-left)
			if (cursor_pos) {
				--cursor_pos;
				CURSOR_BWD(1);
			}
		END
		ACTION(cursor-move-right)
			if (cursor_pos < buf.length()) {
				++cursor_pos;
				CURSOR_FWD(1);
			}
		END
		ACTION(cursor-move-begin)
			while (cursor_pos) {
				--cursor_pos;
				CURSOR_BWD(1);
			}
		END
		ACTION(cursor-move-end)
			while (cursor_pos < buf.length()) {
				++cursor_pos;
				CURSOR_FWD(1);
			}
		END
		// Text editing
		ACTION(cursor-erase)
			size_t len = buf.length();
			if (cursor_pos > 0 && cursor_pos <= len) {
				CURSOR_BWD(cursor_pos);
				buf.erase(--cursor_pos, 1);
				std::cerr << buf << ' ';
				CURSOR_BWD(len-cursor_pos);
			}
		END
		ACTION(cursor-insert)
			if (isspace(c)) first_word = true;
			if (cursor_pos) CURSOR_BWD(cursor_pos);

			buf.insert(cursor_pos++, 1, c);
			std::cerr << buf;
			size_t len = buf.length();
			if (cursor_pos != len)
				CURSOR_BWD(len-cursor_pos);
		END
		// Other
		ACTION(key-return)
			std::cerr << std::endl;
			W_histfile(buf);
			std::cin.clear();
			return true;
		END
		// Tab completion
		ACTION(expand-word)
			if (first_word)
				tab(buf);
			else
				cmd(buf);
			if (dp_list) {
				display_prompt(false);
				std::cerr << buf;
			}
			dp_list = 0;
		END
	};
}

class raw_input_mode {
private:
	struct termios term, old;
	bool new_one;
public:
	raw_input_mode() {
		tcgetattr(STDIN_FILENO, &term);
		old = term;
		term.c_lflag &= ~ICANON;
		term.c_lflag &= ~ECHO;
		tcsetattr(STDIN_FILENO, TCSANOW, &term);
		new_one = true;
	}
	void toggle() {
		tcsetattr(STDIN_FILENO, TCSADRAIN, &(new_one ? old : term));
		new_one = !new_one;
	}
	~raw_input_mode() {
		tcsetattr(STDIN_FILENO, TCSADRAIN, &old);
	}
};

/** Zrc line editor
 *
 * @param {std::string&}buf
 * @return bool
 */
static inline bool zlineedit(std::string& buf) {
	using namespace line_edit;
	std::string s;
	char c;

	raw_input_mode raw;

	// Execute key binding action
	auto exec_act = [&]() {
		if (s.empty())
			return false;
		if (kv_bindkey.find(s) == kv_bindkey.end())
			s.clear();
		if (kv_bindkey[s].zrc_cmd) {
			raw.toggle();
			eval(kv_bindkey[s].cmd);
			raw.toggle();
		} else if (actions.find(kv_bindkey[s].cmd) != actions.end())
			return actions[kv_bindkey[s].cmd](buf, c);
		else
			std::cerr << "syntax error: Invalid line editor command " << kv_bindkey[s].cmd << std::endl;
		return false;
	};
	
	// Check if the keyboard was hit
	static auto kbhit = []() {
		struct pollfd pfd;
		pfd.fd = STDIN_FILENO;
		pfd.events = POLLIN;
		return poll(&pfd, 1, ZRC_BIND_TIMEOUT / 1000) > 0;
	};

	dp_list = first_word = cursor_pos = start_bind = 0;
	
	R_histfile(); buf.clear();
	actions["hist-go-down"](buf, 0);

	// Main loop
	for (;;) {
		int k_found = 0, k_eq = 0;
		if (kbhit()) {
			if (read(STDIN_FILENO, &c, 1) == -1)
				return 0;
			s += c;
			for (auto const& it : kv_bindkey) {
				if (it.first.find(s) == 0) {
					++k_found;
					if (it.first == s)
						++k_eq;
				}
			}
			if (!k_found || (k_found == 1 && k_eq == 1)) {
				if (exec_act())
					return 1;
				s.clear();
			}
		} else {
			if (exec_act()) 
				return 1;
			s.clear();
		}
	}
	std::cin.clear();
	return 1;
}

/** Get a line using whatever method is preferrable.
 *
 * @param {std::istream&}in,{std::string&}str,{bool}show_secondary_prompt
 * @return bool
 */
bool zrc_getline(std::istream& in, std::string& str, bool show_secondary_prompt = false) {
	if (&in == &std::cin && isatty(fileno(stdin))) {
		display_prompt(show_secondary_prompt);
		return zlineedit(str);
	}
	return !std::getline(in, str).fail();
}
