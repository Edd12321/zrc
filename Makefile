UNAME := $(shell uname -o)
CC=$(CXX)

all: expr
	mkdir -p bin
	g++ -w src/lex.yy.c src/y.tab.c src/main.cpp -o bin/zrc
	strip bin/zrc
nozledit: expr
	mkdir -p bin
	g++ -w src/lex.yy.c src/y.tab.c src/main.cpp -o bin/zrc -DUSE_ZLINEEDIT=0
	strip bin/zrc
nohash: expr
	mkdir -p bin
	g++ -w src/lex.yy.c src/y.tab.c src/main.cpp -o bin/zrc -DUSE_HASHCACHE=0
	strip bin/zrc
sloc:
	sloccount . | tee sloccount.txt
install:
ifeq ($(UNAME), Android)
	ln -sf $$(pwd) '/data/data/com.termux/files/zrc'
else
	ln -sf $$(pwd) '/usr/lib/zrc'
endif
corebuf:
	cd corebuf; \
		./build.zrc
expr:
	bison -d src/expr.y -o src/y.tab.c
	flex -o src/lex.yy.c src/expr.l
config:
	[ -f $(HOME) ] || cp .zrc $(HOME)
clean:
	rm bin/zrc

.PHONY: corebuf config
