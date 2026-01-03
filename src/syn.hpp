#ifndef SYN_HPP
#define SYN_HPP
#include <string>
#include <vector>
#include "global.hpp"

struct substit {
	enum class type {
		OUTPUT, OUTPUTQ, PBRANCH, RETURN, VARIABLE, VARIABLEB, PLAIN_TEXT
	} tok_type;
	std::string contents;
	substit(type const& t, std::string&& c)
		: tok_type(t), contents(std::move(c)) {
	}
};

struct token {
	bool bareword, brac;
	std::vector<substit> parts;

	token() : bareword(true), brac(false) {
		parts.reserve(4);
	}
	inline void add_part(std::string&& str, substit::type type) {
		parts.emplace_back(type, std::move(str));
		str.clear();
	}
	template<typename T>
	token(T const& t) : bareword(true), brac(false) {
		parts.emplace_back(substit::type::PLAIN_TEXT, std::string(t));
	}
	operator std::string() const;
};

struct token_list {
	std::vector<token> elems;
	inline void add_word(token&& tok) {
		if (!tok.parts.empty()) {
			elems.emplace_back(std::move(tok));
			tok = token();
		}
	}
};

std::vector<std::string> glob(const char*, int);
token_list lex(const char*, lexer_flags);

inline std::string subst(const char* text) {
	if (!*text) return std::string();
	return lex(text, SUBSTITUTE).elems[0];
}
inline std::string subst(std::string const& text) {
	return subst(text.c_str());
}

#endif
