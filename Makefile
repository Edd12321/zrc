PREFIX ?= /usr
SYSCONFDIR ?= /etc
SRCS = $(wildcard src/*.cpp src/*.hpp)
CXXFLAGS = -D_XOPEN_SOURCE=700 -std=c++11 -pedantic -Wno-unused-result
RELFLAGS ?= $(CXXFLAGS) -O3
DBGFLAGS ?= $(CXXFLAGS) -O0 -Wextra -g -fsanitize=address,undefined -fno-strict-aliasing -fwrapv -fno-omit-frame-pointer
SHELLPATH ?= $(DESTDIR)$(PREFIX)/bin/zrc
CXX ?= g++

.PHONY: release
release: bin/zrc
bin/zrc: $(SRCS) src/y.tab.cpp src/lex.yy.cpp
	mkdir -p bin
	@set -e; peval() { echo $$1; eval $$1; }; \
	case "$$(uname -s)" in \
		CYGWIN*) \
			peval 'cd img/icon && windres winico.rc winico.o && cd ../..'; \
			peval '$(CXX) $(RELFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp img/icon/winico.o -o bin/zrc.exe'; \
			peval 'strip bin/zrc.exe'; \
			;; \
		*) \
			peval '$(CXX) $(RELFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp -o bin/zrc'; \
			peval 'strip bin/zrc'; \
			;; \
	esac

.PHONY: debug
debug: bin/zrc-debug
bin/zrc-debug: $(SRCS) src/y.tab.cpp src/lex.yy.cpp
	mkdir -p bin
	$(CXX) $(DBGFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp -o bin/zrc-debug

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
	@echo release - Build release binary
	@echo debug - Build debugging binary
	@echo all - Build both
	@echo install - Copy release binary to $(SHELLPATH)
	@echo uninstall - Remove said binary
	@echo clean - Clean bin/ folder
