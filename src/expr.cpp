#include "pch.hpp"
#include "global.hpp"
#include "syn.hpp"

namespace expr {
enum expr_assoc { LEFT, RIGHT };
using vstr = std::vector<std::string>;
// X-macro for convenience
#define OPLIST(X) \
/*    NAME        AR   STRINGS                                                              ASSOC    PREC  EXPR                                                                                              */\
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/\
/*                                                         Nullary operators                                                                                                                                 */\
/*    Why not                                                                                                                                                                                                */\
    X(QUESTION,   0,   (vstr{"?"}),                                                         RIGHT,   2,    NAN)                                                                                                \
    X(LPAREN,     0,   (vstr{/*(*/}),                                                       LEFT,    19,   NAN)                                                                                                \
    X(RPAREN,     0,   (vstr{/*)*/}),                                                       LEFT,    19,   NAN)                                                                                                \
/*    Constants                                                                                                                                                                                              */\
    X(PI,         0,   (vstr{"pi", "PI"}),                                                  RIGHT,   18,   M_PI)                                                                                               \
    X(E,          0,   (vstr{"e", "E"}),                                                    RIGHT,   18,   M_E)                                                                                                \
    X(INF,        0,   (vstr{"inf", "INF", "Inf"}),                                         RIGHT,   18,   INFINITY)                                                                                           \
    X(NAN,        0,   (vstr{"nan", "NAN", "NaN"}),                                         RIGHT,   18,   NAN)                                                                                                \
    X(TRUE,       0,   (vstr{"true", "TRUE", "True"}),                                      RIGHT,   18,   true)                                                                                               \
    X(FALSE,      0,   (vstr{"false", "FALSE", "False"}),                                   RIGHT,   18,   false)                                                                                              \
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/\
/*                                                          Unary operators                                                                                                                                  */\
/*    Operators                                                                                                                                                                                              */\
    X(POS,        1,   (vstr{/*+*/}),                                                       RIGHT,   18,   +x)                                                                                                 \
    X(NEG,        1,   (vstr{/*-*/}),                                                       RIGHT,   18,   -x)                                                                                                 \
    X(NOT,        1,   (vstr{"!"}),                                                         RIGHT,   18,   !x)                                                                                                 \
    X(BITNOT,     1,   (vstr{"~"}),                                                         RIGHT,   18,   ~(long long)x)                                                                                      \
/*    Basic functions                                                                                                                                                                                        */\
    X(LOG10,      1,   (vstr{"log10", "log"}),                                              RIGHT,   18,   log10(x))                                                                                           \
    X(LOG2,       1,   (vstr{"log2"}),                                                      RIGHT,   18,   log2(x))                                                                                            \
    X(LN,         1,   (vstr{"ln"}),                                                        RIGHT,   18,   log(x))                                                                                             \
    X(SQRT,       1,   (vstr{"sqrt"}),                                                      RIGHT,   18,   sqrt(x))                                                                                            \
    X(CBRT,       1,   (vstr{"cbrt"}),                                                      RIGHT,   18,   cbrt(x))                                                                                            \
    X(FLOOR,      1,   (vstr{"floor"}),                                                     RIGHT,   18,   floor(x))                                                                                           \
    X(CEIL,       1,   (vstr{"ceil"}),                                                      RIGHT,   18,   ceil(x))                                                                                            \
    X(TRUNC,      1,   (vstr{"trunc"}),                                                     RIGHT,   18,   trunc(x))                                                                                           \
    X(ABS,        1,   (vstr{"abs"}),                                                       RIGHT,   18,   abs(x))                                                                                             \
    X(ROUND,      1,   (vstr{"round"}),                                                     RIGHT,   18,   round(x))                                                                                           \
    X(SGN,        1,   (vstr{"sgn"}),                                                       RIGHT,   18,   (0 < x) - (x < 0))                                                                                  \
/*    Trigonometry                                                                                                                                                                                           */\
    X(SIN,        1,   (vstr{"sin"}),                                                       RIGHT,   18,   sin(x))                                                                                             \
    X(COS,        1,   (vstr{"cos"}),                                                       RIGHT,   18,   cos(x))                                                                                             \
    X(TG,         1,   (vstr{"tan", "tg"}),                                                 RIGHT,   18,   sin(x) / cos(x))                                                                                    \
    X(CTG,        1,   (vstr{"cot", "ctg"}),                                                RIGHT,   18,   cos(x) / sin(x))                                                                                    \
    X(SEC,        1,   (vstr{"sec"}),                                                       RIGHT,   18,   1.0 / cos(x))                                                                                       \
    X(CSC,        1,   (vstr{"cosec", "csc"}),                                              RIGHT,   18,   1.0 / sin(x))                                                                                       \
    X(ARCSIN,     1,   (vstr{"arcsin", "asin"}),                                            RIGHT,   18,   asin(x))                                                                                            \
    X(ARCCOS,     1,   (vstr{"arccos", "acos"}),                                            RIGHT,   18,   acos(x))                                                                                            \
    X(ARCCOT,     1,   (vstr{"arcctg", "arccot", "actg", "acot"}),                          RIGHT,   18,   atan(1.0 / x))                                                                                      \
    X(ARCTAN,     1,   (vstr{"arctg", "arctan", "atg", "atan"}),                            RIGHT,   18,   atan(x))                                                                                            \
    X(ARCSEC,     1,   (vstr{"arcsec", "asec"}),                                            RIGHT,   18,   acos(1.0 / x))                                                                                      \
    X(ARCCSC,     1,   (vstr{"arccsc", "arccosec", "acsc", "acosec"}),                      RIGHT,   18,   asin(1.0 / x))                                                                                      \
/*    Hyperbolic functions                                                                                                                                                                                   */\
    X(SINH,       1,   (vstr{"sinh", "sh"}),                                                RIGHT,   18,   sinh(x))                                                                                            \
    X(COSH,       1,   (vstr{"cosh", "ch"}),                                                RIGHT,   18,   cosh(x))                                                                                            \
    X(TANH,       1,   (vstr{"tanh", "th"}),                                                RIGHT,   18,   tanh(x))                                                                                            \
    X(CTH,        1,   (vstr{"coth", "cth"}),                                               RIGHT,   18,   cosh(x) / sinh(x))                                                                                  \
    X(SECH,       1,   (vstr{"sech"}),                                                      RIGHT,   18,   1.0 / cosh(x))                                                                                      \
    X(CSCH,       1,   (vstr{"cosech", "csch"}),                                            RIGHT,   18,   1.0 / sinh(x))                                                                                      \
    X(ARCSINH,    1,   (vstr{"arcsinh", "arsinh", "asinh", "arcsh", "argsinh", "argsh"}),   RIGHT,   18,   asinh(x))                                                                                           \
    X(ARCCOSH,    1,   (vstr{"arccosh", "arcosh", "acosh", "arcch", "argcosh", "argch"}),   RIGHT,   18,   acosh(x))                                                                                           \
    X(ARCCOTH,    1,   (vstr{"arccoth", "arcoth", "acoth", "argcth"}),                      RIGHT,   18,   atanh(1.0 / x))                                                                                     \
    X(ARCTANH,    1,   (vstr{"arctanh", "artanh", "atanh", "argth"}),                       RIGHT,   18,   atanh(x))                                                                                           \
    X(ARCSECH,    1,   (vstr{"arcsech", "arsech", "asech"}),                                RIGHT,   18,   acosh(1.0 / x))                                                                                     \
    X(ARCCSCH,    1,   (vstr{"arccsch", "arccosech", "acsch", "acosech", "arcsch"}),        RIGHT,   18,   asinh(1.0 / x))                                                                                     \
/*    Advanced                                                                                                                                                                                               */\
    X(ERF,        1,   (vstr{"erf"}),                                                       RIGHT,   18,   erf(x))                                                                                             \
    X(ERFC,       1,   (vstr{"erfc"}),                                                      RIGHT,   18,   erfc(x))                                                                                            \
    X(TGAMMA,     1,   (vstr{"tgamma", "gamma"}),                                           RIGHT,   18,   tgamma(x))                                                                                          \
    X(LGAMMA,     1,   (vstr{"lgamma"}),                                                    RIGHT,   18,   lgamma(x))                                                                                          \
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/\
/*                                                          Binary operators                                                                                                                                 */\
    X(COMMA,      2,   (vstr{","}),                                                         LEFT,    1,    (x, y))                                                                                             \
    X(IFF,        2,   (vstr{"<->"}),                                                       LEFT,    3,    !x == !y)                                                                                           \
    X(IMPL,       2,   (vstr{"->"}),                                                        RIGHT,   4,    !x || y)                                                                                            \
    X(IMPLBY,     2,   (vstr{"<-"}),                                                        LEFT,    4,    x || !y)                                                                                            \
    X(OR,         2,   (vstr{"||"}),                                                        LEFT,    5,    x || y)                                                                                             \
    X(XOR,        2,   (vstr{"^^"}),                                                        LEFT,    6,    !x ^ !y)                                                                                            \
    X(AND,        2,   (vstr{"&&"}),                                                        LEFT,    7,    x && y)                                                                                             \
    X(BITOR,      2,   (vstr{"|"}),                                                         LEFT,    8,    (long long)x | (long long)y)                                                                        \
    X(BITXOR,     2,   (vstr{"^"}),                                                         LEFT,    9,    (long long)x ^ (long long)y)                                                                        \
    X(BITAND,     2,   (vstr{"&"}),                                                         LEFT,    10,   (long long)x & (long long)y)                                                                        \
    X(EQ,         2,   (vstr{"=="}),                                                        LEFT,    11,   x == y)                                                                                             \
    X(NEQ,        2,   (vstr{"!="}),                                                        LEFT,    11,   x != y)                                                                                             \
    X(LT,         2,   (vstr{"<"}),                                                         LEFT,    12,   x < y)                                                                                              \
    X(LEQ,        2,   (vstr{"<="}),                                                        LEFT,    12,   x <= y)                                                                                             \
    X(GT,         2,   (vstr{">"}),                                                         LEFT,    12,   x > y)                                                                                              \
    X(GEQ,        2,   (vstr{">="}),                                                        LEFT,    12,   x >= y)                                                                                             \
    X(SPC,        2,   (vstr{"<=>"}),                                                       LEFT,    13,   x < y ? -1 : (x > y ? 1 : 0))                                                                       \
    X(SHL,        2,   (vstr{"<<"}),                                                        LEFT,    14,   (long long)((unsigned long long)x << ((unsigned long long)y & (sizeof(long long) * CHAR_BIT - 1)))) \
    X(SHR,        2,   (vstr{">>"}),                                                        LEFT,    14,   (long long)x >> ((unsigned long long)y & (sizeof(long long) * CHAR_BIT - 1)))                       \
    X(ADD,        2,   (vstr{"+"}),                                                         LEFT,    15,   x + y)                                                                                              \
    X(SUB,        2,   (vstr{"-"}),                                                         LEFT,    15,   x - y)                                                                                              \
    X(MUL,        2,   (vstr{"*"}),                                                         LEFT,    16,   x * y)                                                                                              \
    X(DIV,        2,   (vstr{"/"}),                                                         LEFT,    16,   x / y)                                                                                              \
    X(MOD,        2,   (vstr{"%"}),                                                         LEFT,    16,   fmod(x, y))                                                                                         \
    X(FDIV,       2,   (vstr{"//"}),                                                        LEFT,    16,   trunc(x / y))                                                                                       \
    X(POW,        2,   (vstr{"**"}),                                                        RIGHT,   17,   pow(x, y))                                                                                          \
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/\
/*                                                          Ternary operators                                                                                                                                */\
    X(COLON,      3,   (vstr{":"}),                                                         RIGHT,   2,    x ? y : z)                                                                                          \
/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/\

// Enum (list of identif.)
enum expr_optype {
#define X(name, arity, strings, assoc, prec, rule) OP_##name,
	OPLIST(X)
#undef X
};
// String-2-enum
std::vector<std::pair<std::string, expr_optype>> str2optype;
// Enum-2-precedence
std::unordered_map<expr_optype, int> optype2prec = {
#define X(name, arity, strings, assoc, prec, rule) { OP_##name, prec },
	OPLIST(X)
#undef X
};
// Enum-2-assoc
std::unordered_map<expr_optype, int> optype2assoc = {
#define X(name, arity, strings, assoc, prec, rule) { OP_##name, assoc },
	OPLIST(X)
#undef X
};
// Enum-2-arity
std::unordered_map<expr_optype, int> optype2ar = {
#define X(name, arity, strings, assoc, prec, rule) { OP_##name, arity },
	OPLIST(X)
#undef X
};

void init() {
#define X(name, arity, strings, assoc, prec, rule) { strings, OP_##name },
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
#define X(name, arity, strings, assoc, prec, rule) case OP_##name: return rule;
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
	if (vals.size() < (size_t)ar) {
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
		// Open parenthesis
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
		// Closed parenthesis
		else if (str[i] == ')') {
			while (!ops.empty() && ops.top() != OP_LPAREN)
				if (!popper(buf, ops, vals))
					return NAN;
			if (!ops.empty())
				ops.pop();
			unary_here = false;
		}

		// Numbers/constants
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
					if (it.first == "-" && unary_here)
						op = OP_NEG;
					if (it.first == "+" && unary_here)
						op = OP_POS;
					int ar = optype2ar.at(op);
					int prec = optype2prec.at(op);
					
					// constants
					if (ar == 0 && op != OP_QUESTION && op != OP_LPAREN && op != OP_RPAREN) {
						vals.push(eval_op(op));
						i += it.first.length()-1;
						unary_here = false;
						found = true;
						break;
					}
					// ?: logical stuff
					if (op == OP_COLON) {
						while (!ops.empty() && ops.top() != OP_QUESTION)
							if (!popper(buf, ops, vals))
								return NAN;
						if (ops.empty())
							return complain(buf);
						ops.pop();
						ops.push(OP_COLON);
						i += it.first.length()-1;
						unary_here = found = true;
						break;
					}

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

}
