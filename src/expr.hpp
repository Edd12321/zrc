//=====ugly kludge=====
#define LOG10 "Q"
#define LOG2  "W"
#define LOG   "E"
#define SQRT  "R"
#define SIN   "T"
#define COS   "Y"
#define CTG   "U"
#define TG    "I"
#define FLOOR "O"
#define CEIL  "P"

#define AND   "A"
#define OR    "S"
#define SHL   "D"
#define SHR   "F"
#define SPC   "G"
#define LEQ   "H"
#define GEQ   "J"
#define EQU   "K"
#define NEQ   "L"
#define POW   "Z"
#define ABS   "X"

#define REP(X, Y) {\
	for (size_t pos = 0 ; ; pos += strlen(Y)) {\
		pos = e.find(X, pos);\
		if (pos == std::string::npos)\
			break;\
		e.erase(pos, strlen(X));\
		e.insert(pos, Y);\
	}\
}
