#include <glob.h>
#include <string.h>

#include <string>
#include <vector>

struct substit {
	enum type {
		OUTPUT, OUTPUTQ, PBRANCH, RETURN, VARIABLE, VARIABLEB, PLAIN_TEXT
	} tok_type;
	std::string contents;
};

struct token {
	bool bareword, brac;
	std::vector<substit> parts;

	inline void add_part(std::string& str, substit::type type) {
		parts.push_back({ type, str });
		str.clear();
	}

	operator std::string() const {
		using TT = substit::type;

		// This is where we do all of the substitutions.
		std::string ret_str;
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
						auto tmp = get_output(it.contents);
						while (!tmp.empty() && isspace(tmp.front())) tmp.erase(0, 1);
						while (!tmp.empty() && isspace(tmp.back()))  tmp.pop_back();
						ret_str += tmp;
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

	token() : bareword(true), brac(false) {}
	
	template<typename T>
	token(T const& t) : bareword(true), brac(false) {
		parts.push_back({ substit::type::PLAIN_TEXT, std::string(t) });
	}
};

struct token_list {
	std::vector<token> elems;
	inline void add_word(token& tok) {
		if (!tok.parts.empty()) {
			elems.push_back(tok);
			tok.parts.clear();
			tok.bareword = true, tok.brac = false;
		}
	}
};

/** Return a list after performing globbing.
 *
 * @param {const char*}str,{int}flags
 * @return none
 */
std::vector<std::string> glob(const char *s, int flags) {
	std::vector<std::string> ret;
	glob_t gvl;
	memset(&gvl, 0, sizeof(glob_t));
	if (!glob(s, flags, NULL, &gvl))
		for (int i = 0; i < gvl.gl_pathc; ++i)
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
	static const auto allowed_var_chars =
		"qwertyuiopasdfghjklzxcvbnm"
		"QWERTYUIOPASDFGHJKLZXCVBNM"
		"1234567890"
		"_$";

	token curr;
	token_list wlst;
	std::string text; text.reserve(RESERVE_STR);

	bool quoted_single = false, quoted_double = false;

	// Completes a token with leftover text
	auto add_remaining_txt = [&](std::string str, bool start_new_word) {
		if (!text.empty())
			curr.add_part(text, TT::PLAIN_TEXT);
		if (start_new_word) {
			if (!str.empty())
				text = str;
			if (!text.empty())
				curr.add_part(text, TT::PLAIN_TEXT);
			wlst.add_word(curr);
		}
	};

	// Balanced quoting (e.g. [...], {...})
	auto append_bquoted_str = [&](TT type, char c1, char c2) {
		long cmpnd = 1, brac = (*p && *p == '{');
		add_remaining_txt("", 0);

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
		curr.add_part(text, type);
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
						curr.add_part(text, TT::PLAIN_TEXT);
					else add_remaining_txt("", 0);
				}
				break;
			case '\'':
				curr.bareword = false;
				if (!quoted_double) quoted_single = !quoted_single; else text += *p;
				if (!quoted_single) {
					if (text.empty())
						curr.add_part(text, TT::PLAIN_TEXT);
					else add_remaining_txt("", 0);
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
					add_remaining_txt("", 0);
					text.clear();
					for (; *p && strchr(allowed_var_chars, *p); ++p)
						text += *p;
					curr.add_part(text, TT::VARIABLE);
					--p;
				}
				break;

			/*************
			 * Semicolon *
			 *************/
			case '\n': /* FALLTHROUGH */
			case  ';':
				if ((flags & SPLIT_WORDS) && !(flags & SEMICOLON) && !quoted_single && !quoted_double) {
					add_remaining_txt("", 1);
				} else if (!quoted_single && !quoted_double && (flags & SEMICOLON)) {
					add_remaining_txt("" , 1);
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

					add_remaining_txt("", 1);
					append_bquoted_str(TT::PLAIN_TEXT, '{', '}');
					wlst.add_word(curr);
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
					add_remaining_txt("", 1);
				break;

			/************
			 * Comments *
			 ************/
			case '#':
				if (quoted_single || quoted_double)
					text += *p;
				else {
					if (!text.empty())
						add_remaining_txt("", 0);
					for (; *p && *p != '\n'; ++p) {} --p;
				}
				break;

			default:
				text += *p;
		}
	}
	add_remaining_txt("", 1);
	return wlst;
}

std::string subst(const char *text) {
	if (!*text) return std::string();
	return lex(text, SUBSTITUTE).elems[0];
}

std::string subst(std::string const& text) {
	return subst(text.c_str());
}
