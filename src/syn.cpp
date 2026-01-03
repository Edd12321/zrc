#include <glob.h>
#include <string.h>
#include <string>
#include <vector>
#include "syn.hpp"
#include "list.hpp"
#include "global.hpp"
#include "config.hpp"
#include "vars.hpp"
#include "command.hpp"

token::operator std::string() const {
	using TT = substit::type;

	// This is where we do all of the substitutions.
	std::string ret_str;
	size_t n = 0;
	for (auto const& it : parts) n += it.contents.length() + RESERVE_STR;
	ret_str.reserve(n);

	for (auto const& it : parts) {
		switch (it.tok_type) {
			case TT::RETURN:
				ret_str += eval(it.contents);
				break;
			case TT::PLAIN_TEXT:
				ret_str += it.contents;
				break;
			case TT::VARIABLEB:
				ret_str += getvar(it.contents);
				break;
			case TT::VARIABLE:
				ret_str += getvar(subst(it.contents));
				break;
			case TT::OUTPUT:
				/* empty */ {
					static const char ws[] = " \t\n\r\f\v";
					auto tmp = get_output(it.contents);
					auto b = tmp.find_first_not_of(ws);
					if (b != std::string::npos) {
						auto e = tmp.find_last_not_of(ws);
						ret_str += tmp.substr(b, e - b + 1);
					}
				}
				break;
			case TT::OUTPUTQ:
				ret_str += get_output(it.contents);
				break;
			case TT::PBRANCH:
				ret_str += get_fifo(it.contents);
				break;
		}
	}
	return ret_str;
}

/** Return a list after performing globbing.
 *
 * @param {const char*}str,{int}flags
 * @return none
 */
std::vector<std::string> glob(const char *s, int flags) {
	std::vector<std::string> ret;
	glob_t gvl;
	memset(&gvl, 0, sizeof gvl);
	if (!glob(s, flags, nullptr, &gvl))
		for (size_t i = 0; i < gvl.gl_pathc; ++i)
			ret.push_back(list(gvl.gl_pathv[i]));
	if (ret.empty())
		ret.push_back(list(s));
	globfree(&gvl);
	return ret;
}

/*********
 *       *
 * Lexer *
 *       *
 *********/
token_list lex(const char *p, lexer_flags flags) {
	using TT = substit::type;
	token curr;
	token_list wlst;
	std::string text; text.reserve(RESERVE_STR);

	bool quoted_single = false, quoted_double = false;

	// Completes a token with leftover text
	auto add_remaining_txt = [&](std::string const& str, bool start_new_word) {
		if (!text.empty())
			curr.add_part(std::move(text), TT::PLAIN_TEXT);
		if (start_new_word) {
			if (!str.empty())
				text = str;
			if (!text.empty())
				curr.add_part(std::move(text), TT::PLAIN_TEXT);
			wlst.add_word(std::move(curr));
		}
	};

	// Balanced quoting (e.g. [...], {...})
	auto append_bquoted_str = [&](TT type, char c1, char c2) {
		long cmpnd = 1, brac = (*p && *p == '{');
		add_remaining_txt(std::string(), 0);

		for (++p; *p; ++p) {
			if (*p == '\\') {
				text += '\\', text += *++p;
				continue;
			}
			if (*p == '{'/*none*/) ++brac;
			if (*p == '}' && brac) --brac;
			if (!brac) {
				if (*p == c1/*none */) ++cmpnd;
				if (*p == c2 && cmpnd) --cmpnd;
			}
			if (!cmpnd && !brac)
				break;
			text += *p;
		}
		curr.add_part(std::move(text), type);
	};

	// Variable name
	auto varchar = [](char c) {
		return isalnum(c) || c == '_' || c == '$';
	};

	for (; *p; ++p) {
		switch (*p) {
			/***********
			 * Quoting *
			 ***********/
			case '"':
				curr.bareword = false;
				if (!quoted_single) quoted_double = !quoted_double; else text += *p;
				if (!quoted_double) {
					if (text.empty())
						curr.add_part(std::move(text), TT::PLAIN_TEXT);
					else add_remaining_txt(std::string(), 0);
				}
				break;
			case '\'':
				curr.bareword = false;
				if (!quoted_double) quoted_single = !quoted_single; else text += *p;
				if (!quoted_single) {
					if (text.empty())
						curr.add_part(std::move(text), TT::PLAIN_TEXT);
					else add_remaining_txt(std::string(), 0);
				}
				break;

			/*****************************
			 * Return value substitution *
			 *****************************/
			case '[':
				curr.bareword = false;
				append_bquoted_str(TT::RETURN, '[', ']');
				break;

			/***********************
			 * Output substitution *
			 ***********************/
			case '`':
				if (*(p+1) == '{') {
					++p;
					append_bquoted_str((quoted_single || quoted_double) ? TT::OUTPUTQ : TT::OUTPUT, '{', '}');
					curr.bareword = false;
				} else
					text += *p;
				break;

			/**********************
			 * Pipeline branching *
			 **********************/
			case '<':
				if (*(p+1) == '{') {
					++p;
					append_bquoted_str(TT::PBRANCH, '{', '}');
					curr.bareword = false;
				} else
					text += *p;
				break;

			/**********************
			 * Variable expansion *
			 **********************/
			case '$':
				curr.bareword = false;
				if (*++p == '{')
					append_bquoted_str(TT::VARIABLEB, '{', '}');
				else {
					add_remaining_txt(std::string(), 0);
					text.clear();
					for (; *p && varchar(*p); ++p)
						text += *p;
					curr.add_part(std::move(text), TT::VARIABLE);
					--p;
				}
				break;

			/*************
			 * Semicolon *
			 *************/
			case '\n': /* FALLTHROUGH */
			case  ';':
				if ((flags & SPLIT_WORDS) && !(flags & SEMICOLON) && !quoted_single && !quoted_double) {
					add_remaining_txt(std::string(), 1);
				} else if (!quoted_single && !quoted_double && (flags & SEMICOLON)) {
					add_remaining_txt(std::string() , 1);
					add_remaining_txt(";", 1);
				} else
					text += *p;
				break;

			/**********
			 * Braces *
			 **********/
			case '{':
				if ((flags & SPLIT_WORDS) && !quoted_single && !quoted_double) {
					curr.bareword = false;
					curr.brac = true;

					add_remaining_txt(std::string(), 1);
					append_bquoted_str(TT::PLAIN_TEXT, '{', '}');
					wlst.add_word(std::move(curr));
				} else
					text += *p;
				break;

			/********************
			 * Escape sequences *
			 ********************/
			case '\\':
				curr.bareword = false;
				switch (*++p) {
					case 'a': text += '\a'              ; break;
					case 'b': text += '\b'              ; break;
					case 'e': text += '\033'            ; break;
					case 'f': text += '\f'              ; break;
					case 'n': text += '\n'              ; break;
					case 'r': text += '\r'              ; break;
					case 't': text += '\t'              ; break;
					case 'v': text += '\v'              ; break;
					case 'c': if (*++p) text += CTRL(*p); break;
			  		default : text += *p                ; break;
				}
				break;
			
			/**************
			 * Whitespace *
			 **************/
			case '\f': /* FALLTHROUGH */
			case '\r': /* FALLTHROUGH */
			case '\t': /* FALLTHROUGH */
			case '\v': /* FALLTHROUGH */
			case  ' ':
				if (quoted_single || quoted_double || !(flags & SPLIT_WORDS))
					text += *p;
				else
					add_remaining_txt(std::string(), 1);
				break;

			/************
			 * Comments *
			 ************/
			case '#':
				if (quoted_single || quoted_double)
					text += *p;
				else {
					if (!text.empty())
						add_remaining_txt(std::string(), 0);
					for (; *p && *p != '\n'; ++p) {} --p;
				}
				break;

			default:
				text += *p;
		}
	}
	add_remaining_txt(std::string(), 1);
	return wlst;
}
