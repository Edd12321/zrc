#define TERMINAL    isatty(fileno(stdin)) && cin_eq_in
#define NORMAL_MODE 0
#define JOB_MODE    1
#define FG          0
#define BG          1
#define ST          2
#define NONE        3

#define HTMP "/tmp/zhere-XXXXXX"

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

#define NO_SIGEXIT \
	chk_exit = 1;
#define ever (;;)
#define sub static Inline void

extern bool w, cin_eq_in;
