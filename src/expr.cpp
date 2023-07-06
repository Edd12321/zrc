#define syn std::cerr << errmsg

#define ld long double
#define li long int
#define isnum(X) (isdigit(X) || X == '.' || islower(X))
#define INVALIDSYN() {\
	syn << old << '\n';\
	return (char*)"nan";\
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
	  {    '(', -13 }, {    ')', -13 },
	  {    '?', -12 }, {    ':', -12 },
	  {  OR[0], -11 },
	  { AND[0], -10 },
	  {    '|',  -9 },
	  {    '^',  -8 },
	  {    '&',  -7 },
	  { EQU[0],  -6 }, { NEQ[0],  -6 },
	  {    '<',  -5 }, { LEQ[0],  -5 }, {    '>',  -5 }, { GEQ[0],  -5 },
	  { SPC[0],  -4 },
	  { SHL[0],  -3 }, { SHR[0],  -3 },
	  {    '+',  -2 }, {    '-',  -2 },
	  {    '*',  -1 }, {    '/',  -1 }, { IND[0],  -1 }, {    '%',  -1 }
	//{ implicit  0 }
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
		len = str.length();
		while (len --> 0 && str.back() == '0')
			str.pop_back();
		if (len && str.back() == '.')
			str.pop_back();
	}
	return str;
}

/** Evaluates an arithmetic expression.
 *
 * @param  void
 * @return void
 */
std::string
expr(std::string e, ExprType mode)
{
	std::string old = e;
	str_subst(e);
	if (!is_expr(e))
		INVALIDSYN();

	#define du(X,Y) {X,[](ld x){return Y;}} 
	DispatchTable<std::string, std::function<ld(ld const&)>> unary_table = {
		// unary ops
		du("!", !x), du("m", -x), du("p", +x), du("~", ~(int)x),

		// 1 arg functions
		du(LOG10, log10(x)),      du(LOG2, log2(x)), du(LOG,   log(x)),
		du(SQRT,  sqrt(x)),       du(SIN,  sin(x)),  du(COS,   cos(x)),
		du(CTG,   cos(x)/sin(x)), du(TG,   tan(x)),  du(FLOOR, floor(x)),
		du(CEIL,  ceil(x)),       du(ABS,  abs(x)),  du(ROUND, round(x))
	};

	#define db(X,Y) { X,[](ld x,ld y){return Y;}}
	#define di(O)   {#O,[](ld x,ld y){return (int)x O (int)y;}}
	#define dc(O)   {#O,[](ld x,ld y){return x O y;}}
	DispatchTable<std::string, std::function<ld(ld const&, ld const&)>> binary_table = {
		// binops
		dc(+), dc(-), dc(*), dc(/), dc(<), dc(>),
		di(&), di(|), di(^),

		db(SHL, (int)x<<(int)y),  db(LEQ, x <=  y),
		db(SHR, (int)x>>(int)y),  db(GEQ, x >=  y),
		db(AND, x && y),          db(NEQ, x !=  y),
		db(OR,  x || y),          db(EQU, x ==  y),
		db(POW,   pow(x, y)),     db(SPC, x<y?-1:(x>y?1:0)),
		db("%",  fmod(x, y)),
		db(IND, floor(x/ y))
	};

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
	REP("nan"  , "0");
	REP("false", "0");
	REP("true" , "1");

	std::stack<char> ops;
	std::stack<ld> nums;
	std::string rpn, tok;

	if (mode == RPN) {
		rpn = e;
	} else {
		e.erase(
			remove_if(
				e.begin(),
				e.end(),
				[](char x){return isspace(x);}
			),
			e.end()
		);

		for (int i = 0, len = e.length(); i < len; ++i) {
			//=====number=====
			if (isnum(e[i])) {
				rpn += e[i];
				if (i+1 >= len || !isnum(e[i+1]))
					rpn += ' ';
	
			//=====opening paren=====
			} else if (e[i] == '(') {
				ops.push('(');
	
			//=====closing paren=====
			} else if (e[i] == ')') {
				while (!ops.empty() && ops.top() != '(') {
					rpn += ops.top();
					rpn += ' ';
					ops.pop();
				}
				if (!ops.empty())
					ops.pop();
	
			//=====other=====
			} else {
				if (!i || !isnum(e[i-1]) && e[i-1] != ')') {
					if (e[i] == '+') e[i] = 'p';
					if (e[i] == '-') e[i] = 'm';
				}
				while (!ops.empty()
				&&     prio[e[i]] <= prio[ops.top()]
				&&     lassoc(e[i])) {
					rpn += ops.top();
					rpn += ' ';
					ops.pop();
				}
				ops.push(e[i]);
			}
		}
		while (!ops.empty()) {
			rpn += ops.top();
			rpn += ' ';
			ops.pop();
		}
	}
	std::stringstream ss{rpn};
	while (ss >> tok) {
		ld val;
		bool isop = false;

		try {
			val = std::stold(tok);
		} catch (std::exception& ex) {
			isop = true;
		}

		li len = nums.size();

		if (isop && !nums.empty()) {
			/*******************************
			 * Unary operators & functions *
			 *******************************/
			if (unary_table.find(tok) != unary_table.end()) {
				if (!len)
					INVALIDSYN();
				ld x1 = nums.top(); nums.pop();
				nums.push(unary_table[tok](x1));

			/********************
			 * Ternary operator *
			 ********************/
			} else if (tok[0] == '?') {
				ops.push(tok[0]);
			} else if (tok[0] == ':') {
				if (ops.top() != '?' || len < 3)
					INVALIDSYN();
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
					INVALIDSYN();
				
				ld x2 = nums.top(); nums.pop();
				ld x1 = nums.top(); nums.pop();
				nums.push(binary_table[tok](x1, x2));
			}
		} else {
			nums.push(val);
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
