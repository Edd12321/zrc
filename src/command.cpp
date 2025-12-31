#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <semaphore.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <functional>
#include <vector>
/** Check if a file descriptor is valid
 *
 * @param {int}fd
 * @return bool
 */
static inline bool good_fd(int fd) {
	return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

/** Run a function with no args.
 *
 * @param {std::string const&}str
 * @return none
 */
bool run_function(std::string str) {
	if (functions.find(str) != functions.end()) {
		invoke(functions.at(str), {str.c_str(), nullptr});
		return true;
	}
	return false;
}

/** General purpose redirector.
 *
 * @param {int}argc,{char**}argv,{redir_flags}flags
 * @return int
 */
static inline zrc_obj redir(int argc, char *argv[], int fd, redir_flags flags) {
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
		if (!isnan(x)) {
			if (x < 0 || x > FD_MAX) {
				std::cerr << "error: Bad file descriptor " << x << '\n';
				return "2";
			}
			fd = x, --argc, ++argv;
			if (argc < 2)
				goto _syn_error_redir;
		}
	}

	if ((flags & NO_CLOBBER) && !access(argv[1], F_OK)) {
		std::cerr << "Cannot clobber " << argv[1] << std::endl;
		return "3";
	}

	int fflags;
	if (flags & OVERWR) fflags = (O_TRUNC | O_WRONLY | O_CREAT);
	if (flags & APPEND) fflags = (O_WRONLY | O_CREAT | O_APPEND);
	if (flags & READFL) fflags = (O_RDONLY);

	int ffd = open(argv[1], fflags, S_IWUSR | S_IRUSR);
	if (ffd < 0) {
		perror(argv[1]);
		return "4";
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

/** Execute an external command (without forking)
 *
 * @param {int}argc,{char**}argv
 * @return void
 */
void exec_extern(int argc, char *argv[]) {
	auto largv = argv[argc];
	argv[argc] = nullptr;
	if (hctable.find(*argv) != hctable.end()) {
		if (execv(hctable[*argv].c_str(), argv)) {
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
zrc_obj exec(int argc, char *argv[]) {
	// If found builtin, run
	if (kv_alias.find(*argv) != kv_alias.end()) {
		auto& at = kv_alias.at(*argv);
		if (at.active)
			vars::status = at(argc, argv);
	}
	// If found function, run
	else if (functions.find(*argv) != functions.end())
		vars::status = functions.at(*argv)(argc, argv);
	// If found builtin, run
	else if (builtins.find(*argv) != builtins.end())
		vars::status = builtins.at(*argv)(argc, argv);
	// Try to run as external
	else {
		auto const& map = !hctable.empty() ? hctable : pathwalk();
		if (map.find(*argv) != map.end()) {
			pid_t pid = fork();
			if (pid == 0) {
				reset_sigs();
				execv(map.at(*argv).c_str(), argv);
				perror(*argv);
				_exit(127);
			} else {
				reaper(pid, WUNTRACED);
			}
		} else if (functions.find("unknown") != functions.end())
			vars::status = functions.at("unknown")(argc, argv);
		else {
			errno = ENOENT;
			perror(*argv);
			vars::status = "127";
		}
	}
	selfpipe_trick();
	return vars::status;
}

/** Forks, launches a script and return the output from the process.
 * 
 * @param {std::string const&}str
 * @return std::string
 */
static inline std::string get_output(std::string const& str) {
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
		eval(str);
		_exit(stonum(vars::status));
	} else {
		close(pd[1]);
		char buf[BUFSIZ];
		int n;
		while ((n = read(pd[0], buf, sizeof buf)) >= 1)
			ret_str.append(buf, n);
		close(pd[0]);
		reaper(pid, 0);
	}
	return ret_str;
}

std::vector<std::string> fifo_cleanup;

/** Perform process substitution/pipeline branching.
 *
 * @param {std::string const&}str
 * @return std::string
 */
static inline std::string get_fifo(std::string const& str) {
	char temp[] = FIFO_DIRNAME;
	std::string fifo_name = mkdtemp(temp);
	std::string fifo_file = fifo_name+"/" FIFO_FILNAME;
	
	mkfifo(fifo_file.c_str(), S_IRUSR | S_IWUSR);
	pid_t pid = fork();
	if (pid == 0) {
		reset_sigs();
		int fd = open(fifo_file.c_str(), O_WRONLY);
		dup2(fd, STDOUT_FILENO);
		eval(str);
		close(fd);
		_exit(0);
	} else {
		fifo_cleanup.push_back(fifo_name);
		return fifo_file;
	}
}

class command {
private:
	std::vector<char*> args = {nullptr};
public:
	command() = default;
	command(command const&) = delete;
	command& operator=(command const&) = delete;
	command& operator=(command&&) = delete;
	command(command&& cmd) {
		swap(args, cmd.args);
	};

	~command() {
		for (int i = 0; i < argc(); ++i)
			free(args[i]);
	}
public:
	command(std::initializer_list<const char*> list) {
		for (auto const& elem : list)
			add_arg(elem);
	}
	// note: NEVER call .add_arg() after .argv()
	inline char **argv() noexcept { return args.data(); }
	inline int argc() const noexcept { return args.size() - 1; }
	inline void add_arg(const char *str) {
		if (!str)
			return;
		char *s = strdup(str);
		if (!s) throw std::bad_alloc();
		args.back() = s;
		args.push_back(nullptr);
	}
};

template<typename Fun>
zrc_obj invoke(Fun& f, std::initializer_list<const char*> list) {
	command cmd(list);
	return f(cmd.argc(), cmd.argv());
}

template<typename Fun>
void invoke_void(Fun const& f, std::initializer_list<const char*> list) {
	command cmd(list);
	f(cmd.argc(), cmd.argv());
}

static inline zrc_obj exec(command& cmd) {
	return exec(cmd.argc(), cmd.argv());
}

class pipeline {
public:
	enum ppl_run_mode {
		AND, OR, NORMAL
	} rmode = NORMAL;
	enum ppl_proc_mode {
		BG, FG
	} pmode = FG;
private:
	inline bool execute_act();
public:
	void execute();
	std::vector<command> cmds;

	inline void add_cmd(command& cmd) {
		if (cmd.argc())
			cmds.push_back(std::move(cmd));
	}

	operator std::string() {
		std::string ret;
		for (size_t i = 0; i < cmds.size(); ++i) {
			ret += list(cmds[i].argc(), cmds[i].argv()) + " ";
			if (i < cmds.size()-1)
				ret += "| ";
		}
		return ret;
	}
};

/***********************************
 *                                 *
 *   Job stuff is written below    *
 * (This should really be a class) *
 *                                 *
 ***********************************/
// Job entry table
struct job {
	std::string ppl;
	pipeline::ppl_proc_mode state;
	pid_t pgid, last_pid;
	std::set<pid_t> pids;
};
std::map<int, job> jobs;
std::map<pid_t, int> pid2jid;
unsigned long long jc;

/** Add a new entry to the job table
 *
 * @param {pipeline const&}ppl,{pipeline::ppl_proc_mode}status,{pid_t}pgid
 * @return int
 */
int add_job(pipeline& ppl, pipeline::ppl_proc_mode status, pid_t pgid, pid_t last_pid, std::set<pid_t>& pids) {
	jc = jobs.empty() ? 1 : ((*jobs.rbegin()).first + 1);
	
	jobs[jc].ppl = (std::string)ppl;
	jobs[jc].state = status;
	jobs[jc].pgid = pgid;
	jobs[jc].last_pid = last_pid;
	for (auto const& it : pids)
		pid2jid[it] = jc;
	jobs[jc].pids = std::move(pids);
	return jc;
}

/** Hang up in interactive mode
 *
 * @param none
 * @return void
 */
void sighupper() {
	for (auto const& j : jobs) {
		auto const& job = j.second;
		kill(-job.pgid, SIGHUP);
		kill(-job.pgid, SIGCONT);
	}
}

/** Reap zombie processes.
 *
 * @param none
 * @return void
 */
void reaper(pid_t who, int how) {
	pid_t pid;
	int status;
	for (;;) {
		pid = waitpid(who, &status, how);
		if (pid < 0) {
			if (errno == EINTR)
				continue; 
			if (errno == ECHILD)
				break;
			perror("waitpid (reaper)");
			break; 
		}
		if (pid == 0) 
			break;
		if (pid2jid.find(pid) == pid2jid.end()) {
			if (WIFEXITED(status))
				vars::status = numtos(WEXITSTATUS(status));
			else if (WIFSIGNALED(status))
				vars::status = numtos(128 + WTERMSIG(status));
			continue;
		}
		int jid = pid2jid.at(pid);
		auto& job = jobs[jid];
		if (WIFSTOPPED(status)) {
			if (interactive_sesh)
				tty << '[' << jid << "] Stopped" << std::endl;
			if (/*who == -job.pgid && */!(how & WNOHANG))
				return;
			continue; 
		}
		job.pids.erase(pid);
		pid2jid.erase(pid);
		bool lproc = pid == job.last_pid;
		if (lproc && job.state == pipeline::ppl_proc_mode::FG) {
			if (WIFEXITED(status))
				vars::status = numtos(WEXITSTATUS(status));
			else if (WIFSIGNALED(status))
				vars::status = numtos(128 + WTERMSIG(status));
		}
		if (job.pids.empty()) {
			if (interactive_sesh) {
				if (job.state == pipeline::ppl_proc_mode::BG && WIFEXITED(status))
					tty << '[' << jid << "] Done" << std::endl;
				else if (WIFSIGNALED(status))
					tty << '[' << jid << "] " << strsignal(WTERMSIG(status)) << std::endl;
			}
			jobs.erase(jid);
			if (who < -1)
				break;
		}
	}
}

void reaper() {
	reaper(WAIT_ANY, WNOHANG | WUNTRACED);
}

void reset_sigs() {
	for (auto const& it : txt2sig) {
		int sig = it.second;
		if (sig == SIGEXIT)
			killed_sigexit = true;
		else signal(sig, SIG_DFL);
	}	
}

void jobstate(int job, int st) { jobs[job].state = static_cast<pipeline::ppl_proc_mode>(st); }
// hacks that only exist because we have only one TU:
pid_t jobpgid(job const& j) { return j.pgid; }
bool jobexists(int jid) { return jobs.find(jid) != jobs.end(); }
bool nojobs() { return jobs.empty(); }

/** Display job table.
 *
 * @param none
 * @return void
 */
inline void show_jobs() {
	using pp = pipeline::ppl_proc_mode;
	for (auto const& it : jobs) {
		auto& v = it.second;
		std::cout << it.first << ' ' << v.pgid << ' ';
		if (v.state == pp::BG)
			std::cout << "bg ";
		else std::cout << "fg ";
		std::cout << v.ppl << std::endl;
	}
}

inline void disown_job(int n) {
	if (jobs.find(n) != jobs.end())
		jobs.erase(n);
}


// Self pipe trick for signal safe trapping
int selfpipe_rd, selfpipe_wrt;
struct pollfd selfpipe_wait;

void selfpipe_trick() {
	static struct pollfd selfpipe_wait;
	selfpipe_wait.fd = selfpipe_rd;
	selfpipe_wait.events = POLLIN;
	
	int sig, ret;
	while ((ret = poll(&selfpipe_wait, 1, 0)) > 0 && (selfpipe_wait.revents & POLLIN)) {
		while (read(selfpipe_rd, &sig, sizeof sig) == sizeof sig) {
			if (sigtraps.find(sig) == sigtraps.end()) {
				if (sig == SIGHUP) {
					sighupper();
					exit(129);
				}
				continue;
			}
			if (sig == SIGCHLD && interactive_sesh && !jobs.empty())
				reaper();
			auto& trap = sigtraps.at(sig);
			if (trap.active)
				trap();
		}
	}
}

/** Executes a pipeline.
 *
 * @param none
 * @return none
 */
// !!! Beware of dragons !!!
inline bool pipeline::execute_act() {
	int input = STDIN_FILENO, old_input = -1;
	pid_t pid, pgid = 0;
	// for reaper 
	std::set<pid_t> pids;
	// not a subshell
	bool main_shell;
	// release children after tcsetpgrp
	sem_t *sem;
	bool sem_inited = false;

	auto init_semaphore = [&]() {
		sem = (sem_t*)mmap(nullptr, sizeof(sem_t),
				PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		if (sem == MAP_FAILED) {
			perror("mmap");
			exit(EXIT_FAILURE);
		}
		if (sem_init(sem, 1, 0) < 0) {
			perror("sem_init");
			exit(EXIT_FAILURE);
		}
		sem_inited = true;
	};

	if (cmds.size() > 1) {
		old_input = fcntl(input, F_DUPFD_CLOEXEC, FD_MAX + 1);
		init_semaphore();
	}

	SCOPE_EXIT {
		if (sem_inited) {
			sem_destroy(sem);
			munmap(sem, sizeof(sem_t));
		}
		if (cmds.size() > 1) {
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
		pipe(pd);

		main_shell = (interactive_sesh && getpid() == tty_pid);
		if ((pid = fork()) == 0) {
			reset_sigs();
			if (main_shell) {
				if (!pgid)
					pgid = getpid();
				setpgid(0, pgid);
				if (this->pmode == ppl_proc_mode::FG)
					sem_wait(sem);
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
			if (kv_alias.find(*argv) != kv_alias.end()) {
				auto& at = kv_alias.at(*argv);
				if (at.active)
					_exit(uint8_t(atoi(at(argc, argv).c_str())));
			}
			// If found function, run (atoi used for no throw)
			if (functions.find(*argv) != functions.end())
				_exit(uint8_t(atoi(functions.at(*argv)(argc, argv).c_str())));
			// If found builtin, run
			if (builtins.find(*argv) != builtins.end())
				_exit(uint8_t(atoi(builtins.at(*argv)(argc, argv).c_str())));
			// Try to run as external
			auto const& map = !hctable.empty() ? hctable : pathwalk();
			if (map.find(*argv) != map.end())
				execv(map.at(*argv).c_str(), argv);
			struct stat sb;
			if (strchr(*argv, '/') && !stat(*argv, &sb))
				execv(*argv, argv);
			// If that failed, run as `unknown`
			if (functions.find("unknown") != functions.end()) {
				auto ret = functions.at("unknown")(argc, argv);
				_exit(uint8_t(atoi(ret.c_str())));
			}
			perror(argv[0]);
			_exit(127);
		} else {
			if (main_shell) {
				pids.insert(pid);
				if (!pgid)
					pgid = pid;	
				if (setpgid(pid, pgid) < 0)
					perror("setpgid (pipeline)");
			}
			if (input != STDIN_FILENO)
				close(input);
			input = fcntl(pd[0], F_DUPFD_CLOEXEC, FD_MAX + 1);
			close(pd[0]);
			close(pd[1]);
		}
	}
	if (input != STDIN_FILENO) {
		dup2(input, STDIN_FILENO);
		close(input);
		input = -1;
	}

	// Last one!
	int argc = cmds.back().argc();
	char **argv = cmds.back().argv();
	// This is literally the exec function but with extra stuff:


	bool is_alias = kv_alias.find(*argv) != kv_alias.end() && kv_alias.at(*argv).active;
	bool is_fun = functions.find(*argv) != functions.end();
	bool is_builtin = builtins.find(*argv) != builtins.end();

	if (this->pmode == ppl_proc_mode::FG && is_alias && cmds.size() == 1) {
		vars::status = kv_alias.at(*argv)(argc, argv);
		return true;
	}
	// If found function and FG, run (for side effects in the shell)
	if (this->pmode == ppl_proc_mode::FG && is_fun && cmds.size() == 1) {
		vars::status = functions.at(*argv)(argc, argv);
		return true;
	}
	// If found builtin and FG, run (for side effects in the shell)
	else if (this->pmode == ppl_proc_mode::FG && is_builtin && cmds.size() == 1) {
		vars::status = builtins.at(*argv)(argc, argv);
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
			if (map.find(*argv) != map.end()) {
				ok = IS_COMMAND_PATH;
				full_path = map.at(*argv);
			} else if (functions.find("unknown") != functions.end())
				ok = IS_UNKNOWN;
		}
	}
	if (ok == IS_UNKNOWN && this->pmode == ppl_proc_mode::FG)
		vars::status = functions.at("unknown")(argc, argv);
	else {
		main_shell = (interactive_sesh && getpid() == tty_pid);
		if (cmds.size() == 1)
			init_semaphore();
		if ((pid = fork()) == 0) {
			reset_sigs();
			if (main_shell) {
				if (!pgid)
					pgid = getpid();
				setpgid(0, pgid);
				if (this->pmode == ppl_proc_mode::FG)
					sem_wait(sem);
			}
			if (old_input >= 0) {
				close(old_input);
				old_input = -1;
			}
			switch (ok) {
				case IS_ALIAS: _exit(uint8_t(atoi(kv_alias.at(*argv)(argc, argv).c_str())));
				case IS_FUNCTION: _exit(uint8_t(atoi(functions.at(*argv)(argc, argv).c_str())));
				case IS_BUILTIN: _exit(uint8_t(atoi(builtins.at(*argv)(argc, argv).c_str())));
				case IS_COMMAND_FILE: execv(*argv, argv); perror(*argv); _exit(127);
				case IS_COMMAND_PATH: execv(full_path.c_str(), argv); perror(*argv); _exit(127);
				case IS_UNKNOWN: _exit(uint8_t(atoi(functions.at("unknown")(argc, argv).c_str())));
				case IS_NOTHING: errno = ENOENT; perror(*argv); _exit(127);
			}
			_exit(127); // just in case
		} else if (main_shell) {
			pids.insert(pid);
			if (!pgid)
				pgid = pid;
			if (setpgid(pid, pgid) < 0)
				perror("setpgid (final)");
			
			auto jid = add_job(*this, pmode, pgid, pid, pids);	
			if (this->pmode == ppl_proc_mode::FG) {
				// Foreground jobs transfer the terminal control to the child
				if (tcsetpgrp2(pgid) < 0)
					perror("tcsetpgrp #1");
				kill(-pgid, SIGCONT);
				for (int i = 0; i < cmds.size(); ++i)
					sem_post(sem);
#if WINDOWS
				struct timespec ts = { CYG_HACK_TIMEOUT }; // 4 ms
				nanosleep(&ts, nullptr); // no idea.
#endif
				reaper(-pgid, WUNTRACED);
				if (tcsetpgrp2(getpgrp()) < 0)
					perror("tcsetpgrp #2");
			} else {
				tty << '[' << jid << "] " << jobs[jid].ppl << std::endl;
			}
		} else if (this->pmode == ppl_proc_mode::FG) {
			reaper(pid, WUNTRACED);
		}
	}
	return true;
}

/* Ditto */
void pipeline::execute() {
	selfpipe_trick();
	if (cmds.empty()) {
		if (!jobs.empty())
			reaper();
		return;
	}
	SCOPE_EXIT {
		cmds.clear();
		for (auto const& it : fifo_cleanup) {
			unlink((it + "/" FIFO_FILNAME).c_str());
			rmdir(it.c_str());
		}
		fifo_cleanup.clear();
	};
	switch (rmode) {
		case ppl_run_mode::AND: !stonum(vars::status) && execute_act(); break;
		case ppl_run_mode::OR:  !stonum(vars::status) || execute_act(); break;
		case ppl_run_mode::NORMAL:                       execute_act(); break;
	}
	selfpipe_trick();
	if (!jobs.empty())
		reaper();
}
