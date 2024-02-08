#include <cstdio>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

struct Matching
{
  std::string_view& ref;
  
  bool accepting;
  
  std::string_view match;
  
  constexpr Matching(std::string_view& in) :
    ref(in),
    accepting(true),
    match(in)
  {
  }
  
  constexpr char matchAnyChar()
  {
    accepting&=
      (not match.empty());
    
    if(accepting)
      {
	const char c=
	  match.front();
	
	match.remove_prefix(1);
	
	return c;
      }
    else
      return '\0';
  }
  
  constexpr Matching clone()
  {
    return *this;
  }
  
  constexpr bool matchChar(const char& c)
  {
    accepting&=
      (not match.empty()) and match.starts_with(c);
    
    const char* p=match.begin();
    if(accepting)
      match.remove_prefix(1);
    
    if(not std::is_constant_evaluated())
      printf("matchChar(%c) accepting: %d, %s -> %s\n",c,accepting,p,match.begin());
    return accepting;
  }
  
  constexpr char matchCharNotIn(const std::string_view& filt)
  {
    if(accepting&=not match.empty())
      if(const char c=match.front();accepting&=filt.find_first_of(c)==filt.npos)
	{
	  match.remove_prefix(1);
	  if(not std::is_constant_evaluated())
	    printf("filt.find_first_of(match.front())==filt.size(): filt=%s match.front()=%c filt.find_first_of(match.front())=%zu filt.size()=%zu %d\n",
		   filt.begin(),match.front(),filt.find_first_of(match.front()),filt.npos,accepting);
	    
	  return c;
	}
    
    return '\0';
  }
  
  constexpr Matching(const Matching& oth)=delete;
  
  constexpr Matching(Matching& oth) :
    ref(oth.match),
    accepting(oth.accepting),
    match(ref)
  {
  }
  
  constexpr char matchAnyCharIn(const std::string_view& filt)
  {
    if(accepting&=not match.empty())
      {
	const size_t pos=
	  filt.find_first_of(match.front());
	
	if(not std::is_constant_evaluated())
	  printf("ecco %zu\n",pos);
	
	if(accepting&=pos!=filt.npos)
	  {
	    const char c=
	      match[pos];
	    
	    match.remove_prefix(1);
	    
	    return c;
	  }
      }
    
    return '\0';
  }
  
  constexpr ~Matching()
  {
    if(accepting and ref.begin()!=match.begin())
      {
	if(not std::is_constant_evaluated())
	  printf("Accepting! %s -> %s\n",ref.begin(),match.begin());
	
	ref=match;
      }
  }
};


/// Holds a node in the parse tree for regex
struct RegexParserNode
{
  /// Possible ypes of the node
  enum Type{OR,AND,OPT,MANY,NONZERO,CHAR};
  
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
    {"MANY",1,'?'},
    {"NONZERO",1,'+'},
    {"CHAR",0,'#'}};
  
  /// Subnodes of the present node
  std::vector<RegexParserNode> subNodes;
  
  /// First char matched by the node
  char begChar;
  
  /// Past last char matched by the node
  char endChar;
  
  /// Print the current node, and all subnodes iteratively, indenting more and more
  constexpr void printf(const int& indLv=0) const
  {
    /// String used to indent
    char* ind=new char[indLv+1];
    
    for(int i=0;i<indLv;i++)
      ind[i]=' ';
    
    ind[indLv]='\0';
    ::printf("%s %s %c %c\n",ind,typeSpecs[type].tag,begChar,endChar);
    
    for(const auto& subNode : subNodes)
      subNode.printf(indLv+1);
    
    delete[] ind;
  }
  
  RegexParserNode()=delete;
  
  /// Construct from type, subnodes, beging and past end char
  constexpr RegexParserNode(const Type& type,
			    std::vector<RegexParserNode>&& subNodes,
			    const int begChar=0,
			    const int endChar=0) :
    type(type),
    subNodes(subNodes),
    begChar(begChar),
    endChar(endChar)
  {
  }
};

/////////////////////////////////////////////////////////////////

/// Matches a specific char, advancing the stream
constexpr bool match(const char*& str,
		     const char& c)
{
  const bool m=
    *str==c;
  
  if(m)
    str++;
  
  return m;
}

/// Match two expressions joined by "|"
///
/// Forward declaration
constexpr std::optional<RegexParserNode> matchAndAddPossiblyOrredExpr(Matching matchIn);

constexpr std::optional<RegexParserNode> matchSubExpr(Matching match)
{
  if(match.matchChar('('))
    if(std::optional<RegexParserNode> s=matchAndAddPossiblyOrredExpr(match);s and match.matchChar(')'))
      return s;
  
  return {};
}

constexpr std::optional<RegexParserNode> matchDot(Matching probe)
{
  if(probe.matchChar('.'))
    return RegexParserNode{RegexParserNode::Type::CHAR,{},0,std::numeric_limits<char>::max()};
  else
    return {};
}

constexpr char maybeEscape(const char& c)
{
  switch (c)
    {
    case 'b': return '\b';
    case 'n': return '\n';
    case 'f': return '\f';
    case 'r': return '\r';
    case 't': return '\t';
    }
  
  return c;
}

constexpr std::optional<RegexParserNode> matchEscaped(Matching match)
{
  using enum RegexParserNode::Type;
  
  if(const char c=match.matchCharNotIn("|*+?()"))
    {
      if(not std::is_constant_evaluated())
	printf("matched char %c from %s pointing to %s\n",c,match.ref.begin(),match.match.begin());
      if(const char d=(c=='\\')?maybeEscape(match.clone().matchAnyChar()):c)
	{
	  printf("kkkkk %c %d %s %s\n",d,match.accepting,match.ref.begin(),match.match.begin());
	return RegexParserNode{CHAR,{},d,d+1};
	}}
  return {};
}

constexpr std::optional<RegexParserNode> matchAndAddPossiblyPostfixedExpr(Matching matchIn)
{
  using enum RegexParserNode::Type;
  
  std::optional<RegexParserNode> m=
    matchSubExpr(matchIn);
  printf("%s %s %d %d\n",__PRETTY_FUNCTION__,matchIn.match.begin(),__LINE__,m.has_value());
  
  if(not m)
    m=matchDot(matchIn);
  if(not m)
    m=matchEscaped(matchIn);
  
  if(m)
    if(Matching probe=matchIn.clone();const int c=probe.matchAnyCharIn("+?*"))
      m=RegexParserNode{(c=='+')?NONZERO:((c=='?')?OPT:MANY),{std::move(*m)}};
  
  printf("%s %s %d %d\n",__PRETTY_FUNCTION__,matchIn.match.begin(),__LINE__,m.has_value());
  
  return m;
}

constexpr std::optional<RegexParserNode> matchAndAddPossiblyAndedExpr(Matching match)
{
  using enum RegexParserNode::Type;
  
  std::optional<RegexParserNode> lhs=
    matchAndAddPossiblyPostfixedExpr(match);
  if(not std::is_constant_evaluated())
    printf("bubba %s\n",match.match.begin());
  
  if(lhs)
    if(std::optional<RegexParserNode> rhs=matchAndAddPossiblyAndedExpr(match.clone()))
      return RegexParserNode{AND,{std::move(*lhs),std::move(*rhs)}};
  printf("buuuuubba %s\n",match.match.begin());
  
  return lhs;
}

constexpr std::optional<RegexParserNode> matchAndAddPossiblyOrredExpr(Matching match)
{
  using enum RegexParserNode::Type;
  
  printf("bba %s\n",match.match.begin());
  std::optional<RegexParserNode> lhs=
    matchAndAddPossiblyAndedExpr(match);
  printf("buuuuuuuuuubba %s\n",match.match.begin());
  
  if(Matching probe=match.clone();probe.matchChar('|'))
    if(std::optional<RegexParserNode> rhs=matchAndAddPossiblyAndedExpr(probe))
      return RegexParserNode{OR,{std::move(*lhs),std::move(*rhs)}};
  
  return lhs;
}

constexpr bool test(const char* str)
{
  std::string_view toParse=str;
  
  if(not std::is_constant_evaluated())
    printf("%zu\n",toParse.size());
  Matching match(toParse);
  
  std::optional<RegexParserNode> t=
    matchAndAddPossiblyOrredExpr(match);
  
  if(not std::is_constant_evaluated())
    printf("t: %d, *probe:\n",(bool)t);
  
  //if(t and (probe==str+len or (len==0 and *probe=='\0')))
  if(not std::is_constant_evaluated())
    t->printf();
  
  return true;
}

int main(int narg,char** arg)
{
  if(narg>1)
    test(arg[1]);
  else
    const auto i=test("c|d(f?|g)");
  
  return 0;
}
