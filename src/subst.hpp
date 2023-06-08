#define SCALAR "1234567890"\
               "$:_("\
               "QWERTYUIOPASDFGHJKLZXCVBNM"\
               "qwertyuiopasdfghjklzxcvbnm"

#define ITERATE_PAREN(X,Y) {                   \
    for (++i, cmpnd = 1; i < len; ++i) {       \
        if (i == len-1 && str[i] != Y)         \
            die("Unmatched paren");            \
        if (str[i] == X) ++cmpnd;              \
        if (str[i] == Y) --cmpnd;              \
        if (!cmpnd)                            \
            break;                             \
        tmp2 += str[i];                        \
    }                                          \
}

#define NO_QUOTES (!quote['\''] && !quote['"'] && !quote['`'])
/** Get the variable of a string.
 * 
 * @param {string}str
 * @return string
 */
static std::string
get_var(std::string str)
{
	str_subst(str);
	return getvar(str);
}

/** Performs word expansion/substitution.
 * 
 * @param {string&}str
 * @return void
 */
void
str_subst(std::string& str)
{
	size_t i, len, pos, cmpnd;
	std::string tmp1, tmp2, res = "";
	std::map<char, bool> quote;

	len = str.length();

	// Brace quoting:
	// No substitutions are performed!
	if (str[0] == '{' && str.back() == '}') {
		rq(str);
		return;
	}

	for (i = 0; i < len; ++i) {
		switch(str[i]) {

		/*******************
		 * Escape sequence *
		 *******************/
		case '\\':
			if (i < len-1) {
				switch(str[++i]) {
				case 'a': res += '\a'  ; break;
				case 'b': res += '\b'  ; break;
				case 'f': res += '\f'  ; break;
				case 'n': res += '\n'  ; break;
				case 'r': res += '\r'  ; break;
				case 't': res += '\t'  ; break;
				case 'v': res += '\v'  ; break;
				default : res += str[i]; break;
				}
			}
			break;

		case '\'':
		case  '"':
			if NO_QUOTES
				quote[str[i]] = true;
			else if (quote[str[i]])
				quote[str[i]] = false;
			else
				res += str[i];
			break;

		case '`':
			tmp1.clear();
			tmp2.clear();
			/************************
			 * Command substitution *
			 ************************/
			if (i < len-1 && str[i+1] == '{') {
				++i;
				ITERATE_PAREN('{','}');
				// Remove trailing newline unless specified otherwise
				if (tmp2.front() == '{' && tmp2.back() == '}')
					rq(tmp2);
				std::string out = io_cap(tmp2);
				if (NO_QUOTES && !out.empty() && out.back() == '\n')
					out.pop_back();
				res += out;
			} else {
				res += '`';
			}
			break;

		case '$':
			tmp1.clear();
			tmp2.clear();
		/**********************
		 * Variable expansion *
		 **********************/
			if (i < len-1 && strchr("?!", str[i+1])) {
				std::string tmp;
				tmp += str[++i];
				res += getvar(tmp);
			} else {
				bool arr_ok = false;
				for (++i; i < len && strchr(SCALAR, str[i]); ++i) {
					tmp1 += str[i];
					if (str[i] == '(') {
						ITERATE_PAREN('(',')');
						tmp1 += tmp2+")";
						arr_ok = true;
						break;
					}
				}
				if (!arr_ok) {
					str_subst(tmp1);
					--i;
				}
				res += get_var(tmp1);
			}
			break;


		/***********************
		 * Return substitution *
		 ***********************/
		case '[':
			tmp2.clear();
			ITERATE_PAREN('[',']');
			res += eval(tmp2);
			break;
		default:
			res += str[i];
			break;
		}
	}
	str = res;
}

