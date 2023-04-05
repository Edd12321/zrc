#define syn std::cerr << errmsg

#define ld long double
#define li long int
#define isnum(X) (isdigit(X) || X == '.' || islower(X))
#define INVALIDSYN() {\
	syn << old << '\n';\
	return (char*)"nan";\
}

std::stack<char*> unfreed;

/** Checks if a string contains only mathematical chars.
 *
 * @param  void
 * @return void
 */
extern inline bool
is_expr(std::string str)
{
	return str.find_first_not_of(
		".0123456789 \t\n"
		"<=>+-*/%^~!?:()<>&|"
		"qwertyuiopasdfghjklzxcvbnm"
	) == std::string::npos;
}

/** Checks operator precedence (PEMDAS).
 *
 * @param  void
 * @return void
 */
static inline int
prio(char op)
{
	if (op == '?'    || op == ':')                                 return  0;
	if (op ==  OR[0])                                              return  1;
	if (op == AND[0])                                              return  2;
	if (op == '|')                                                 return  3;
	if (op == '^')                                                 return  4;
	if (op == '&')                                                 return  5;
	if (op == EQU[0] || op == NEQ[0])                              return  6;
	if (op == '<'    || op == LEQ[0] || op == '>' || op == GEQ[0]) return  7;
	if (op == SPC[0])                                              return  8;
	if (op == SHL[0] || op == SHR[0])                              return  9;
	if (op == '+'    || op == '-')                                 return 10;
	if (op == '*'    || op == '/'    || op == '%')                 return 11;
	if (op == '('    || op == ')')                                 return -1;
	/* [...] */                                                    return 12;
}

/** Check left-associative operators.
 *
 * @param {char}op
 * @return bool
 */
static inline bool
lassoc(char op)
{
	return !strchr("!~pm", op);
}

/** Frees memory garbage
 * 
 * @param  none
 * @return void
 */
static void
cleanup_memory()
{
	while (!unfreed.empty()) {
		free(unfreed.top());
		unfreed.pop();
	}
}

/** Converts a number to a char pointer/array.
 *
 * @param  void
 * @return void
 */
static inline char*
ldtoa(ld x)
{
	int i, k, max = 4+LDBL_DIG-LDBL_MIN_10_EXP;
	char *p, *buf = (char*)malloc(max);
	sprintf(buf, "%Lf", x);
	p = strchr(buf, '.');
	if (p != NULL) {
		k = max;
		while (k--) {
			if (*p == '\0')
				break;
			++p;
		}
		*p-- = '\0';
		while (*p == '0')
			*p-- = '\0';
		if (*p == '.')
			*p = '\0';
	}
	unfreed.push(buf);
	return buf;
}

/** Evaluates an arithmetic expression.
 *
 * @param  void
 * @return void
 */
char *
expr(std::string e)
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
		du(CEIL,  ceil(x)),       du(ABS,  abs(x))
	};

	#define db(X,Y) { X,[](ld x,ld y){return Y;}}
	#define di(O)   {#O,[](ld x,ld y){return (int)x O (int)y;}}
	#define dc(O)   {#O,[](ld x,ld y){return x O y;}}
	DispatchTable<std::string, std::function<ld(ld const&, ld const&)>> binary_table = {
		// binops
		dc(+), dc(-), dc(*), dc(/), dc(<), dc(>),
		di(&), di(|), di(^),

		db(SHL, (int)x<<(int)y), db(LEQ, x <=  y),
		db(SHR, (int)x>>(int)y), db(GEQ, x >=  y),
		db(AND, x && y),         db(NEQ, x !=  y),
		db(OR,  x || y),         db(EQU, x ==  y),
		db(POW,  pow(x, y)),     db(SPC, x<y?-1:(x>y?1:0)),
		db("%", fmod(x, y))
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
	REP("abs"  , ABS  );

	//=====alt=====
	REP("and"  , AND);
	REP("or"   , OR );
	REP("nan"  , "0");
	REP("false", "0");
	REP("true" , "1");

	e.erase(
		remove_if(
			e.begin(),
			e.end(),
			[](char x){return isspace(x);}
		),
		e.end()
	);

	std::stack<char> ops;
	std::stack<ld>   nums;
	std::string      rpn;

	char *tok, *endptr;
	ld val;

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
			&&     prio(e[i]) <= prio(ops.top())
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

	tok = strtok(&rpn[0], " ");
	while (tok != NULL) {
		val = strtold(tok, &endptr);
		li len = nums.size();

		if (tok == endptr && !nums.empty()) {
			/*******************************
			 * Unary operators & functions *
			 *******************************/
			if (strchr("!~pmQWERTYUIOPX", tok[0])) {
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
		tok = strtok(NULL, " ");
	}

	if (!nums.empty())
		return ldtoa(nums.top());
	else
		return (char*)"nan";
}
