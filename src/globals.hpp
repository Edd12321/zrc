/*
 * Global structs, macros, classes and subroutines.
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <functional>
#include <map>
#include <stack>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__)
	#define WINDOWS 1 /* POSIX API-capable windows thing */
#endif

#ifndef WAIT_ANY
  #define WAIT_ANY -1
#endif

// \c...
#define KEY_ESC 27
#ifndef CTRL
	#define CTRL(X) ((X) & 037)
#endif

// Bit flags
#define ARG(...) __VA_ARGS__
#define FLAG_TYPE(x, y)                                                       \
  enum x##_flags : unsigned char { y };                                       \
  inline x##_flags operator|(x##_flags a, x##_flags b) {                      \
    return static_cast<x##_flags>(static_cast<int>(a) | static_cast<int>(b)); \
  }

FLAG_TYPE(lexer, ARG(
	SUBSTITUTE   =      0, // 00000000
	SPLIT_WORDS  = 1 << 0, // 00000001
	SEMICOLON    = 1 << 1  // 00000010
))

FLAG_TYPE(redir, ARG(
	NO_CLOBBER   = 1 << 0, // 00000001
	DO_CLOBBER   = 1 << 1, // 00000010
	OVERWR       = 1 << 2, // 00000100
	APPEND       = 1 << 3, // 00001000
	READFL       = 1 << 4, // 00010000
	OPTFD_Y      = 1 << 5, // 00100000
	OPTFD_N      = 1 << 6, // 01000000
))

// Custom types
class command;
class pipeline;
class return_handler;
class break_handler;
class return_handler;
class return_handler;
class block_handler;
class new_fd;
class ttybuf;

struct substit;
struct token;
struct token_list;
struct job;
struct zrc_custom_cmd;
struct zrc_fun;
struct zrc_alias;
struct zrc_trap;

using zrc_obj = std::string;
using zrc_num = long double;
using zrc_arr = std::unordered_map<std::string, zrc_obj>;
using CMD_TBL = std::unordered_map<std::string, std::function<zrc_obj(int, char**)>>;

// Command dispatch tables
extern CMD_TBL builtins;
// User functions
std::unordered_map<std::string, zrc_fun> functions;
// Alias table
std::unordered_map<std::string, zrc_alias> kv_alias;
// Trap table
std::unordered_map<int, zrc_trap> sigtraps;
// Job table
extern std::map<int, job> jobs;
extern std::map<pid_t, int> pid2jid;

std::string basename(std::string const& str) {
	auto fnd = str.rfind('/');
	if (fnd == std::string::npos)
		return str;
	return str.substr(fnd + 1);
}

int tty_fd, tty_pid;
bool interactive_sesh, login_sesh, killed_sigexit;

int FD_MAX;
// Fetches a new FD.
struct new_fd {
	int index;

	new_fd(int fd) {
		index = fcntl(fd, F_DUPFD_CLOEXEC, FD_MAX + 1);
		if (index < 0)
			throw std::runtime_error("could not create new fd");
	}
	~new_fd() noexcept {
		if (index >= 0)
			close(index);
	}
	inline operator int() const { return index; }
	new_fd(new_fd const&) = delete;
	new_fd& operator=(new_fd const&) = delete;
	new_fd(new_fd&& rhs) : index(rhs.index) {
		rhs.index = -1;
	}
	new_fd& operator=(new_fd&& rhs) {
		if (this != &rhs) {
			if (index >= 0)
				close(index);
			index = rhs.index;
			rhs.index = -1;
		}
		return *this;
	}
};

// inspired by alexandrescu SCOPE_EXIT talk
enum class scope_exit_dummy {};

template<typename Fun>
class scope_exit {
private:
	Fun f;
	bool active;
public:
	scope_exit(Fun&& fn)
			: f(std::move(fn)), active(true) {}
	scope_exit(scope_exit&& rhs)
			: f(std::move(rhs.f)), active(rhs.active) {
		rhs.active = false;
	}
	scope_exit(scope_exit const&) = delete;
	scope_exit& operator=(scope_exit const&) = delete;
	scope_exit& operator=(scope_exit&&) = delete;
	~scope_exit() noexcept { if (active) f(); }
};

template<typename Fun>
constexpr scope_exit<typename std::decay<Fun>::type>
operator+(scope_exit_dummy d, Fun&& f) {
	return { std::forward<Fun>(f) };
}

#define CONCAT_IMPL(s1, s2) s1 ## s2
#define CONCAT(s1, s2) CONCAT_IMPL(s1, s2)
#ifdef __COUNTER__
	#define NEW_ID CONCAT(scope_exit_, __COUNTER__)
#else
	#define NEW_ID CONCAT(scope_exit_, __LINE__)
#endif
#define SCOPE_EXIT auto NEW_ID = scope_exit_dummy() + [&]()


// TTY streambuf
class ttybuf : public std::streambuf {
protected:
	virtual int overflow(int ch) override {
		if (ch != EOF) {
			char c = ch;
			if (write(tty_fd, &c, 1) != 1)
				return EOF;
		}
		return ch;
	}
} _ttybuf;
std::ostream tty(&_ttybuf);

// MAIN.CPP
std::unordered_map<std::string, std::string> pathwalk();
bool source(std::string const&, bool err = true);
static inline zrc_obj eval(std::vector<token> const& wlst);
static inline zrc_obj eval(std::string const&);
static inline void eval_stream(std::istream&);
zrc_arr copy_argv(int, char**);
int main(int, char**);
int tcsetpgrp2(pid_t);

// VARS.CPP
static inline std::string getvar(std::string const&);
static inline std::string setvar(std::string const&, zrc_obj const&);
static inline void unsetvar(std::string const&);

// SYN.CPP
token_list lex(const char*, lexer_flags);
std::string subst(const char*);
std::string subst(std::string const&);
std::vector<std::string> glob(const char*, int);

// COMMAND.CPP
static inline bool good_fd(int);
static inline zrc_obj redir(int, char**, int, redir_flags);
bool run_function(std::string);
zrc_obj exec(int, char**);
static inline std::string get_output(std::string const&);
static inline std::string get_fifo(std::string const&);
int add_job(pipeline const&, pid_t);
inline void show_jobs();
inline void disown_job(int);
pid_t job2pid(int);
bool jobexists(int);
pid_t jobpgid(job const&);
void jobstate(int, int);
void reaper();
void reaper(pid_t, int);
void reset_sigs();
void selfpipe_trick();

template<typename Fun>
zrc_obj invoke(Fun&, std::initializer_list<const char*>);

template<typename Fun>
void invoke_void(Fun const&, std::initializer_list<const char*>);

// DISPATCH.CPP
static inline std::string concat(int, char**, int);
static inline void eoe(int, char**, int);
static inline void prints(std::stack<std::string>);

// LIST.CPP
zrc_obj list(int, const char**);
inline zrc_obj list(int, char**);
inline zrc_obj list(std::vector<token>&);
inline zrc_obj list(std::string const&);

// ZLINEEDIT.CPP
namespace line_edit {
	static inline void init_term(size_t&, size_t&);
	std::vector<std::string> histfile;
}
