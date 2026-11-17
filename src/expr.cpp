#include "pch.hpp"
#include "expr.hpp"
#include "global.hpp"
#include "syn.hpp"

namespace expr {
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
// Doumentation
std::vector<row4doc> doc = {
#define X(name, arity, strings, assoc, prec, rule) { #name, arity, strings, assoc, prec, #rule },
	OPLIST(X)
#undef X
};

void init() {
#define X(name, arity, strings, assoc, prec, rule) { strings, OP_##name },
	std::vector<std::pair<vstr, expr_optype>> vec = {
		OPLIST(X)
	};
#undef X
	for (auto& row : doc) {
		if (row.name == "POS") row.strings = {"+"};
		if (row.name == "NEG") row.strings = {"-"};
	}
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
