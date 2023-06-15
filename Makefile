all:
	mkdir -p bin
	g++ src/main.cpp -o bin/zrc
sloc:
	sloccount . | tee sloccount.txt
install:
	ln -sf $$(pwd) '/usr/lib/zrc'
corebuf:
	cd corebuf; \
		./build.zrc
clean:
	rm bin/zrc

.PHONY: corebuf
