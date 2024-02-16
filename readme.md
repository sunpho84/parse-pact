# A compile-time parser generator

## Aim of the project 
Providing a parser generator which produce a `lalr(1)` parser at
compile time. This will allow to parse expressions at compile time.

## Style 
The project is written in plain `C++-20`, has no dependency, and the
aim is to put everything in a single file. 

**Status**: regex parser is working so far.

**Based on:** The project is based on [lalr parser
generator](https://github.com/cwbaker/lalr/) by Charles Baker.

See the [development notes](doc/develop.md)
