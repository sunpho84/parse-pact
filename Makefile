all : testConv astBuilder

testConv: testConv.cpp Makefile parsePact.hpp
#	g++ -o test test.cpp --std=c++20 -Wall -ggdb3
	clang++ -o testConv testConv.cpp --std=c++20 -Wall -O3 -fconstexpr-steps=100000000

astBuilder: astBuilder.cpp Makefile parsePact.hpp
#	g++ -o test test.cpp --std=c++20 -Wall -ggdb3
	clang++ -o astBuilder astBuilder.cpp --std=c++20 -Wall -ggdb3 -fconstexpr-steps=100000000

test: test.cpp Makefile parsePact.hpp
#	g++ -o test test.cpp --std=c++20 -Wall -ggdb3
	clang++ -o test test.cpp --std=c++20 -Wall -ggdb3 -fconstexpr-steps=100000000
