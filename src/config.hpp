#ifndef CONFIG_HPP
#define CONFIG_HPP
#include "pch.hpp"
#include "global.hpp"

#define ZVERSION         "zrc v2.7 staging" /* Version string */
#define ZCONF            ".zrc"             /* Configuration file */
#define ZHIST            ".zrc_history"     /* History file */
#define ZLOGIN1          ".zrc_profile"     /* Login filename #1 */
#define ZLOGIN2          ".zrc_login"       /* Login filename #2 */
#define ZLOGOUT          ".zrc_logout"      /* Logout filename */
#define DEFAULT_PPROMPT  "zrc% "            /* Defualt primary prompt */
#define DEFAULT_SPROMPT  "> "               /* Default secondary prompt */
#ifdef __TERMUX__
#define FIFO_DIRNAME     "/data/data/com.termux/files/usr/tmp/zrc.XXXXXXX"
#else
#define FIFO_DIRNAME     "/tmp/zrc.XXXXXXX" /* Default dir name for FIFOs / FCs */
#endif
#define FIFO_FILNAME     "fifo"             /* Default filename for FIFOs */
#define FC_FILNAME       "fc"               /* Default filename for FCs */
#define FC_EDITOR        "vi"               /* Default fallback editor for fc */
#define LAM_STR          "<lambda>"         /* ${argv 0} in a lambda, mutable for reasons */

#define RESERVE_STR      256                /* How many bytes to reserve for string values by default */
#define ZRC_BIND_TIMEOUT 400000             /* Line editor timeout (be careful, some values yield unexpected results */
#define CYG_HACK_TIMEOUT 550000             /* Empirical and racey, because Cygwin sucks. */
#define REHASH_STARTUP   1                  /* Generate lookup table for commands at startup? */

/* If you have an atypical terminal */
#define BACKSPACE 127
#define ALTBSPACE 8
#if defined(_WIN32)
	#define KEY_RET 13
#else
	#define KEY_RET '\n'
#endif

/* Default keybindings */
struct zrc_bind { 
	std::string cmd;
	bool zrc_cmd;
};
#define Sc(x) std::string(1, char(x))
inline std::unordered_map<std::string, zrc_bind>& kv_bindkey() {
	static std::unordered_map<std::string, zrc_bind> ret_val = {
		{ Sc(CTRL('D')), { "echo; exit"       , 1 } },
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
	};
	return ret_val;
#endif
};
#undef Sc
#endif
