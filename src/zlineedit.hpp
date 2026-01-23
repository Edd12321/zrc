#ifndef ZLINEEDIT_HPP
#define ZLINEEDIT_HPP
#include <istream>
#include <streambuf>
#include <string>
#include <stddef.h>
#include <unistd.h>
#include <vector>
#include "global.hpp"

class ttybuf : public std::streambuf {
protected:
	virtual int overflow(int ch) override {
		if (ch != EOF) {
			char c = ch;
			int n;
			do
				n = write(tty_fd, &c, sizeof c);
			while (n == -1 && errno == EINTR);
			if (n != 1)
				return EOF;
		}
		return ch;
	}
};
extern std::ostream tty;

namespace line_edit {
	extern std::vector<std::string> histfile;
	extern bool use, in_prompt;
	void init_term(size_t&, size_t&);
}

bool zrc_getline(std::istream&, std::string&, bool);
// Dunno where else to put this
std::string basename(std::string const&);

#endif
