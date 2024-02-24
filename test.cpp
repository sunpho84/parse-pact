#include <parserGenerator.hpp>

constexpr void test()
{
  // constexpr auto specs=createParserFromRegex("c|d(f?|g)","anna","[ciao]",".*");
  // constexpr auto parser=createParserFromRegex<specs>("c|d(f?|g)","anna","[ciao]",".*");
  constexpr auto specs=estimateRegexParserSize("(\\+|\\-)?[0-9]+","(\\+|\\-)?[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?","[^h]");
  constexpr auto parser=createParserFromRegex<specs>("(\\+|\\-)?[0-9]+","(\\+|\\-)?[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?","[^h]");
  // if(constexpr auto u=parser.parse("-332.235e-34"))
  //   printf("tok %zu\n",*u);
  if(constexpr auto u=parser.parse("i"))
    printf("tok %zu\n",*u);
  
  // static_assert(parser.parse("3")==0);
}

int main(int narg,char** arg)
{
  // Matching m("/* *ciao  d* \n mondo */    // come va \n qui { %left bene");
  
  constexpr char rrr[]="error_handling_calculator {\
    %whitespace \"[ \\t\\r\\n]*\";\
    %none error;\
    %left '(' ')';\
    %left '+' '-';\
    %left '*' '/';\
    %none integer;\
    stmts: stmts stmt | stmt | %precedence '(';\
    stmt: \
        expr ';' [result] | \
        error ';' [unexpected_error]\
    ;\
    expr:\
        expr '+' expr [add] |\
        expr '-' expr [subtract] |\
        expr '*' expr [multiply] |\
        expr '/' expr [divide] |\
        expr error expr [unknown_operator_error] |\
        '(' expr ')' [compound] |\
        integer [integer]\
    ;\
    integer: \"[0-9]+\";\
}";
    
  Grammar rrrr(rrr);  
   
  //Grammar gr("ciao { %left r s t; %right y e \"f+t?\"; %whitespace " "; gino : ballerino |danzatrice| odalisca %precedence spon [act];}");
  
  // Matching m("'ciao \\' bellissimo'");

  
  // if(const auto s=m.matchLiteralOrRegex('\''))
  //   diagnostic(*s,"\n");
  
  // test();
  
  // Matching m("[a-gi-me-j]");
  // auto n=matchBracketExpr(m);
  // if(n)
  //   n->printf();
  
  return 0;
}
