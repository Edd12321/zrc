all:
	mkdir -p bin
	g++ src/main.cpp -o bin/zrc
install:
	ln -sf $$(pwd) '/usr/lib/zrc'
corebuf:
	cd corebuf; \
		touch bin/true; \
		chmod 777 bin/true; \
		./build.zrc
clean:
	rm bin/zrc

.PHONY: corebuf
