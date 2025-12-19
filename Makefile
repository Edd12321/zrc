PREFIX = /usr#/local
SYSCONFDIR = /etc
SRCS = $(wildcard src/*.cpp src/*.hpp)
CXXFLAGS = -std=c++11 -pedantic -Wno-unused-result -O3
DBGFLAGS = -std=c++11 -pedantic -g -Wall -Wextra -Wno-unused-result -O0 -fsanitize=undefined -fno-omit-frame-pointer
SHELLPATH = $(DESTDIR)$(PREFIX)/bin/zrc
CXX ?= g++

.PHONY: all
all: bin/zrc
bin/zrc: $(SRCS) src/y.tab.cpp src/lex.yy.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp -o bin/zrc
	strip bin/zrc

.PHONY: debug
debug: bin/zrc-debug
bin/zrc-debug: $(SRCS) src/y.tab.cpp src/lex.yy.cpp
	mkdir -p bin
	$(CXX) $(DBGFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp -o bin/zrc-debug

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
