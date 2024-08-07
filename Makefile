test: test.cpp Makefile parserGenerator.hpp
#	g++ -o test test.cpp --std=c++20 -Wall -ggdb3
	clang++ -o test test.cpp --std=c++20 -Wall -ggdb3 -fconstexpr-steps=10000000
