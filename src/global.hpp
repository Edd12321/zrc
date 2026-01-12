#ifndef GLOBAL_HPP
#define GLOBAL_HPP
#include "pch.hpp"
// Macro stuff
#ifndef WAIT_ANY
  #define WAIT_ANY -1
#endif
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__)
	#define WINDOWS 1 /* POSIX API-capable windows thing */
#endif
// \c...
#define KEY_ESC 27
#undef CTRL
#define CTRL(X) (((X) == '?') ? 127 : ((X) & 037))
#define UNDO_CTRL(X) (((X) == 127) ? '?' : ((char)((X) | '@')))
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
// Types
using zrc_obj = std::string;
using zrc_arr = std::unordered_map<zrc_obj, zrc_obj>;
using zrc_num = long double;
using sighandler_t = decltype(SIG_DFL);
class zrc_frame;
class zrc_fun;
class zrc_alias;
class zrc_trap;
struct token;

// Tables
extern std::unordered_map<std::string, std::function<zrc_obj(int, char**)>> builtins;
extern std::unordered_map<std::string, zrc_fun> functions;
extern std::unordered_map<std::string, zrc_alias> kv_alias;
extern std::unordered_map<int, zrc_trap> sigtraps;

// Shelly globals
extern bool interactive_sesh;
extern bool killed_sigexit;
extern int tty_fd, tty_pid, FD_MAX;
// Scripty globals
extern int argc; extern char **argv;
extern std::string script_name; extern bool is_script;
extern std::string fun_name; extern bool is_fun;
extern bool in_loop, in_switch, in_func;
extern std::vector<zrc_frame> callstack;

// Functions in main
bool source(std::string const&, bool = false);
zrc_obj eval(std::vector<token> const& wlst);
zrc_obj eval(std::string const&);
void eval_stream(std::istream&);
zrc_arr copy_argv(int, char**);

// (inspired by alexandrescu SCOPE_EXIT talk)
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


#endif
