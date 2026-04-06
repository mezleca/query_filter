build:
	mkdir -p build && clang++ -o ./build/app main.cpp -std=c++17
run:
	./build/app
