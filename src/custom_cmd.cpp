#include "pch.hpp"
#include "custom_cmd.hpp"
#include "global.hpp"
#include "vars.hpp"
#include "syn.hpp"

std::vector<zrc_frame> callstack;

zrc_obj zrc_fun::operator()(int argc, char **argv) {
	zrc_obj zargc_old = vars::argc;
	int argc_old = ::argc;
	zrc_arr zargv_old = std::move(vars::argv);
	char **argv_old = ::argv;
	vars::argc = numtos(argc);
	::argc = argc;
	vars::argv = copy_argv(argc, argv);
	::argv = argv;
	// Break, continue and fallthrough don't work inside functions
	bool in_switch = ::in_switch, in_loop = ::in_loop;
	::in_switch = ::in_loop = false;
	// For the caller command
	bool is_fun_old = is_fun;
	std::string fun_name_old = fun_name;
	callstack.push_back({is_fun, fun_name, is_script, script_name});
	is_fun = true;
	fun_name = argv[0];
	SCOPE_EXIT {
		vars::argc = zargc_old;
		::argc = argc_old;
		vars::argv = std::move(zargv_old);
		::argv = argv_old;
		::in_switch = in_switch;
		::in_loop = in_loop;
		is_fun = is_fun_old;
		fun_name = fun_name_old;
		callstack.pop_back();
	};
	block_handler fh(&in_func);
	try {
		eval(lexbody);
	} catch (return_handler const& ex) {
	}
	// Don't forget
	return vars::status;
}

zrc_obj zrc_alias::operator()(int argc, char **argv) {
	active = false;
	for (int i = 1; i < argc; ++i)
		lexbody.emplace_back(token(argv[i]));
	SCOPE_EXIT {
		for (int i = 1; i < argc; ++i)
			lexbody.pop_back();
		active = true;
	};
	return eval(lexbody);
}

zrc_obj zrc_trap::operator()(int /*= 0*/, char** /*= nullptr*/) {
	active = false;
	SCOPE_EXIT { active = true; };
	return eval(lexbody);
}
