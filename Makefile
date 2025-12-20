PREFIX = /usr#/local
SYSCONFDIR = /etc
SRCS = $(wildcard src/*.cpp src/*.hpp)
CXXFLAGS = -D_XOPEN_SOURCE=700 -std=c++11 -pedantic -Wno-unused-result
RELFLAGS = -O3
DBGFLAGS = -O0 -g -Wall -Wextra -fsanitize=undefined -fno-strict-alias -fwrapv -fno-omit-frame-pointer
SHELLPATH = $(DESTDIR)$(PREFIX)/bin/zrc
CXX ?= g++

.PHONY: all
all: bin/zrc
bin/zrc: $(SRCS) src/y.tab.cpp src/lex.yy.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(RELFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp -o bin/zrc
	strip bin/zrc

.PHONY: debug
debug: bin/zrc-debug
bin/zrc-debug: $(SRCS) src/y.tab.cpp src/lex.yy.cpp
	mkdir -p bin
	$(CXX) $(DBGFLAGS) $(DBGFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp -o bin/zrc-debug

src/y.tab.cpp: src/expr.y
	bison -d src/expr.y -o src/y.tab.cpp

src/lex.yy.cpp: src/expr.l
	flex -o src/lex.yy.cpp src/expr.l

.PHONY: install
install:
	install -Dm755 bin/zrc $(SHELLPATH)
	@grep -qxF '$(SHELLPATH)' $(SYSCONFDIR)/shells || echo $(SHELLPATH) | tee -a $(SYSCONFDIR)/shells

.PHONY: uninstall
uninstall:
	rm -f $(SHELLPATH)
	sed -i '\#$(SHELLPATH)#d' $(SYSCONFDIR)/shells

.PHONY: clean
clean:
	rm bin/*
