#include <array>
#include <cctype>
#include <cstdio>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

void errorEmitter(const char*)
{
}

/// Keep track of what matched
struct Matching
{
  /// Reference to the string view holding the data
  std::string_view ref;
  
  /// Construct from a string view
  constexpr Matching(const std::string_view& in) :
    ref(in)
  {
  }
  
  /// Match any char
  constexpr char matchAnyChar()
  {
    if(not ref.empty())
      {
	const char c=
	  ref.front();
	
	ref.remove_prefix(1);
	
	return c;
      }
    else
      return '\0';
  }
  
  /// Match a single char
  constexpr bool matchChar(const char& c)
  {
    const bool accepting=
      (not ref.empty()) and ref.starts_with(c);
    
    if(accepting)
      ref.remove_prefix(1);
	
    return accepting;
  }
  
  /// Match a char if not in the filt list
  constexpr char matchCharNotIn(const std::string_view& filt)
  {
    if(not ref.empty())
      if(const char c=ref.front();filt.find_first_of(c)==filt.npos)
	{
	  ref.remove_prefix(1);
	  
	  return c;
	}
    
    return '\0';
  }
  
  /// Match a char if it is in the list
  constexpr char matchAnyCharIn(const std::string_view& filt)
  {
    if(not ref.empty())
      {
	const size_t pos=
	  filt.find_first_of(ref.front());
	
	if(pos!=filt.npos)
	  {
	    const char c=
	      filt[pos];
	    
	    ref.remove_prefix(1);
	    
	    return c;
	  }
      }
    
    return '\0';
  }
  
  /// Forbids taking a copy
  Matching(const Matching&)=delete;
  
  /// Forbids default construct
  Matching()=delete;
};

/// Holds a node in the parse tree for regex
struct RegexParserNode
{
  /// Possible ypes of the node
  enum Type{OR,AND,OPT,MANY,NONZERO,CHAR,TOKEN};
  
  /// Specifications of the node type
  struct TypeSpecs
  {
    /// Tag associated to the type
    const char* const tag;
    
    /// Number of subnodes
    const size_t nSubNodes;
    
    /// Symbol representing the node
    const char symbol;
  };
  
  /// Type of present node
  Type type;
  
  /// Collection of the type specifications in the same order of the enum
  static constexpr TypeSpecs typeSpecs[]={
    {"OR",2,'|'},
    {"AND",2,'&'},
    {"OPT",1,'?'},
    {"MANY",1,'*'},
    {"NONZERO",1,'+'},
    {"CHAR",0,'#'},
    {"TOKEN",0,'@'}};
  
  /// Subnodes of the present node
  std::vector<RegexParserNode> subNodes;
  
  /// First char matched by the node
  char begChar;
  
  /// Past last char matched by the node
  char endChar;
  
  /// Id of the matched token
  int tokId;
  
  /// Id of the node
  size_t id;
  
  /// Determine if the node is nullable
  bool nullable;
  
  /// Determine the first elements of the subtree that must match something
  std::vector<RegexParserNode*> firsts;
  
  /// Determine the last elements of the subtree that must match something
  std::vector<RegexParserNode*> lasts;
  
  /// Determine the following elements
  std::vector<RegexParserNode*> follows;
  
  /// Print the current node, and all subnodes iteratively, indenting more and more
  constexpr void printf(const int& indLv=0) const
  {
    /// String used to indent
    char* ind=new char[indLv+1];
    
    for(int i=0;i<indLv;i++)
      ind[i]=' ';
    
    ind[indLv]='\0';
    ::printf("%s %s, id: %zu, ",ind,typeSpecs[type].tag,id);
    if(nullable)
      ::printf("nullable ");
    for(const auto& [tag,vec] : {std::make_pair("firsts",firsts),{"lasts",lasts},{"follows",follows}})
      {
	::printf("%s: {",tag);
	for(size_t i=0;const auto& v : vec)
	  ::printf("%s%zu",(i++==0)?"":",",v->id);
	::printf("}, ");
      }
    switch(type)
      {
      case(CHAR):
	::printf("[%d - %d) = {",begChar,endChar);
	for(char b=begChar;b<endChar;b++)
	  if(std::isalnum(b))
	    ::printf("%c",b);
	::printf("}\n");
	break;
      case(TOKEN):
	::printf("tok %d\n",tokId);
	break;
      default:
	::printf("\n");
      }
    
    for(const auto& subNode : subNodes)
      subNode.printf(indLv+1);
    
    delete[] ind;
  }
  
  /// Sets the id of the node
  constexpr void setRecursivelyId(size_t& id)
  {
    for(auto& subNode : subNodes)
      subNode.setRecursivelyId(id);
    
    this->id=id++;
  }
  
  /// Sets the id of all the nodes
  constexpr size_t setAllIds()
  {
    size_t iD=0;
    
    setRecursivelyId(iD);
    
    return iD;
  }
  
  /// Set whethere the node can be skipped
  constexpr void setNullable()
  {
    for(auto& subNode : subNodes)
      subNode.setNullable();
    
    switch(type)
      {
      case OR:
	nullable=subNodes[0].nullable or subNodes[1].nullable;
	break;
      case AND:
	nullable=subNodes[0].nullable and subNodes[1].nullable;
	break;
      case OPT:
	nullable=true;
	break;
      case MANY:
	nullable=true;
	break;
      case NONZERO:
	nullable=subNodes[0].nullable;
	break;
      case CHAR:
	nullable=endChar==begChar;
	break;
      case TOKEN:
	nullable=true;
      break;
    }
  }
  
  /// Set the first and the last nodes of the subtree which must match something
  constexpr void setFirstsLasts()
  {
    for(auto& subNode : subNodes)
      subNode.setFirstsLasts();
    
    switch(type)
      {
      case OR:
	for(const auto& subNode : subNodes)
	  {
	    firsts.insert(firsts.end(),subNode.firsts.begin(),subNode.firsts.end());
	    lasts.insert(lasts.end(),subNode.lasts.begin(),subNode.lasts.end());
	  }
	break;
      case AND:
	firsts=subNodes[0].firsts;
	if(subNodes[0].nullable)
	  firsts.insert(firsts.cend(),subNodes[1].firsts.begin(),subNodes[1].firsts.end());
	lasts=subNodes[1].lasts;
	if(subNodes[1].nullable)
	  lasts.insert(lasts.end(),subNodes[0].lasts.begin(),subNodes[0].lasts.end());
	break;
      case OPT:
      case MANY:
      case NONZERO:
	firsts=subNodes[0].firsts;
	lasts=subNodes[0].lasts;
	break;
      case CHAR:
      case TOKEN:
	firsts.push_back(this);
	lasts.push_back(this);
      break;
    }
  }
  
  /// Set the possible following node
  constexpr void setFollows()
  {
    for(auto& subNode : subNodes)
      subNode.setFollows();
    
    if(type==AND)
      for(auto& l0 : subNodes[0].lasts)
	l0->follows.insert(l0->follows.end(),subNodes[1].firsts.begin(),subNodes[1].firsts.end());
    else if (type==MANY or type==NONZERO)
      for(auto& l0 : subNodes[0].lasts)
	if(l0->type==CHAR or l0->type==TOKEN)
	  l0->follows.insert(l0->follows.end(),firsts.begin(),firsts.end());
  }
  
  /// Forbids default construct of the node
  RegexParserNode()=delete;
  
  /// Construct from type, subnodes, beging and past end char
  constexpr RegexParserNode(const Type& type,
			    std::vector<RegexParserNode>&& subNodes,
			    const char begChar='\0',
			    const char endChar='\0',
			    const int tokId=0) :
    type(type),
    subNodes(subNodes),
    begChar(begChar),
    endChar(endChar),
    tokId(tokId),
    id{0},
    nullable(false)
  {
  }
};

/////////////////////////////////////////////////////////////////

/// Match two expressions joined by "|"
///
/// Forward declaration
constexpr std::optional<RegexParserNode> matchAndParsePossiblyOrredExpr(Matching& matchIn);

/// Matches a subexpression
constexpr std::optional<RegexParserNode> matchSubExpr(Matching& matchIn)
{
  /// Keep track of the original point in case of needed backup
  std::string_view backup=
    matchIn.ref;
  
  if(matchIn.matchChar('('))
    if(std::optional<RegexParserNode> s=matchAndParsePossiblyOrredExpr(matchIn);s and matchIn.matchChar(')'))
      return s;

  matchIn.ref=backup;
  
  return {};
}

/// Matches any char but '\0'
constexpr std::optional<RegexParserNode> matchDot(Matching& matchIn)
{
  if(matchIn.matchChar('.'))
    return RegexParserNode{RegexParserNode::Type::CHAR,{},1,std::numeric_limits<char>::max()};
  
  return {};
}

/// Return the escaped counterpart of the escaped part in a few cases, or the char itself
constexpr char maybeEscape(const char& c)
{
  switch (c)
    {
    case 'b':return '\b';
    case 'n':return '\n';
    case 'f':return '\f';
    case 'r':return '\r';
    case 't':return '\t';
    }
  
  return c;
}

/// Match a char including possible escape
constexpr std::optional<RegexParserNode> matchPossiblyEscapedChar(Matching& matchIn)
{
  if(const char c=matchIn.matchCharNotIn("|*+?()"))
    if(const char d=(c=='\\')?maybeEscape(matchIn.matchAnyChar()):c)
      return RegexParserNode{RegexParserNode::Type::CHAR,{},d,(char)(d+1)};
  
  return {};
}

/// Match an expression and possible postfix
constexpr std::optional<RegexParserNode> matchAndParseExprWithPossiblePostfix(Matching& matchIn)
{
  using enum RegexParserNode::Type;
  
  /// Result to be returned
  std::optional<RegexParserNode> m;
  
  if(not (m=matchSubExpr(matchIn)))
    if(not (m=matchDot(matchIn)))
      m=matchPossiblyEscapedChar(matchIn);
  
  if(m)
    if(const char c=matchIn.matchAnyCharIn("+?*"))
      m=RegexParserNode{(c=='+')?NONZERO:((c=='?')?OPT:MANY),{std::move(*m)}};
  
  return m;
}

/// Match one or two expressions
constexpr std::optional<RegexParserNode> matchAndParsePossiblyAndedExpr(Matching& matchIn)
{
  /// First and possible only subexpression
  std::optional<RegexParserNode> lhs=
    matchAndParseExprWithPossiblePostfix(matchIn);
  
  if(lhs)
    if(std::optional<RegexParserNode> rhs=matchAndParsePossiblyAndedExpr(matchIn))
      return RegexParserNode{RegexParserNode::Type::AND,{std::move(*lhs),std::move(*rhs)}};
  
  return lhs;
}

/// Match one or two expression, the second is optionally matched
constexpr std::optional<RegexParserNode> matchAndParsePossiblyOrredExpr(Matching& matchIn)
{
  /// First and possible only subexpression
  std::optional<RegexParserNode> lhs=
    matchAndParsePossiblyAndedExpr(matchIn);
  
  /// Keep track of the original point in case of needed backup
  std::string_view backup=matchIn.ref;
  
  if(matchIn.matchChar('|'))
    if(std::optional<RegexParserNode> rhs=matchAndParsePossiblyAndedExpr(matchIn))
      return RegexParserNode{RegexParserNode::Type::OR,{std::move(*lhs),std::move(*rhs)}};
  
  matchIn.ref=backup;
  
  return lhs;
}

/// Gets the parse tree from a list of regex
template <size_t ITok=0,
	  typename...Tail>
requires(std::is_same_v<char,Tail> and ...)
constexpr std::optional<RegexParserNode> parseTreeFromRegex(const char* headRE,
							    const Tail*...tailRE)
{
  using enum RegexParserNode::Type;
  
  if(Matching match(headRE);std::optional<RegexParserNode> t=matchAndParsePossiblyOrredExpr(match))
    if(t and match.ref.empty())
      {
	/// Result, to be returned if last to be matched
	RegexParserNode res(AND,{std::move(*t),{TOKEN,{},'\0','\0',ITok}});
	
	if constexpr(sizeof...(tailRE))
	  if(std::optional<RegexParserNode> n=parseTreeFromRegex<ITok+1>(tailRE...))
	    return RegexParserNode(OR,{std::move(res),std::move(*n)});
	  else
	    return {};
	else
	  return res;
      }
  
  return {};
}

/// Transition in the state machine for the regex parsing
struct RegexMachineTransition
{
  /// State from which to move
  size_t iDStateFrom;
  
  /// First matching char
  char beg;
  
  /// End char
  char end;
  
  /// Next state
  size_t nextDState;
  
  /// Print the transition
 void printf() const
  {
    ::printf(" stateFrom: %zu, [%c-%c), dState: %zu\n",iDStateFrom,beg,end,nextDState);
  }
};

/// Specifications for the DState
struct DState
{
  /// Index of the first transition for the given state
  size_t transitionsBegin;
  
  /// Determine whether this state accepts for a certain token
  bool accepting;
  
  /// Index of the possible token matched by the state
  size_t iToken;
};

/// Specifications of the machine
struct RegexMachineSpecs
{
  /// Number of dStates
  const size_t nDStates;
  
  /// Number of transitions
  const size_t nTranstitions;
  
  /// Detects if the machine is empty
  constexpr bool isNull() const
  {
    return nDStates==0 and nTranstitions==0;
  }
};

template <RegexMachineSpecs Specs>
struct ConstexprRegexParser
{
  std::array<DState,Specs.nDStates> dStates;
  
  std::array<RegexMachineTransition,Specs.nTranstitions> transitions;
  
  constexpr ConstexprRegexParser(const std::vector<DState>& dStateSpecs,
			const std::vector<RegexMachineTransition>& transitions)
  {
    for(size_t i=0;i<Specs.nDStates;i++)
      this->dStates[i]=dStateSpecs[i];
    for(size_t i=0;i<Specs.nTranstitions;i++)
      this->transitions[i]=transitions[i];
  }
  
  constexpr std::optional<size_t> parse(std::string_view v) const
  {
    size_t dState=0;
    
    while(dState<dStates.size())
      {
	if(not std::is_constant_evaluated())
	  printf("%zu\n",dState);
	const char& c=
	  v.empty()?'\0':v.front();
	
	auto trans=transitions.begin()+dStates[dState].transitionsBegin;
	while(trans!=transitions.end() and trans->iDStateFrom==dState and not((trans->beg<=c and trans->end>c)))
	  trans++;
	
	if(trans!=transitions.end() and trans->iDStateFrom==dState)
	  {
	    if(not std::is_constant_evaluated())
	      printf("matched %c with trans %zu %c - %c\n",c,trans->iDStateFrom,trans->beg,trans->end);
	    dState=trans->nextDState;
	    v.remove_prefix(1);
	  }
	else
	  if(dStates[dState].accepting)
	    return {dStates[dState].iToken};
	  else
	    dState=dStates.size();
      }
    
    return {};
  }
};

/// Create parser from regexp
template <RegexMachineSpecs RPS=RegexMachineSpecs{},
	  typename...T>
requires(std::is_same_v<char,T> and ...)
constexpr auto createParserFromRegex(const T*...str)
{
  std::optional<RegexParserNode> parseTree=
    parseTreeFromRegex(str...);
  
  parseTree->setAllIds();
  parseTree->setNullable();
  parseTree->setFirstsLasts();
  parseTree->setFollows();
  
  /////////////////////////////////////////////////////////////////
  
  //if(t and (probe==str+len or (len==0 and *probe=='\0')))
  if(not std::is_constant_evaluated())
    parseTree->printf();
  
  using DStateStates=
    std::vector<RegexParserNode*>;
  
  std::vector<DStateStates> dStates=
    {parseTree->firsts};
  
  std::vector<std::pair<size_t,size_t>> acceptingDStates;
  
  std::vector<RegexMachineTransition> transitions;
  
  for(size_t iDState=0;iDState<dStates.size();iDState++)
    {
      using RangeDel=
	std::pair<char,bool>;
      
      std::vector<RangeDel> rangeDels;
      
      for(const auto& f : dStates[iDState])
	{
	  auto cur=
	    rangeDels.begin();
	  
	  bool startNewRange=
	    false;
	  
	  const char& b=
	    f->begChar;
	  
	  const char &e=
	    f->endChar;
	  
	  while(cur!=rangeDels.end() and cur->first<b)
	    startNewRange=(cur++)->second;
	  
	  if(cur==rangeDels.end() or cur->first!=b)
	    cur=rangeDels.insert(cur,{b,true})+1;
	  
	  while(cur!=rangeDels.end() and cur->first<e)
	    {
	      startNewRange=cur->second;
	      cur++->second=true;
	    }
	  
	  if(cur==rangeDels.end() or cur->first!=e)
	    rangeDels.insert(cur,{e,startNewRange});
	}
      
      if(not std::is_constant_evaluated())
	{
	  printf("dState: {");
	  for(size_t i=0;const auto& n : dStates[iDState])
	    printf("%s%zu",(i++==0)?"":",",n->id);
	  printf("}\n");
	}
      
      for(size_t iRangeBeg=0;iRangeBeg+1<rangeDels.size();iRangeBeg++)
	{
	  const char& b=rangeDels[iRangeBeg].first;
	  const char& e=rangeDels[iRangeBeg+1].first;
	  
	  DStateStates nextDState;
	  std::vector<int> recogTokens;
	  for(const auto& f : dStates[iDState])
	    {
	      if(b>=f->begChar and e<=f->endChar)
		nextDState.insert(nextDState.end(),f->follows.begin(),f->follows.end());
	      if(f->type==RegexParserNode::TOKEN)
		recogTokens.push_back(f->tokId);
	    }
	  
	  if(not rangeDels[iRangeBeg+1].second)
	    iRangeBeg++;
	  
	  constexpr auto dStateDiffers=
		      [](const auto& a,
			 const auto& b)
		      {
			bool d=a.size()!=b.size();
			for(size_t i=0;i<a.size() and not d;i++)
			  d|=a[i]!=b[i];
			return d;
		      };
	  
	  size_t iNextDState=0;
	  while(iNextDState<dStates.size() and dStateDiffers(dStates[iNextDState],nextDState))
	    {
	      // if(not std::is_constant_evaluated())
	      // 	{
	      // 	  printf("a: ");
	      // for(const auto& f : dStates[iNextDState])
	      // 	printf("%p ",f);
	      // printf(", b: ");
	      // for(const auto& f : nextDState)
	      // 	printf("%p ",f);
	      // printf("\n");
	      // 	}
	      iNextDState++;
	    }
	  
	  if(not std::is_constant_evaluated())
	    {
	      printf(" range [%c - %c) goes to state {",b,e);
	      for(size_t i=0;const auto& n : nextDState)
		printf("%s%zu",(i++==0)?"":",",n->id);
	      printf("}, %zu\n",iNextDState);
	    }
	  
	  if(iNextDState==dStates.size() and nextDState.size())
	    dStates.push_back(nextDState);
	  
	  // if(recogTokens.size()>1)
	  //   errorEmitter("multiple token recognized");
	  
	  // if(recogTokens.size()==1 and (b!=e))
	  //   errorEmitter("token recognize when chars accepted");
	  
	  if(recogTokens.size()==0 and (b==e))
	    errorEmitter("token not recognized when chars not accepted");
	  
	  if(recogTokens.size())
	    acceptingDStates.emplace_back(iDState,recogTokens.front());
	  
	  transitions.push_back({iDState,b,e,(b==e)?recogTokens.front():iNextDState});
	}
    }
  
  std::vector<DState> dStateSpecs(dStates.size()+1,DState{.accepting=false,.iToken=0});
  
  for(const auto& [iDState,b,e,iNext] : transitions)
    dStateSpecs[iDState+1].transitionsBegin++;
  for(size_t iDState=1;iDState<dStates.size();iDState++)
    dStateSpecs[iDState].transitionsBegin+=dStateSpecs[iDState-1].transitionsBegin;
  
  for(const auto& [iDState,iToken] : acceptingDStates)
    {
      dStateSpecs[iDState].accepting=true;
      dStateSpecs[iDState].iToken=iToken;
    }
  
  if(not std::is_constant_evaluated())
    for(size_t iDState=0;iDState<dStates.size();iDState++)
      {
	printf("dState %zu {",iDState);
	for(size_t i=0;const auto& n : dStates[iDState])
	  printf("%s%zu",(i++==0)?"":",",n->id);
	printf("} has the following transitions: \n");
	
	for(size_t iTransition=dStateSpecs[iDState].transitionsBegin;iTransition<transitions.size() and transitions[iTransition].iDStateFrom==iDState;iTransition++)
	  transitions[iTransition].printf();
	
	if(dStateSpecs[iDState].accepting)
	  printf(" accepting token %zu\n",dStateSpecs[iDState].iToken);
      }
  
  if constexpr(RPS.isNull())
    return RegexMachineSpecs{.nDStates=dStates.size(),.nTranstitions=transitions.size()};
  else
    return ConstexprRegexParser<RPS>(dStateSpecs,transitions);
}

constexpr size_t test()
{
  constexpr auto specs=createParserFromRegex("c|d(f?|g)","anna",".*");
  constexpr auto parser=createParserFromRegex<specs>("c|d(f?|g)","anna",".*");
  if(constexpr auto oi=parser.parse("ann"))
    return *oi;
  else
    return -1;
}

int main(int narg,char** arg)
{
  test();
  
  return 0;
}
