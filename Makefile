UNAME := $(shell uname -o)

all:
	mkdir -p bin
	g++ src/main.cpp -o bin/zrc
	strip bin/zrc
nozledit:
	mkdir -p bin
	g++ src/main.cpp -o bin/zrc -lstdc++fs -DUSE_ZLINEEDIT=0
	strip bin/zrc
nohash:
	mkdir -p bin
	g++ src/main.cpp -o bin/zrc -lstdc++fs -DUSE_HASHCACHE=0
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
clean:
	rm bin/zrc

.PHONY: corebuf
