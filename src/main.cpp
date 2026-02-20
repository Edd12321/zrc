#include "pch.hpp"
#include "config.hpp"
#include "custom_cmd.hpp"
#include "expr.hpp"
#include "global.hpp"
#include "sig.hpp"
#include "syn.hpp"
#include "vars.hpp"
#include "zlineedit.hpp"
#include "command.hpp"
#include "path.hpp"

decltype(kv_alias) kv_alias;
decltype(functions) functions;
std::string script_name; bool is_script;
std::string fun_name; bool is_fun;
bool interactive_sesh, login_sesh, killed_sigexit;
int tty_pid, tty_fd;
int argc; char **argv;

/** Load a file and evaluate it.
 *
 * @param {std::string const&}str
 * @return none
 */
bool source(std::string const& str, bool err/* = true */) {
	std::ifstream f(str);
	if (!f) {
		if (err)
			perror(str.c_str());
		return false;
	} else {
		bool is_script_old = is_script, is_fun_old = is_fun;
		std::string script_name_old = script_name;
		callstack.push_back({is_fun, fun_name, is_script, script_name});
		is_fun = false;
		is_script = true; script_name = str;
		SCOPE_EXIT {
			is_fun = is_fun_old;
			is_script = is_script_old; script_name = script_name_old;
			callstack.pop_back();
		};
		eval_stream(f);
		return true;
	}
}

/** Evaluate a script and return the result.
 *
 * @param {std::string const&}str
 * @return std::string
 */
int eval_level;
std::map<int, std::string> eval_defer;
zrc_obj eval(std::string const& str) {
	auto wlst = lex(str.c_str(), SEMICOLON | SPLIT_WORDS).elems;
	return eval(wlst);
}

zrc_obj eval(std::vector<token> const& wlst) {
	pipeline ppl;
	command cmd;
	
	using pm = pipeline::proc_mode;
	using rm = pipeline::run_mode;
	auto run_pipeline = [&](rm run, pm proc) {
		ppl.pmode = proc;
		
		ppl.add_cmd(std::move(cmd)); // Just in case
		ppl.execute();

		ppl.pmode = pm::FG;
		ppl.rmode = run;
	};

	++eval_level;
	SCOPE_EXIT {
		if (eval_level > 1 && eval_defer.find(eval_level) != eval_defer.end()) {
			eval(eval_defer.at(eval_level));
			eval_defer.erase(eval_level);
		}
		--eval_level;
	};

	for (size_t i = 0; i < wlst.size(); ++i) {
		std::string conv = wlst[i];

		// Command separators
		if (wlst[i].bareword) {
			if (conv == "&&") { run_pipeline(rm::AND    , pm::FG); continue; }
			if (conv == "||") { run_pipeline(rm::OR     , pm::FG); continue; }
			if (conv == ";")  { run_pipeline(rm::NORMAL , pm::FG); continue; }
			if (conv == "&")  { run_pipeline(rm::NORMAL , pm::BG); continue; }
			if (conv == "|")  { ppl.add_cmd(std::move(cmd));       continue; }
		}
		
		// Word expansion {*}
		if (wlst[i].brac && conv == "*" && i < wlst.size()-1) {
			for (auto const& t : lex(std::string(wlst[++i]).c_str(), SPLIT_WORDS).elems)
				cmd.add_arg(std::string(t).c_str());
			continue;
		}
		cmd.add_arg(conv.c_str());
	}
	ppl.add_cmd(std::move(cmd));
	ppl.execute();
	return vars::status;
}

/** Gets logical line from input stream
 *
 * @param {std::ifstream&}in,{std::string&}str
 * @return bool
 */
static inline bool get_phrase(std::istream& in, std::string& str) {
	// State trackers
	bool single_quote = false;
	bool double_quote = false;
	bool backslash_newline = false;
	int brace = 0;
	std::stack<char> stk;

	// Working vars
	std::string buf;
	str.clear();

	while (zrc_getline(in, buf, brace || !stk.empty() || backslash_newline)) {
		backslash_newline = false;
		for (size_t i = 0; i < buf.length(); ++i) {
			if (buf[i] == '\'' && !double_quote && !brace) {
				if (!single_quote)
					stk.push('\'');
				else
					stk.pop();
				single_quote = !single_quote;
			}
			if (buf[i] == '"' && !single_quote && !brace) {
				if (!double_quote)
					stk.push('"');
				else
					stk.pop();
				double_quote = !double_quote;
			}
			if (buf[i] == '[' && !brace)
				stk.push('[');
			if (buf[i] == ']' && !brace && !stk.empty() && stk.top() == '[')
				stk.pop();
			if (buf[i] == '{' && !single_quote && !double_quote)
				++brace;
			if (buf[i] == '}' && !single_quote && !double_quote && brace)
				--brace;
			if (buf[i] == '#' && !single_quote && !double_quote && !brace)
				i = buf.length();
			if (buf[i] == '\\' && ++i == buf.length()) {
				buf.pop_back();
				backslash_newline = true;
			}
		}
		str += buf;
		if (!backslash_newline && !brace && stk.empty())
			return true;
		if (!backslash_newline)
			str += '\n';
	}
	if (brace)
		std::cerr << "Script has unclosed brace" << std::endl;
	if (!stk.empty())
		std::cerr << "Script has unclosed substitution/quote" << std::endl;
	return false;
}

/** Evaluate stream
 *
 * @param {std::istream&}in
 * @return none
 */
void eval_stream(std::istream& in) {
	std::string str;
	SCOPE_EXIT {
		if (!eval_level && eval_defer.find(1) != eval_defer.end())
			eval(eval_defer.at(1));
	};
	while (get_phrase(in, str))
		eval(str);
}

/** Set current argv to the one passed here.
 * 
 * @param {int}argc,{char**}argv
 * @return zrc_arr
 */
zrc_arr copy_argv(int argc, char **argv) {
	zrc_arr ret;
	for (int i = 0; i < argc; ++i)
		ret[std::to_string(i)] = argv[i];
	setvar("argc", numtos(argc));
	return ret;
}

/** Random message printers **/
static inline void usage() {
	std::cerr << "usage: zrc [--help|--version] [-c <script>|<file>] [<args...>]" << std::endl;
	exit(EXIT_FAILURE);
}

static inline void version() {
	std::cerr << ZVERSION << std::endl;
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	// Heavily commented, because it does so much dang stuff at once

	std::ios_base::sync_with_stdio(false);
	// It's over 9000! (not really)
	struct rlimit rlim;
	if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
		std::cerr << "warning: Could not getrlimit()\n";
	FD_MAX = rlim.rlim_cur = rlim.rlim_max;
	FD_MAX /= 2;
	if (setrlimit(RLIMIT_NOFILE, &rlim) < 0)
		std::cerr << "warning: Could not setrlimit()\n";
	// Shells have no buffering
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	// Setup arguments
	::argc = argc, ::argv = argv;
	vars::argv = copy_argv(argc, argv);
	// Setup expr
	expr::init();
#if REHASH_STARTUP == 1
	hctable = pathwalk();
#endif

	// Interactive job control stuff
	if (argc == 1) {
		// Terminal file descriptor for interactive use
		int target_fd = open("/dev/tty", O_RDWR);
		if (target_fd < 0) {
			perror("open");
			return EXIT_FAILURE;
		}
		::tty_pid = getpid();
		::tty_fd = fcntl(target_fd, F_DUPFD_CLOEXEC, FD_MAX + 1);
		if (::tty_fd < 0) {
			perror("fcntl");
			return EXIT_FAILURE;
		}
		close(target_fd);
		tty << std::unitbuf;

		// Set flags to mark this is interactive
		is_script = false;
		interactive_sesh = true;
		// Stop self till foreground	
		pid_t pgid;
		while (tcgetpgrp(::tty_fd) != (pgid = getpgrp()))
			kill(-pgid, SIGTTIN);
		if (getsid(0) != tty_pid && setpgid(0, pgid) < 0) {
			perror("setpgrp");
			return EXIT_FAILURE;
		}
		if (tcsetpgrp(::tty_fd, pgid) < 0) {
			perror("tcsetpgrp");
			return EXIT_FAILURE;
		};
	}
	// This runs for both argc == 1 and otherwise
	// Sighandlers
	int selfpipe[2];
	if (pipe(selfpipe) < 0) {
		perror("pipe");
		return EXIT_FAILURE;
	}
	moveup(selfpipe[0], selfpipe_rd);
	moveup(selfpipe[1], selfpipe_wrt);
	fcntl(selfpipe_wrt, F_SETFL, O_NONBLOCK);
	fcntl(selfpipe_rd, F_SETFL, O_NONBLOCK);
	for (auto const& it : sig2txt)
		signal2(it.first, sighandler);
	atexit([] {
		if (login_sesh) {
			auto pw = getpwuid(getuid());
			if (pw) {
				std::string filename = pw->pw_dir;
				// ~/.zrc_logout (by default)
				source(filename + "/" ZLOGOUT, false);
			}
		}
		if (interactive_sesh)
			jtable.sighupper();
		if (!killed_sigexit && sigtraps.find(SIGEXIT) != sigtraps.end())
			sigtraps.at(SIGEXIT)();
	});

	// Continue interactive logic
	if (argc == 1) {
		auto pw = getpwuid(getuid());
		if (pw) {
			std::string filename = pw->pw_dir;	
			// ~/.zrc (by default)
			source(filename + "/" ZCONF, false);
			if (argv[0][0] == '-') {
				// ~/.zrc_profile (by default)
				if (!source(filename + "/" ZLOGIN1, false))
					// ~/.zrc_login (by default)
					source(filename + "/" ZLOGIN2, false);
				login_sesh = true;
			}
		}
		eval_stream(std::cin);
	} else {
		for (auto const& it : dflsigs)
			signal2(it, SIG_DFL);
		is_script = true;
		interactive_sesh = false;
		if (!strcmp(argv[1], "--version")) version();
		if (!strcmp(argv[1], "--help")) usage();
		if (!strcmp(argv[1], "-c")) {
			if (argc == 3) {
				SCOPE_EXIT {
					if (!eval_level && eval_defer.find(1) != eval_defer.end())
						eval(eval_defer.at(1));
				};
				eval(argv[2]);
			} else
				usage();
		} else {
			// Don't use source so `caller` is empty
			std::ifstream fin(argv[1]);
			if (!fin) {
				perror(argv[1]);
				return EXIT_FAILURE;
			}
			eval_stream(fin);
			exit(stonum(vars::status));
		}
	}
	return stonum(vars::status);
}
