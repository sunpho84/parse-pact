# Provide Autonomous Regex Scanners & Entire Parsers At Compile Time

## Aim of the project 
Providing a parser generator which produce a `lalr(1)` parser at
compile time. This will allow to parse expressions at compile time.

## Style 
The project is written in plain `C++-20`, has no dependency, and the
aim is to put everything in a single file. 

**Status**: the grammar is completely created, parsing is still in
work, no action is issued. Lexer do not track position yet.

**Based on:** The project is based on [lalr parser
generator](https://github.com/cwbaker/lalr/) by Charles Baker.

See the [development notes](doc/develop.md)

## Example - outdated
In the following example we show the regex parser in action, detecting a json real number, at compile time
```c++
#include <parserGenerator.hpp>

int main()
{
  /// Recognized token
  enum {JSON_NUMBER,JSON_REAL_NUMER,TEXT_NOT_CONTAINING_H};
  
  /// Pattern to recognize a number
  constexpr char jsonNumberPattern[]="(\\+|\\-)?[0-9]+";
  
  /// Pattern to recognize a real number
  constexpr char jsonRealNumberPattern[]="(\\+|\\-)?[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?";
  
  /// Pattern to recognize a string not containing char 'h'
  constexpr char testNotContainingHPattern[]="[^h]+";
  
  /////////////////////////////////////////////////////////////////
  
  /// Storage properties of the regex pattern matcher
  constexpr auto specs=estimateRegexParserSize(jsonNumberPattern,jsonRealNumberPattern,testNotContainingHPattern);
  
  /// Regex parser matcher
  constexpr auto parser=createParserFromRegex<specs>(jsonNumberPattern,jsonRealNumberPattern,testNotContainingHPattern);
  
  static_assert(parser.parse("-332.235e-34")==JSON_REAL_NUMER);
  static_assert(parser.parse("33")==JSON_NUMBER);
  static_assert(parser.parse("ello world!")==TEXT_NOT_CONTAINING_H);
  
  return 0;
}
```
