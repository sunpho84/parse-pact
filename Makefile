test_nissa: test_nissa.cpp Makefile parsePact.hpp
#	g++ -o test test.cpp --std=c++20 -Wall -ggdb3
	clang++ -o test_nissa test_nissa.cpp --std=c++20 -Wall -ggdb3 -fconstexpr-steps=10000000

test: test.cpp Makefile parsePact.hpp
#	g++ -o test test.cpp --std=c++20 -Wall -ggdb3
	clang++ -o test test.cpp --std=c++20 -Wall -ggdb3 -fconstexpr-steps=10000000
