#pragma GCC optimize("O3")
#include <sys/resource.h>
#include <sys/stat.h>
#include <fstream>
#include <pwd.h>

#include <stdlib.h>
#include <istream>
#include <iostream>
#include <string>

std::string script_name; bool is_script;
std::string fun_name; bool is_fun;
int argc; char **argv;
#include "globals.hpp"
#include "config.hpp"

#include "syn.cpp"
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
		auto cleanup = make_scope_exit([&]() {
			is_fun = is_fun_old;
			is_script = is_script_old; script_name = script_name_old;
			callstack.pop_back();
		});
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
	std::istringstream iss{getvar(PATH)};
	std::string tmp;
	std::unordered_map<std::string, std::string> ret_val;
	struct stat sb;
	
	while (getline(iss, tmp, ':')) {
		struct dirent *entry;
		DIR *d = opendir(tmp.c_str());
		if (d) {
			auto cleanup = make_scope_exit([&](){closedir(d);});
			while ((entry = readdir(d))) {
				std::string short_name = entry->d_name;
				std::string full_name = tmp + "/" + short_name;
				if ((!stat(full_name.c_str(), &sb))
				&&  (S_ISREG(sb.st_mode))
				&&  (sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
				&&  (ret_val.find(short_name) == ret_val.end()))
					ret_val[short_name] = full_name;
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

		// Aliases
		if (can_alias == true && !cmd.argc() && kv_alias.find(conv) != kv_alias.end()) {
			can_alias = false;
			auto alst = lex(kv_alias[conv].c_str(), SEMICOLON | SPLIT_WORDS).elems;
			wlst.insert(wlst.begin()+i+1, alst.begin(), alst.end());
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
	std::ios_base::sync_with_stdio(false);
	// It's over 9000! (not really)
	struct rlimit rlim;
	if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
		std::cerr << "warning: Could not getrlimit()\n";
	FD_MAX = rlim.rlim_cur = rlim.rlim_max;
	FD_MAX /= 2;
	if (setrlimit(RLIMIT_NOFILE, &rlim) < 0)
		std::cerr << "warning: Could not setrlimit()\n";

	// For PGID stuff
	new_fd tty_fd(STDOUT_FILENO);
	::tty_fd = tty_fd;
	setpgrp();

	// Shells have no buffering
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;

	// Setup arguments
	::argc = argc, ::argv = argv;
	vars::argv = copy_argv(argc, argv);

	// Setup getopts stuff
	setvar("opterr", std::to_string(opterr));
	setvar("optind", std::to_string(optind));
	
	// Sighandlers
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
		run_function("sigexit");
	});	
	signal(SIGCHLD, [](int sig) {
		chld_notif = true;
		run_function("sigchld");
	});
	if (argc == 1) {
		is_script = false;
		interactive_sesh = true;
		tcsetpgrp(0, 0);
		signal(SIGTTOU, SIG_IGN);
		signal(SIGINT, [](int sig) { run_function("sigint"); });
		signal(SIGTSTP, [](int sig) { run_function("sigtstp"); });
		signal(SIGHUP, [](int sig) {
			if (functions.find("sighup") != functions.end())
				run_function("sighup");
			else {
				sighupper();
				exit(129);
			}
		});
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
		is_script = true;
		interactive_sesh = false;
		if (!strcmp(argv[1], "--version")) version();
		if (!strcmp(argv[1], "--help")) usage();
		if (!strcmp(argv[1], "-c")) {
			if (argc == 3)
				eval(argv[2]);
			else
				usage();
		} else if (!source(argv[1]))
			return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
