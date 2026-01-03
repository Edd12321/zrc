#ifndef EXPR_HPP
#define EXPR_HPP
#include "global.hpp"

namespace expr {

zrc_num eval(const char*);
inline zrc_num eval(std::string const& str) {
	return eval(str.c_str());
}
void init();

}

#endif
