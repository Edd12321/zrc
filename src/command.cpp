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
			fd = x, --argc, ++argv;
			if (argc < 2)
				goto _syn_error_redir;
			if (!good_fd(x)) {
				std::cerr << "error: Bad file descriptor " << x << std::endl;
				return 2;
			}
		}
	}

	if ((flags & NO_CLOBBER) && access(argv[1], F_OK)) {
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

/** Run arguments as a function, builtin or external command.
 *
 * @param {int}argc,{char**}argv
 * @return none
 */
void exec(int argc, char *argv[])
{
	auto largv = argv[argc];
	argv[argc] = nullptr;

	// Shell builtins/functions
	for (auto const& it : { functions, builtins }) {
		if (it.find(argv[0]) != it.end()) {
			vars::status = it.at(argv[0])(argc, argv);
			// Don't forget to flush buffers
			std::cout << std::flush;
			std::cerr << std::flush;
			return;
		}
	}

	// External programs
	pid_t pid = fork();
	if (pid == 0) {
		if (execvp(*argv, argv)) {
			perror(*argv);
			_exit(127);
		}
	} else {
		int status;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			vars::status = numtos(WEXITSTATUS(status));
	}
	argv[argc] = largv;
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
	std::string fifo_file = fifo_name+"/"FIFO_FILNAME;
	
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
		BG, FG, STOPPED
	} pmode = FG;
private:
	std::vector<command> cmds;

	inline bool execute_fg();
	inline bool execute_bg();
public:
	void execute();

	inline void add_cmd(command& cmd)
	{
		if (cmd.argc)
			cmds.push_back(std::move(cmd));
	}
};

/** Executes a pipeline.
 *
 * @param none
 * @return none
 */
inline bool pipeline::execute_fg()
{
	int input = STDIN_FILENO;
	new_fd old_input(input);
	size_t i;
	for (i = 0; i < cmds.size()-1; ++i) {
		int pd[2];
		pipe(pd);

		pid_t pid = fork();
		if (pid == 0) {
			dup2(input,  STDIN_FILENO); close(input);
			dup2(pd[1], STDOUT_FILENO); close(pd[1]);
			exec(cmds[i]);
			_exit(0);
		} else {
			close(pd[1]);
			input = pd[0];
		}
	}
	dup2(input, STDIN_FILENO);
	exec(cmds[i]);
	close(input);
	dup2(old_input, STDIN_FILENO);
	close(old_input);

	return true; /* DUMMY */
}

/* Ditto */
inline bool pipeline::execute_bg()
{
	pid_t pid = fork();
	if (pid == 0) {
		execute_fg();
		_exit(0);
	}

	return true; /* DUMMY */
}

/* Ditto */
void pipeline::execute()
{
	if (cmds.empty())
		return;
	std::function<bool(void)> fn;
	switch (pmode) {
		case ppl_proc_mode::BG: fn = std::bind(&pipeline::execute_bg, this); break;
		case ppl_proc_mode::FG: fn = std::bind(&pipeline::execute_fg, this); break;
	}
	switch (rmode) {
		case ppl_run_mode::AND: !stonum(vars::status) && fn(); break;
		case ppl_run_mode::OR:  !stonum(vars::status) || fn(); break;
		case ppl_run_mode::NORMAL:                       fn(); break;
	}
	cmds.clear();
	for (auto const& it : fifo_cleanup) {
		unlink((it+"/"FIFO_FILNAME).c_str());
		rmdir(it.c_str());
	}
	fifo_cleanup.clear();
}
