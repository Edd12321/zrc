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
#define MAX_FD 4096

static std::string errmsg         = "syntax error: ";
static std::string default_prompt = "zrc% ";
static std::string ver            = "zrc v0.7";
