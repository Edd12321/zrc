#ifndef CUSTOM_CMD_HPP
#define CUSTOM_CMD_HPP
#include <string>
#include "syn.hpp"
#include "global.hpp"

// User functions
struct zrc_frame {
	bool is_fun;
	std::string fun;
	bool is_script;
	std::string script;
};
extern std::vector<zrc_frame> callstack;

class block_handler {
private:
	bool ok = false, *ref = &in_loop;
public:
	block_handler(bool *ref) {
		this->ref = ref;
		if (!*ref)
			ok = true;
		*ref = true;
	}
	~block_handler() {
		if (ok)
			*ref = false;
	}
};

// Helps us to see if we can change control flow
#define EXCEPTION_CLASS(x)                               \
  class x##_handler : std::exception {                   \
  public:                                                \
    virtual const char *what() const noexcept override { \
      return "Caught " #x;                               \
    }                                                     \
  };
EXCEPTION_CLASS(fallthrough)
EXCEPTION_CLASS(break)
EXCEPTION_CLASS(continue)
EXCEPTION_CLASS(return)
EXCEPTION_CLASS(regex)

struct zrc_custom_cmd {
	std::string body;
	std::vector<token> lexbody;
	
	zrc_custom_cmd() = default;
	zrc_custom_cmd(std::string const& b)
		: body(b), lexbody(lex(b.c_str(), SEMICOLON | SPLIT_WORDS).elems) {
	}
	virtual ~zrc_custom_cmd() = default;
	virtual zrc_obj operator()(int, char**) = 0;
};

struct zrc_fun : zrc_custom_cmd {
	using zrc_custom_cmd::zrc_custom_cmd;
	virtual zrc_obj operator()(int, char**) override;
};

struct zrc_alias : public zrc_custom_cmd {
	using zrc_custom_cmd::zrc_custom_cmd;
	bool active = true;
	virtual zrc_obj operator()(int, char**) override;
};

struct zrc_trap : public zrc_custom_cmd {
	using zrc_custom_cmd::zrc_custom_cmd;
	bool active = true;
	virtual zrc_obj operator()(int = 0, char** = 0) override;
};

#endif
