#include "pch.hpp"
#include "global.hpp"
#include "syn.hpp"
#include "vars.hpp"

zrc_num stonum(std::string const& str) {
	try {
		size_t idx;
		zrc_num ret = std::stold(str, &idx);
		if (idx != str.size())
			return NAN;
		return ret;
	} catch (std::exception& ex) {
		return NAN;
	}
}

namespace vars {
	std::unordered_map<std::string, zrc_obj> vmap;
	std::unordered_map<std::string, zrc_arr> amap;
	zrc_arr& argv = amap["argv"];

#define X(name) zrc_var name(#name);
	DECLARE_VAR0(X)
#undef X
#define X(name, val) zrc_var name(#name, val);
	DECLARE_VAR1(X)
#undef X
}

/** Get a variable's value
 * 
 * @param {std::string const&}str
 * @return std::string
 */
zrc_obj getvar(std::string const& str) {
	using namespace vars;

	auto wlst = lex(str.c_str(), SPLIT_WORDS).elems;
	auto len = wlst.size();
	if (len < 1 || len > 2)
		std::cerr << "syntax error: Expected one or two words in variable substitution ${" << str << "}, got " << len << '\n';
	
	// Try to get a scalar value
	else if (len == 1) {
		std::string var = wlst[0];
		if (amap.find(var) != amap.end())
			std::cerr << "error: " << var << " is an array!\n";
		else if (std::all_of(var.begin(), var.end(), isdigit))
			return getvar("argv " + var);
		else if (getenv(var.c_str()))
			return getenv(var.c_str());
		else if (vmap.find(var) != vmap.end())
			return vmap.at(var);

	// Try to get an array key
	} else {
		std::string arr = wlst[0];
		std::string key = wlst[1];
		
		if (vmap.find(arr) != vmap.end()) {
			std::cerr << "error: " << arr << " is a scalar!\n";
			return "";
		}
		if (amap.find(arr) != amap.end()) {
			auto& found = amap.at(arr);
			if (found.find(key) != found.end())
				return found.at(key);
		}
	}
	return "";
}

/** Set a variable's value
 *
 * @param {std::string const&}str
 * @return std::string
 */
zrc_obj setvar(std::string const& key, zrc_obj const& val) {
	using namespace vars;

	auto wlst = lex(key.c_str(), SPLIT_WORDS).elems;
	auto len = wlst.size();
	if (len < 1 || len > 2)
		std::cerr << "syntax error: Expected one or two words in variable {" << key << "}, got " << len << '\n';

	// Try to set a scalar value
	else if (len == 1) {
		std::string var = wlst[0];
		if (amap.find(var) != amap.end())
			std::cerr << "error: " << var << " is an array!\n";
		else if (std::all_of(var.begin(), var.end(), isdigit))
			return setvar("argv " + var, val);
		else if (getenv(var.c_str()))
			setenv(var.c_str(), val.c_str(), 1);
		else
			vmap[var] = val;

	// Try to set an array key
	} else {
		std::string arr = wlst[0];
		std::string key = wlst[1];
		if (vmap.find(arr) != vmap.end())
			std::cerr << "error: " << arr << " is a scalar!\n";
		else amap[arr][key] = val;
	}
	return val;
}

/** Unset a variable
 *
 * @param {std::string const&} str
 * @return none
 */
void unsetvar(std::string const& key) {
	using namespace vars;

	auto wlst = lex(key.c_str(), SPLIT_WORDS).elems;
	auto len = wlst.size();

	if (len < 1 || len > 2) {
		std::cerr << "syntax error: Expected one or two words in variable {" << key << "}, got " << len << '\n';
		return;
	}

	if (len == 1) {
		std::string var = wlst[0];
		if (getenv(var.c_str()))
			unsetenv(var.c_str());
		else if (vmap.find(var) != vmap.end())
			vars::vmap.erase(var);
		else if (amap.find(var) != amap.end())
			vars::amap.erase(var);
	} else {
		std::string arr = wlst[0];
		std::string key = wlst[1];
		if (vmap.find(arr) != vmap.end())
			std::cerr << "error: " << arr << " is a scalar!\n";
		else if (amap.find(arr) != amap.end())
			amap[arr].erase(key);
	}
}
