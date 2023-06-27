/* Default lexer token length (optimization) */
#define DEFAULT_TOKSIZ 256
/* Where include searches */
#define LIBPATH "/usr/lib/zrc/stdlib"
#define LIBEXT  ".zrc"
/* Choose the option to use/not to use line editor */
#ifndef USE_ZLINEEDIT
	#define USE_ZLINEEDIT 1
#endif

static std::string errmsg         = "syntax error: ";
static std::string default_prompt = "zrc% ";
static std::string ver            = "zrc v0.7";
