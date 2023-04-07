#pragma GCC optimize("Ofast")
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <float.h>
#include <glob.h>
#ifndef GLOB_TILDE
	#define GLOB_TILDE 0x0800
#endif
#include <math.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <istream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#define ever (;;)
#define MAX_FD 4096
#define sub static inline void

#define CHK_ESC {                                 \
    len = line.length(); ltmp = "";               \
    while (!line.empty() && line.back() == '\\') {\
        line.pop_back();                          \
        if (zrc_read_line(in, ltmp, '>'))         \
                line += ltmp,                     \
                len  += ltmp.length()-1;          \
            else                                  \
                break;                            \
        }                                         \
}

#define RUN_CURRENT_COMMAND(X) do {               \
	make_new_jobs = true;                         \
    args[k] = NULL;                               \
    if (can_runcmd) {                             \
        exec(k, args);                            \
        if (!bg_or_fg.empty())                    \
            bg_or_fg.pop_front();                 \
    }                                             \
    can_runcmd = X;                               \
    k = 0;                                        \
} while (0)

typedef int Jid;
/***** GLOBAL VARIABLES BEGIN *****/
  extern char **environ;

  int o_in, o_out; 
  bool cin_eq_in, w, make_new_jobs = true;
  pid_t zrcpid = getpid();
  std::string ret_val;
  std::string filename;
  std::deque<bool> bg_or_fg;
  std::vector<std::pair<int, int>> baks;
  #include "global.hpp"
  #include "config.hpp"
/***** GLOBAL VARIABLES END *****/

class WordList;

/***** FUNCTIONDECLS BEGIN *****/
  static bool               die            (std::string_view          );
  static inline bool        is_number      (std::string_view          );
  WordList                  glob           (std::string_view          );
  static std::string        eval_stream    (std::istream&             );
  template<typename T>      std::string          eval(T               );
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
  void                      deljob         (Jid                       );
  static pid_t              getfg          (void                      );
  void                      upper          (char*                     );
  void                      sigchld_handler(int                       );
  void                      sigint_handler (int                       );
  void                      sigtstp_handler(int                       );
  void                      sigquit_handler(int                       );
  extern std::string        bg_fg          (int, char**               );
  // SIGHANDLER.HPP

  static std::string        get_var        (std::string_view          );
  void                      str_subst      (std::string&              );
  void                      rq             (std::string&              );
  // SUBST.HPP

  static inline int         prio           (char                      );
  static inline bool        lassoc         (char                      );
  static inline char       *ldtoa          (ld                        );
  extern inline bool        is_expr        (std::string               );
  char                     *expr           (std::string               );
  static void               cleanup_memory (void                      );
  // EXPR.CPP, EXPR.HPP

  template<typename T> std::string  combine(int          , T, int     );
  extern void               exec           (int          , char**     );
  std::string               io_cap         (std::string               );
  void                      io_left        (std::string               );
  void                      io_right       (std::string  , bool       );
  void                      io_pipe        (int          , char**     );
  // EXEC.HPP
  
  std::string               getvar         (std::string               );
  void                      setvar         (std::string  , std::string);
  void                      unsetvar       (std::string               );
  // VARIABLE.HPP
  
  inline void               fd_parse       (std::string_view          );
  // FD.HPP

  static inline char        zrawch         (char&                     );
  bool                      zlineedit      (std::string&              );
  template<typename T> bool zrc_read_line  (std::istream&,std::string&,T const&);
  inline bool               zrc_read_line  (std::istream&,std::string&);
  // ZLINEEDIT.HPP

  #include "variable.hpp"
  #include "lex.hpp"
  #include "array.hpp"
  #include "string.hpp"
  #include "dispatch.hpp"
  #include "sighandler.hpp"
  #include "subst.hpp"
  #include "expr.hpp"
  #include "expr.cpp"
  #include "exec.hpp"
  #include "zlineedit.hpp"
  #include "fd.hpp"
/***** FUNCTIONDECLS END *****/

void
rq(std::string& str)
{
	if (!str.empty()) str.erase(0, 1);
	if (!str.empty()) str.pop_back();
}

/** Terminates zrc with a message.
 * 
 * @param {string_view}err
 * @return bool (none)
 */
static bool
die(std::string_view err)
{
	std::cerr << err;
	exit(EXIT_FAILURE);
	
	// Dummy return
	return 0;
}

/** Returns whether or not a string is a number.
 * 
 * @param {string_view}str
 * @return bool
 */
static inline bool
is_number(std::string_view str)
{
	return str.find_first_not_of("0123456789")
		== std::string::npos;
}

/** Perform globbing
 * 
 * @param {string_view}s
 * @return vector<string>
 */
WordList
glob(std::string_view s)
{
	WordList wl;
	glob_t   glob_fin;
	int      i, j;
	bool     ok;

	memset(&glob_fin, 0, sizeof(glob_t));
	if (!glob(s.data(), GLOB_TILDE, NULL, &glob_fin))
		for (i = 0; i < glob_fin.gl_pathc; ++i)
			wl.add_token((std::string)glob_fin.gl_pathv[i]);
	if (!wl.size())
		wl.add_token(s);
	globfree(&glob_fin);
	return wl;
}

/** Evaluates a stream.
 * 
 * @param {istream&}in
 * @return string
 */
static std::string
eval_stream(std::istream& in)
{
	std::string line, ltmp, temp;
	WordList    toks, split;
	size_t      i, j, k, len;
	char       *args[BUFSIZ], q, p;
	int         fd, fd1, fd2;
	bool        can_runcmd = true;

	cin_eq_in = (&std::cin == &in);
	while (zrc_read_line(in, line)) {
		bg_or_fg.clear();
        CHK_ESC;
	
		/**************
		 * Lexer call *
		 **************/
		toks = tokenize(line, in);

		char *args[BUFSIZ];
		k = 0;
		if (!toks.size())
			goto _skip;

		if (toks.back() != "&" && toks.back() != ";") {
			toks.add_token(";");
			bg_or_fg.push_back(FG);
		}

		/**********
		 * Parser *
		 **********/
		for (i = k = 0; i < toks.size(); ++i) {
			len = toks.wl[i].length();
			//-- No condition
			if (toks.wl[i] == "&" || toks.wl[i] == ";")
				RUN_CURRENT_COMMAND(true);
			//-- AND
			else if (toks.wl[i] == "&&")
				RUN_CURRENT_COMMAND(ret_val == "0");
			//-- OR
			else if (toks.wl[i] == "||")
				RUN_CURRENT_COMMAND(ret_val != "0");

			// Explode multiple words
			else if (toks.wl[i] == "{*}" && i < toks.size()-1) {
				str_subst(toks.wl[++i]);
				split = tokenize(toks.wl[i], in);
				for (std::string& str : split.wl)
					args[k++] = strdup(str.c_str());
			}

			else if (!k && aliases.find(toks.wl[i]) != aliases.end()) {
				for (std::string& str : aliases[toks.wl[i]].wl) {
					str_subst(str);
					args[k++] = strdup(str.c_str());
				}
			}

			// Pipeline operators
			else if (toks.wl[i] == "<" ) io_left (toks.wl[++i]      );
			else if (toks.wl[i] == ">" ) io_right(toks.wl[++i], 0, 1);
			else if (toks.wl[i] == ">>") io_right(toks.wl[++i], 1, 1);
			else if (toks.wl[i] == "|") {
				if (!can_runcmd)
					continue;
				args[k] = NULL;
				io_pipe(k, args);
				k = 0;
			}

			// File descriptor manipulation
			else if (toks.wl[i].size() > 3
			     &&  toks.wl[i].front() == '>'
			     &&  toks.wl[i][1]      == '('
			     &&  toks.wl[i].back()  == ')')
			{
				fd_parse(toks, i);
				continue;
			}
			else if (!toks.is_bare(i)) {
				str_subst(toks.wl[i]);
				args[k++] = strdup(toks.wl[i].c_str());
			} else {
				// Glob
				WordList glob_vec = glob(toks.wl[i]);
				for (j = 0, len = glob_vec.size(); j < len; ++j)
					args[k++] = strdup(glob_vec.wl[j].c_str());
			}
		}
_skip:
		cin_eq_in = (&std::cin == &in);
	}
	in.clear();
	return ret_val;
}

/** Evaluates a string.
 * 
 * @param {string_view}sv
 * @return string
 */
template<typename T> __attribute__((always_inline)) std::string
eval(T sv)
{
	std::stringstream ss;
	ss << sv;
	return eval_stream(ss);
}

static inline __attribute__((always_inline)) void
version()
{
	std::cout << ver << std::endl;
	exit(EXIT_SUCCESS);
}

static inline __attribute__((always_inline)) void
usage()
{
	std::cout << "zrc [--help][--version] [file] [args...]" << std::endl;
	exit(EXIT_SUCCESS);
}

/** Main method.
 * 
 * @param {int}argc,{char**}argv,{char**}envp
 * @return int
 */
int
main(int argc, char *argv[])
{
	std::ios_base::sync_with_stdio(false);
	//std::cin.tie(NULL);
	//std::cout.tie(NULL);
	o_in  = dup(STDIN_FILENO);
	o_out = dup(STDOUT_FILENO);

	signal2(SIGCHLD, sigchld_handler);
	signal2(SIGINT,   sigint_handler);
	signal2(SIGTSTP, sigtstp_handler);
	signal2(SIGQUIT, sigquit_handler);
	signal2(SIGTTIN, SIG_IGN);
	signal2(SIGTTOU, SIG_IGN);

	INIT_ZRC_ARGS;

	if (argc == 1) {
		// Load user config
		struct passwd *pw = getpwuid(getuid());
		filename = "";
		filename += pw->pw_dir;
		filename += "/.zrc";
		std::ifstream fin(filename);
		eval_stream(fin);
		fin.close();
		// Load REPL
		eval_stream(std::cin);
	} else {
		if (!strcmp(argv[1], "--version")) version();
		if (!strcmp(argv[1], "--help"))    usage();

		if (access(argv[1], F_OK) == 0) {
			std::ifstream fin(argv[1]);
			eval_stream(fin);
			fin.close();
		} else {
			perror(argv[1]);
			goto _err;
		}
	}
_suc:
	return is_number(ret_val)
		? std::stoi(ret_val)
		: EXIT_SUCCESS;
_err:
	return EXIT_FAILURE;
}

