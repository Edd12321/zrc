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
/** Stringify anything **/
template<typename T> Inline
std::string S(T const& t)
{
	std::ostringstream ss;
	ss << t;
	return ss.str();
}

inline std::string Sc(char t)
	{ return S(t); } 

/** \c... **/
#define KEY_ESC 27
#ifndef CTRL
	#define CTRL(X) ((X) & 037)
#endif

#define NO_SIGEXIT \
	chk_exit = 1;
#define ever (;;)
#define sub static Inline void

#define REP(X, Y) {\
	char y[2];\
	y[0] = Y;\
	y[1] = '\0';\
	for (size_t pos = 0;;pos += strlen(y)) {\
		pos = e.find(X, pos);\
		if (pos == std::string::npos)\
			break;\
		e.erase(pos, strlen(X));\
		e.insert(pos, y);\
	}\
}

// ugly kludge
// don't ever do this type of thing
#define LOG10 'Q'
#define LOG2  'W'
#define LOG   'E'
#define SQRT  'R'
#define SIN   'T'
#define COS   'Y'
#define CTG   'U'
#define TG    'I'
#define FLOOR 'O'
#define CEIL  'P'
#define ROUND 'V'

#define AND   'A'
#define OR    'S'
#define SHL   'D'
#define SHR   'F'
#define SPC   'G'
#define LEQ   'H'
#define GEQ   'J'
#define EQU   'K'
#define NEQ   'L'
#define POW   'Z'
#define ABS   'X'
#define IND   'C'
