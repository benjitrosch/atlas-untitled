build-c:
	clang main.c -o pack -std=c99

build-cpp:
	clang main.cpp -o pack -std=c++17 -lstdc++

demo:
	./pack --demo -s 960 -b 4 -v
