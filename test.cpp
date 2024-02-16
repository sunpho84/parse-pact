#include <parserGenerator.hpp>

constexpr void test()
{
  // constexpr auto specs=createParserFromRegex("c|d(f?|g)","anna","[ciao]",".*");
  // constexpr auto parser=createParserFromRegex<specs>("c|d(f?|g)","anna","[ciao]",".*");
  constexpr auto specs=createParserFromRegex("(\\+|\\-)?[0-9]+","(\\+|\\-)?[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?","[^h]");
  constexpr auto parser=createParserFromRegex<specs>("(\\+|\\-)?[0-9]+","(\\+|\\-)?[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?","[^h]");
  // if(constexpr auto u=parser.parse("-332.235e-34"))
  //   printf("tok %zu\n",*u);
  if(constexpr auto u=parser.parse("i"))
    printf("tok %zu\n",*u);
  
  // static_assert(parser.parse("3")==0);
}

int main(int narg,char** arg)
{
   test();
  
  // Matching m("[a-gi-me-j]");
  // auto n=matchBracketExpr(m);
  // if(n)
  //   n->printf();
  
  return 0;
}
