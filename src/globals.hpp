/*
 * Global structs, macros, classes and subroutines.
 */
#include <stdlib.h>
#include <unistd.h>

#include <functional>
#include <stack>
#include <string>
#include <unordered_map>

// \c...
#define KEY_ESC 27
#ifndef CTRL
	#define CTRL(X) ((X) & 037)
#endif

// Custom types
#define zrc_arr std::unordered_map<std::string, zrc_obj>
#define CMD_TBL std::unordered_map<std::string, std::function<zrc_obj(int, char**)> >
typedef std::string zrc_obj;
typedef long double zrc_num;

// Bit flags
#define ARG(...) __VA_ARGS__
#define FLAG_TYPE(x, y)                                                       \
  enum x##_flags : unsigned char { y };                                       \
  inline x##_flags operator|(x##_flags a, x##_flags b)                        \
  {                                                                           \
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

class pipeline;
class return_handler;
class break_handler;
class return_handler;
class return_handler;
class block_handler;

struct substit;
struct token;
struct token_list;
struct command;

// Command dispatch tables
extern CMD_TBL builtins, functions;
// Alias table
std::unordered_map<std::string, std::string> kv_alias;

// Fetches a new FD.
struct new_fd {
	static int fdcount;
	int index;

	new_fd(int fd) { dup2(fd, (index = ++fdcount)); }
	~new_fd() { close(index), fdcount--; }

	inline operator int() const { return index; }
};
int FD_MAX, new_fd::fdcount;

// MAIN.CPP
bool source(std::string const&);
static inline std::string eval(std::string const&);
static inline void eval_stream(std::istream&);
zrc_arr copy_argv(int, char**);
int main(int, char**);

// VARS.CPP
static inline std::string getvar(std::string const&);
static inline std::string setvar(std::string const&, zrc_obj const&);
static inline void unsetvar(std::string const&);

// SYN.CPP
token_list lex(const char*, lexer_flags);
std::string subst(const char*);
std::string subst(std::string const&);

// COMMAND.CPP
static inline bool good_fd(int);
static inline int redir(int, char**, int, redir_flags);
bool run_function(std::string const&);
void exec(int, char**);
static inline std::string get_output(std::string const&);
static inline std::string get_fifo(std::string const&);

// DISPATCH.CPP
static inline std::string concat(int, char**, int);
static inline zrc_num expr(std::string const&);
static inline void eoe(int, char**, int);
static inline void prints(std::stack<std::string>);

// EXPR.L
extern zrc_num expr(const char*);
