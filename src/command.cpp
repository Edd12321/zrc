#include "pch.hpp"
#include "custom_cmd.hpp"
#include "command.hpp"
#include "list.hpp"
#include "vars.hpp"
#include "zlineedit.hpp"
#include "sig.hpp"
#include "path.hpp"
#include "global.hpp"

std::vector<std::string> fifo_cleanup;
void do_fifo_cleanup(std::vector<std::string>& fifo_cleanup = ::fifo_cleanup) {
	for (auto const& it : fifo_cleanup) {
		unlink((it + "/" FIFO_FILNAME).c_str());
		rmdir(it.c_str());
	}
	fifo_cleanup.clear();
}
//
// class command
//
command::command(command&& cmd) {
	swap(args, cmd.args);
}

command::~command() {
	for (int i = 0; i < argc(); ++i)
		std::free(args[i]);
}

command::command(std::initializer_list<const char*> list) {
	for (auto const& elem : list)
		add_arg(elem);
}

void command::add_arg(const char *str) {
	if (str == nullptr)
		return;
	char *s = strdup(str);
	if (s == nullptr)
		throw std::bad_alloc();
	new (s) char[strlen(s) + 1];
	args.back() = s;
	args.push_back(nullptr);
}

//
// class pipeline
//
void pipeline::add_cmd(command&& cmd) {
	if (cmd.argc())
		cmds.push_back(std::move(cmd));
}

pipeline::operator std::string() {
	std::string ret;
	for (size_t i = 0; i < cmds.size(); ++i) {
		ret += list(cmds[i].argc(), cmds[i].argv()) + " ";
		if (i < cmds.size() - 1)
			ret += "| ";
	}
	return ret;
}

bool pipeline::execute_act(pplexec_flags flags /* = NORMAL */) {
	bool in_coprocess = flags & COPROCESS;
	bool in_subshell = flags & SUBSHELL;
	if (in_coprocess)
		in_subshell = true;

	int input = STDIN_FILENO, old_input = -1;
	pid_t pid, pgid = 0;
	// for reaper 
	std::vector<pid_t> pids;
	// not a subshell
	bool main_shell;

	bool need_restore = cmds.size() > 1, failed = false;
	if (need_restore)
		old_input = fcntl(input, F_DUPFD_CLOEXEC, FD_MAX + 1);
	SCOPE_EXIT {
		if (need_restore) {
			if (old_input >= 0) {
				dup2(old_input, STDIN_FILENO);
				close(old_input);
			}
			if (input != STDIN_FILENO && input >= 0)
				close(input);
		}
	};
	for (size_t i = 0; i < cmds.size() - 1; ++i) {
		int argc = cmds[i].argc();
		char **argv = cmds[i].argv();
		int pd[2];
		if (pipe(pd) < 0) {
			perror("pipe");
			failed = true;
			break;
		}

		main_shell = (interactive_sesh && getpid() == tty_pid);
		if ((pid = fork()) < 0) {
			perror("fork");
			failed = true;
			break;
		} else if (pid == 0) {
			reset_sigs();
			SCOPE_EXIT { _exit(127); };
			if (main_shell) {
				if (!pgid)
					pgid = getpid();
				setpgid(0, pgid);
			}
			if (input != STDIN_FILENO) {
				dup2(input, STDIN_FILENO);
				close(input);
			}
			dup2(pd[1], STDOUT_FILENO);
			close(pd[1]);
			close(pd[0]);
			if (old_input >= 0) {
				close(old_input);
				old_input = -1;
			}

			// If found alias, run
			auto found_alias = kv_alias.find(*argv);
			if (found_alias != kv_alias.end() && found_alias->second.active)
				_exit(std::uint8_t(atoi(found_alias->second(argc, argv).c_str())));
			
			// If found function, run (atoi used for no throw)
			auto found_fn = functions.find(*argv);
			if (found_fn != functions.end())
				_exit(std::uint8_t(atoi(found_fn->second(argc, argv).c_str())));
			
			// If found builtin, run
			auto found_builtin = builtins.find(*argv);
			if (found_builtin != builtins.end())
				_exit(std::uint8_t(atoi(found_builtin->second(argc, argv).c_str())));
			
			// Try to run as external
			auto const& map = !hctable.empty() ? hctable : pathwalk();
			auto found_cmd = map.find(*argv);
			if (found_cmd != map.end())
				execv(found_cmd->second.c_str(), argv);
			struct stat sb;
			if (strchr(*argv, '/') && !stat(*argv, &sb))
				execv(*argv, argv);

			// If that failed, run as `unknown`
			auto found_unknown = functions.find("unknown");
			if (found_unknown != functions.end()) {
				auto ret = found_unknown->second(argc, argv);
				_exit(std::uint8_t(atoi(ret.c_str())));
			}
			perror(argv[0]);
		} else {
			if (main_shell) {
				pids.push_back(pid);
				if (!pgid)
					pgid = pid;	
				if (setpgid(pid, pgid) < 0)
					perror("setpgid (pipeline)");
			}
			if (input != STDIN_FILENO)
				close(input);
			moveup(pd[0], input);
			close(pd[1]);
		}
	}
	if (input != STDIN_FILENO) {
		dup2(input, STDIN_FILENO);
		close(input);
		input = -1;
	}
	if (failed) return true;

	// Last one!
	int argc = cmds.back().argc();
	char **argv = cmds.back().argv();
	// This is literally the exec function but with extra stuff:

	auto found_alias = kv_alias.find(*argv);
	bool is_alias = found_alias != kv_alias.end() && found_alias->second.active;
	if (this->pmode == proc_mode::FG && is_alias && !in_subshell && cmds.size() == 1) {
		vars::status = found_alias->second(argc, argv);
		return true;
	}
	// If found function and FG, run (for side effects in the shell)
	auto found_fun = functions.find(*argv);
	bool is_fun = found_fun != functions.end();
	if (this->pmode == proc_mode::FG && is_fun && !in_subshell && cmds.size() == 1) {
		vars::status = found_fun->second(argc, argv);
		return true;
	}
	// If found builtin and FG, run (for side effects in the shell)
	auto found_builtin = builtins.find(*argv);
	bool is_builtin = found_builtin != builtins.end();
	if (this->pmode == proc_mode::FG && is_builtin && !in_subshell && cmds.size() == 1) {
		vars::status = found_builtin->second(argc, argv);
		return true;
	}
	// Try to run as external/BG
	std::string full_path;
	enum stuff_known {
		IS_NOTHING,
		IS_UNKNOWN,
		IS_ALIAS,
		IS_FUNCTION,
		IS_BUILTIN,
		IS_COMMAND_PATH,
		IS_COMMAND_FILE,
	} ok = IS_NOTHING;
	if (is_alias)
		ok = IS_ALIAS;
	else if (is_fun)
		ok = IS_FUNCTION;
	else if (is_builtin)
		ok = IS_BUILTIN;
	else {
		struct stat sb;
		if (strchr(*argv, '/') && !stat(*argv, &sb))
			ok = IS_COMMAND_FILE;
		else {	
			auto const& map = !hctable.empty() ? hctable : pathwalk();
			auto found_cmd = map.find(*argv);
			if (found_cmd != map.end()) {
				ok = IS_COMMAND_PATH;
				full_path = found_cmd->second;
			} else if (functions.find("unknown") != functions.end())
				ok = IS_UNKNOWN;
		}
	}
	if (ok == IS_UNKNOWN && this->pmode == proc_mode::FG && !in_subshell)
		vars::status = functions.at("unknown")(argc, argv);
	else {
		main_shell = (interactive_sesh && getpid() == tty_pid);
		int c2p[2], p2c[2];
		if (in_coprocess) {
			if (pipe(c2p) < 0) {
				perror("pipe");
				return 1;
			}
			if (pipe(p2c) < 0) {
				perror("pipe");
				return 1;
			}
		}
		if ((pid = fork()) < 0) {
			perror("fork");
		} else if (pid == 0) {
			SCOPE_EXIT { _exit(127); }; // just in case
			if (in_coprocess) {
				dup2(p2c[0], STDIN_FILENO);
				dup2(c2p[1], STDOUT_FILENO);
				close(c2p[0]); close(p2c[1]);
				close(c2p[1]); close(p2c[0]);
			}
			reset_sigs();
			if (main_shell) {
				if (!pgid)
					pgid = getpid();
				setpgid(0, pgid);
			}
			if (old_input >= 0) {
				close(old_input);
				old_input = -1;
			}
			switch (ok) {
				case IS_ALIAS: _exit(std::uint8_t(atoi(found_alias->second(argc, argv).c_str())));
				case IS_FUNCTION: _exit(std::uint8_t(atoi(found_fun->second(argc, argv).c_str())));
				case IS_BUILTIN: _exit(std::uint8_t(atoi(found_builtin->second(argc, argv).c_str())));
				case IS_COMMAND_FILE: execv(*argv, argv); perror(*argv); _exit(127);
				case IS_COMMAND_PATH: execv(full_path.c_str(), argv); perror(*argv); _exit(127);
				case IS_UNKNOWN: _exit(std::uint8_t(atoi(functions.at("unknown")(argc, argv).c_str())));
				case IS_NOTHING: errno = ENOENT; perror(*argv); _exit(127);
			}
		} else {
			pids.push_back(pid);
			if (in_coprocess) {
				coproc_in = c2p[0]; vars::amap[coproc_name]["0"] = numtos(c2p[0]);
				coproc_out = p2c[1]; vars::amap[coproc_name]["1"] = numtos(p2c[1]);
				close(c2p[1]); close(p2c[0]);
			}
			if (main_shell) {
				if (!pgid)
					pgid = pid;
				if (setpgid(pid, pgid) < 0 && errno != EACCES && errno != ESRCH)
					perror("setpgid (final)");
				
				if (this->pmode == proc_mode::FG) {
					// Foreground jobs transfer the terminal control to the child
					if (tcsetpgrp2(pgid) < 0)
						perror("tcsetpgrp #1");
					kill(-pgid, SIGCONT);
					jtable.add_job(std::move(*this), std::move(pids));
#if WINDOWS
					struct timespec ts = { CYG_HACK_TIMEOUT }; // 4 ms
					nanosleep(&ts, nullptr); // no idea.
#endif
					jtable.reaper(-pgid, WUNTRACED);
					if (tcsetpgrp2(getpgrp()) < 0)
						perror("tcsetpgrp #2");
				} else {
					auto jid = jtable.add_job(std::move(*this), std::move(pids));	
					tty << '[' << jid << "]\t" << pgid << std::endl;
				}
			} else {
				jtable.add_job(std::move(*this), std::move(pids));
				if (this->pmode == proc_mode::FG)
					jtable.reaper(pid, WUNTRACED);
			}
		}
	}
	return true;
}

void pipeline::execute() {
	selfpipe_trick();
	if (cmds.empty()) {
		if (!jtable.empty())
			jtable.reaper();
		return;
	}
	SCOPE_EXIT {
		if (eval_level == 1 && line_edit::in_prompt
		&&  cmds.size() == 1 && pmode == proc_mode::FG
		&&  !fifo_cleanup.empty())
			do_fifo_cleanup();
		cmds.clear();
	};
	switch (rmode) {
		case run_mode::AND: !stonum(vars::status) && execute_act(); break;
		case run_mode::OR:  !stonum(vars::status) || execute_act(); break;
		case run_mode::NORMAL:                       execute_act(); break; 
	}
	selfpipe_trick();
	if (!jtable.empty())
		jtable.reaper();
}

//
// class job_table
//

int job_table::add_job(pipeline&& ppl, std::vector<pid_t>&& pids) {
	jc = jid2job.empty() ? 1 : ((*jid2job.rbegin()).first + 1);
	jid2job[jc].ppl = std::move(ppl);
	for (auto const& it : pids)
		pid2jid[it] = jc;
	jid2job[jc].pids = std::move(pids);
	jid2job[jc].fifo_cleanup = std::move(fifo_cleanup);
	fifo_cleanup.clear();
	return jc;
}

void job_table::sighupper() {
	for (auto const& it : jid2job) {
		kill(-it.second.pids[0], SIGHUP);
		kill(-it.second.pids[0], SIGCONT);
	}
}

void job_table::reaper(pid_t who, int how) {
	auto main_shell = (interactive_sesh && getpid() == tty_pid);
	for (;;) {
		int status, wp = waitpid(who, &status, how);
		if (wp == -1) {
			if (errno == EINTR)
				continue;
			if (errno == ECHILD)
				break;
			perror("waitpid (reaper)");
			break;
		}
		if (wp == 0)
			break;
		auto at_jid = pid2jid.find(wp);
		if (at_jid == pid2jid.end())
			continue;
		int jid = at_jid->second;
		job_table::job& job = jid2job.at(jid);
		bool lproc = wp == job.pids.back();
		
		if (WIFSTOPPED(status)) {
			if ((lproc || who < -1) && main_shell)
				tty << "[" << jid << "] Stopped\t" << (std::string)job.ppl << std::endl;
			if (who < -1)
				return;
		} else {
			pid2jid.erase(wp);
			bool sigd = WIFSIGNALED(status);
			if (sigd) {
				if (lproc || who < -1) {
					int sig = WTERMSIG(status);
					if (main_shell)
						tty << "[" << jid << "] " << strsignal(sig)
						    << " (" << sig2txt.at(sig) << ")\t" << (std::string)job.ppl << std::endl;
					if (job.ppl.pmode == pipeline::proc_mode::FG)
						vars::status = numtos(128 + sig);
				}
			} else if (lproc && WIFEXITED(status)) {
				if (job.ppl.pmode == pipeline::proc_mode::FG)
					vars::status = numtos(WEXITSTATUS(status));
				else if (main_shell)
					tty << "[" << jid << "] Done\t" << (std::string)job.ppl << std::endl;
			}
			for (size_t i = 0; i < job.pids.size(); ++i) {
				if (job.pids[i] == wp) {
					job.pids.erase(job.pids.begin() + i);
					break;
				}
			}
			if ((sigd && who < -1) || job.pids.empty()) {
				do_fifo_cleanup(job.fifo_cleanup);
				jid2job.erase(jid);
			}
		}
	}
}

void job_table::reaper() {
	reaper(WAIT_ANY, WNOHANG | WUNTRACED);
}

void job_table::disown(int jid) {
	jid2job.erase(jid);
}

std::ostream& operator<<(std::ostream& out, job_table& jt) {
	using pp = pipeline::proc_mode;
	for (auto& it : jt.jid2job) {
		auto& v = it.second;
		out << it.first << ' ' << v.pids[0];
		if (v.ppl.pmode == pp::BG)
			out << " bg ";
		else out << " fg ";
		out << (std::string)v.ppl << std::endl;
	}
	return out;
}

job_table jtable;

//
// struct new_fd
//
new_fd::new_fd(int fd) {
	index = fcntl(fd, F_DUPFD_CLOEXEC, FD_MAX + 1);
	if (index < 0 && errno != EBADF) {
		perror("fcntl");
		exit(EXIT_FAILURE);
	}
}

new_fd::~new_fd() noexcept {
	if (index >= 0)
		close(index);
}

new_fd& new_fd::operator=(new_fd&& rhs) {
	if (this != &rhs) {
		if (index >= 0)
			close(index);
		index = rhs.index;
		rhs.index = -1;
	}
	return *this;
}

int FD_MAX;

//
// Remaining code
//
int moveup(int who, int& where) {
	int ret;
	do
		ret = fcntl(who, F_DUPFD_CLOEXEC, FD_MAX + 1);
	while (ret == -1 && errno == EINTR);
	if (ret != -1) {
		close(who);
		where = ret;
	}
	return ret;
}
int moveup(int& both) {
	return moveup(both, both);
}

/** Run a function with no args.
 *
 * @param {std::string const&}str
 * @return none
 */
bool run_function(std::string const& str) {
	auto found_fn = functions.find(str);
	if (found_fn != functions.end()) {
		invoke(found_fn->second, {str.c_str(), nullptr});
		return true;
	}
	return false;
}

/** Execute an external command (without forking)
 *
 * @param {int}argc,{char**}argv
 * @return void
 */
void exec_extern(int argc, char **argv) {
	auto largv = argv[argc];
	argv[argc] = nullptr;
	auto found_cmd = hctable.find(*argv);
	if (found_cmd != hctable.end()) {
		if (execv(found_cmd->second.c_str(), argv)) {
			perror(*argv);
			_exit(127);
		}
	}
	if (execvp(*argv, argv)) {
		argv[argc] = largv;
		perror(*argv);
		_exit(127);
	}
}

/** Run arguments as a function, builtin or external command.
 *
 * @param {int}argc,{char**}argv
 * @return none
 */
zrc_obj exec(int argc, char **argv) {
	command cmd;
	for (int i = 0; i < argc; ++i)
		cmd.add_arg(argv[i]);
	pipeline ppl;
	ppl.pmode = pipeline::proc_mode::FG;
	ppl.add_cmd(std::move(cmd));
	ppl.execute_act();
	return vars::status;
}

// Eval-or-exec behaviour on arguments
void eoe(int argc, char **argv, int i) {
	if (argc == i+1) 
		eval(argv[i]);
	else
		exec(argc-i, argv+i);
}

/** Forks, launches a script and return the output from the process.
 * 
 * @param {std::string const&}str
 * @return std::string
 */
std::string get_output(std::string const& str) {
	std::string ret_str;
	ret_str.reserve(RESERVE_STR); // Try to speed up just a bit
	int pd[2];

	pipe(pd);
	pid_t pid = fork();
	if (pid == 0) {
		reset_sigs();
		dup2(pd[1], STDOUT_FILENO);
		close(pd[0]);
		close(pd[1]);
		auto st = stonum(vars::status);
		SCOPE_EXIT { _exit(std::uint8_t(!isfinite(st) ? 0 : st)); };
		eval(str);
	} else {
		close(pd[1]);
		char buf[BUFSIZ];
		int n;
		while ((n = read(pd[0], buf, sizeof buf)) >= 1)
			ret_str.append(buf, n);
		close(pd[0]);
		jtable.reaper(pid, 0);
	}
	return ret_str;
}


/** General purpose redirector.
 *
 * @param {int}argc,{char**}argv,{redir_flags}flags
 * @return int
 */
zrc_obj redir(int argc, char **argv, int fd, redir_flags flags) {
	if (argc < 3) {
_syn_error_redir:
		std::cerr << "syntax error: " << argv[0];
		if (flags & OPTFD_Y)
			std::cerr << " [<fd>]";
		std::cerr << " <file> <eoe>\n";
		return "1";
	}

	if (flags & OPTFD_Y) {
		auto x = stonum(argv[1]);
		if (isfinite(x)) {
			if (x < 0 || x > FD_MAX) {
				std::cerr << "error: Bad file descriptor " << x << '\n';
				return "2";
			}
			fd = x, --argc, ++argv;
			if (argc < 2)
				goto _syn_error_redir;
		}
	}

	int fflags = 0, ffd;
	if (flags & OVERWR) fflags = (O_TRUNC | O_WRONLY | O_CREAT);
	if (flags & APPEND) fflags = (O_WRONLY | O_CREAT | O_APPEND);
	if (flags & READFL) fflags = (O_RDONLY);
	if (flags & RDWRFL) fflags = (O_RDWR | O_CREAT);
	if (flags & NO_CLOBBER)
		fflags |= O_EXCL;

	//
	// Networking like in bash (udp/tcp)
	//
	char *found;
	std::string addr, port;
	bool tcp = !strncmp(argv[1], "/dev/tcp/", 9);
	bool udp = !strncmp(argv[1], "/dev/udp/", 9);
	if (tcp || udp) {
		if ((found = strchr(argv[1] + 9, '/')) && *(found + 1)) {
			*found = '\0', addr = argv[1] + 9, *found = '/';
			port = found + 1;
		} else tcp = udp = false;
	}
	if (tcp || udp) {
		// stolen from getaddrinfo manpage
		struct addrinfo *result, *rp;
		struct addrinfo hints{};
		hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_ADDRCONFIG;
		hints.ai_protocol = 0;
		hints.ai_socktype = tcp ? SOCK_STREAM : SOCK_DGRAM;
		int s = getaddrinfo(addr.c_str(), port.c_str(), &hints, &result);
		if (s != 0) {
			std::cerr << "getaddrinfo: " << gai_strerror(s) << std::endl;
			return "4";
		}
		for (rp = result; rp; rp = rp->ai_next) {
			ffd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (ffd == -1)
				continue;
			if (connect(ffd, rp->ai_addr, rp->ai_addrlen) != -1)
				break;
			close(ffd);
		}
		freeaddrinfo(result);
		if (!rp) {
			std::cerr << argv[0] << ": Could not connect\n";
			return "4";
		}
	//
	// Normal
	//
	} else {
		ffd = open(argv[1], fflags, S_IWUSR | S_IRUSR);
		if (ffd < 0) {
			perror(argv[1]);
			return "3";
		}
	}
	new_fd nfd(fd);
	dup2(ffd, fd);
	SCOPE_EXIT {
		close(ffd);
		dup2(nfd, fd);
	};
	eoe(argc, argv, 2);
	return vars::status;
}

/** Perform process substitution/pipeline branching.
 *
 * @param {std::string const&}str
 * @return std::string
 */
std::string get_fifo(std::string const& str) {
	char temp[] = FIFO_DIRNAME;
	std::string fifo_name = mkdtemp(temp);
	std::string fifo_file = fifo_name+"/" FIFO_FILNAME;
	
	mkfifo(fifo_file.c_str(), S_IRUSR | S_IWUSR);
	pid_t pid = fork();
	if (pid == 0) {
		reset_sigs();
		int fd = open(fifo_file.c_str(), O_WRONLY);
		if (fd < 0) {
			perror("open");
			_exit(1);
		}
		dup2(fd, STDOUT_FILENO);
		eval(str);
		close(fd);
		_exit(0);
	} else {
		fifo_cleanup.push_back(fifo_name);
		return fifo_file;
	}
}
