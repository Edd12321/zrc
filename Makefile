PREFIX = /usr/local
SRCS = $(wildcard src/*.cpp src/*.hpp)
CXXFLAGS = -std=gnu++11 -Wno-unused-result -O3

.PHONY: all
all: bin/zrc

bin/zrc: $(SRCS) src/y.tab.cpp src/lex.yy.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) src/lex.yy.cpp src/y.tab.cpp src/main.cpp -o bin/zrc

src/y.tab.cpp: src/expr.y
	bison -d src/expr.y -o src/y.tab.cpp

src/lex.yy.cpp: src/expr.l
	flex -o src/lex.yy.cpp src/expr.l

.PHONY: install
install:
	install -Dm755 bin/zrc $(DESTDIR)$(PREFIX)/bin/zrc

.PHONY: clean
clean:
	rm bin/*
