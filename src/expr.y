%{
#include <math.h>
#include <iostream>
#include "global.hpp"
#define YYSTYPE ld

int yylex();
YYSTYPE last_ret = NAN;

void yyerror(const char *msg)
	{ std::cerr << msg << '\n'; }
%}

%define parse.error verbose

%token YYEOF 0
%token NUM
%token LOG10 LOG2 LN SQRT SIN COS CTG TG SEC CSC ARCSIN ARCCOS ARCCTG ARCTG ARCSEC ARCCSC FLOOR TRUNC CEIL ABS ROUND

%left ','
%right '?' ':'
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NEQ
%left '<' LEQ '>' GEQ
%left SPC
%left SHL SHR
%left '+' '-'
%left '*' '/' '%' POW FDIV
%right '!' '~' LOG10 LOG2 LN SQRT SIN COS CTG TG SEC CSC ARCSIN ARCCOS ARCCTG ARCTG ARCSEC ARCCSC FLOOR TRUNC CEIL ABS ROUND
%left '(' ')'
%%
goal : expr YYEOF { last_ret = $1; YYACCEPT; }
     | YYEOF      { last_ret = 0; YYACCEPT; }
     ;

expr : expr ',' expr           { $$ = $3; }
     | expr '+' expr           { $$ = $1 + $3; }
     | expr '-' expr           { $$ = $1 - $3; }
     | expr '*' expr           { $$ = $1 * $3; }
     | expr '/' expr           { $$ = $1 / $3; }
     | expr '%' expr           { $$ = $1 - trunc($1/$3)*$3; }
     | expr '|' expr           { $$ = (ll)$1 | (ll)$3; }
     | expr '&' expr           { $$ = (ll)$1 & (ll)$3; }
     | expr '^' expr           { $$ = (ll)$1 ^ (ll)$3; }
     | expr '<' expr           { $$ = $1 < $3; }
     | expr '>' expr           { $$ = $1 > $3; }
     | expr SHL expr           { $$ = (ll)$1 << (ll)$3; }
     | expr SHR expr           { $$ = (ll)$1 >> (ll)$3; }
     | expr AND expr           { $$ = $1 && $3; }
     | expr OR expr            { $$ = $1 || $3; }
     | expr POW expr           { $$ = pow($1, $3); }
     | expr EQ expr            { $$ = $1 == $3; }
     | expr NEQ expr           { $$ = $1 != $3; }
     | expr LEQ expr           { $$ = $1 <= $3; }
     | expr GEQ expr           { $$ = $1 >= $3; }
     | expr SPC expr           { $$ = $1 < $3 ? -1: ($1 > $3 ? 1 : 0); }
     | expr FDIV expr          { $$ = floor($1/$3); }
     | LOG10 expr              { $$ = log10($2); }
     | LOG2 expr               { $$ = log2($2); }
     | LN expr                 { $$ = log($2); }
     | SQRT expr               { $$ = sqrt($2); }
     | SIN expr                { $$ = sin($2); }
     | COS expr                { $$ = cos($2); }
     | CTG expr                { $$ = cos($2)/sin($2); }
     | TG expr                 { $$ = sin($2)/cos($2); }
     | SEC expr                { $$ = 1.0/cos($2); }
     | CSC expr                { $$ = 1.0/sin($2); }
     | ARCSIN expr             { $$ = asin($2); }
     | ARCCOS expr             { $$ = acos($2); }
     | ARCCTG expr             { $$ = atan(1.0/$2); }
     | ARCTG expr              { $$ = atan($2); }
     | ARCSEC expr             { $$ = acos(1.0/$2); }
     | ARCCSC expr             { $$ = asin(1.0/$2); }
     | FLOOR expr              { $$ = floor($2); }
     | TRUNC expr              { $$ = trunc($2); }
     | CEIL expr               { $$ = ceil($2); }
     | ABS expr                { $$ = abs($2); }
     | ROUND expr              { $$ = round($2); }
     | '-' expr                { $$ = -$2; }
     | '+' expr                { $$ = +$2; }
     | '!' expr                { $$ = !$2; }
     | '~' expr                { $$ = ~(ll)$2; }
     | expr '?' expr ':' expr  { $$ = $1 ? $3 : $5; }
     | '(' expr ')'            { $$ = $2; }
     | '(' ')'                 { $$ = 0; }
     | NUM                     { $$ = $1; }
     ;
%%
