#include <algorithm>
#include <unordered_map>
#include <string>
#include <stack>

#include <math.h>

namespace expr {
// X-macros for convenience
#define OPLIST_NULLARY(X) \
	X(QUESTION, {"?"},                                            RIGHT, 2,  NAN)
#define OPLIST_UNARY(X) \
	X(POS,      {},                                               RIGHT, 15, +x) \
	X(NEG,      {},                                               RIGHT, 15, -x) \
	X(LPAREN,   {"("},                                            LEFT,  16, NAN) \
	X(RPAREN,   {")"},                                            LEFT,  16, NAN) /* why not */ \
	X(NOT,      {"!"},                                            RIGHT, 15, !x) \
	X(BITNOT,   {"~"},                                            RIGHT, 15, ~(long long)x) \
	X(LOG10,    (vstr{"log10", "log"}),                           RIGHT, 15, log10(x)) \
	X(LOG2,     {"log2"},                                         RIGHT, 15, log2(x)) \
	X(LN,       {"ln"},                                           RIGHT, 15, log(x)) \
	X(SQRT,     {"sqrt"},                                         RIGHT, 15, sqrt(x)) \
	X(SIN,      {"sin"},                                          RIGHT, 15, sin(x)) \
	X(COS,      {"cos"},                                          RIGHT, 15, cos(x)) \
	X(CTG,      (vstr{"ctg", "cot"}),                             RIGHT, 15, cos(x) / sin(x)) \
	X(TG,       (vstr{"tan", "tg"}),                              RIGHT, 15, sin(x) / cos(x)) \
	X(SEC,      {"sec"},                                          RIGHT, 15, 1.0 / cos(x)) \
	X(CSC,      (vstr{ "cosec", "csc" }),                         RIGHT, 15, 1.0 / sin(x)) \
	X(ARCSIN,   (vstr{ "arcsin", "asin" }),                       RIGHT, 15, asin(x)) \
	X(ARCCOS,   (vstr{ "arccos", "acos" }),                       RIGHT, 15, acos(x)) \
	X(ARCCTG,   (vstr{ "arcctg", "arccot", "actg", "acot" }),     RIGHT, 15, atan(1.0 / x)) \
	X(ARCTG,    (vstr{ "arctg", "arctan", "atg", "atan" }),       RIGHT, 15, atan(x)) \
	X(ARCSEC,   (vstr{ "arcsec", "asec" }),                       RIGHT, 15, acos(1.0 / x)) \
	X(ARCCSC,   (vstr{ "arccsc", "arccosec", "acsc", "acosec" }), RIGHT, 15, asin(1.0 / x)) \
	X(FLOOR,    {"floor"},                                        RIGHT, 15, floor(x)) \
	X(TRUNC,    {"trunc"},                                        RIGHT, 15, trunc(x)) \
	X(CEIL,     {"ceil"},                                         RIGHT, 15, ceil(x)) \
	X(ABS,      {"abs"},                                          RIGHT, 15, abs(x)) \
	X(ROUND,    {"round"},                                        RIGHT, 15, round(x)) \
	X(ERF,      {"erf"},                                          RIGHT, 15, erf(x)) \
	X(ERFC,     {"erfc"},                                         RIGHT, 15, erfc(x)) \
	X(TGAMMA,   {"tgamma"},                                       RIGHT, 15, tgamma(x)) \
	X(LGAMMA,   {"lgamma"},                                       RIGHT, 15, lgamma(x)) \
	X(SGN,      {"sgn"},                                          RIGHT, 15, (0 < x) - (x < 0))
#define OPLIST_BINARY(X) \
	X(COMMA,    {","},                                            LEFT,  1,  (x, y)) \
	X(OR,       {"||"},                                           LEFT,  3,  (long long)x || (long long)y) \
	X(XOR,      {"^^"},                                           LEFT,  4,  !x ^ !y) \
	X(AND,      {"&&"},                                           LEFT,  5,  (long long)x && (long long)y) \
	X(BITOR,    {"|"},                                            LEFT,  6,  (long long)x | (long long)y) \
	X(BITXOR,   {"^"},                                            LEFT,  7,  (long long)x ^ (long long)y) \
	X(BITAND,   {"&"},                                            LEFT,  8,  (long long)x & (long long)y) \
	X(EQ,       {"=="},                                           LEFT,  9,  x == y) \
	X(NEQ,      {"!="},                                           LEFT,  9,  x != y) \
	X(LT,       {"<"},                                            LEFT,  10, x < y) \
	X(LEQ,      {"<="},                                           LEFT,  10, x <= y) \
	X(GT,       {">"},                                            LEFT,  10, x > y) \
	X(GEQ,      {">="},                                           LEFT,  10, x >= y) \
	X(SPC,      {"<=>"},                                          LEFT,  11, x < y ? -1 : (x > y ? 1 : 0)) \
	X(SHL,      {"<<"},                                           LEFT,  12, (long long)x << (int)y) \
	X(SHR,      {">>"},                                           LEFT,  12, (long long)x >> (int)y) \
	X(ADD,      {"+"},                                            LEFT,  13, x + y) \
	X(SUB,      {"-"},                                            LEFT,  13, x - y) \
	X(MUL,      {"*"},                                            LEFT,  14, x * y) \
	X(DIV,      {"/"},                                            LEFT,  14, x / y) \
	X(MOD,      {"%"},                                            LEFT,  14, fmod(x, y)) \
	X(POW,      {"**"},                                           RIGHT, 14, pow(x, y)) \
	X(FDIV,     {"//"},                                           LEFT,  14, trunc(x / y))
#define OPLIST_TERNARY(X) \
	X(COLON,    {":"},                                            RIGHT, 2,  x ? y : z)
// All
#define OPLIST(X) \
	OPLIST_NULLARY(X) \
	OPLIST_UNARY(X) \
	OPLIST_BINARY(X) \
	OPLIST_TERNARY(X)

// Enum (list of identif.)
enum expr_optype {
#define X(name, strings, assoc, prec, rule) OP_##name,
	OPLIST(X)
#undef X
};
enum expr_assoc {
	LEFT, RIGHT
};

using vstr = std::vector<std::string>;

// String-2-enum
std::vector<std::pair<std::string, expr_optype>> str2optype;

// Enum-2-precedence
std::unordered_map<expr_optype, int> optype2prec = {
#define X(name, strings, assoc, prec, rule) { OP_##name, prec },
	OPLIST(X)
#undef X
};

// Enum-2-assoc
std::unordered_map<expr_optype, int> optype2assoc = {
#define X(name, strings, assoc, prec, rule) { OP_##name, assoc },
	OPLIST(X)
#undef X
};

// Enum-2-arity
std::unordered_map<expr_optype, int> optype2ar = {
#define X(name, strings, assoc, prec, rule) { OP_##name, 0 },
	OPLIST_NULLARY(X)
#undef X
#define X(name, strings, assoc, prec, rule) { OP_##name, 1 },
	OPLIST_UNARY(X)
#undef X
#define X(name, strings, assoc, prec, rule) { OP_##name, 2 },
	OPLIST_BINARY(X)
#undef X
#define X(name, strings, assoc, prec, rule) { OP_##name, 3 },
	OPLIST_TERNARY(X)
#undef X
};

void init() {
#define X(name, strings, assoc, prec, rule) { strings, OP_##name },
	std::vector<std::pair<vstr, expr_optype>> vec = { OPLIST(X) };
#undef X
	using pso = std::pair<std::string, expr_optype>;
	if (!str2optype.empty())
		return;
	for (auto const& it : vec)
		for (auto const& jt : it.first)
			str2optype.emplace_back(jt, it.second);
	std::sort(str2optype.begin(), str2optype.end(), [](pso const& lhs, pso const& rhs) {
		return lhs.first.length() > rhs.first.length();
	});
}

zrc_num eval_op(expr_optype op, zrc_num x = 0, zrc_num y = 0, zrc_num z = 0) {
	using namespace std;
	switch (op) {
#define X(name, strings, assoc, prec, rule) case OP_##name: return rule;
		OPLIST(X)
#undef X
		default: return 0;
	}
}

inline zrc_num complain(const char *buf) {
	std::cerr << "syntax error: bad expression\n" << buf << std::endl;
	return NAN;
}

bool popper(const char *buf, std::stack<expr_optype>& ops, std::stack<zrc_num>& vals) {
	zrc_num x, y, z;
	auto op = ops.top();
	int ar = optype2ar.at(op);
	if (vals.size() < ar) {
		complain(buf);
		return false;
	}
	if (ar >= 3) { z = vals.top(); vals.pop(); }
	if (ar >= 2) { y = vals.top(); vals.pop(); }
	if (ar >= 1) { x = vals.top(); vals.pop(); }
	vals.push(eval_op(op, x, y, z));
	ops.pop();
	return true;
}

zrc_num eval(const char *buf) {
	std::string str = subst(buf);
	if (str.empty())
		return 0;
	std::stack<expr_optype> ops;
	std::stack<zrc_num> vals;
	bool unary_here = true;
	for (size_t i = 0; i < str.length(); ++i) {
		if (isspace(str[i]))
			continue;
		// Parentheses
		else if (str[i] == '(') {
			while (isspace(str[++i]))
				;
			if (str[i] == ')') {
				vals.push(0);
				continue;
			} else --i;
			ops.push(OP_LPAREN);
			unary_here = true;
		}
		else if (str[i] == ')') {
			while (!ops.empty() && ops.top() != OP_LPAREN)
				if (!popper(buf, ops, vals))
					return NAN;
			if (!ops.empty())
				ops.pop();
			unary_here = false;
		}

		// Numbers/constants
		else if (str.compare(i, 3, "nan") == 0) vals.push(NAN), i += 2, unary_here = false;
		else if (str.compare(i, 3, "inf") == 0) vals.push(INFINITY), i += 2, unary_here = false;
		else if (str.compare(i, 4, "true") == 0) vals.push(true), i += 3, unary_here = false;
		else if (str.compare(i, 5, "false") == 0) vals.push(false), i += 4, unary_here = false;
		else if (isdigit(str[i]) || str[i] == '.' && i < str.length()-1 && isdigit(str[i+1])) {
			zrc_num val;
			char *end;
			const char *beg = str.c_str() + i;
			if (str[i] == '0' && i < str.length()-1 && str[i+1] == 'x')
				val = strtoll(beg, &end, 16);
			else val = strtold(beg, &end);
			vals.push(val);
			i += end - beg - 1;
			unary_here = false;
		}

		// Operator
		else {
			bool found = false;
			for (auto const& it : str2optype) {
				if (str.compare(i, it.first.length(), it.first) == 0) {
					expr_optype op = it.second;
					
					// Special cases
					// 1) unary +/-
					if (it.first == "-" && unary_here)
						op = OP_NEG;
					if (it.first == "+" && unary_here)
						op = OP_POS;
					// 2) ?: logical stuff
					if (op == OP_COLON) {
						while (!ops.empty() && ops.top() != OP_QUESTION)
							if (!popper(buf, ops, vals))
								return NAN;
						if (ops.empty())
							return complain(buf);
						ops.pop();
						ops.push(OP_COLON);
						i += it.first.length()-1;
						unary_here  = found = true;
						break;
					}

					int prec = optype2prec.at(op);
					while (!ops.empty() && ops.top() != OP_LPAREN) {
						int top_prec = optype2prec.at(ops.top());
						int top_assoc = optype2assoc.at(ops.top());
						if (top_assoc == LEFT && top_prec >= prec
						||  top_assoc == RIGHT && top_prec > prec) {
							if (!popper(buf, ops, vals))
								return NAN;
							} else break;
					}
					ops.push(op);
					i += it.first.length() - 1;
					unary_here = found = true;
					break;
				}
			}
			if (!found) 
				return complain(buf);
		}
	}
	while (!ops.empty())
		if (!popper(buf, ops, vals))
			return NAN;
	return vals.size() == 1 ? vals.top() : complain(buf);
}

inline zrc_num eval(std::string const& str) { return eval(str.c_str()); }

}
