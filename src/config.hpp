#include <string>
#include <unordered_map>

#define ZVERSION         "zrc v2.3"         /* Version string */
#define ZCONF            ".zrc"             /* Configuration file */
#define ZHIST            ".zrc_history"     /* History file */
#define DEFAULT_PPROMPT  "zrc% "            /* Defualt primary prompt */
#define DEFAULT_SPROMPT  "> "               /* Default secondary prompt */
#define FIFO_DIRNAME     "/tmp/zrc.XXXXXXX" /* Default dir name for FIFOs */
#define FIFO_FILNAME     "fifo"             /* Default filename for FIFOs */
#define CDPATH           "CDPATH"           /* What variable to use as our $CDPATH */ 
#define PATH             "PATH"             /* What variable to use as our $PATH */
char    LAM_STR[] =      "<lambda>";        /* ${argv 0} in a lambda, mutable for reasons */

#define RESERVE_STR      256                /* How many bytes to reserve for string values by default */
#define ZRC_BIND_TIMEOUT 400000             /* Line editor timeout (be careful, some values yield unexpected results */

/* If you have an atypical terminal */
#define BACKSPACE 127
#define ALTBSPACE 8
#if defined(_WIN32)
	#define KEY_RET 13
#else
	#define KEY_RET '\n'
#endif

/* Default keybindings */
struct bind { 
	std::string cmd;
	bool zrc_cmd;
};
#define Sc(x) std::string(1, char(x))
std::unordered_map<std::string, bind> kv_bindkey {
	{ Sc(CTRL('D')), { "exit"             , 1 } },
	{ Sc(CTRL('A')), { "cursor-move-begin", 0 } },
	{ Sc(CTRL('E')), { "cursor-move-end"  , 0 } },
	{ Sc(BACKSPACE), { "cursor-erase"     , 0 } },
	{ Sc(ALTBSPACE), { "cursor-erase"     , 0 } },
	{ Sc(KEY_RET)  , { "key-return"       , 0 } },
	{          "\t", { "expand-word"      , 0 } },
	{            "", { "cursor-insert"    , 0 } },
#if defined(_WIN32)
	{       "\033H", { "hist-go-up"       , 0 } },
	{       "\033P", { "hist-go-down"     , 0 } },
	{       "\033K", { "cursor-move-left" , 0 } },
	{       "\033M", { "cursor-move-right", 0 } },
#else
	{      "\033[A", { "hist-go-up"       , 0 } },
	{      "\033[B", { "hist-go-down"     , 0 } },
	{      "\033[C", { "cursor-move-right", 0 } },
	{      "\033[D", { "cursor-move-left" , 0 } },
#endif
};
#undef Sc
