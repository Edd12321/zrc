#pragma GCC optimize("Ofast")
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <fcntl.h>
#include <float.h>
#include <glob.h>
#ifndef GLOB_TILDE
	#define GLOB_TILDE 0x0800
#endif
#include <libgen.h>
#include <limits.h>
#include <list>
#include <math.h>
#include <pwd.h>
#include <set>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <istream>
#include <iomanip>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "global.hpp"
/** Stringify anything (macro hack) **/
template<typename T> Inline
std::string S(T const& t)
{
	std::ostringstream ss;
	ss << t;
	return ss.str();
}

Inline std::string Sc(char t)
	{ return S(t); } 
#include "config.hpp"

/** See if we can print $PS1. **/
#define REFRESH_TTY cin_eq_in = (&std::cin == &in)
/** Current index (in WordList) **/
#define CIND std::distance(zwl.wl.begin(), it)
/** Expect a word pointer + increment pointer. **/
#define FAILSAFE(X) {            \
    auto x = it+1;               \
    if (it+1 != zwl.wl.end() && std::set<std::string>{"&", ";", "&&", "||"}.count(*(it+1))) {\
        std::cerr << errmsg << "Unexpected '" << *(it+1) << "'\n";\
        can_runcmd = 0;          \
        continue;                \
    }                            \
 		X or (can_runcmd=0);         \
		++it;                        \
    continue;                    \
}

/**
 * Check escapes at the end of a line
 */
#define CHK_ESC {                                 \
    len = line.length(); ltmp = "";               \
    while (!line.empty() && line.back() == '\\') {\
        line.pop_back();                          \
        if (!line.empty() && line.back() == '\\'){\
            line += "\\\\";                       \
            break;                                \
        } else if (zrc_read_line(in, ltmp, here_prompt)) {\
             line += ltmp;                        \
             len  += ltmp.length()-1;             \
        } else {                                  \
             break;                               \
        }                                         \
    }                                             \
}

/**
 * Run command and set a condition for the next one
 */
#define RC(X) do {                                \
    make_new_jobs = true;                         \
    args[k] = NULL;                               \
    std::cin.clear();                             \
    if (can_runcmd) {                             \
        red.do_redirs();                          \
        exec(k, args);                            \
        red.fdh.~FdHelper();                      \
        baks.~FdHelper();                         \
        if (!bg_or_fg.empty())                    \
            bg_or_fg.pop_front();                 \
    }                                             \
    can_runcmd = X;                               \
    alias = k = 0;                                \
} while (0)

namespace std
{
#ifndef __cpp_lib_string_view
		typedef std::string const& string_view;
#endif
#ifndef __cpp_lib_make_unique
	// Thanks, stackoverflow!
	template<typename T, typename... Var> std::unique_ptr<T>
	make_unique(Var&&... v)
		{ return std::unique_ptr<T>(new T(std::forward<Var>(v)...)); }
#endif
};

class Array;
class WordList;
class NullIOSink;
class ZrcReturnHandler;
class ZrcBreakHandler;
class ZrcContinueHandler;
class BlockHandler;
class FdHelper;
class RawInputMode;
struct Fifo;
struct Job;
struct Bind;

class NullIOSink : public std::streambuf
{
public:
	int overflow(int x) { return x; }
};

struct Fifo {
	int eval_level;
	pid_t pid;
	std::string filename;

	Fifo(int eval_new, pid_t pid_new, std::string f_new)
		{ eval_level = eval_new, pid = pid_new, filename = f_new; }

	~Fifo()
	{
		kill(pid, SIGKILL);
		unlink(filename.data());

		// ugly hack for old C++11 compilers
		char *fn = strdup(filename.data());
		rmdir(dirname(fn));
		free(fn);
	}	
};

/***** GLOBAL VARIABLES BEGIN *****/
  extern char **environ;

  int fd_offset;
  bool cin_eq_in, w, make_new_jobs = true, chk_exit;
  pid_t zrcpid = getpid();
  std::string ret_val;
  std::deque<bool> bg_or_fg;
	std::list<std::unique_ptr<Fifo> > fifos;

	extern bool w, cin_eq_in;
  long ch_mode;
/***** GLOBAL VARIABLES END *****/

/***** FUNCTIONDECLS BEGIN *****/
  static bool               die             (std::string_view                   );
  static inline bool        is_number       (std::string_view                   );
  WordList                  glob            (std::string_view                   );
  template<typename... Var> std::string zrc_fmt(const char *fmt , Var... args   );
  static std::string        eval_stream     (std::istream&                      );
  template<typename T>      std::string           eval(T const&                 );
  // MAIN.CPP

  WordList                  tokenize        (std::string , std::istream&        );
  // LEX.HPP

  long                      array_magic     (char* , int                        );
  std::string               array           (int   , char**                     );
  std::string               string          (int   , char**                     );
  int                       echo            (int   , char**                     );
  // ARRAY.HPP, STRING.HPP, READ.HPP

  extern inline void        jobs            (void                               );
  Jid                       addjob          (pid_t , int , int , char**         );
  static void               deljob          (Jid                                );
  static pid_t              getfg           (void                               );
  void                      upper           (char*                              );
  void                      sigchld_handler (int                                );
  void                      sigint_handler  (int                                );
  void                      sigtstp_handler (int                                );
  void                      sigquit_handler (int                                );
  static inline void        async_message   (int   , int , int , const char*    );
  extern std::string        bg_fg           (int   , char**                     );
  
  typedef void Handle(int);
  Handle*                   signal2         (int, Handle*                       );
  // SIGHANDLER.HPP

  bool                      str_subst       (std::string&                       );
  // SUBST.HPP

	extern ld                 expr            (std::string                        );
  // EXPR.CPP, EXPR.HPP

  template<typename T> std::string  combine (int          , T, int              );
  extern void               exec            (int          , char**              );
  std::string               io_cap          (std::string                        );
  std::string               io_proc         (std::string                        );
  bool                      str_subst_expect(std::string  , std::istream& , bool);
  bool                      io_left         (std::string  , FdHelper&           );
  bool                      io_right        (std::string  , int  , bool , bool ,
                                                                    FdHelper& );
  bool                      io_hedoc        (std::string  ,  std::istream&,bool );
  void                      io_pipe         (int          ,  char** , FdHelper& );
  static inline void        run_function    (std::string const&                 );
  // EXEC.HPP
  
  std::string               getvar          (std::string                        );
  void                      setvar          (std::string , std::string          );
  void                      unsetvar        (std::string                        );
  // VARIABLE.HPP
  
  inline bool               fd_parse        (std::string_view , size_t          );
  // FD.HPP

  static inline bool        zrawch          (char&                              );
  bool                      zlineedit       (std::string&                       );
  template<typename T> bool zrc_read_line   (std::istream&,std::string&,T const&);
  inline bool               zrc_read_line   (std::istream&,std::string&         );
  // ZLINEEDIT.HPP

  #include "lex.hpp"
  #include "variable.hpp"
  #include "dispatch.hpp"
  #include "sighandler.hpp"
  #include "subst.hpp"
  #include "exec.hpp"
  #include "zlineedit.hpp"
/***** FUNCTIONDECLS END *****/

static bool
die(std::string_view err)
{
	std::cerr << err << '\n';
	exit(EXIT_FAILURE);
	return 0;//dummy return
}

/** Return whether or not a string is a number.
 *
 * @param {string_view}str
 * @return bool
 */
static Inline bool is_number(std::string_view str)
	{ return str.find_first_not_of("1234567890") == std::string::npos; }

/** Perform globbing.
 * 
 * @param {string_view}s
 * @return vector<string>
 */
WordList
glob(std::string_view s)
{
	WordList wl;
	glob_t gvl;
	int i, j, ok;
	memset(&gvl, 0, sizeof(glob_t));
	if (!glob(s.data(), GLOB_TILDE|GLOB_NOESCAPE, NULL, &gvl))
		for (i=0; i<gvl.gl_pathc; ++i)
			wl.add_token(gvl.gl_pathv[i]);
	if (!wl.size())
		wl.add_token(s);
	globfree(&gvl);
	return wl;
}

/** Formatted string support
 * 
 * @param {const char*}fmt,...
 * @return string
 */
template<typename... Var> std::string
zrc_fmt(const char *fmt, Var... args)
{
	size_t len = snprintf(nullptr, 0, fmt, args...)+1;
	char *ret = new char[len];
	snprintf(ret, len, fmt, args...);

	std::string ret_str{ret};
	delete [] ret;
	return ret_str;
}

/** Evaluate a text stream.
 *
 * @param {istream&}in
 * @return void
 */

static std::string
eval_stream(std::istream& in)
{
	std::string line, ltmp;
	WordList    zwl, gbzwl, spl;
	bool   sword, glb, can_runcmd=1;
	bool   ret = 0, brk = 0, con = 0;
	bool   found_pipe = 0, alias = 0;
	long   k;
	size_t len;
	// Each "eval level" restores its file descriptors
	fd_offset += ZRC_DEFAULT_FD_OFFSET;
	FdHelper baks;
	Redirector red;
	REFRESH_TTY;
	char **args = new char*[ARG_MAX];
	try {
		while (zrc_read_line(in, line)) {
			bg_or_fg.clear();
			CHK_ESC;
			zwl = tokenize(line, in);
			if (!zwl.size()) {
				REFRESH_TTY;
				continue;
			}
			if (zwl.back() != ";" || zwl.back() != "&") {
				zwl.add_token(";");
				bg_or_fg.push_back(FG);
			}
			k = 0;
			for (auto it = zwl.wl.begin(); it != zwl.wl.end(); ++it) {
				sword = glb = 0;
				if (zwl.is_bare(CIND)) {
					sword = 1;
					/** I/O Aliases **/
					if (*it ==  "^") *it =  "2>"; if (*it == "^^") *it = "2>>";
					if (*it ==  ">") *it =  "1>"; if (*it == ">>") *it = "1>>";
					if (*it == "^?") *it = "2>?"; if (*it == ">?") *it = "1>?";
					auto len = (*it).length(); 
					/** Separators **/
					/*!*/if (*it == "&" || *it == ";")
						RC(1);
					/*!*/else if (*it == "&&")
						RC(ret_val=="0");
					/*!*/else if (*it == "||")
						RC(ret_val!="0");

					/** I/O redirection **/
					/*!*/else if (len == 2 && isdigit((*it)[0]) && (*it)[1] == '>')
						{ FAILSAFE(io_right(*(it+1), (*it)[0]-'0', 0, 0, red)) }
					/*!*/else if (len == 3 && isdigit((*it)[0]) && (*it)[1] == '>' && (*it)[2] == '>')
						{ FAILSAFE(io_right(*(it+1), (*it)[0]-'0', 1, 0, red)) }
					/*!*/else if (len == 3 && isdigit((*it)[0]) && (*it)[1] == '>' && (*it)[2] == '?')
						{ FAILSAFE(io_right(*(it+1), (*it)[0]-'0', 0, 1, red)) }
					/*!*/else if (*it == "<<")
						{ FAILSAFE(io_hedoc(*(it+1), in, STDIN_FILENO, red)) }
					/*!*/else if (*it == "<<<")
						{ FAILSAFE(io_hedoc(*(it+1), in, STDOUT_FILENO, red)) }
					/*!*/else if (*it == "<")
						{ FAILSAFE(io_left(*(it+1), red)) }
					/** Pipes **/
					/*!*/else if (*it == "|") {
						if (!can_runcmd)
							continue;
						if (!red.fdh.find(STDOUT_FILENO))
							red.fdh.add_fd(STDOUT_FILENO);
						if (!baks.find(STDIN_FILENO))
							baks.add_fd(STDIN_FILENO);
						args[k] = NULL;
						io_pipe(k, args, red);
						alias = k = 0;

					/** Alias **/
					} else if (!k && !alias && aliases.find(*it) != aliases.end()) {
						auto *al = &aliases[*it];
						auto ind = CIND;

						zwl.wl.insert(it+1, al->wl.begin(), al->wl.end());
						it = zwl.wl.begin()+ind;
						alias = 1;
						continue;
					
					/** Other words/globbing **/
					} else {
						sword = 0;
						str_subst(*it); // Remove backslashes
						gbzwl = glob(*it);
						if (gbzwl.size() > 0)
							glb = 1;
						for (auto const& it : gbzwl.wl)
							args[k++] = strdup(it.c_str());
					}
				}
				if (!glb && (!zwl.is_bare(CIND) || !sword)) {
					/*!*/if (*it == "{*}"/* && it<zwl.wl.end()-1*/) {
						sword = 1;
						if (!str_subst(*(++it)))
							can_runcmd = 0;
						spl = tokenize(*it, in);
						for (std::string& str : spl.wl)
							args[k++] = strdup(str.c_str());
					} else {
						if (!str_subst(*it))
							can_runcmd = 0;
						args[k++] = strdup((*it).c_str());
					}
				}
			}
			REFRESH_TTY;
		}
	} catch   (ZrcReturnHandler ex) { ret = 1; }
	  catch    (ZrcBreakHandler ex) { brk = 1; }
	  catch (ZrcContinueHandler ex) { con = 1; }
	delete [] args;
	fd_offset -= ZRC_DEFAULT_FD_OFFSET;
	in.clear();
	if (ret) throw ZrcReturnHandler();
	if (brk) throw ZrcBreakHandler();
	if (con) throw ZrcContinueHandler();
	return ret_val;
}

/** Evaluate a string.
 * 
 * @param {T}sv
 * @return void
 */
template<typename T> Inline 
std::string eval(T const& sv)
{
	std::istringstream ss{sv};
	return eval_stream(ss);
}

sub version() { die(ver); }
sub usage()   { die("zrc [--help][--version][-c <cmd>] [<file>] [<args...>]"); }

int
main(int argc, char *argv[])
{
	std::string filename;
	std::ifstream fp;
	struct passwd *pw;

	// make faster&unbuffered I/O
	std::ios_base::sync_with_stdio(false);
	setvbuf(stdout, NULL, _IONBF, 0);

	// signal handlers
	signal2(SIGCHLD, sigchld_handler);
	signal2(SIGINT,   sigint_handler);
	signal2(SIGTSTP, sigtstp_handler);
	signal2(SIGQUIT, sigquit_handler);
	// signal ignore
	signal2(SIGTTIN, SIG_IGN);
	signal2(SIGTTOU, SIG_IGN);
	// sigexit
	atexit([](){
			if (!chk_exit)
				eval(funcs["sigexit"]);
			sigchld_handler(SIGCHLD);
		});
	
	// $argv(0), $argv(1), $argv(2), ..., $argv([expr $argc-1])
	INIT_ZRC_ARGS;
	// $env(...)
	INIT_ZRC_ENVVARS;
	// $pid
	setvar($PID, std::to_string(getpid()));

#if defined USE_HASHCACHE && USE_HASHCACHE == 1
	zrc_builtin_rehash(0, NULL);
#endif
	if (argc == 1) {
		//load user config file
		pw = getpwuid(getuid());
		filename += pw->pw_dir;
		filename += "/" ZCONF;
		fp.open(filename, std::ios::in);
		eval_stream(fp);
		fp.close();

		//load zrc REPL
		eval_stream(std::cin);
	} else {
		if (!strcmp(argv[1], "--version")) version();
		if (!strcmp(argv[1], "--help")) usage();
		if (!strcmp(argv[1], "-c")) {
			if (argc == 3)
				eval(argv[2]);
			else
				usage();
		} else if (!access(argv[1], F_OK)) {
			fp.open(argv[1], std::ios::in);
			eval_stream(fp);
			fp.close();
		} else {
			perror(argv[1]);
			goto _err;
		}
	}
_suc: EXIT_SESSION; 
_err: return EXIT_FAILURE;
}
