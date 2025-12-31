#include <sys/resource.h>
#include <sys/stat.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>

#include <fstream>
#include <istream>
#include <iostream>
#include <string>
#include <set>

std::string script_name; bool is_script;
std::string fun_name; bool is_fun;
int argc; char **argv;
#include "globals.hpp"
#include "config.hpp"

#include "syn.cpp"
#include "expr.cpp"
#include "vars.cpp"
#include "list.cpp"
#include "dispatch.cpp"
#include "command.cpp"
#include "zlineedit.cpp"

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

/** Walk through $PATH and return its contents
 *
 * @param none
 * @return std::vector<std::string>
 */
std::unordered_map<std::string, std::string> pathwalk() {
	std::stringstream iss;
	std::string tmp;
	std::unordered_map<std::string, std::string> ret_val;
#if WINDOWS
	std::vector<std::string> pathext;
	std::set<std::string> replaceable;
	iss << vars::PATHEXT;
	while (getline(iss, tmp, ';'))
		pathext.push_back(tmp);
	iss.str(std::string());
	iss.clear();
#endif
	iss << vars::PATH;
	auto is_ok_file = [&](std::string const& name, std::string const& short_name) {
		struct stat sb;	
		return (stat(name.c_str(), &sb) == 0)
		&&     (S_ISREG(sb.st_mode))
		&&     (sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		&&     (
#if WINDOWS
		           replaceable.find(short_name) != replaceable.end() ||
#endif
		           ret_val.find(short_name) == ret_val.end()
	           );
	};
	while (getline(iss, tmp, ':')) {
		struct dirent *entry;
		DIR *d = opendir(tmp.c_str());
		if (d) {
			SCOPE_EXIT { closedir(d); };
			while ((entry = readdir(d))) {
				std::string short_name = entry->d_name;
				std::string full_name = tmp + "/" + short_name;
				if (is_ok_file(full_name, short_name)) {
					ret_val[short_name] = full_name;
#if WINDOWS
					bool has_ext = false;
					for (auto const& ext : pathext) {
						if (short_name.length() > ext.length()) {
							size_t dif = short_name.length() - ext.length();
							std::string suff = short_name.substr(dif);
							if (!strcasecmp(suff.c_str(), ext.c_str())) {
								has_ext = true;
								std::string file_no_ext = short_name.substr(0, dif);
								replaceable.insert(file_no_ext);
								ret_val[file_no_ext] = full_name;
							}
						}
					}
					if (!has_ext)
						replaceable.erase(short_name);
#endif			
				}
			}
		}
	}
	return ret_val;
}


/** Evaluate a script and return the result.
 *
 * @param {std::string const&}str
 * @return std::string
 */
static inline std::string eval(std::string const& str) {
	auto wlst = lex(str.c_str(), SEMICOLON | SPLIT_WORDS).elems;
	return eval(wlst);
}

/** Tcsetpgrp replacement **/
int tcsetpgrp2(pid_t pgid) {
	sigset_t mask, old;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTTOU);
	sigaddset(&mask, SIGTTIN);
	sigaddset(&mask, SIGTSTP);
	sigprocmask(SIG_BLOCK, &mask, &old);
	int ret_val = tcsetpgrp(tty_fd, pgid);
	sigprocmask(SIG_SETMASK, &old, NULL);
	return ret_val;
}

static inline zrc_obj eval(std::vector<token> const& wlst) {
	pipeline ppl;
	command cmd;
	bool can_alias = true;
	using pm = pipeline::ppl_proc_mode;
	using rm = pipeline::ppl_run_mode;
	auto run_pipeline = [&](rm run, pm proc) {
		ppl.pmode = proc;
		
		ppl.add_cmd(cmd); // Just in case
		ppl.execute();

		ppl.pmode = pm::FG;
		ppl.rmode = run;
	};

	for (size_t i = 0; i < wlst.size(); ++i) {
		std::string conv = wlst[i];

		// Command separators
		if (wlst[i].bareword) {
			if (conv == "&&") { can_alias = true; run_pipeline(rm::AND    , pm::FG); continue; }
			if (conv == "||") { can_alias = true; run_pipeline(rm::OR     , pm::FG); continue; }
			if (conv == ";")  { can_alias = true; run_pipeline(rm::NORMAL , pm::FG); continue; }
			if (conv == "&")  { can_alias = true; run_pipeline(rm::NORMAL , pm::BG); continue; }
			if (conv == "|")  { can_alias = true; ppl.add_cmd(cmd);                  continue; }
		}
		
		// Word expansion {*}
		if (wlst[i].brac && conv == "*" && i < wlst.size()-1) {
			for (auto const& t : lex(std::string(wlst[++i]).c_str(), SPLIT_WORDS).elems)
				cmd.add_arg(std::string(t).c_str());
			continue;
		}
		cmd.add_arg(conv.c_str());
	}
	ppl.add_cmd(cmd);
	ppl.execute();
	return vars::status;
}

/** Gets logical line from input stream
 *
 * @param {std::ifstream&}in,{std::string&}str
 * @return bool
 */
bool get_phrase(std::istream& in, std::string& str) {
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
static inline void eval_stream(std::istream& in) {
	std::string str;
	while (get_phrase(in, str))
		eval(str);
}

/** Set current argv to the one passed here.
 * 
 * @param {int}argc,{char**}argv
 * @return zrc_arr
 */
zrc_arr copy_argv(int argc, char *argv[]) {
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

int main(int argc, char *argv[]) {
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
	selfpipe_rd = fcntl(selfpipe[0], F_DUPFD_CLOEXEC, FD_MAX + 1); close(selfpipe[0]);
	selfpipe_wrt = fcntl(selfpipe[1], F_DUPFD_CLOEXEC, FD_MAX + 1); close(selfpipe[1]);
	fcntl(selfpipe_wrt, F_SETFL, O_NONBLOCK);
	fcntl(selfpipe_rd, F_SETFL, O_NONBLOCK);
	for (auto const& it : sig2txt)
		signal(it.first, sighandler);
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
			sighupper();
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
			signal(it, SIG_DFL);
		is_script = true;
		interactive_sesh = false;
		if (!strcmp(argv[1], "--version")) version();
		if (!strcmp(argv[1], "--help")) usage();
		if (!strcmp(argv[1], "-c")) {
			if (argc == 3)
				eval(argv[2]);
			else
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
