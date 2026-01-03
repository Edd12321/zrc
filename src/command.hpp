#ifndef COMMAND_HPP
#define COMMAND_HPP
#include <sys/types.h>
#include <map>
#include <string>
#include <vector>
#include <initializer_list>
#include <fcntl.h>
#include "global.hpp"

// Command (word list)
class command {
private:
	std::vector<char*> args = {nullptr};
public:
	command() = default;
	command(command const&) = delete;
	command& operator=(command const&) = delete;
	command& operator=(command&&) = delete;

	command(command&& cmd);
	~command();
public:
	command(std::initializer_list<const char*>);
	inline char **argv() noexcept {
		return args.data();
	}
	inline int argc() noexcept {
		return args.size() - 1;
	}
	void add_arg(const char*);
};

// Pipeline (sequence of commands)
class pipeline {
public:
	enum class run_mode {
		AND, OR, NORMAL
	} rmode = run_mode::NORMAL;
	enum class proc_mode {
		BG, FG
	} pmode = proc_mode::FG;
	std::vector<command> cmds;
// private:
	bool execute_act(bool);
public:
	void execute();
	void add_cmd(command&&);
	operator std::string();
};

// Job table (list of pipelines + metadata)
class job_table {
public:
	struct job {
		pipeline ppl;
		std::vector<pid_t> pids;
	};
//private: // encapsulation is too much of a hassle here
	std::map<int, job> jid2job;
	std::map<pid_t, int> pid2jid;
public:
	unsigned long long jc = 0;

	job_table() = default;
	job_table(job_table const&) = delete;
	job_table& operator=(job_table const&) = delete;
	job_table(job_table&&) = delete;
	job_table& operator=(job_table&&) = delete;

	int add_job(pipeline&&, std::vector<pid_t>&&);
	void sighupper();
	void reaper(pid_t who, int how);
	void reaper();
	void disown(int);
	inline bool empty() {
		return jid2job.empty();
	}

	friend std::ostream& operator<<(std::ostream&, job_table/* const */&);
};
extern job_table jtable;

// Fetches a new FD.
extern int FD_MAX;
struct new_fd {
	int index;
	new_fd(int);
	~new_fd() noexcept;
	inline operator int() const noexcept {
		return index;
	}
	new_fd(new_fd const&) = delete;
	new_fd& operator=(new_fd const&) = delete;
	inline new_fd(new_fd&& rhs) : index(rhs.index) {
		rhs.index = -1;
	}
	new_fd& operator=(new_fd&&);
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

bool run_function(std::string const&);
void exec_extern(int, char**);
zrc_obj exec(int, char**);
void eoe(int, char**, int);
std::string get_output(std::string const&);
zrc_obj redir(int, char**, int, redir_flags);
std::string get_fifo(std::string const&);


static inline zrc_obj exec(command& cmd) {
	return exec(cmd.argc(), cmd.argv());
}
static inline bool good_fd(int fd) {
	return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

#endif
