#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <array>
#include <functional>
#include <vector>
volatile sig_atomic_t chld_notif;

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
bool run_function(std::string const& str)
{
	if (functions.find(str) != functions.end()) {
		functions.at(str)(0, nullptr);
		return true;
	}
	return false;
}

/** General purpose redirector.
 *
 * @param {int}argc,{char**}argv,{redir_flags}flags
 * @return int
 */
static inline int redir(int argc, char *argv[], int fd, redir_flags flags)
{
	if (argc < 3) {
_syn_error_redir:
		std::cerr << "syntax error: " << argv[0];
		if (flags & OPTFD_Y)
			std::cerr << " [<fd>]";
		std::cerr << " <file> <eoe>\n";
		return 1;
	}

	if (flags & OPTFD_Y) {
		auto x = stonum(argv[1]);
		if (!isnan(x)) {
			if (x < 0 || x >= FD_MAX) {
				std::cerr << "error: Bad file descriptor " << x << '\n';
				return 2;
			}
			fd = x, --argc, ++argv;
			if (argc < 2)
				goto _syn_error_redir;
		}
	}

	if ((flags & NO_CLOBBER) && !access(argv[1], F_OK)) {
		std::cerr << "Cannot clobber " << argv[1] << std::endl;
		return 3;
	}

	int fflags;
	if (flags & OVERWR) fflags = (O_WRONLY | O_CREAT);
	if (flags & APPEND) fflags = (O_WRONLY | O_CREAT | O_APPEND);
	if (flags & READFL) fflags = (O_RDONLY);

	int ffd = open(argv[1], fflags, S_IWUSR | S_IRUSR);
	if (ffd < 0) {
		perror(argv[1]);
		return 4;
	}
	new_fd nfd(fd);
	dup2(ffd, fd);
	eoe(argc, argv, 2);
	close(ffd);
	dup2(nfd, fd);
	return 0;
}

/** Check if a command is a builtin or function.
 *
 * @param {int}argc,{char**}argv
 * @return bool
 */
bool builtin_check(int argc, char *argv[])
{
	auto largv = argv[argc];
	argv[argc] = nullptr;

#define TRY_BUILTIN_FROM(thing)                        \
	if (thing.find(argv[0]) != thing.end()) {          \
		vars::status = thing.at(argv[0])(argc, argv);  \
		/* Don't forget to flush buffers */            \
		std::cout << std::flush;                       \
		std::cerr << std::flush;                       \
		return true;                                   \
	}

	TRY_BUILTIN_FROM(functions)
	TRY_BUILTIN_FROM(builtins)
	argv[argc] = largv;
	return false;
}

/** Tcl-like `unknown` command
 *
 * @param {int}argc,{char**}argv
 * @return void
 */
bool unknown_check(int argc, char *argv[])
{
	auto doesnt_exist = [](const char *fname) -> bool
	{
		struct stat buf;
		return stat(fname, &buf);
	};

	if (!hctable.empty() && hctable.find(*argv) == hctable.end() && doesnt_exist(argv[0])
	&& functions.find("unknown") != functions.end()) {
		functions.at("unknown")(argc, argv);
		return true;
	}
	return false;
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
	for (; new_fd::fdcount > FD_MAX; --new_fd::fdcount)
		close(new_fd::fdcount);
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
	if (!builtin_check(argc, argv)) {
		pid_t pid = fork();
		if (pid == 0)
			exec_extern(argc, argv);
		else
			reaper(pid, WUNTRACED);
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
		_exit(0);
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
		if (argc == argv.size()-1) {
			std::cerr << "Too many args!\n";
			return;
		} else {
			argv[argc++] = strdup(str);
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
		if (jid < 0) continue;
		if (getpid() == tty_pid)
			tcsetpgrp(tty_fd, tty_pid);
		if (WIFSTOPPED(status)) {
			if (interactive_sesh)
				std::cerr << "Stopped" << std::endl;
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

/** Executes a pipeline.
 *
 * @param none
 * @return none
 */
inline bool pipeline::execute_act()
{
	int input = STDIN_FILENO;
	pid_t pgid = 0;
	new_fd old_input(input);
	std::vector<int> to_close;
	size_t i;
	for (i = 0; i < cmds.size()-1; ++i) {
		int pd[2];
		pipe(pd);

		pid_t pid = fork();
		if (pid == 0) {
			setpgid(0, pgid);
			dup2(input,  STDIN_FILENO); close(input);
			dup2(pd[1], STDOUT_FILENO); close(pd[1]);
			close(pd[0]);

			// Exec stuff happens here
			if (!builtin_check(cmds[i].argc, cmds[i].argv.data())
			&&  !unknown_check(cmds[i].argc, cmds[i].argv.data()))
				exec_extern(cmds[i].argc, cmds[i].argv.data());
			_exit(0);
		} else {
			if (!pgid)
				pgid = pid;
			close(pd[1]);
			input = pd[0];
			to_close.push_back(input);
		}
	}
	dup2(input, STDIN_FILENO);
	// Last one! (ditto)
	if (!builtin_check(cmds[i].argc, cmds[i].argv.data())
	&&  !unknown_check(cmds[i].argc, cmds[i].argv.data())) {
		pid_t pid = fork();
		if (pid == 0) {
			setpgid(0, pgid);
			exec_extern(cmds[i].argc, cmds[i].argv.data());
		} else {
			if (!pgid)
				pgid = pid;
			setpgid(0, pgid);
			
			auto jid = add_job(*this, pmode, pgid);
			if (this->pmode == ppl_proc_mode::FG) {
				// Foreground jobs transfer the terminal control to the child
				if (getpid() == tty_pid) {
					tcsetpgrp(tty_fd, pgid);
					reaper(pid, WUNTRACED);
					tcsetpgrp(tty_fd, tty_pid);
				} else
					reaper(pid, WUNTRACED);	
			} else {
				if (interactive_sesh)
					std::cerr << "[" << jid << "] " << jobs[jid].ppl << std::endl;
			}
		}
	}
	for (auto const& fd : to_close)
		close(fd);
	dup2(old_input, STDIN_FILENO);
	close(old_input);

	return true; /* DUMMY */
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
