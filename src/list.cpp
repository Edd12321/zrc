#include "pch.hpp"
#include "list.hpp"
#include "syn.hpp"

std::string list(int argc, const char **argv) {
	std::string ret;

	for (int i = 0; i < argc; ++i) {
		bool contains_space = false;
		for (int j = 0, c; (c = argv[i][j]); ++j)
			if (isspace(c)) {
				contains_space = true;
				break;
			}
		if (contains_space)
			ret += '\'';
		if (!argv[i][0])
			ret += "\'\'";
		for (int j = 0, c; (c = argv[i][j]); ++j) {
			switch (c) {
				case  '<': /* FALLTHROUGH */
				case  '`':
					if (argv[i][j+1] == '{') {
						ret += "\\{";
						++j;
					}
					ret += c;
					break;

				case  '#': /* FALLTHROUGH */
				case  '"': /* FALLTHROUGH */
				case  '{': /* FALLTHROUGH */
				case  '[': /* FALLTHROUGH */
				case  '$': /* FALLTHROUGH */
				case '\'': /* FALLTHROUGH */ 
				case '\\':
					ret += '\\', ret += c;
					break;

				case   '\a': ret += "\\a"; break;
				case   '\b': ret += "\\b"; break;
				case '\033': ret += "\\e"; break;
				case   '\f': ret += "\\f"; break;
				case   '\n': ret += "\\n"; break;
				case   '\r': ret += "\\r"; break;
				case   '\t': ret += "\\t"; break;
				case   '\v': ret += "\\v"; break;
				default:
					if (c >= CTRL('A') && c <= CTRL('Z') || c == CTRL('@')
					||  c == CTRL('[') || c == CTRL(']') || c == CTRL('\\')
					||  c == CTRL(']') || c == CTRL('^') || c == CTRL('_')
					||  c == CTRL('?')) {
						ret += "\\c";
						ret += UNDO_CTRL(c);
					} else ret += c;
			}
		}
		if (contains_space)
			ret += '\'';
		if (i < argc-1)
			ret += ' ';
	}
	return ret;
}

std::string list(int argc, char **argv) {
	return list(argc, const_cast<const char**>(argv));
}

std::string list(std::string const& str) {
	const char *argv[] = { str.c_str(), nullptr };
	return list(1, argv);
}

std::string list(std::vector<token>& vec) {
	std::string ret;
	for (size_t i = 0; i < vec.size(); ++i) {
		ret += list((std::string)vec[i]);
		if (i < vec.size() - 1)
			ret += ' ';
	}
	return ret;
}
