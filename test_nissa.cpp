#include <parsePact.hpp>

using namespace pp;

using namespace std;

int main()
{
  constexpr const char nissaGrammar[]=
    "nissa {"
    "document: document assignment"
    "        | assignment [assegna];"
    "assignment: var \"=\" int;"
    "var: \"[A-Z]+\" ;"
    "int: \"[0-9]+\" ;"
    "}";
  
   const auto nissa=createGrammar(nissaGrammar);
   //constexpr auto nissa=createGrammar<nissaGrammar>();
  
  // for(int iProduction=0;iProduction<nissa.nProductions();iProduction++)
  //   {
  //     cout<<"Production "<<iProduction<<endl;
  //     cout<<"---------------------"<<endl;
  //     cout<<nissa.production(iProduction).describe()<<endl;
  //     cout<<endl;
  //   }
  
  // for(int iState=0;iState<nissa.nStates();iState++)
  //   {
  //     cout<<"State "<<iState<<endl;
  //     cout<<"---------------------"<<endl;
  //     cout<<nissa.state(iState).describe()<<endl;
  //     cout<<endl;
  //   }

using namespace pp::internal;

  constexpr char nissaExample[]="ALA=8";
  
  string_view x=nissaExample;
  bool m;
  size_t i=0;
  vector<size_t> states{0};
  vector<size_t> symbols{0};
  size_t cursor=1;
  
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
	  diagnostic("No need to parse, nextToken from cursor: ",iNextSymbol,"\n");
	}
      else
	{
	  diagnostic("Parsed ",i," tokens, going to parse: ",x,"\n");
	  
	  auto r=c.regexMatcher.match(x);
	  m=r.has_value();
	  
	  if(r)
	    {
	      x={x.begin()+r->matchedString.length(),x.end()};
	      iNextSymbol=c.iSymbolOfRegex[r->iToken];
	      if(iNextSymbol!=c.iWhitespaceSymbol)
		symbols.emplace(symbols.begin()+cursor,iNextSymbol);
	      
	      diagnostic("matched string: \"",r->matchedString,"\" corresponding to symbol ",iNextSymbol," \"",c.symbols[iNextSymbol].name,"\"\n");
	      i++;
	    }
	  else
	    diagnostic("unable to parse \"",x,"\"\n");
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
	    errorEmitter("Unable to find grammar transition");
	}
    }
  while(m and x.length());
 bug: 9 non è riconosciuto
    bug:.end non è riconosciuto
  
  return 0;
}
