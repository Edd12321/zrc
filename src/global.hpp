#define TERMINAL    isatty(fileno(stdin)) && cin_eq_in
#define NORMAL_MODE 0
#define JOB_MODE    1
#define FG          0
#define BG          1
#define ST          2
#define NONE        3

#define ld long double
extern bool w, cin_eq_in;
