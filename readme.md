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

## Example
In the following example we show the regex parser in action, detecting a json real number, at compile time
```
#include <parserGenerator.hpp>

int main()
{
  /// Integer number in the json format
  static constexpr char jsonInt[]="(\\+|\\-)?[0-9]+";
  
  /// Real number in the json format
  static constexpr char jsonReal[]="(\\+|\\-)?[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?";
  
  /// Estimate the constexpr parser size
  constexpr auto parserSize=estimateRegexParserSize(jsonInt,jsonReal);
  
  /// Creates the parser
  constexpr auto parser=createParserFromRegex<parserSize>(jsonInt,jsonReal);
  
  static_assert(*parser.parse("-332.235e-34")==1);
  
  return 0;
}
```
