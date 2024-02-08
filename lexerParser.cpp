#include <cstdio>
#include <limits>
#include <vector>

/// Holds a node in the parse tree for regex
struct RegexParserNode
{
  /// Possible ypes of the node
  enum Type{UNDEF,OR,AND,OPT,MANY,NONZERO,CHAR};
  
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
    {"UNDEF",0,'\0'},
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
  
  /// Node is true if not undefinite
  constexpr operator bool() const
  {
    return type!=UNDEF;
  }
};

/////////////////////////////////////////////////////////////////

/// Returns true if the char c is not in the terminated string list
constexpr bool isCharNotInList(const char* list,
			       const char& c)
{
  /// State whether the character is in the list
  bool isNot=
    c!='\0';
  
  for(const char* li=list;*li!='\0' and isNot;li++)
    isNot&=c!=*li;
  
  return isNot;
}

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
constexpr RegexParserNode matchAndAddPossiblyOrredExpr(const char*& str);

/// Match two consecurive expressions
constexpr RegexParserNode matchAndAddSubExpr(const char*& str)
{
  /// String to be probed
  const char* probe=
    str;
  
  if(match(probe,'('))
    if(RegexParserNode s=matchAndAddPossiblyOrredExpr(probe);s)
      if(match(probe,')'))
	{
	  str=probe;
	  
	  return s;
	}
  
  return {RegexParserNode::UNDEF,{}};
}

/// Match an expression of char type, possibly escaped
constexpr RegexParserNode matchAndAddPossiblyEscapedChar(const char*& str)
{
  using enum RegexParserNode::Type;
  
  if(match(str,'.'))
    return {CHAR,{},0,std::numeric_limits<char>::max()};
  else if(const char* tmp=str;match(tmp,'\\') and *tmp!='\0')
    {
      str+=2;
      printf("escaping: %c\n",*tmp);
      for(const auto& [c,r] : {std::make_pair('b','\b'),{'n','\n'},{'f','\f'},{'r','\r'},{'t','\t'}})
	if(*tmp==c)
	  return {CHAR,{},r,r+1};
      
      return {CHAR,{},*tmp,*tmp+1};
    }
  else if(const char c=*str;isCharNotInList("|*+?()",c))
    {
      str++;
      
      return {CHAR,{},c,c+1};
    }
  
  return {UNDEF,{}};
}

constexpr RegexParserNode matchAndAddPossiblyPostfixedExpr(const char*& str)
{
  using enum RegexParserNode::Type;
  
  RegexParserNode m=matchAndAddSubExpr(str);
  
  if(not m)
    m=matchAndAddPossiblyEscapedChar(str);
  
  if(m)
    {
      if(match(str,'+'))
	return {NONZERO,{std::move(m)}};
      else if(match(str,'?'))
	return {OPT,{std::move(m)}};
      else if (match(str,'*'))
	return {MANY,{std::move(m)}};
    }
  
  return m;
}

constexpr RegexParserNode matchAndAddPossiblyAndedExpr(const char*& str)
{
  using enum RegexParserNode::Type;
  
  RegexParserNode lhs=matchAndAddPossiblyPostfixedExpr(str);
  
  if(lhs)
    if(RegexParserNode rhs=matchAndAddPossiblyAndedExpr(str);rhs)
      return {AND,{std::move(lhs),std::move(rhs)}};
  
  return lhs;
}

constexpr RegexParserNode matchAndAddPossiblyOrredExpr(const char*& str)
{
  using enum RegexParserNode::Type;
  
  if(RegexParserNode lhs=matchAndAddPossiblyAndedExpr(str);lhs)
    {
      if(match(str,'|'))
	{
	  if(RegexParserNode rhs=matchAndAddPossiblyAndedExpr(str);rhs)
	    return {OR,{std::move(lhs),std::move(rhs)}};
	  else
	    return lhs;
	}
      else
	return lhs;
    }
  else
    return {UNDEF,{}};
}

constexpr bool test(const char* const str,
		    const size_t& len=0)
{
  const char* probe=
    str;
  
  RegexParserNode t=
    matchAndAddPossiblyOrredExpr(probe);
  
  printf("t: %d, *probe: %c\n",(bool)t,*probe);
  
  //if(t and (probe==str+len or (len==0 and *probe=='\0')))
    t.printf();
  
  return t;
}

int main(int narg,char** arg)
{
  if(narg>1)
    test(arg[1]);
  else
    test("c|d(f?|g)");
  
  return 0;
}
