%{
#define YYSTYPE long double
#include <iostream>
#include <string>
#include "y.tab.hpp"
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
"<="|le   return LEQ;
">="|ge   return GEQ;
"=="|eq   return EQ;
"!="|ne   return NEQ;
"<=>"     return SPC;
"&&"|and  return AND;
"||"|or   return OR;
"^^"      return XOR;

log10|log                   return LOG10;
log2                        return LOG2;
ln                          return LN;
sqrt                        return SQRT;
sin                         return SIN;
cos                         return COS;
ctg|cot                     return CTG;
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
erf                         return ERF;
erfc                        return ERFC;
lgamma                      return LGAMMA;
tgamma                      return TGAMMA;
sgn                         return SGN;
%%
#include <string.h>
#include <math.h>
#define YYSTYPE long double

extern YYSTYPE last_ret;
extern std::string subst(const char *);

long double expr(const char *buf)
{
	std::string ret = subst(buf);
	YY_BUFFER_STATE sbuf = yy_scan_string(ret.c_str());
	yy_switch_to_buffer(sbuf);
	if (yyparse())
		std::cerr << "(in expression \"" << buf << "\")\n";
	yy_delete_buffer(sbuf);
	return last_ret;
}
