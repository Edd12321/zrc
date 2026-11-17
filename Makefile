#  vim: set tabstop=4 shiftwidth=4 noexpandtab nowrap: -*- tab-width: 4; indent-tabs-mode: t; truncate-lines: t; -*-
.POSIX:

# Do not override these
SRCS            = src/command.cpp src/custom_cmd.cpp src/dispatch.cpp  src/expr.cpp                     \
                  src/list.cpp    src/main.cpp       src/path.cpp      src/sig.cpp                      \
                  src/syn.cpp     src/vars.cpp       src/zlineedit.cpp
OBJS            = $(SRCS:.cpp=.o)
DOBJS           = $(SRCS:.cpp=.do)
CXXFLAGS        = -std=c++11 -pedantic -Wno-unused-result -Wno-unused-value -Winvalid-pch -flto         \
                  -Iinclude -Wno-c2y-extensions
SETUP_CUSTOM_SH =                                                                                       \
    set -e;                                                                                             \
    peval() { echo "$$1";           eval "$$1"; };                                                      \
    pcomp() { echo "  CXX     $$2"; eval "$$1 $$2"; };                                                  \
    pcdbg() { echo "  CXX (D) $$2"; eval "$$1 $$2"; };                                                  \
    case "$$($(CXX) --version | head -n 1)" in                                                          \
        *clang*)                                                                                        \
            PCH_EXT="pch"; PCH_FLAGS="-x c++-header -Xclang -emit-pch";                                 \
            PCH_CXX="-include-pch ./include/pch.hpp.pch";                                               \
            ;;                                                                                          \
        *)                                                                                              \
            PCH_EXT="gch"; PCH_FLAGS="-x c++-header";                                                   \
            PCH_CXX=""                                                                                  \
            ;;                                                                                          \
    esac;
# Override these 
CXX             = c++
RELFLAGS        = $(CXXFLAGS) -O3 -funroll-loops
DBGFLAGS        = $(CXXFLAGS) -O0 -Wextra -g -fsanitize=address,undefined                               \
                              -fno-strict-aliasing -fwrapv -fno-omit-frame-pointer
PREFIX          = /usr
SYSCONFDIR      = $(DESTDIR)/etc
SHELLPATH       = $(DESTDIR)$(PREFIX)/bin/zrc
STRIP           = n

.SUFFIXES: .cpp .o .do
.cpp.o:
	@$(SETUP_CUSTOM_SH)                                                                                 \
	pcomp '$(CXX) $(RELFLAGS) $$PCH_CXX -c -o $@' '$<'
.cpp.do:
	@$(SETUP_CUSTOM_SH)                                                                                 \
	pcdbg '$(CXX) $(DBGFLAGS) $$PCH_CXX -c -o $@' '$<'

release: bin/zrc
bin/zrc: $(OBJS)
	@$(SETUP_CUSTOM_SH)                                                                                 \
	[ -d bin ] || peval 'mkdir -p bin';                                                                 \
	case "$$(uname -s)" in                                                                              \
	    CYGWIN*)                                                                                        \
	        peval 'cd img/icon && windres winico.rc winico.o && cd ../..';                              \
	        pcomp '$(CXX) $(RELFLAGS) -o bin/zrc.exe' '$(OBJS) img/icon/winico.o';                      \
	        ! [ "$(STRIP)" = y ] || peval 'strip bin/zrc.exe';                                          \
	        ;;                                                                                          \
	    *)                                                                                              \
	        pcomp '$(CXX) $(RELFLAGS) -o bin/zrc' '$(OBJS)';                                            \
	        ! [ "$(STRIP)" = y ] || peval 'strip bin/zrc';                                              \
	        ;;                                                                                          \
	esac

debug: bin/zrc-debug
bin/zrc-debug: $(DOBJS)
	@$(SETUP_CUSTOM_SH)                                                                                 \
	[ -d bin ] || peval 'mkdir -p bin';                                                                 \
	pcdbg '$(CXX) $(DBGFLAGS) -o bin/zrc-debug' '$(DOBJS)'

all: release debug

include/pch-rel.stamp: include/pch.hpp
	@$(SETUP_CUSTOM_SH)                                                                                 \
	pcomp "$(CXX) $(RELFLAGS) $$PCH_FLAGS -o include/pch.hpp.$$PCH_EXT" "include/pch.hpp";              \
	touch include/pch-rel.stamp
include/pch-dbg.stamp: include/pch.hpp
	@$(SETUP_CUSTOM_SH)                                                                                 \
	pcdbg "$(CXX) $(DBGFLAGS) $$PCH_FLAGS -o include/pch.hpp.$$PCH_EXT" "include/pch.hpp";              \
	touch include/pch-dbg.stamp
$(OBJS):  include/pch-rel.stamp
$(DOBJS): include/pch-dbg.stamp

# Like .PHONY but portable
FORCE: 

install:   FORCE
	@$(SETUP_CUSTOM_SH)                                                                                 \
	[ -d $(DESTDIR)$(PREFIX)/bin ] || peval 'mkdir -p $(DESTDIR)$(PREFIX)/bin';                         \
	peval 'cp -f bin/zrc $(SHELLPATH)';                                                                 \
	peval 'chmod 755 $(SHELLPATH)';                                                                     \
	[ -d $(SYSCONFDIR) ] || peval 'mkdir -p $(SYSCONFDIR)';                                             \
	grep 2>/dev/null -qxF $(SHELLPATH) $(SYSCONFDIR)/shells                                             \
	    || peval 'echo $(SHELLPATH) >> $(SYSCONFDIR)/shells'

uninstall: FORCE
	@$(SETUP_CUSTOM_SH)                                                                                 \
	! [ -f $(SHELLPATH) ] || peval 'rm -f $(SHELLPATH)';                                                \
	SHELLPATH_ESC=$$(printf "%s" $(SHELLPATH) | sed 's|/|\\/|g');                                       \
	peval "{ echo 'g/$$SHELLPATH_ESC/d'; echo 'w'; echo 'q'; } | ed -s $(SYSCONFDIR)/shells"

clean:     FORCE
	rm -f bin/zrc* src/*.o src/*.do img/icon/*.o include/*.pch include/*.gch include/*.stamp

help:      FORCE
	@echo "release   - Build release binary"
	@echo "debug     - Build debug binary"
	@echo "all       - Build release and debug binaries"
	@echo "install   - Copy release binary to $(SHELLPATH) and register shell in /etc/shells"
	@echo "uninstall - Remove release binary from $(SHELLPATH) and unregister shell from /etc/shells"
	@echo "clean     - Clean bin/ folder"

# Track headers manually
src/command.o src/command.do:       include/pch.hpp        include/custom_cmd.hpp include/command.hpp   \
                                    include/list.hpp       include/vars.hpp       include/zlineedit.hpp \
                                    include/sig.hpp        include/path.hpp

src/custom_cmd.o src/custom_cmd.do: include/pch.hpp        include/custom_cmd.hpp include/global.hpp    \
                                    include/syn.hpp        include/vars.hpp

src/dispatch.o src/dispatch.do:     include/pch.hpp        include/custom_cmd.hpp include/command.hpp   \
                                    include/expr.hpp       include/list.hpp       include/path.hpp      \
                                    include/sig.hpp        include/syn.hpp        include/vars.hpp      \
                                    include/zlineedit.hpp  include/config.hpp

src/expr.o src/expr.do:             include/pch.hpp        include/expr.hpp       include/global.hpp    \
                                    include/syn.hpp

src/list.o src/list.do:             include/pch.hpp        include/list.hpp       include/syn.hpp

src/main.o src/main.do:             include/pch.hpp        include/command.hpp    include/config.hpp    \
                                    include/custom_cmd.hpp include/expr.hpp       include/global.hpp    \
                                    include/sig.hpp        include/syn.hpp        include/vars.hpp      \
                                    include/path.hpp       include/zlineedit.hpp

src/path.o src/path.do:             include/pch.hpp        include/path.hpp       include/vars.hpp

src/sig.o src/sig.do:               include/pch.hpp        include/global.hpp     include/command.hpp   \
                                    include/custom_cmd.hpp include/sig.hpp        include/vars.hpp

src/syn.o src/syn.do:               include/pch.hpp        include/syn.hpp        include/list.hpp      \
                                    include/global.hpp     include/vars.hpp       include/command.hpp   \
                                    include/config.hpp

src/vars.o src/vars.do:             include/pch.hpp        include/global.hpp     include/syn.hpp       \
                                    include/vars.hpp

src/zlineedit.o src/zlineedit.do:   include/pch.hpp        include/custom_cmd.hpp include/zlineedit.hpp \
                                    include/config.hpp     include/global.hpp     include/vars.hpp      \
                                    include/syn.hpp        include/sig.hpp        include/path.hpp
