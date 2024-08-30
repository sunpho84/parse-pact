#include <parserPact.hpp>

using namespace pp;

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
  
  static_assert(parser.parse("-332.235e-34")->iToken==JSON_REAL_NUMER);
  static_assert(parser.parse("33")->iToken==JSON_NUMBER);
  static_assert(parser.parse("ello world!")->iToken==TEXT_NOT_CONTAINING_H);
  
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
   %whitespace \"[ \\t\\r\\n]+\";\
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
  constexpr auto c2=createConstexprGrammar([]() constexpr{return xmlGrammar;});
  
  constexpr size_t c2Size=sizeof(c2.regexParser);
  static_assert(c2Size==1,"");
  
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


  constexpr char xmlExample[]=
#include "xmlExample.xml"
	      ;
  
  std::string_view x=xmlExample;
  bool m;
  size_t i=0;
  std::vector<size_t> states{0};
  std::vector<size_t> symbols{0};
  size_t cursor=1;
  
  diagnostic("nStates: ",states.size(),"\n");
  
  do
    {
      const size_t iState=states.back();
      
      diagnostic("/////////////////////////////////////////////////////////////////\n");
      
      diagnostic("At state: ",iState,"\n");
      diagnostic(c.describe(c.stateItems[iState]));
      for(const GrammarTransition& t : c.stateTransitions[iState])
	diagnostic(c.describe(t));
      
      size_t nextToken=0;
      
      if(cursor<symbols.size())
	{
	  nextToken=symbols[cursor];
	  diagnostic("No need to parse, nextToken from cursor: ",nextToken,"\n");
	}
      else
	{
	  diagnostic("Parsed ",i," tokens, going to parse: ",x,"\n");
	  
	  auto r=c.regexParser.parse(x);
	  m=r.has_value();
	  
	  if(r)
	    {
	      x={x.begin()+r->str.length(),x.end()};
	      nextToken=r->iToken;
	      if(nextToken!=c.iWhitespaceSymbol)
		symbols.emplace(symbols.begin()+cursor,nextToken);
	      
	      diagnostic("matched string: \"",r->str,"\" corresponding to token ",r->iToken," \"",c.symbols[r->iToken].name,"\"\n");
	      i++;
	    }
	  else
	    diagnostic("unable to parse \"",x,"\"\n");
	}
      
      diagnostic("mmmm: ",m,"\n");
      if(m and nextToken!=c.iWhitespaceSymbol)
	{
	  const std::vector<GrammarTransition>& transitions=c.stateTransitions[iState];
	  
	  size_t iTransition=0;
	  while(iTransition<transitions.size() and transitions[iTransition].iSymbol!=nextToken)
	    iTransition++;
	  
	  if(iTransition<transitions.size())
	    {
	      const GrammarTransition& t=transitions[iTransition];
	      
	      diagnostic("Going to use transition: ",c.describe(t),"\n");
	      if(t.type==GrammarTransition::Type::REDUCE)
		{
		  const GrammarProduction& production=c.productions[t.iStateOrProduction];
		  //states.pop_back();
		  symbols.erase(symbols.begin()+cursor-production.iRhsList.size(),symbols.begin()+cursor);
		  cursor-=production.iRhsList.size();
		  symbols.emplace(symbols.begin()+cursor,production.iLhs);
		}
	      else
		{
		  states.push_back(t.iStateOrProduction);
		  cursor++;
		}
	      
	      diagnostic("States:\n");
	      for(const size_t& iState : states)
		diagnostic("   ",iState,"\n");
	      
	      diagnostic("Symbols:\n");
	      for(size_t iiSymbol=0;iiSymbol<symbols.size();iiSymbol++)
		{
		  const size_t iSymbol=symbols[iiSymbol];
		  
		  diagnostic("   ",iSymbol," ",c.symbols[iSymbol].name,"\n");
		  if(cursor==iiSymbol)
		    diagnostic(".......\n");
		}
	    }
	  else
	    errorEmitter("Unable to find transition");
	}
    }
  while(m and x.length());
  
  // diagnostic("/////////////////////////////////////////////////////////////////\n");
  // auto par=createParserFromRegex("<",">","<\\?xml","\\?>","/>","</","=","[A-Za-z_:][A-Za-z0-9_:\\.-]*");
  // if(const auto u=par.parse("<ciao"))
  //   diagnostic("parsed: ",*u,"\n");
  // else
  //   diagnostic("unable to parse\n");
   // < -> 4
   // > -> 5
   // <\?xml -> 9
   // \?> -> 11
   // /> -> 13
   // </ -> 14
   // = -> 16
   // [A-Za-z_:][A-Za-z0-9_:\.-]* -> 17
   // [\"']:string: -> 18

  
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
