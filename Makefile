PREFIX = /usr#/local
SYSCONFDIR = /etc
SRCS = $(wildcard src/*.cpp src/*.hpp)
CXXFLAGS = -D_XOPEN_SOURCE=700 -std=c++11 -pedantic -Wno-unused-result
RELFLAGS = -O3
DBGFLAGS = -O0 -Wextra -g -fsanitize=address,undefined -fno-strict-aliasing -fwrapv -fno-omit-frame-pointer
SHELLPATH = $(DESTDIR)$(PREFIX)/bin/zrc
CXX ?= g++

.PHONY: release
release: bin/zrc
bin/zrc: $(SRCS) src/y.tab.cpp src/lex.yy.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(RELFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp -o bin/zrc
	strip bin/zrc

.PHONY: debug
debug: bin/zrc-debug
bin/zrc-debug: $(SRCS) src/y.tab.cpp src/lex.yy.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp -o bin/zrc-debug

.PHONY: all
all: release debug

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
	rm -f bin/zrc*

.PHONY: help
help:
	@echo release - Build -O3 stripped binary
	@echo debug - Build -O0 ASan/UBSan binary
	@echo all - Build both
	@echo install - Copy release binary to $(SHELLPATH)
	@echo uninstall - Remove said binary
	@echo clean - Clean bin/ folder
