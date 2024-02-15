#include <parserGenerator.hpp>

constexpr void test()
{
  constexpr auto specs=createParserFromRegex("c|d(f?|g)","anna","[ciao]",".*");
  constexpr auto parser=createParserFromRegex<specs>("c|d(f?|g)","anna","[ciao]",".*");
  
  static_assert(parser.parse("ann")==2);
}

int main(int narg,char** arg)
{
  test();
  
  // Matching m("[0-9]");//[abc[:alnum:]]");
  // auto n=matchBracketExpr(m);
  // if(n)
  //   n->printf();
  
  return 0;
}
