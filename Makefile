.POSIX:
# Do not override these
SRCS = src/command.cpp src/config.hpp src/dispatch.cpp src/expr.cpp src/globals.hpp src/list.cpp src/main.cpp src/syn.cpp src/vars.cpp src/zlineedit.cpp
CXXFLAGS = -D_XOPEN_SOURCE=700 -std=c++11 -pedantic -Wno-unused-result
SETUP_PEVAL = set -e; peval() { echo "$$1"; eval "$$1"; }

# These you can
CXX = c++
RELFLAGS = $(CXXFLAGS) -O3 -funroll-loops
DBGFLAGS = $(CXXFLAGS) -O0 -Wextra -g -fsanitize=address,undefined -fno-strict-aliasing -fwrapv -fno-omit-frame-pointer
PREFIX = /usr
SYSCONFDIR = $(DESTDIR)/etc
SHELLPATH = $(DESTDIR)$(PREFIX)/bin/zrc

# .PHONY: release
release: bin/zrc
bin/zrc: $(SRCS)
	mkdir -p bin
	@$(SETUP_PEVAL); \
	case "$$(uname -s)" in \
		CYGWIN*) \
			peval 'cd img/icon && windres winico.rc winico.o && cd ../..'; \
			peval '$(CXX) $(RELFLAGS) src/main.cpp img/icon/winico.o -o bin/zrc.exe'; \
			peval 'strip bin/zrc.exe'; \
			;; \
		*) \
			peval '$(CXX) $(RELFLAGS) src/main.cpp -o bin/zrc'; \
			peval 'strip bin/zrc'; \
			;; \
	esac

# .PHONY: debug
debug: bin/zrc-debug
bin/zrc-debug: $(SRCS)
	@$(SETUP_PEVAL); \
	test -d bin || peval 'mkdir -p bin'
	$(CXX) $(DBGFLAGS) src/main.cpp -o bin/zrc-debug

# .PHONY: all
all: release debug

# .PHONY: install
install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f bin/zrc $(SHELLPATH)
	chmod 755 $(SHELLPATH)
	@$(SETUP_PEVAL); \
	test -d $(SYSCONFDIR) || peval 'mkdir -p $(SYSCONFDIR)'; \
	grep 2>/dev/null -qxF $(SHELLPATH) $(SYSCONFDIR)/shells || peval 'echo $(SHELLPATH) >> $(SYSCONFDIR)/shells'

# .PHONY: uninstall
uninstall:
	rm -f $(SHELLPATH)
	@$(SETUP_PEVAL); \
	SHELLPATH_ESC=$$(printf "%s" $(SHELLPATH) | sed 's|/|\\/|g'); \
	peval "printf "%s" 'g/$$SHELLPATH_ESC/d\nw\nq\n' | ed -s $(SYSCONFDIR)/shells"

# .PHONY: clean
clean:
	rm -f bin/zrc*

# .PHONY: help
help:
	@echo release - Build release binary
	@echo debug - Build debugging binary
	@echo all - Build both
	@echo install - Copy release binary to $(SHELLPATH)
	@echo uninstall - Remove said binary
	@echo clean - Clean bin/ folder
