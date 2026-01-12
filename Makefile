# vim: set tabstop=4 shiftwidth=4 noexpandtab: -*- tab-width: 4; indent-tabs-mode: t -*-
.POSIX:

# Do not override these
SRCS = src/command.cpp src/custom_cmd.cpp src/dispatch.cpp                                          \
       src/expr.cpp src/list.cpp src/main.cpp src/path.cpp                                          \
       src/sig.cpp src/syn.cpp src/vars.cpp src/zlineedit.cpp
OBJS = $(SRCS:.cpp=.o)
DOBJS = $(SRCS:.cpp=.do)
CXXFLAGS = -std=c++11 -pedantic -Wno-unused-result -Winvalid-pch
SETUP_CUSTOM_SH =
    set -e;                                                                                         \
    peval() { echo "$$1";           eval "$$1"; };                                                  \
    pcomp() { echo "  CXX     $$2"; eval "$$1 $$2"; };                                              \
    pcdbg() { echo "  CXX (D) $$2"; eval "$$1 $$2"; };                                              \
    case "$$($(CXX) --version | head -n 1)" in                                                      \
        *clang*)                                                                                    \
            PCH_EXT=pch; PCH_FLAGS="-x c++-header -Xclang -emit-pch"                                \
            ;;                                                                                      \
        *)                                                                                          \
            PCH_EXT=gch; PCH_FLAGS="-x c++-header"                                                  \
            ;;                                                                                      \
    esac;
# Override these 
CXX = c++
RELFLAGS = $(CXXFLAGS) -O3 -funroll-loops
DBGFLAGS = $(CXXFLAGS) -O0 -Wextra -g -fsanitize=address,undefined                                  \
                       -fno-strict-aliasing -fwrapv -fno-omit-frame-pointer
PREFIX = /usr
SYSCONFDIR = $(DESTDIR)/etc
SHELLPATH = $(DESTDIR)$(PREFIX)/bin/zrc
STRIP = y

.SUFFIXES: .cpp .o .do
.cpp.o:
	@$(SETUP_CUSTOM_SH)                                                                             \
	pcomp '$(CXX) $(RELFLAGS) -c -o $@' '$<'
.cpp.do:
	@$(SETUP_CUSTOM_SH)                                                                             \
	pcdbg '$(CXX) $(DBGFLAGS) -c -o $@' '$<'

release: bin/zrc
bin/zrc: $(OBJS)
	@$(SETUP_CUSTOM_SH)                                                                             \
	[ -d bin ] || peval 'mkdir -p bin';                                                             \
	case "$$(uname -s)" in                                                                          \
	    CYGWIN*)                                                                                    \
	        peval 'cd img/icon && windres winico.rc winico.o && cd ../..';                          \
	        pcomp '$(CXX) $(RELFLAGS) -o bin/zrc.exe' '$(OBJS) img/icon/winico.o';                  \
	        ! [ "$(STRIP)" = y ] || peval 'strip bin/zrc.exe';                                      \
	        ;;                                                                                      \
	    *)                                                                                          \
	        pcomp '$(CXX) $(RELFLAGS) -o bin/zrc' '$(OBJS)';                                        \
	        ! [ "$(STRIP)" = y ] || peval 'strip bin/zrc';                                          \
	        ;;                                                                                      \
	esac

debug: bin/zrc-debug
bin/zrc-debug: $(DOBJS)
	@$(SETUP_CUSTOM_SH)                                                                             \
	[ -d bin ] || peval 'mkdir -p bin';                                                             \
	pcdbg '$(CXX) $(DBGFLAGS) -o bin/zrc-debug' '$(DOBJS)'

all: release debug

src/pch-rel.stamp: src/pch.hpp
	@$(SETUP_CUSTOM_SH)                                                                             \
	pcomp "$(CXX) $(RELFLAGS) $$PCH_FLAGS -o src/pch.hpp.$$PCH_EXT" "src/pch.hpp";                  \
	touch src/pch-rel.stamp
src/pch-dbg.stamp: src/pch.hpp
	@$(SETUP_CUSTOM_SH)                                                                             \
	pcdbg "$(CXX) $(DBGFLAGS) $$PCH_FLAGS -o src/pch.hpp.$$PCH_EXT" "src/pch.hpp";                  \
	touch src/pch-dbg.stamp
$(OBJS): src/pch-rel.stamp
$(DOBJS): src/pch-dbg.stamp

# Like .PHONY but portable
FORCE: 

install: FORCE
	@$(SETUP_CUSTOM_SH)                                                                             \
	[ -d $(DESTDIR)$(PREFIX)/bin ] || peval 'mkdir -p $(DESTDIR)$(PREFIX)/bin';                     \
	peval 'cp -f bin/zrc $(SHELLPATH)';                                                             \
	peval 'chmod 755 $(SHELLPATH)';                                                                 \
	[ -d $(SYSCONFDIR) ] || peval 'mkdir -p $(SYSCONFDIR)';                                         \
	grep 2>/dev/null -qxF $(SHELLPATH) $(SYSCONFDIR)/shells                                         \
	    || peval 'echo $(SHELLPATH) >> $(SYSCONFDIR)/shells'

uninstall: FORCE
	@$(SETUP_CUSTOM_SH)                                                                             \
	! [ -f $(SHELLPATH) ] || peval 'rm -f $(SHELLPATH)';                                            \
	SHELLPATH_ESC=$$(printf "%s" $(SHELLPATH) | sed 's|/|\\/|g');                                   \
	peval "{ echo 'g/$$SHELLPATH_ESC/d'; echo 'w'; echo 'q'; } | ed -s $(SYSCONFDIR)/shells"

clean: FORCE
	rm -f bin/zrc* src/*.o src/*.do img/icon/*.o src/*.pch src/*.gch src/*.stamp

help: FORCE
	@echo release - Build release binary
	@echo debug - Build debugging binary
	@echo all - Build both
	@echo install - Copy release binary to $(SHELLPATH)
	@echo uninstall - Remove said binary
	@echo clean - Clean bin/ folder

# Track headers manually
src/command.o src/command.do:       src/pch.hpp src/custom_cmd.hpp src/command.hpp src/list.hpp     \
                                    src/vars.hpp src/zlineedit.hpp src/sig.hpp src/path.hpp

src/custom_cmd.o src/custom_cmd.do: src/pch.hpp src/custom_cmd.hpp src/global.hpp                   \
                                    src/vars.hpp src/syn.hpp

src/dispatch.o src/dispatch.do:     src/pch.hpp src/custom_cmd.hpp src/command.hpp                  \
                                    src/sig.hpp src/vars.hpp src/expr.hpp src/path.hpp src/list.hpp \
                                    src/config.hpp src/syn.hpp src/zlineedit.hpp

src/expr.o src/expr.do:             src/pch.hpp src/global.hpp src/syn.hpp

src/list.o src/list.do:             src/pch.hpp src/list.hpp src/syn.hpp

src/main.o src/main.do:             src/pch.hpp src/command.hpp src/config.hpp src/custom_cmd.hpp   \
                                    src/expr.hpp src/global.hpp src/sig.hpp src/syn.hpp src/vars.hpp\
                                    src/zlineedit.hpp

src/path.o src/path.do:             src/pch.hpp src/path.hpp src/vars.hpp

src/sig.o src/sig.do:               src/pch.hpp src/global.hpp src/command.hpp src/custom_cmd.hpp   \
                                    src/sig.hpp src/vars.hpp

src/syn.o src/syn.do:               src/pch.hpp src/syn.hpp src/list.hpp src/global.hpp             \
                                    src/config.hpp src/vars.hpp src/command.hpp

src/vars.o src/vars.do:             src/pch.hpp src/global.hpp src/syn.hpp src/vars.hpp

src/zlineedit.o src/zlineedit.do:   src/pch.hpp src/custom_cmd.hpp src/zlineedit.hpp src/config.hpp \
                                    src/global.hpp src/vars.hpp src/syn.hpp src/sig.hpp src/path.hpp
