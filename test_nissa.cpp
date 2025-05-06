#include <parsePact.hpp>

using namespace pp;

using namespace std;

using namespace pp::internal;

int main()
{
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
	      "%whitespace \" *\";"
	      "document: document expression"
	      "        | expression;"
	      "expression: expression \"\\*\" expression [mul]"
	      "          | expression \"\\+\" expression [sum]"
	      "          | expression \"\\^\" expression [pow]"
	      "          | '\\(' expression '\\)' [bracket]"
	      "          | \"[0-9]+\" [int] ;"
	      "}";
  
   const auto nissa=createGrammar(calcGrammar);
   //constexpr auto nissa=createGrammar<nissaGrammar>();
  
  for(int iProduction=0;iProduction<nissa.productions.size();iProduction++)
    {
      cout<<"Production "<<iProduction<<endl;
      cout<<"---------------------"<<endl;
      cout<<nissa.productions[iProduction].describe(nissa.symbols)<<endl;
      cout<<endl;
    }
  
  for(int iState=0;iState<nissa.stateItems.size();iState++)
    {
      cout<<"State "<<iState<<endl;
      cout<<"---------------------"<<endl;
      cout<<nissa.stateItems[iState].describe(nissa.items,nissa.productions,nissa.symbols)<<endl;
      cout<<endl;
      
      for(const GrammarTransition& t : nissa.stateTransitions[iState])
	diagnostic(nissa.describe(t));
      cout<<endl;
    }
  
  //constexpr char nissaExample[]="ALAZ=9 BAMBO=1";
  constexpr char calcExample[]="9+3^2*(4+5)";
  
  string_view x=calcExample;
  bool endReached=false;
  bool m;
  size_t i=0;
  vector<size_t> states{0};
  vector<size_t> symbols{};
  size_t cursor=0;

  struct ParseTreeNode
  {
    std::string_view txt;
    
    std::vector<ParseTreeNode> subNodes;
  };
  
  std::vector<ParseTreeNode> parsedSymbols;
  
  diagnostic("nStates: ",states.size(),"\n");
  auto& c=nissa;
  do
    {
      const size_t iState=states.back();
      
      diagnostic("/////////////////////////////////////////////////////////////////\n");
      
      diagnostic("At state: ",iState,"\n");
      diagnostic(c.describe(c.stateItems[iState]));
      for(const GrammarTransition& t : c.stateTransitions[iState])
	diagnostic(c.describe(t));
      
      size_t iNextSymbol=0;
      
      if(cursor<symbols.size())
	{
	  iNextSymbol=symbols[cursor];
	  diagnostic("No need to parse, nextToken from cursor: ",iNextSymbol,"=\"",nissa.symbols[iNextSymbol].name,"\"\n");
	}
      else
	{
	  diagnostic("Parsed ",i," tokens, going to parse: \"",x,"\"\n");
	  
	  if(x.empty())
	    {
	      if(not endReached)
		{
		  diagnostic("Reached the end of the string\n");
		  endReached=true;
		  iNextSymbol=nissa.iEndSymbol;
		  symbols.emplace(symbols.begin()+cursor,iNextSymbol);
		  m=true;
		}
	      else
		diagnostic("End of the string already reached\n");
	    }
	  else
	    {
	      auto r=c.regexMatcher.match(x);
	      m=r.has_value();
	      
	      if(r)
		{
		  x={x.begin()+r->matchedString.length(),x.end()};
		  iNextSymbol=c.iSymbolOfRegex[r->iToken];
		  if(iNextSymbol!=c.iWhitespaceSymbol)
		    {
		      symbols.emplace(symbols.begin()+cursor,iNextSymbol);
		      parsedSymbols.push_back({r->matchedString});
		    }
		  
		  diagnostic("matched string: \"",r->matchedString,"\" corresponding to symbol ",iNextSymbol," \"",c.symbols[iNextSymbol].name,"\"\n");
		  i++;
		}
	      else
		diagnostic("unable to parse \"",x,"\"\n");
	    }
	}
      
      diagnostic("mmmm: ",m,"\n");
      if(m and iNextSymbol!=c.iWhitespaceSymbol)
	{
	  const vector<GrammarTransition>& transitions=c.stateTransitions[iState];
	  
	  size_t iTransition=0;
	  while(iTransition<transitions.size() and transitions[iTransition].iSymbol!=iNextSymbol)
	    {
	      diagnostic("skipping transition ",transitions[iTransition].describe(c.items,c.productions,c.symbols,c.stateItems)," as ",transitions[iTransition].iSymbol,"!=",iNextSymbol,"\n");
	      iTransition++;
	    }
	  
	  if(iTransition<transitions.size())
	    {
	      const GrammarTransition& t=transitions[iTransition];
	      const bool isReduce=t.type==GrammarTransition::Type::REDUCE;
	      
	      diagnostic("Going to use ",isReduce?"reduce ":"","transition: ",c.describe(t),"\n");
	      if(isReduce)
		{
		  const GrammarProduction& production=c.productions[t.iStateOrProduction];
		  //states.pop_back();
		  const size_t beg=cursor-production.iRhsList.size();
		  const size_t end=cursor;
		  symbols.erase(symbols.begin()+beg,symbols.begin()+end);
		  
		  ParseTreeNode res{production.action,{std::make_move_iterator(parsedSymbols.begin()+beg),std::make_move_iterator(parsedSymbols.begin()+end)}};
		  parsedSymbols.erase(parsedSymbols.begin()+beg,parsedSymbols.begin()+end);
		  parsedSymbols.insert(parsedSymbols.begin()+beg,res);
		  
		  states.erase(states.end()-production.iRhsList.size(),states.end());
		  
		  cursor-=production.iRhsList.size();
		  diagnostic("reduction ",production.action,"\n");
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
	    errorEmitter("Unable to find grammar transition");
	}
    }
  while(m and not(states.size()==1 and states[0]==0 and symbols.size()==2 and symbols[0]==nissa.iStartSymbol and symbols[1]==nissa.iEndSymbol and x.length()==0));

  auto it=[](const auto& self,
	     ParseTreeNode& p) ->void
  {
    diagnostic(p.txt);
    
    if(p.subNodes.size())
      {
	diagnostic("(");
	self(self,p.subNodes.front());
	for(size_t i=1;i<p.subNodes.size();i++)
	  {
	    diagnostic(",");
	    self(self,p.subNodes[i]);
	  }
	diagnostic(")");
      }
  };
  
  it(it,parsedSymbols.front());
  diagnostic("\n");
  diagnostic(calcExample,"\n");

 bug: lexer is not reporting error
  
  return 0;
}
