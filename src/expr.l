%{
#define YYSTYPE long double

#include <string>
#include "y.tab.h"
int yyparse();
%}
%option noyywrap

num  ([0-9]+\.?|[0-9]*\.[0-9]+)|(0x[0-9A-Fa-f]+)|inf|nan

%%
{num} {
	yylval = std::stold(yytext);
	return NUM;
}

true { yylval = true; return NUM; }
false { yylval = false; return NUM; }

[ \t\r\n\v\f\r]+ ;
. return *yytext;

"**"      return POW;
"//"      return FDIV;
"<<"      return SHL;
">>"      return SHR;
"<="      return LEQ;
">="      return GEQ;
"=="      return EQ;
"!="      return NEQ;
"<=>"     return SPC;
"&&"|and  return AND;
"||"|or   return OR;

log10|log                   return LOG10;
log2                        return LOG2;
ln                          return LN;
sqrt                        return SQRT;
sin                         return SIN;
cos                         return COS;
ctg                         return CTG;
tg|tan                      return TG;
sec                         return SEC;
csc|cosec                   return CSC;
arcsin|asin                 return ARCSIN;
arccos|acos                 return ARCCOS;
arcctg|arccot|actg|acot     return ARCCTG;
arctg|arctan|atg|atan       return ARCTG;
arcsec|asec                 return ARCSEC;
arccsc|arccosec|acsc|acosec return ARCCSC;
floor                       return FLOOR;
trunc                       return TRUNC;
ceil                        return CEIL;
abs                         return ABS;
round                       return ROUND;
%%
#include <string.h>
#include <math.h>
#include "global.hpp"
#define YYSTYPE long double

extern YYSTYPE last_ret;
extern bool str_subst(std::string&);

/** Evaluates arithmetic expressions.
 * 
 * @param {string}s
 * @return ld
 */
ld
expr(std::string s)
{
	if (!str_subst(s))
		return NAN;
	auto len = s.size();
	char *buf = new char[len+2];
	strcpy(buf, s.c_str());
	// Apparently yacc needs double null-termination.
	buf[len+1] = '\0';

	YY_BUFFER_STATE sbuf = yy_scan_string(buf);
	yy_switch_to_buffer(sbuf);
	yyparse();
	yy_delete_buffer(sbuf);
	return last_ret;
}
