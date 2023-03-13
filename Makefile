all:
	mkdir -p bin
	g++ src/main.cpp -o bin/zrc
clean:
	rm bin/zrc
