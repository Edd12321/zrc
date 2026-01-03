.POSIX:
# Do not override these
SRCS = src/command.cpp src/custom_cmd.cpp src/dispatch.cpp \
	   src/expr.cpp src/list.cpp src/main.cpp src/path.cpp \
	   src/sig.cpp src/syn.cpp src/vars.cpp src/zlineedit.cpp
OBJS = $(SRCS:.cpp=.o)
DOBJS = $(SRCS:.cpp=.do)
CXXFLAGS = -D_XOPEN_SOURCE=700 -std=c++11 -pedantic -Wno-unused-result
SETUP_PEVAL = set -e; peval() { echo "$$1"; eval "$$1"; }
# Override these 
CXX = c++
RELFLAGS = $(CXXFLAGS) -O3 -funroll-loops
DBGFLAGS = $(CXXFLAGS) -O0 -Wextra -g -fsanitize=address,undefined -fno-strict-aliasing -fwrapv -fno-omit-frame-pointer
PREFIX = /usr
SYSCONFDIR = $(DESTDIR)/etc
SHELLPATH = $(DESTDIR)$(PREFIX)/bin/zrc

.SUFFIXES: .cpp .o .do
.cpp.o:
	$(CXX) $(RELFLAGS) -c $< -o $@
.cpp.do:
	$(CXX) $(DBGFLAGS) -c $< -o $@

# .PHONY: release
release: bin/zrc
bin/zrc: $(OBJS)
	mkdir -p bin
	@$(SETUP_PEVAL); \
	case "$$(uname -s)" in \
		CYGWIN*) \
			peval 'cd img/icon && windres winico.rc winico.o && cd ../..'; \
			peval '$(CXX) $(RELFLAGS) $(OBJS) img/icon/winico.o -o bin/zrc.exe'; \
			peval 'strip bin/zrc.exe'; \
			;; \
		*) \
			peval '$(CXX) $(RELFLAGS) $(OBJS) -o bin/zrc'; \
			peval 'strip bin/zrc'; \
			;; \
	esac

# .PHONY: debug
debug: bin/zrc-debug
bin/zrc-debug: $(DOBJS)
	@$(SETUP_PEVAL); \
	test -d bin || peval 'mkdir -p bin'
	$(CXX) $(DBGFLAGS) $(DOBJS) -o bin/zrc-debug

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
	rm -f bin/zrc* src/*.o src/*.do img/icon/*.o

# .PHONY: help
help:
	@echo release - Build release binary
	@echo debug - Build debugging binary
	@echo all - Build both
	@echo install - Copy release binary to $(SHELLPATH)
	@echo uninstall - Remove said binary
	@echo clean - Clean bin/ folder

# Track headers manually
src/command.o src/command.do: src/custom_cmd.hpp src/command.hpp src/list.hpp src/vars.hpp src/zlineedit.hpp src/sig.hpp src/path.hpp
src/custom_cmd.o src/custom_cmd.do: src/custom_cmd.hpp src/global.hpp src/vars.hpp src/syn.hpp
src/dispatch.o src/dispatch.do: src/custom_cmd.hpp src/command.hpp src/global.hpp src/sig.hpp src/vars.hpp src/expr.hpp src/path.hpp src/list.hpp src/config.hpp src/syn.hpp src/zlineedit.hpp
src/expr.o src/expr.do: src/global.hpp src/syn.hpp
src/list.o src/list.do: src/list.hpp src/syn.hpp
src/main.o src/main.do: src/command.hpp src/config.hpp src/custom_cmd.hpp src/expr.hpp src/global.hpp src/sig.hpp src/syn.hpp src/vars.hpp src/zlineedit.hpp
src/path.o src/path.do: src/path.hpp src/vars.hpp
src/sig.o src/sig.do: src/global.hpp src/command.hpp src/custom_cmd.hpp src/sig.hpp src/vars.hpp
src/syn.o src/syn.do: src/syn.hpp src/list.hpp src/global.hpp src/config.hpp src/vars.hpp src/command.hpp
src/vars.o src/vars.do: src/global.hpp src/syn.hpp src/vars.hpp
src/zlineedit.o src/zlineedit.do: src/custom_cmd.hpp src/zlineedit.hpp src/config.hpp src/global.hpp src/vars.hpp src/syn.hpp src/sig.hpp src/path.hpp
