#include <cstdio>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

struct Matching
{
  std::string_view& ref;
  
  constexpr Matching(std::string_view& in) :
    ref(in)
  {
  }
  
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
  
  constexpr bool matchChar(const char& c)
  {
    const bool accepting=
      (not ref.empty()) and ref.starts_with(c);
    
    const char* p=
      ref.begin();
    if(accepting)
      ref.remove_prefix(1);
    
    if(not std::is_constant_evaluated())
      printf("matchChar(%c) accepting: %d, %s -> %s\n",c,accepting,p,ref.begin());
    return accepting;
  }
  
  constexpr char matchCharNotIn(const std::string_view& filt)
  {
    if(not ref.empty())
      if(const char c=ref.front();filt.find_first_of(c)==filt.npos)
	{
	  ref.remove_prefix(1);
	  if(not std::is_constant_evaluated())
	    printf("filt.find_first_of(match.front())==filt.size(): filt=%s match.front()=%c filt.find_first_of(match.front())=%zu filt.size()=%zu\n",
		   filt.begin(),ref.front(),filt.find_first_of(ref.front()),filt.npos);
	    
	  return c;
	}
    
    return '\0';
  }
  
  constexpr char matchAnyCharIn(const std::string_view& filt)
  {
    if(not ref.empty())
      {
	const size_t pos=
	  filt.find_first_of(ref.front());
	
	if(not std::is_constant_evaluated())
	  printf("ecco %zu\n",pos);
	
	if(pos!=filt.npos)
	  {
	    const char c=
	      ref[pos];
	    
	    ref.remove_prefix(1);
	    
	    return c;
	  }
      }
    
    return '\0';
  }
  
  Matching(const Matching&)=delete;
  
  Matching()=delete;
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
    ::printf("%s %s ",ind,typeSpecs[type].tag);
    if(type==CHAR)
      ::printf("%c %c",begChar,endChar);
    ::printf("\n");
    
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

/// Match two expressions joined by "|"
///
/// Forward declaration
constexpr std::optional<RegexParserNode> matchAndAddPossiblyOrredExpr(Matching& matchIn);

constexpr std::optional<RegexParserNode> matchSubExpr(Matching& matchIn)
{
  std::string_view backup=
    matchIn.ref;
  
  if(matchIn.matchChar('('))
    if(std::optional<RegexParserNode> s=matchAndAddPossiblyOrredExpr(matchIn);s and matchIn.matchChar(')'))
      return s;

  matchIn.ref=backup;
  
  return {};
}

constexpr std::optional<RegexParserNode> matchDot(Matching& matchIn)
{
  if(matchIn.matchChar('.'))
    return RegexParserNode{RegexParserNode::Type::CHAR,{},0,std::numeric_limits<char>::max()};
  
  return {};
}

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

constexpr std::optional<RegexParserNode> matchEscaped(Matching& matchIn)
{
  using enum RegexParserNode::Type;
  
  if(const char c=matchIn.matchCharNotIn("|*+?()"))
    {
      if(not std::is_constant_evaluated())
	printf("matched char %c from %s pointing to %s\n",c,matchIn.ref.begin(),matchIn.ref.begin());
      if(const char d=(c=='\\')?maybeEscape(matchIn.matchAnyChar()):c)
	{
	  if(not std::is_constant_evaluated())
	    printf("kkkkk %c %s %s\n",d,matchIn.ref.begin(),matchIn.ref.begin());
	return RegexParserNode{CHAR,{},d,d+1};
	}
    }
  return {};
}

constexpr std::optional<RegexParserNode> matchAndAddExprWithPossiblePostfix(Matching& matchIn)
{
  using enum RegexParserNode::Type;
  
  std::optional<RegexParserNode> m;

  if(not (m=matchSubExpr(matchIn)))
    if(not (m=matchDot(matchIn)))
      m=matchEscaped(matchIn);
  
  if(m)
    if(const int c=matchIn.matchAnyCharIn("+?*"))
      m=RegexParserNode{(c=='+')?NONZERO:((c=='?')?OPT:MANY),{std::move(*m)}};
  
  if(not std::is_constant_evaluated())
    printf("%s %s %d %d\n",__PRETTY_FUNCTION__,matchIn.ref.begin(),__LINE__,m.has_value());
  
  return m;
}

constexpr std::optional<RegexParserNode> matchAndAddPossiblyAndedExpr(Matching& matchIn)
{
  using enum RegexParserNode::Type;
  
  std::optional<RegexParserNode> lhs=
    matchAndAddExprWithPossiblePostfix(matchIn);
  if(not std::is_constant_evaluated())
    printf("bubba %s\n",matchIn.ref.begin());
  
  if(lhs)
    if(std::optional<RegexParserNode> rhs=matchAndAddPossiblyAndedExpr(matchIn))
      return RegexParserNode{AND,{std::move(*lhs),std::move(*rhs)}};
  if(not std::is_constant_evaluated())
    printf("buuuuubba %s\n",matchIn.ref.begin());
  
  return lhs;
}

constexpr std::optional<RegexParserNode> matchAndAddPossiblyOrredExpr(Matching& matchIn)
{
  using enum RegexParserNode::Type;
  
  if(not std::is_constant_evaluated())
    printf("bba %s\n",matchIn.ref.begin());
  std::optional<RegexParserNode> lhs=
    matchAndAddPossiblyAndedExpr(matchIn);
  if(not std::is_constant_evaluated())
    printf("buuuuuuuuuubba %s\n",matchIn.ref.begin());
  
  std::string_view backup=matchIn.ref;
  if(matchIn.matchChar('|'))
    if(std::optional<RegexParserNode> rhs=matchAndAddPossiblyAndedExpr(matchIn))
      return RegexParserNode{OR,{std::move(*lhs),std::move(*rhs)}};
  
  matchIn.ref=backup;
  
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
    constexpr auto i=test("c|d(f?|g)");
  
  return 0;
}
