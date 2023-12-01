typedef std::string FunctionName;
typedef std::string CodeBlock;
typedef std::string AliasName;
typedef std::string Path;
typedef int Jid;
typedef long long ll;
typedef unsigned long long ull;
typedef long double ld;
#define OrderedDispatchTable std::map
#define DispatchTable std::unordered_map

#define TERMINAL    isatty(fileno(stdin)) && cin_eq_in
#define NORMAL_MODE 0
#define JOB_MODE    1
#define FG          0
#define BG          1
#define ST          2
#define NONE        3

#define HTMP "/tmp/zhere-XXXXXX"
#define PTMP "/tmp/zproc-XXXXXX"

#define CLRSCR "\e[1;1H\e[2J"<<std::flush

// File descriptors
#define ZRC_DEFAULT_FD_OFFSET 10
#define ZRC_DEFAULT_RETURN    "0"
#define ZRC_ERRONE_RETURN     "1"
#define ZRC_ERRTWO_RETURN     "2"
#define ZRC_NOTFOUND          127

#define ld long double

// Special variables
#define $PID    "pid"
#define $RETURN "?"
#define $LPID   "!"
#define $ARGV   "argv"
#define $ARGC   "argc"
#define $PS1    "PS1"
#define $ENV    "env"
#define $PATH   $ENV"(PATH)"
#define $CDPATH $ENV"(CDPATH)"

// Ugly hack for forcing inline functions
#define Inline inline __attribute__((always_inline))
// Number to str
#define itoa ldtos
#define syn std::cerr << errmsg

/** \c... **/
#define KEY_ESC 27
#ifndef CTRL
	#define CTRL(X) ((X) & 037)
#endif

#define NO_SIGEXIT \
	chk_exit = 1;
#define ever (;;)
#define sub static Inline void
