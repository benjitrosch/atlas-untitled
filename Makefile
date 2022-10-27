CC 		= clang
CFLAGS 	=
TARGET	= pack

build:
	if [[ "$(lang)" == "cpp" ]]; then \
		echo "\x1B[32mBuilding with c++17...\x1B[0m"; \
        $(CC) $(CFLAGS) -o $(TARGET) cpp/main.cpp -std=c++17 -lstdc++; \
	else \
		echo "\x1B[32mBuilding with c99...\x1B[0m"; \
        $(CC) $(CFLAGS) -o $(TARGET) c/main.c -std=c99; \
	fi

demo:
	./pack --demo -s 960 -b 4 -v
