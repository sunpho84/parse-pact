# Provide Autonomous Regex Scanners & Entire Parsers At Compile Time

Single header `C++-20` library to:
- generate `lexer` (as for the "REGular EXpression Scanner" part of the name) 
- create full `parser` given a `BNF` grammar
lexer and parser can be generated (and even used!) even at compile time!

## Key features
- __Single header__: immediate to streamline in your project:

```c++
#include <parsePact.hpp>

const auto parser=pp::generateParser(" ... grammar...");
parser.parse("...text to be parsed");
```
- Plain standard `C++-20`, no extra dependency
- Supports `lalr(1)` grammar
- Can parse expressions at compile time! Some funny example will come here.

**Status**: the grammar is completely created, parsing is still in
progress, no action is issued. Lexer do not track position yet.

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
