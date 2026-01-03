#ifndef VARS_HPP
#define VARS_HPP
#include <string>
#include <sstream>
#include <limits>
#include <iomanip>
#include <unistd.h>
#include "global.hpp"
#include "config.hpp"

template<typename T>
std::string numtos(const T x) {
	std::string content;
	/* EMPTY */ {
		std::stringstream ss;
		ss << std::fixed << std::setprecision(std::numeric_limits<long double>::digits10+1) << x;
		content = ss.str();
	}
	if (content.find('.') != std::string::npos) {
		while (!content.empty() && content.back() == '0')
			content.pop_back();
		if (!content.empty() && content.back() == '.')
			content.pop_back();
	}
	return content;
}

zrc_num stonum(std::string const&);
zrc_obj getvar(std::string const&);
zrc_obj setvar(std::string const&, zrc_obj const&);
void unsetvar(std::string const&);

namespace vars {

extern std::unordered_map<std::string, zrc_obj> vmap;
extern std::unordered_map<std::string, zrc_arr> amap;
extern zrc_arr& argv;

class zrc_var {
private:
	std::string key;
public:
	explicit zrc_var(std::string k)
		: key(std::move(k)) {}
	explicit zrc_var(std::string k, std::string const& how)
		: key(std::move(k)) {
		setvar(key, how);
	}
	inline operator std::string() const {
		return getvar(key);
	}
	inline zrc_obj operator=(zrc_obj const& val) const {
		return setvar(key, val);
	}
	zrc_var() = delete;
	~zrc_var() = default;
	zrc_var& operator=(zrc_var const&) = delete;
	zrc_var& operator=(zrc_var&&) = delete;
	zrc_var(zrc_var const&) = delete;
	zrc_var(zrc_var&&) = delete;
	inline friend std::ostream& operator<<(std::ostream&, zrc_var const&);
};
#define DECLARE_VAR0(X) \
	X(argc) \
	X(CDPATH) \
	X(editor) X(EDITOR) \
	X(ifs) X(IFS) \
	X(status) \
	X(PATH) X(PATHEXT) \
	X(reply) \
	X(optarg)
#define DECLARE_VAR1(X) \
	X(opterr, std::to_string(::opterr)) X(optind, std::to_string(::optind)) \
	X(prompt1, DEFAULT_PPROMPT) X(prompt2, DEFAULT_SPROMPT)

#define X(name) extern zrc_var name;
	DECLARE_VAR0(X)
#undef X
#define X(name, val) extern zrc_var name;
	DECLARE_VAR1(X)
#undef X
inline std::ostream& operator<<(std::ostream& out, zrc_var const& var) {
	return out << (zrc_obj)var;
}


}
#endif
