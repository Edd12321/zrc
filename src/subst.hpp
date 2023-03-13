#define SCALAR "1234567890"\
               "$:_?!"\
               "QWERTYUIOPASDFGHJKLZXCVBNM"\
               "qwertyuiopasdfghjklzxcvbnm"

#define ARRAY  "1234567890"\
               "_@()"\
               "QWERTYUIOPASDFGHJKLZXCVBNM"\
               "qwertyuiopasdfghjklzxcvbnm"

#define ITERATE_PAREN {                        \
    for (++i, cmpnd = 1; i < len; ++i) {       \
        if (i == len-1 && str[i] != ')')       \
            die("Unmatched paren");            \
        if (str[i] == '(') ++cmpnd;            \
        if (str[i] == ')') --cmpnd;            \
        if (!cmpnd)                            \
            break;                             \
        tmp2 += str[i];                        \
    }                                          \
}
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
	bool quote = false;

	len = str.length();

	// Brace quoting:
	// No substitutions are performed!
	if ((str[0] == '{' && str.back() == '}')
	||  (str[0] == '`' && str.back() == '`')) {
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
		case  '`':
			quote = !quote;
			break;

		case '$':
			tmp1.clear();
			tmp2.clear();

		/************************
		 * Command substitution *
		 ************************/
			if (i < len-1 && str[i+1] == '(') {
				++i;
				ITERATE_PAREN;
				res += io_cap(tmp2);

		/**********************
		 * Variable expansion *
		 **********************/
			} else {
				for (++i; i < len && strchr(SCALAR, str[i]); ++i)
					tmp1 += str[i];
				str_subst(tmp1);
				res += get_var(tmp1);
				--i;
			}
			break;
		case '@':
			tmp1.clear();
			tmp2.clear();
			for (++i; i < len && strchr(ARRAY, str[i]); ++i) {
				tmp1 += str[i];
				if (str[i] == '(') {
					ITERATE_PAREN;
					str_subst(tmp2);
					tmp1 += tmp2+")";
					str_subst(tmp1);
					res += get_var(tmp1);
					break;
				}
			}
			break;


		/***********************
		 * Return substitution *
		 ***********************/
		case '[':
			tmp1.clear();
			for (++i, cmpnd = 1; i < len; ++i) {
				if (i == len-1 && str[i] != ']')
					die("Unmatched bracket");
				if (str[i] == '[') ++cmpnd;
				if (str[i] == ']') --cmpnd;
				if (!cmpnd)
					break;
				tmp1 += str[i];
			}
			res += eval(tmp1);
			break;
		default:
			res += str[i];
			break;
		}
	}
	str = res;
}

