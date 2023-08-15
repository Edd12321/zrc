#include ".expr_ops.hpp"
#define syn std::cerr << errmsg

#define ld long double
#define ll long long
#define isnum(X) (isdigit(X) || X == '.' || islower(X))
#define INVALIDSYN {\
	syn << old << '\n';\
	return "nan";\
}

/** Checks if a string contains only mathematical chars.
 *
 * @param  void
 * @return void
 */
extern inline bool
is_expr(std::string_view str)
{
	return str.find_first_not_of(
		".0123456789 \t\n\r\v\f"
		"<=>+-*/%^~!?:()<>&|"
		"qwertyuiopasdfghjklzxcvbnm"
	) == std::string::npos;
}

std::unordered_map<char, short> prio = {
	  { '(', -13 }, { ')', -13 },
	  { '?', -12 }, { ':', -12 },
	  {  OR, -11 },
	  { AND, -10 },
	  { '|',  -9 },
	  { '^',  -8 },
	  { '&',  -7 },
	  { EQU,  -6 }, { NEQ,  -6 },
	  { '<',  -5 }, { LEQ,  -5 }, { '>', -5 }, { GEQ, -5 },
	  { SPC,  -4 },
	  { SHL,  -3 }, { SHR,  -3 },
	  { '+',  -2 }, { '-',  -2 },
	  { '*',  -1 }, { '/',  -1 }, { IND, -1 }, { '%', -1 }
	//{ implicit  0 }
};

#define du(X,Y) {X,[](ld x){return Y;}} 
DispatchTable<char, std::function<ld(ld const&)>> unary_table = {
	// unary ops
	du('!', !x), du('m', -x), du('p', +x), du('~', ~(ll)x),

	// 1 arg functions
	du(LOG10, log10(x)),      du(LOG2, log2(x)), du(LOG,   log(x)),
	du(SQRT,  sqrt(x)),       du(SIN,  sin(x)),  du(COS,   cos(x)),
	du(CTG,   cos(x)/sin(x)), du(TG,   tan(x)),  du(FLOOR, floor(x)),
	du(CEIL,  ceil(x)),       du(ABS,  abs(x)),  du(ROUND, round(x))
};

#define db(X,Y) { X,[](ld x,ld y){return Y;}}
DispatchTable<char, std::function<ld(ld const&, ld const&)>> binary_table = {
	// binops
	db(SHL, (ll)x << (ll)y),
	db(SHR, (ll)x >> (ll)y),
	db('&', (ll)x & (ll)y),
	db('|', (ll)x | (ll)y),
	db('^', (ll)x ^ (ll)y),
	db(AND, x && y),
	db(OR,  x || y),
	db(POW, pow(x, y)),
	db(IND, floor(x/y)),
	db('+', x + y),
	db('-', x - y),
	db('*', x * y),
	db('/', x / y),
	db('%', fmod(x, y)),
	db('<', x < y),
	db('>', x > y),
	db(LEQ, x <= y),
	db(GEQ, x >= y),
	db(NEQ, x != y),
	db(EQU, x == y),
	db(SPC, x<y?-1:(x>y?1:0)/*x <=> y*/)
};

enum ExprType {
	INFIX,
	RPN
};

/** Check left-associative operators.
 *
 * @param {char}op
 * @return bool
 */
#define lassoc(X) (!(strchr("!~pm", X)))

/** Converts a number to a std::string object.
 *
 * @param  void
 * @return void
 */
static Inline std::string
ldtos(ld x)
{
	std::string str;
	size_t len;
	/* empty */ {
		std::stringstream ss;
		ss << std::fixed << x;
		str = ss.str();
	}
	if (str.find('.') != std::string::npos) {
		while (!str.empty() && str.back() == '0')
			str.pop_back();
		if (!str.empty() && str.back() == '.')
			str.pop_back();
	}
	return str;
}

/** "Execute" operator in the Shunting-Yard alg.
 * 
 * @param {char}op,{stack<ld>&}nums,{stack<char>&}ops
 * @return bool
 */
static bool
expr_op(char op, std::stack<ld>& nums, std::stack<char>& ops)
{
	ll len = nums.size();

	/*******************************
 	 * Unary operators & functions *
	 *******************************/
	if (unary_table.find(op) != unary_table.end()) {
		if (!len)
			return false;
		ld x1 = nums.top(); nums.pop();
		nums.push(unary_table[op](x1));

	/********************
	 * Ternary operator *
	 ********************/
	} else if (op == '?') {
		ops.push(op);
	} else if (op == ':') {
		if (ops.top() != '?' || len < 3)
			return false;
		ops.pop();
		ld x3 = nums.top(); nums.pop();
		ld x2 = nums.top(); nums.pop();
		ld x1 = nums.top(); nums.pop();
		nums.push(x1?x2:x3);

	/********************
	 * Binary operators *
	 ********************/
	} else {
		if (len < 2)
			return false;
		ld x2 = nums.top(); nums.pop();
		ld x1 = nums.top(); nums.pop();
		nums.push(binary_table[op](x1, x2));
	}
	return true;
}

/** Evaluates an arithmetic expression.
 *
 * @param  void
 * @return void
 */
std::string
expr(std::string e, ExprType mode)
{
	std::stack<char> ops;
	std::stack<ld> nums;
	std::string tok;
	std::string old = e;

	str_subst(e);
	if (!is_expr(e))
		INVALIDSYN;
	
	//=====functions===== =====operators=====
	REP("log10", LOG10);  REP("&&" , AND);
	REP("log2" , LOG2 );  REP("||" , OR );
	REP("log"  , LOG  );  REP("<<" , SHL);
	REP("sqrt" , SQRT );  REP(">>" , SHR);
	REP("sin"  , SIN  );  REP("<=>", SPC);
	REP("cos"  , COS  );  REP("<=" , LEQ);
	REP("ctg"  , CTG  );  REP(">=" , GEQ);
	REP("tg"   , TG   );  REP("==" , EQU);
	REP("floor", FLOOR);  REP("!=" , NEQ);
	REP("ceil" , CEIL );  REP("**" , POW);
	REP("abs"  , ABS  );  REP("//" , IND);
	REP("round", ROUND);
	//=====alt=====
	REP("and"  , AND);
	REP("or"   , OR );
	REP("nan"  , '0');
	REP("false", '0');
	REP("true" , '1');

	if (mode == INFIX) {
		e.erase(remove_if(e.begin(), e.end(),
					[](char x){return isspace(x);}), e.end());
		for (int i = 0, len = e.length(); i < len; ++i) {
			/// number
			if (isnum(e[i])) {
				std::string buf;
				while (isnum(e[i]) && i < len)
					buf += e[i++];
				--i;
				try {
					nums.push(std::stold(buf));
				} catch (std::exception& ex) 
					{}

			/// opening paren
			} else if (e[i] == '(') {
				ops.push('(');
			
			/// closing paren
			} else if (e[i] == ')') {
				while (!ops.empty() && ops.top() != '(') {
					if (!expr_op(ops.top(), nums, ops))
						INVALIDSYN;
					ops.pop();
				}
				if (!ops.empty())
					ops.pop();
			
			/// other (operator)
			} else {
				if (!i || !isnum(e[i-1]) && e[i-1] != ')') {
					if (e[i] == '+') e[i] = 'p';
					if (e[i] == '-') e[i] = 'm';
				}
				while (!ops.empty()
				&& prio[e[i]] <= prio[ops.top()]
				&& lassoc(e[i])) {
					if (!expr_op(ops.top(), nums, ops))
						INVALIDSYN;
					ops.pop();
				}
				ops.push(e[i]);
			}
		}
		while (!ops.empty()) {
			if (!expr_op(ops.top(), nums, ops))
				INVALIDSYN;
			ops.pop();
		}
	} else {
		std::stringstream ss{e};
		ld val;
		while (ss >> tok) {
			bool isop = false;
			try {
				val = std::stold(tok);
			} catch (std::exception& ex) {
				isop = true;
			}
			if (isop && !nums.empty()) {
				if (!expr_op(tok[0], nums, ops))
					INVALIDSYN;
			} else {
				nums.push(val);
			}
		}
	}

	if (!nums.empty())
		return ldtos(nums.top());
	else
		return "nan";
}

// Default action
std::string expr(std::string e)
	{ return expr(e, ExprType::INFIX); }
