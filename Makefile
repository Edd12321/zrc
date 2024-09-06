all: expr
	mkdir -p bin
	$(CXX) -pedantic -std=c++11 -Wall -w src/lex.yy.c src/y.tab.c src/main.cpp -o bin/zrc
	strip bin/zrc
expr:
	bison -d src/expr.y -o src/y.tab.c
	flex -o src/lex.yy.c src/expr.l
clean:
	rm bin/*
