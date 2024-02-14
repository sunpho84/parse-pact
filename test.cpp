#include <parserGenerator.hpp>

constexpr void test()
{
  constexpr auto specs=createParserFromRegex("c|d(f?|g)","anna",".*");
  constexpr auto parser=createParserFromRegex<specs>("c|d(f?|g)","anna",".*");
  
  static_assert(parser.parse("ann")==2);
}

int main(int narg,char** arg)
{
  test();
  
  return 0;
}
