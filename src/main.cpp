#pragma GCC optimize("Ofast")
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <float.h>
#include <glob.h>
#ifndef GLOB_TILDE
	#define GLOB_TILDE 0x0800
#endif
#include <libgen.h>
#include <limits.h>
#include <math.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
//#include <format>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <istream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#define REFRESH_TTY cin_eq_in = (&std::cin == &in)
#define CIND std::distance(zwl.wl.begin(), it)
#define FAIL or (can_runcmd=0); continue;

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
        exec(k, args);                            \
        RESET_FD;                                 \
        baks.clear();                             \
        if (!bg_or_fg.empty())                    \
            bg_or_fg.pop_front();                 \
    }                                             \
    can_runcmd = X;                               \
    k = 0;                                        \
} while (0)

/**
 * Reset all file descriptors for the next command
 */
#define RESET_FD {                                \
    for (auto const& it : baks) {                 \
        dup2(it.second, it.first);                \
    }                                             \
}

/**
 * Parse >(...) file descriptor syntax
 */
#define CHK_FD                                    \
    /*if (std::regex_match(*it,m,e)){ */          \
    if ((*it).length()>3 && (*it)[0]=='>' && (*it)[1]=='(' && (*it).back()==')') [[unlikely]] {\
        sword = 1;                                \
        size_t i = 2;                             \
        int fd1 = 0, fd2 = 0;                     \
        bool c = false;                           \
        /* phase 1 scanning */                    \
        for (; (*it)[i] != '=' && i < (*it).length()-1; ++i) {\
            if (isdigit((*it)[i])) {              \
                fd1 = fd1*10+((*it)[i]-'0');      \
            } else {                              \
                std::cerr << errmsg << ">(num..)\n";\
                c = true;                         \
                break;                            \
            }                                     \
        }                                         \
        if (c) continue;                          \
        /* phase 2 scanning */                    \
        if ((*it)[i] == '=') {                    \
            /* phase 3 scanning */                \
            if (i == (*it).length()-2) {          \
                baks[fd1] = dup(fd1);             \
                close(fd1);                       \
            } else {                              \
                for (++i; i < (*it).length()-1; ++i) {\
                    if (isdigit((*it)[i])) {      \
                        fd2 = fd2*10+((*it)[i]-'0');\
                    } else {                      \
                        std::cerr << errmsg << ">(..num)\n";\
                        break;                    \
                    }                             \
                }                                 \
                baks[fd1] = dup(fd1);             \
                dup2(dup(fd2), fd1);              \
            }                                     \
        } else if (i == (*it).length()-1) {       \
            baks[fd1] = dup(fd1);                 \
            io_right(*(++it), 0, fd1);            \
        } else {                                  \
            std::cerr << errmsg << ">(..?)";      \
        }                                         \
        continue;                                 \
}

/**
 * Check if a word is an interp alias
 */
#define CHK_ALIAS                                 \
    if (!k && aliases.find(*it) != aliases.end()){\
        for (std::string& str : aliases[*it].wl) {\
            str_subst(str);                       \
            args[k++] = strdup(str.c_str());      \
        }                                         \
        continue;                                 \
    }

/**
 * Create an empty "dummy" stream
 */
#define NullFin \
	NullIOSink ns;\
	std::istream fin(&ns)

class Array;
class WordList;
class NullIOSink;
class ZrcReturnHandler;
class ZrcBreakHandler;
class ZrcContinueHandler;
class BlockHandler;

class NullIOSink : public std::streambuf
{
public:
	int overflow(int x) { return x; }
};

typedef std::string FunctionName;
typedef std::string CodeBlock;
typedef std::string AliasName;
typedef std::string Path;
typedef int Jid;
#define DispatchTable std::map
/***** GLOBAL VARIABLES BEGIN *****/
  extern char **environ;

  int o_in, o_out; 
  bool cin_eq_in, w, make_new_jobs = true, chk_exit;
  pid_t zrcpid = getpid();
  std::string ret_val;
  std::deque<bool> bg_or_fg;
  DispatchTable<CodeBlock, WordList> zwlcache;
  long ch_mode;
  #include "global.hpp"
  #include "config.hpp"
/***** GLOBAL VARIABLES END *****/

/***** FUNCTIONDECLS BEGIN *****/
  static bool               die            (std::string_view          );
  static inline bool        is_number      (std::string_view          );
  WordList                  glob           (std::string_view          );
	template<typename... Var> std::string zrc_fmt(const char *fmt, Var... args);
  static std::string        eval_stream    (std::istream&             );
  template<typename T>      std::string          eval(T const&        );
  // MAIN.CPP

  WordList                  tokenize       (std::string, std::istream&);
  // LEX.HPP

  long                      array_magic    (char* , int               );
  std::string               array          (int   , char**            );
  std::string               string         (int   , char**            );
  int                       echo           (int   , char**            );
  // ARRAY.HPP, STRING.HPP, READ.HPP

  extern inline void        jobs           (void                      );
  Jid                       addjob         (pid_t , int , int , char**);
  static void               deljob         (Jid                       );
  static pid_t              getfg          (void                      );
  void                      upper          (char*                     );
  void                      sigchld_handler(int                       );
  void                      sigint_handler (int                       );
  void                      sigtstp_handler(int                       );
  void                      sigquit_handler(int                       );
  static inline void        async_message  (int, int, int, const char*);
  extern std::string        bg_fg          (int, char**               );
  
  typedef void Handle(int);
  Handle*                   signal2        (int, Handle*              );
  // SIGHANDLER.HPP

  static std::string        get_var        (std::string_view          );
  void                      str_subst      (std::string&              );
  void                      rq             (std::string&              );
  // SUBST.HPP

  static inline bool        lassoc         (char                      );
  static inline std::string ldtos          (ld                        );
  extern inline bool        is_expr        (std::string_view          );
	std::string               expr           (std::string               );
  // EXPR.CPP, EXPR.HPP

  template<typename T> std::string  combine(int          , T, int     );
  extern void               exec           (int          , char**     );
  std::string               io_cap         (std::string               );
  bool                     str_subst_expect(std::string  , std::istream&,bool);
  bool                      io_left        (std::string               );
  bool                      io_right       (std::string  , bool, int  );
  bool                      io_hedoc       (std::string  , std::istream&,bool);
  void                      io_pipe        (int          , char**     );
  static inline void        run_function   (std::string const&        );
	// EXEC.HPP
  
  std::string               getvar         (std::string               );
  void                      setvar         (std::string  , std::string);
  void                      unsetvar       (std::string               );
  // VARIABLE.HPP
  
  inline bool               fd_parse       (std::string_view,size_t   );
  // FD.HPP

  static inline bool        zrawch         (char&                     );
  bool                      zlineedit      (std::string&              );
  template<typename T> bool zrc_read_line  (std::istream&,std::string&,T const&);
  inline bool               zrc_read_line  (std::istream&,std::string&);
  // ZLINEEDIT.HPP

  #include "lex.hpp"
  #include "variable.hpp"
  #include "string.hpp"
  #include "expr.cpp"
  #include "dispatch.hpp"
  #include "sighandler.hpp"
  #include "subst.hpp"
  #include "exec.hpp"
  #include "zlineedit.hpp"
/***** FUNCTIONDECLS END *****/

void
rq(std::string& str)
{
	if (!str.empty()) str.erase(0, 1);
	if (!str.empty()) str.pop_back();
}

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
	if (!glob(s.data(), GLOB_TILDE, NULL, &gvl)) [[unlikely]]
		for (i=0; i<gvl.gl_pathc; ++i)
			wl.add_token(gvl.gl_pathv[i]);
	if (!wl.size()) [[likely]]
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
	long   k;
	size_t len;
	/*FD expr 1*///std::regex const e{"\\>\\((.*?)\\)"};
	/*FD expr 2*///std::smatch m;
    std::unordered_map<int, int> baks;

	int o_in2, o_out2;
	o_in2   = o_in;
	o_out2  = o_out;
	o_in    = dup(STDIN_FILENO);
	o_out   = dup(STDOUT_FILENO);
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
			if (!strchr("&;", zwl.back()[0])) {
				zwl.add_token(";");
				bg_or_fg.push_back(FG);
			}
			k = 0;
			for (auto it = zwl.wl.begin(); it != zwl.wl.end(); ++it) {
				sword = glb = 0;
				if (zwl.is_bare(CIND)) {
					/** Separators **/
					/*!*/if (*it == "&" || *it == ";") { sword = 1; RC(1); }
					/*!*/else if (*it == "&&")         { sword = 1; RC(ret_val=="0"); }
					/*!*/else if (*it == "||")         { sword = 1; RC(ret_val!="0"); }
					/** I/O redirection **/
					/*!*/else if (*it == "<<" ) { sword = 1; io_hedoc(*(++it), in, 0) FAIL }
					/*!*/else if (*it == "<<<") { sword = 1; io_hedoc(*(++it), in, 1) FAIL }
					/*!*/else if (*it == "<"  ) { sword = 1; io_left (*(++it)       ) FAIL }
					/*!*/else if (*it == ">"  ) { sword = 1; io_right(*(++it), 0 , 1) FAIL }
					/*!*/else if (*it == ">>" ) { sword = 1; io_right(*(++it), 1 , 1) FAIL }
					/*!*/else if (*it == "|"  ) {
						sword = 1;
						if (!can_runcmd)
							continue;
							args[k] = NULL;
						io_pipe(k, args);
						RESET_FD;
						k = 0;
					}
					/*!*/else CHK_ALIAS else {
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
						str_subst(*(++it));
						spl = tokenize(*it, in);
						for (std::string& str : spl.wl)
							args[k++] = strdup(str.c_str());
					} else CHK_FD else {
						str_subst(*it);
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
	close(o_in);
	close(o_out);
	o_in  = o_in2;
	o_out = o_out2;
	RESET_FD;
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

	if (argc == 1) {
		//load user config file
		pw = getpwuid(getuid());
		filename += pw->pw_dir;
		filename += "/.zrc";
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
