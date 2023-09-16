/* Default lexer token length (optimization) */
#define DEFAULT_TOKSIZ 256
/* Where include searches */
#ifdef __ANDROID__
	#define LIBPATH "/data/data/com.termux/files/zrc/stdlib"
#else
	#define LIBPATH "/usr/lib/zrc/stdlib"
#endif
#define LIBEXT  ".zrc"

/* Choose the option to use/not to use line editor */
#ifndef USE_ZLINEEDIT
	#define USE_ZLINEEDIT 1
#endif
/* Choose the option to enable path hashing */
#ifndef USE_HASHCACHE
	#define USE_HASHCACHE 1
#endif

/* If you have an atypcal terminal */
#define BACKSPACE 127
#define ALTBSPACE 8
#if defined(_WIN32)
	#define KEY_RET 13
#else
	#define KEY_RET '\n'
#endif

#define MAX_FD 4096
/* Max arg count */
#ifndef ARG_MAX
	const auto ARG_MAX = sysconf(_SC_ARG_MAX);
#endif
/* Where to write job control messages. Recommended channel is stderr. */
#define JOB_MSG STDERR_FILENO

#if defined USE_ZLINEEDIT && USE_ZLINEEDIT == 1
/* Line editor timeout (be careful changing this, some values yield unpredictable behavior) */
#define ZRC_BIND_TIMEOUT 400000
/* Hist-file and config file names */
#define ZHISTFILE ".zrc_history"
#define ZCONF     ".zrc"
/* Default key bindings */
struct Bind {
	std::string cmd;
	bool zcmd;
};
DispatchTable<std::string, Bind> keybinds = {
	{ Sc(CTRL('D')), { "exit"             , 1 } },
	{ Sc(CTRL('A')), { "cursor-move-begin", 0 } },
	{ Sc(CTRL('E')), { "cursor-move-end"  , 0 } },
	{ Sc(BACKSPACE), { "cursor-erase"     , 0 } },
	{ Sc(ALTBSPACE), { "cursor-erase"     , 0 } },
	{ Sc(KEY_RET)  , { "key-return"       , 0 } },
	{          "\t", { "expand-word"      , 0 } },
	{            "", { "cursor-insert"    , 0 } },
#if defined(_WIN32)
	{         "\eH", { "hist-go-up"       , 0 } },
	{         "\eP", { "hist-go-down"     , 0 } },
	{         "\eK", { "cursor-move-left" , 0 } },
	{         "\eM", { "cursor-move-right", 0 } },
#else
	{        "\e[A", { "hist-go-up"       , 0 } },
	{        "\e[B", { "hist-go-down"     , 0 } },
	{        "\e[C", { "cursor-move-right", 0 } },
	{        "\e[D", { "cursor-move-left" , 0 } },
#endif
};
#endif

static std::string errmsg         = "syntax error: ";
static std::string warnmsg        = "warning: ";
static char here_prompt           = '>';
static std::string default_prompt = "zrc% ";
static std::string ver            = "zrc v0.9b";
