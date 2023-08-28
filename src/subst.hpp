#define SCALAR "1234567890"\
               "$_("\
               "QWERTYUIOPASDFGHJKLZXCVBNM"\
               "qwertyuiopasdfghjklzxcvbnm"

#define ITERATE_PAREN(X,Y) {                   \
    for (++i, cmpnd = 1; i < len; ++i) {       \
        if (i == len-1 && str[i] != Y)         \
            std::cerr << "Unmatched paren\n";  \
        if (str[i] == '\\') {                  \
          tmp2 += '\\';                        \
          tmp2 += str[++i];                    \
          continue;                            \
        }                                      \
        if (str[i] == X) ++cmpnd;              \
        if (str[i] == Y) --cmpnd;              \
        if (!cmpnd)                            \
            break;                             \
        tmp2 += str[i];                        \
    }                                          \
}

#define NO_QUOTES (!quote['\''] && !quote['"'] && !quote['`'])

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
	bool orig_f = false;

	len = str.length();

	// Brace quoting:
	// No substitutions are performed!
	if (str[0] == '{' && str.back() == '}') {
		str.erase(0, 1);
		str.pop_back();
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
				/*if NO_QUOTES ++i;*/ ++i;
				ITERATE_PAREN('{','}');
				orig_f = in_func;
				in_func = false;
				std::string out = io_cap(tmp2);
				in_func = orig_f;
				// Remove trailing newline unless specified otherwise
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
			
			// $?, $!
			if (i < len-1 && strchr("?!", str[i+1])) {
				std::string tmp;
				tmp += str[++i];
				res += getvar(tmp);

			// ${...} syntax
			} else if (str[i+1] == '{') {
				/*if NO_QUOTES ++i;*/ ++i;
				ITERATE_PAREN('{', '}');
				str_subst(tmp2);
				res += getvar(tmp2);
			
			// $... and $...(...) syntax
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
				str_subst(tmp1);
				res += getvar(tmp1);
			}
			break;


		/***********************
		 * Return substitution *
		 ***********************/
		case '[':
			tmp2.clear();
			ITERATE_PAREN('[',']');

			orig_f = in_func;
			in_func = false;
			res += eval(tmp2);
			in_func = orig_f;

			break;
		default:
			res += str[i];
			break;
		}
	}
	str = res;
}

