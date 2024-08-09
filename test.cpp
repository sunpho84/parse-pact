#include <parserGenerator.hpp>

constexpr void test()
{
  //parseTreeFromRegex("(\\+|\\-)?[0-9]+","(\\+|\\-)?[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?","[^h]");
  //constexpr auto specs=createParserFromRegex("c|d(f?|g)","anna","[ciao]",".*").getSizes();
  //constexpr auto parser=createParserFromRegex<specs>("c|d(f?|g)","anna","[ciao]",".*");
  
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
  
  // if(constexpr auto u=parser.parse("-332.235e-34"))
  //   printf("tok %zu\n",*u);
  // // if(constexpr auto u=parser.parse("i"))
  // //   printf("tok %zu\n",*u);
  
  //  static_assert(parser.parse("3")==0);
}

int main(int narg,char** arg)
{
  test();
  // Matching m("/* *ciao  d* \n mondo */    // come va \n qui { %left bene");
  
  static constexpr char jsonGrammar[]=
  "json {\
     %whitespace \"[ \\t\\r\\n]*\";\
     document: '{' attributes '}' [document] | ;\
     attributes: attributes ',' attribute [add_to_object] | attribute [create_object] | ;\
     attribute: name ':' value [attribute];\
     elements: elements ',' value [add_to_array] | value [create_array] | ;\
     value:\
        null [null] |\
        boolean [value] |\
        integer [value] |\
        real [value] |\
        string [value] |\
        '{' attributes '}' [object] |\
        '[' elements ']' [array]\
     ;\
     name: \"[\\\"']:string:\";\
     null: 'null';\
     boolean: \"true|false\";\
     integer: \"(\\+|\\-)?[0-9]+\";\
     real: \"(\\+|\\-)?[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?\";\
     string: \"[\\\"']:string:\";\
  }";
  (void)jsonGrammar;
  
  static constexpr char xmlGrammar[]=
    "xml {\
   %whitespace \"[ \\t\\r\\n]*\";\
   %left '<' '>';\
   %left name;\
   document: prolog element [document];\
   prolog: \"<\\?xml\" attributes \"\\?>\" | ;\
   elements: elements element [add_element] | element [create_element] | %precedence '<';\
   element: '<' name attributes '/>' [short_element] | '<' name attributes '>' elements '</' name '>' [long_element];\
   attributes: attributes attribute [add_attribute] | attribute [create_attribute] | %precedence name;\
   attribute: name '=' value [attribute];\
   name: \"[A-Za-z_:][A-Za-z0-9_:\\.-]*\";\
   value: \"[\\\"']:string:\";\
}";

  //Grammar grammar(jsonGrammar);
  auto c=createGrammar(xmlGrammar);
  
  diagnostic("Productions (dynamic instantiation):\n");
  diagnostic("------------\n");
  for(const GrammarProduction& p : c.productions)
    diagnostic(c.describe(p),"\n");
  diagnostic("\n");
  
  constexpr auto specs=estimateGrammarSize(xmlGrammar);
  
  constexpr auto stackGrammar=createGrammar<specs>(xmlGrammar);
  
  diagnostic("Symbols:\n");
  diagnostic("--------\n");
  for(size_t i=0;const BaseGrammarSymbol& s : stackGrammar.symbols)
    diagnostic(i++,": ",s.name,"   ",s.typeTag()," symbol\n");
  diagnostic("\n");
  
  diagnostic("Productions:\n");
  diagnostic("------------\n");
  for(size_t i=0;i<stackGrammar.productionsData.size();i++)
    diagnostic(std::to_string(i)+") "+stackGrammar.production(i).describe(),"\n");
  diagnostic("\n");
  
  diagnostic("Items:\n");
  diagnostic("------\n");
  for(size_t iItem=0;iItem<stackGrammar.items.size();iItem++)
    diagnostic(std::to_string(iItem)+") "+stackGrammar.item(iItem).describe(),"\n");
  diagnostic("\n");
  
  diagnostic("States:\n");
  diagnostic("------\n");
  for(size_t iState=0;iState<stackGrammar.nStates();iState++)
    diagnostic("State ",std::to_string(iState)+")\n"+stackGrammar.state(iState).describe("   "),"\n");
  diagnostic("\n");
  
  diagnostic("Regex machine:\n");
  diagnostic("------\n");
  diagnostic("nDStates: ",specs.regexMachinePars.nDStates,"\n");
  diagnostic("nTransitions: ",specs.regexMachinePars.nTransitions,"\n");
  diagnostic("\n");

//   constexpr GrammarParser<jsonGrammar> jSonGrammarParser;

//   constexpr char jsonText[]=/* Some json text*/;
  
//   constexpr auto parsedData=jsonGrammarParser(jsonText);

//   static_assert(parsedData.myParameter==42,"Check at compile failed");
  
//   constexpr char rrr[]="error_handling_calculator {\
//     %whitespace \"[ \\t\\r\\n]*\";\
//     %none error;\
//     %left '(' ')';\
//     %left '+' '-';\
//     %left '*' '/';\
//     %none integer;\
//     stmts: stmts stmt | stmt | %precedence '(';\
//     stmt: \
//         expr ';' [result] | \
//         error ';' [unexpected_error]\
//     ;\
//     expr:\
//         expr '+' expr [add] |\
//         expr '-' expr [subtract] |\
//         expr '*' expr [multiply] |\
//         expr '/' expr [divide] |\
//         expr error expr [unknown_operator_error] |\
//         '(' expr ')' [compound] |\
//         integer [integer]\
//     ;\
//     integer: \"[0-9]+\";\
// }";
    
  // Grammar rrrr(rrr);  
   
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
