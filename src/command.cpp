#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <array>
#include <functional>
#include <vector>
// volatile sig_atomic_t chld_notif;
// in dispatch.cpp now

/** Check if a file descriptor is valid
 *
 * @param {int}fd
 * @return bool
 */
static inline bool good_fd(int fd)
{
	return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

/** Run a function with no args.
 *
 * @param {std::string const&}str
 * @return none
 */
bool run_function(std::string str)
{
	if (functions.find(str) != functions.end()) {
		functions.at(str)(1, (char*[]){&str[0], nullptr});
		return true;
	}
	return false;
}

/** General purpose redirector.
 *
 * @param {int}argc,{char**}argv,{redir_flags}flags
 * @return int
 */
static inline zrc_obj redir(int argc, char *argv[], int fd, redir_flags flags)
{
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
	try {
		eoe(argc, argv, 2);
	} catch (...) { // too much effort to make it proper
		close(ffd);
		dup2(nfd, fd);
		throw;
	}
	close(ffd);
	dup2(nfd, fd);
	return vars::status;
}

/** Execute an external command (without forking)
 *
 * @param {int}argc,{char**}argv
 * @return void
 */
void exec_extern(int argc, char *argv[])
{
	auto largv = argv[argc];
	argv[argc] = nullptr;
	if (hctable.find(*argv) != hctable.end()) {
		if (execv(hctable[*argv].c_str(), argv)) {
			perror(*argv);
			_exit(127);
		}
	}
	if (execvp(*argv, argv)) {
		perror(*argv);
		_exit(127);
	}
	argv[argc] = largv;
}

/** Run arguments as a function, builtin or external command.
 *
 * @param {int}argc,{char**}argv
 * @return none
 */
void exec(int argc, char *argv[])
{
	// If found function, run
	if (functions.find(*argv) != functions.end())
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
}

/** Forks, launches a script and return the output from the process.
 * 
 * @param {std::string const&}str
 * @return std::string
 */
static inline std::string get_output(std::string const& str)
{
	std::string ret_str;
	ret_str.reserve(RESERVE_STR); // Try to speed up just a bit
	int pd[2];

	pipe(pd);
	pid_t pid = fork();
	if (pid == 0) {
		dup2(pd[1], STDOUT_FILENO);
		close(pd[0]);
		close(pd[1]);
		eval(str);
		_exit(stonum(vars::status));
	} else {
		close(pd[1]);
		char c;
		while (read(pd[0], &c, 1) >= 1)
			ret_str += c;
		close(pd[0]);
	}
	return ret_str;
}

std::vector<std::string> fifo_cleanup;

/** Perform process substitution/pipeline branching.
 *
 * @param {std::string const&}str
 * @return std::string
 */
static inline std::string get_fifo(std::string const& str)
{
	char temp[] = FIFO_DIRNAME;
	std::string fifo_name = mkdtemp(temp);
	std::string fifo_file = fifo_name+"/" FIFO_FILNAME;
	
	mkfifo(fifo_file.c_str(), S_IRUSR | S_IWUSR);
	pid_t pid = fork();
	if (pid == 0) {
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

struct command {
	std::array<char*, 4096> argv;
	int argc = 0;
	command()=default;

	command(command&& cmd)
	{
		std::swap(argv, cmd.argv);
		argc = cmd.argc;
		cmd.argv = std::array<char*, 4096>();
		cmd.argc = 0;
	}

	~command()
	{
		for (int i = 0; i < argc; ++i)
			free(argv[i]);
	}
public:

	inline void add_arg(const char *str)
	{
		if (argc == argv.size()-2) {
			std::cerr << "Too many args!\n";
			return;
		} else {
			argv[argc++] = strdup(str);
			argv[argc] = NULL;
		}
	}
};

/* Ditto */
static inline void exec(command& cmd)
{
	exec(cmd.argc, cmd.argv.data());
}

class pipeline
{
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

	inline void add_cmd(command& cmd)
	{
		if (cmd.argc)
			cmds.push_back(std::move(cmd));
	}

	operator std::string() const
	{
		std::string ret;
		for (size_t i = 0; i < cmds.size(); ++i) {
			ret += list(cmds[i].argc, const_cast<char**>(cmds[i].argv.data())) + " ";
			if (i < cmds.size()-1)
				ret += "| ";
		}
		return ret;
	}
};

/******************************
 *                            *
 * Job stuff is written below *
 *                            *
 ******************************/
// Job entry table
struct job {
	std::string ppl;
	pipeline::ppl_proc_mode state;
	pid_t pgid;
};
std::map<int, job> jobs;
unsigned long long jc;

/** Add a new entry to the job table
 *
 * @param {pipeline const&}ppl,{pipeline::ppl_proc_mode}status,{pid_t}pgid
 * @return int
 */
int add_job(pipeline const& ppl, pipeline::ppl_proc_mode status, pid_t pgid)
{
	jc = jobs.empty() ? 1 : ((*jobs.rbegin()).first + 1);
	jobs[jc] = { (std::string)ppl, status, pgid };

	return jc;
}

/** Hang up in interactive mode
 *
 * @param none
 * @return void
 */
void sighupper()
{
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
void reaper(int who, int how)
{
	pid_t pid;
	int status;
	while ((pid = waitpid(who, &status, how)) > 0) {
		int jid = -1;
		for (auto const& it : jobs) {
			if (it.second.pgid == pid) {
				jid = it.first;
				break;
			}
		}
		if (jid < 0) {
			if (WIFEXITED(status))
				// Not a job, so launched by eval-or-exec
				// This must still return tho
				vars::status = numtos(WEXITSTATUS(status));
			continue;
		}
		if (getpid() == tty_pid && interactive_sesh)
			tcsetpgrp(tty_fd, tty_pid);
		if (WIFSTOPPED(status)) {
			if (interactive_sesh)
				std::cerr << "[" << jid << "] Stopped" << std::endl;
			break;
		}
		if (WIFSIGNALED(status)) {
			if (interactive_sesh)
				std::cerr << "[" << jid << "] " << strsignal(WTERMSIG(status)) << std::endl;
			jobs.erase(jid);
		}
		if (WIFEXITED(status)) {
			if (jobs[jid].state == pipeline::ppl_proc_mode::BG) {
				if (interactive_sesh)
					std::cerr << "[" << jid << "] Done" << std::endl;
			} else vars::status = numtos(WEXITSTATUS(status));
			jobs.erase(jid);
		}
	}
}

void reaper()
{
	reaper(WAIT_ANY, WNOHANG | WUNTRACED);
}

pid_t job2pid(int jid)
{
	if (jobs.find(jid) != jobs.end())
		return jobs[jid].pgid;
	return -1;
}

int pid2job(pid_t pid)
{
	for (auto const& it : jobs)
		if (it.second.pgid == pid)
			return it.first;
	return -1;
}

void jobstate(int job, int st)
{
	jobs[job].state = static_cast<pipeline::ppl_proc_mode>(st);
}

/** Display job table.
 *
 * @param none
 * @return void
 */
inline void show_jobs()
{
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

inline void disown_job(int n)
{
	if (jobs.find(n) != jobs.end())
		jobs.erase(n);
}


/** Executes a pipeline.
 *
 * @param none
 * @return none
 */
// !!! Beware of dragons !!!
inline bool pipeline::execute_act()
{
	int input = STDIN_FILENO;
	new_fd old_input(input);
	int pgid = 0;

	struct fd_closer_guard {
		std::vector<int> v;
		inline void cleanup()
		{
			for (auto const& fd : v)
				close(fd);
		}
		~fd_closer_guard()
		{
			cleanup();
		}
	} to_close;

	for (size_t i = 0; i < cmds.size() - 1; ++i) {
		int argc = cmds[i].argc;
		char **argv = cmds[i].argv.data();
		int pd[2];
		pipe(pd);

		pid_t pid = fork();
		if (pid == 0) {
			setpgid(0, pgid);
			dup2(input, STDIN_FILENO); close(input);
			dup2(pd[1], STDOUT_FILENO); close(pd[1]);
			close(pd[0]);

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
		} else {
			if (!pgid)
				pgid = pid;
			close(pd[1]);
			input = pd[0];
			to_close.v.push_back(input);
		}
	}
	if (input != STDIN_FILENO)
		dup2(input, STDIN_FILENO);

	// Last one!
	int argc = cmds.back().argc;
	char **argv = cmds.back().argv.data();
	// This is literally the exec function but with extra stuff:

	bool is_fun = functions.find(*argv) != functions.end();
	bool is_builtin = builtins.find(*argv) != builtins.end();
	// If found function and FG, run (for side effects in the shell)
	if (this->pmode == ppl_proc_mode::FG && is_fun)
		vars::status = functions.at(*argv)(argc, argv);
	// If found builtin and FG, run (for side effects in the shell)
	else if (this->pmode == ppl_proc_mode::FG && is_builtin)
		vars::status = builtins.at(*argv)(argc, argv);
	// Try to run as external/BG
	else {
		std::string full_path;
		enum stuff_known {
			IS_NOTHING,
			IS_UNKNOWN,
			IS_FUNCTION,
			IS_BUILTIN,
			IS_COMMAND_PATH,
			IS_COMMAND_FILE,
		} ok = IS_NOTHING;
		if (is_fun)
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
			pid_t pid = fork();
			if (pid == 0) {
				setpgid(0, pgid);
				switch (ok) {
					case IS_FUNCTION: _exit(uint8_t(atoi(functions.at(*argv)(argc, argv).c_str())));
					case IS_BUILTIN: _exit(uint8_t(atoi(builtins.at(*argv)(argc, argv).c_str())));
					case IS_COMMAND_FILE: execv(*argv, argv); perror(*argv); _exit(127);
					case IS_COMMAND_PATH: execv(full_path.c_str(), argv); perror(*argv); _exit(127);
					case IS_UNKNOWN: _exit(uint8_t(atoi(functions.at("unknown")(argc, argv).c_str())));
					case IS_NOTHING: errno = ENOENT; perror(*argv); _exit(127);
				}
			} else {
				if (!pgid)
					pgid = pid;
				setpgid(pid, pgid);
				auto jid = add_job(*this, pmode, pgid);
				if (this->pmode == ppl_proc_mode::FG) {
					// Foreground jobs transfer the terminal control to the child
					if (getpid() == tty_pid && interactive_sesh) {
						tcsetpgrp(tty_fd, pgid);
						reaper(pid, WUNTRACED);
						tcsetpgrp(tty_fd, tty_pid);
					} else
						reaper(pid, WUNTRACED);
				} else if (interactive_sesh)
					std::cerr << "[" << jid << "] " << jobs[jid].ppl << std::endl;
			}
		}
	}	
	to_close.cleanup();
	dup2(old_input, STDIN_FILENO);
	close(old_input);
	return true;
}

/* Ditto */
void pipeline::execute()
{
	reaper();
	if (cmds.empty())
		return;
	switch (rmode) {
		case ppl_run_mode::AND: !stonum(vars::status) && execute_act(); break;
		case ppl_run_mode::OR:  !stonum(vars::status) || execute_act(); break;
		case ppl_run_mode::NORMAL:                       execute_act(); break;
	}
	cmds.clear();
	for (auto const& it : fifo_cleanup) {
		unlink((it+"/" FIFO_FILNAME).c_str());
		rmdir(it.c_str());
	}
	fifo_cleanup.clear();
}
