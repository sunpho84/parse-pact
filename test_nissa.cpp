#include <parsePact.hpp>

#include <set>

using namespace pp;

using namespace std;

using namespace pp::internal;

/// Specifications of the grammar
struct ParseTreeSpecs
{
  // /// Number of symbols
  // const size_t nSymbols;
  
  // const Stack2DVectorPars productionPars;
  
  // const size_t nItems;
  
  // const Stack2DVectorPars stateItemsPars;
  
  // const Stack2DVectorPars stateTransitionsPars;
  
  // const RegexMatcherSizes regexMachinePars;
  
  /// Detects if the grammar is empty
  constexpr bool isNull() const
  {
    return true;
      // nSymbols==0 and
      // productionPars.isNull() and
      // 	nItems==0 and
      // stateItemsPars.isNull() and
      // stateTransitionsPars.isNull() and
      // regexMachinePars.isNull();
  }
};

template <typename G,
  ParseTreeSpecs Specs=ParseTreeSpecs{}>
  constexpr auto getParseTree(const G& grammar,
			      std::string_view input)
{
  bool endReached=false;
  bool m;
  size_t i=0;
  vector<size_t> states{0};
  vector<size_t> symbols{};
  size_t cursor=0;
  
  /// Holds a node in the parse tree of the expression
  struct ParseTreeNode
  {
    std::string_view txt;
    
    std::vector<ParseTreeNode> subNodes;
  };
  
  std::vector<ParseTreeNode> parsedSymbols;
  
  do
    {
      const size_t iState=states.back();
      const auto& state=grammar.state(iState);
      
      diagnostic("/////////////////////////////////////////////////////////////////\n");
      
      diagnostic("At state: ",iState,"\n");
      diagnostic(grammar.describeState(iState));
      
      for(size_t iTransition=0;iTransition<state.nTransitions();iTransition++)
	diagnostic(grammar.describeStateTransition(iState,iTransition));
      
      size_t iNextSymbol=0;
      
      if(cursor<symbols.size())
	{
	  iNextSymbol=symbols[cursor];
	  diagnostic("No need to parse, nextToken from cursor: ",iNextSymbol,"=\"",grammar.symbols[iNextSymbol].name,"\"\n");
	}
      else
	{
	  diagnostic("Parsed ",i," tokens, going to parse: \"",input,"\"\n");
	  
	  if(input.empty())
	    {
	      if(not endReached)
		{
		  diagnostic("Reached the end of the string\n");
		  endReached=true;
		  iNextSymbol=grammar.iEndSymbol;
		  symbols.emplace(symbols.begin()+cursor,iNextSymbol);
		  m=true;
		}
	      else
		diagnostic("End of the string already reached\n");
	    }
	  else
	    {
	      auto r=grammar.regexMatcher.match(input);
	      m=r.has_value();
	      
	      if(r)
		{
		  input={input.begin()+r->matchedString.length(),input.end()};
		  iNextSymbol=grammar.iSymbolOfRegex[r->iToken];
		  if(iNextSymbol!=grammar.iWhitespaceSymbol)
		    {
		      symbols.emplace(symbols.begin()+cursor,iNextSymbol);
		      parsedSymbols.push_back({r->matchedString});
		    }
		  
		  diagnostic("matched string: \"",r->matchedString,"\" corresponding to symbol ",iNextSymbol," \"",grammar.symbols[iNextSymbol].name,"\"\n");
		  i++;
		}
	      else
		errorEmitter("unable to match \"",input,"\"");
	    }
	}
      
      diagnostic("mmmm: ",m,"\n");
      if(m and iNextSymbol!=grammar.iWhitespaceSymbol)
	{
	  size_t iTransition=0;
	  while(iTransition<grammar.nTransitions() and state.transition(iTransition).iSymbol!=iNextSymbol)
	    {
	      diagnostic("skipping transition ",grammar.describeStateTransition(iState,iTransition)," as ",state.transition(iTransition).iSymbol,"!=",iNextSymbol,"\n");
	      iTransition++;
	    }
	  
	  if(iTransition<grammar.nTransitions())
	    {
	      const GrammarTransition& t=state.transition(iTransition);
	      const bool isReduce=t.type==GrammarTransition::Type::REDUCE;
	      
	      diagnostic("Going to use ",isReduce?"reduce ":"","transition: ",grammar.describeStateTransition(iState,iTransition),"\n");
	      if(isReduce)
		{
		  const auto& production=grammar.production(t.iStateOrProduction);
		  //states.pop_back();
		  const size_t beg=cursor-production.nRhs();
		  const size_t end=cursor;
		  symbols.erase(symbols.begin()+beg,symbols.begin()+end);

		  const std::string_view& action=grammar.action(t.iStateOrProduction);
		  
		  ParseTreeNode res{action,{std::make_move_iterator(parsedSymbols.begin()+beg),std::make_move_iterator(parsedSymbols.begin()+end)}};
		  parsedSymbols.erase(parsedSymbols.begin()+beg,parsedSymbols.begin()+end);
		  parsedSymbols.insert(parsedSymbols.begin()+beg,res);
		  
		  states.erase(states.end()-production.nRhs(),states.end());
		  
		  cursor-=production.nRhs();
		  diagnostic("reduction ",action,"\n");
		  symbols.emplace(symbols.begin()+cursor,production.iLhs());
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
		  
		  diagnostic("   ",iSymbol," ",grammar.symbols[iSymbol].name,"\n");
		  if(cursor==iiSymbol)
		    diagnostic(".......\n");
		}
	    }
	  else
	    errorEmitter("Unable to find grammar transition");
	}
    }
  while(m and not(states.size()==1 and states[0]==0 and symbols.size()==2 and symbols[0]==grammar.iStartSymbol and symbols[1]==grammar.iEndSymbol and input.length()==0));

  int t=0;
  auto count=
    [&t](auto self,
	 const ParseTreeNode& n)->void
  {
    t++;
    for(const auto& s : n.subNodes)
      self(self,s);
  };

  count(count,parsedSymbols.front());
  
  return t;
}

int main()
{
  [[maybe_unused]]
  constexpr const char nissaGrammar[]=
    "nissa {"
    "%whitespace \" *\";"
    "document: document assignment"
    "        | assignment [assegna];"
    "assignment: var \"=\" int;"
    "var: \"[A-Z]+\" ;"
    "int: \"[0-9]+\" ;"
    "}";
  
  constexpr const char calcGrammar[]=
	      "nissa {"
	      "%left \"\\+\";"
	      "%left \"\\*\";"
	      "%left \"\\^\";"
	      "%whitespace \" +\";"
	      "document: document expression"
	      "        | expression;"
	      "expression: expression \"\\*\" expression [mul]"
	      "          | expression \"\\+\" expression [sum]"
	      "          | expression \"\\^\" expression [pow]"
	      "          | '\\(' expression '\\)' [bracket]"
	      "          | \"[0-9]+\" [int] ;"
	      "}";
  
  constexpr auto nissa=createGrammar<calcGrammar>();
   //constexpr auto nissa=createGrammar<nissaGrammar>();
  
  // for(int iProduction=0;iProduction<nissa.productions.size();iProduction++)
  //   {
  //     cout<<"Production "<<iProduction<<endl;
  //     cout<<"---------------------"<<endl;
  //     cout<<nissa.productions[iProduction].describe(nissa.symbols)<<endl;
  //     cout<<endl;
  //   }
  
  // for(int iState=0;iState<nissa.stateItems.size();iState++)
  //   {
  //     cout<<"State "<<iState<<endl;
  //     cout<<"---------------------"<<endl;
  //     cout<<nissa.stateItems[iState].describe(nissa.items,nissa.productions,nissa.symbols)<<endl;
  //     cout<<endl;
      
  //     for(const GrammarTransition& t : nissa.stateTransitions[iState])
  // 	diagnostic(nissa.describe(t));
  //     cout<<endl;
  //   }
  
  // //constexpr char nissaExample[]="ALAZ=9 BAMBO=1";
  constexpr char calcExample[]="9+3^2*(4+5)";
  
  constexpr size_t n=getParseTree(nissa,calcExample);
  cout<<n<<endl;
  
  // std::map<const ParseTreeNode*,std::string> ms;
  // int iddd=0;
  // auto getName=
  //   [&ms,&iddd](const ParseTreeNode& p) ->std::string
  // {
  //   std::string& tmp=ms[&p];
  //   if(tmp=="")
  //     tmp=(std::string)p.txt+"_"+std::to_string(iddd++);
    
  //   return tmp;
  // };
  
  // auto it=[&](const auto& self,
  // 	      const ParseTreeNode& p) ->void
  // {
  //   diagnostic("\"",getName(p),"\" [label=\"",p.txt,"\"]\n");
    
  //   for(const auto& s : p.subNodes)
  //     {
  // 	diagnostic("\"",getName(p),"\" -> \"",getName(s),"\"\n");
	
  // 	self(self,s);
  //     }
  // };
  
  // it(it,parsedSymbols.front());
  // diagnostic("\n");
  // diagnostic(calcExample,"\n");
  
  return 0;
}
