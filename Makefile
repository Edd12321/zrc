all:
	mkdir -p bin
	g++ src/main.cpp -o bin/zrc
corebuf:
	cd corebuf; \
		touch bin/true; \
		chmod 777 bin/true; \
		./build.zrc
clean:
	rm bin/zrc

.PHONY: corebuf
