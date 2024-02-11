#include <cctype>
#include <cstdio>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

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
  constexpr void setId(size_t& id)
  {
    for(auto& subNode : subNodes)
      subNode.setId(id);
    
    this->id=id++;
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
	nullable=false;
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
	firsts.reserve(subNodes[0].firsts.size()+subNodes[1].firsts.size());
	lasts.reserve(subNodes[0].lasts.size()+subNodes[1].lasts.size());
	for(const auto& subNode : subNodes)
	  {
	    firsts.insert(firsts.end(),subNode.firsts.begin(),subNode.firsts.end());
	    lasts.insert(lasts.end(),subNode.lasts.begin(),subNode.lasts.end());
	  }
	break;
      case AND:
	firsts=subNodes[0].nullable?subNodes[1].firsts:subNodes[0].firsts;
	lasts=subNodes[1].nullable?subNodes[0].lasts:subNodes[1].lasts;
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

/// Matches any char
constexpr std::optional<RegexParserNode> matchDot(Matching& matchIn)
{
  if(matchIn.matchChar('.'))
    return RegexParserNode{RegexParserNode::Type::CHAR,{},0,std::numeric_limits<char>::max()};
  
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

/// Gets the parse tree from many regex
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
	/// Result if last to be matched
	RegexParserNode res(AND,{std::move(*t),{TOKEN,{},'\0','\0',ITok}});
	
	if constexpr(sizeof...(tailRE))
	  if(std::optional<RegexParserNode> n=*parseTreeFromRegex<ITok+1>(tailRE...))
	    return RegexParserNode(OR,{std::move(res),std::move(*n)});
	  else
	    return {};
	else
	  return res;
      }
  
  return {};
}

template <typename...T>
requires(std::is_same_v<char,T> and ...)
constexpr bool test(const T*...str)
{
  std::optional<RegexParserNode> t=parseTreeFromRegex(str...);
  size_t nSubNodes=0;
  t->setId(nSubNodes);
  t->setNullable();
  t->setFirstsLasts();
  t->setFollows();
  
  //if(t and (probe==str+len or (len==0 and *probe=='\0')))
  // if(not std::is_constant_evaluated())
  t->printf();
  
  using DState=
    std::vector<RegexParserNode*>;
  
  std::vector<DState> dStates=
    {t->firsts};
  
  for(size_t iDState=0;iDState<dStates.size();iDState++)
    {
      /**
	 
	 
	 
       */

      
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
      
      printf("dState: {");
      for(size_t i=0;const auto& n : dStates[iDState])
	printf("%s%zu",(i++==0)?"":",",n->id);
      printf("}\n");
      for(auto rangeBeg=rangeDels.begin();rangeBeg+1<rangeDels.end();rangeBeg++)
	{
	  const char& b=rangeBeg->first;
	  const char& e=(rangeBeg+1)->first;
	  
	  DState next;
	  for(const auto& f : dStates[iDState])
	    if(b>=f->begChar and e<=f->endChar)
	      next.push_back(f);
	  
	  printf(" range [%c - %c) goes to state {",b,e);
	  for(size_t i=0;const auto& n : next)
	    printf("%s%zu",(i++==0)?"":",",n->id);
	  printf("}\n");
	  
	  if(not (rangeBeg+1)->second)
	    rangeBeg++;
	}
    }
  
  return t.has_value();
}

int main(int narg,char** arg)
{

  if(narg>1)
    test(arg[1]);
  else
    {
      const bool i=test("c|d(f?|g)","anna");
      i;}
  return 0;
}
