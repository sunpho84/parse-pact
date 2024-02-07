#include <cstdio>
#include <limits>
#include <vector>

struct RegexParserNode;

struct RegexParserNode
{
  enum Type{UNDEF,OR,AND,OPT,MANY,NONZERO,CHAR};
  
  struct TypeSpecs
  {
    const char* const tag;
    
    const size_t nSubNodes;
    
    const char symbol;
  };
  
  const Type type;
  
  static constexpr TypeSpecs typeSpecs[]={
    {"UNDEF",0,'\0'},
    {"OR",2,'|'},
    {"AND",2,'&'},
    {"OPT",1,'?'},
    {"MANY",1,'?'},
    {"NONZERO",1,'+'},
    {"CHAR",0,'#'}};
  
  const std::vector<RegexParserNode> subNodes;
  
  const int begChar;
  
  const int endChar;
  
  constexpr void printf(const int& indLv=0) const
  {
    char* ind=new char[indLv+1];
    for(int i=0;i<indLv;i++)
      ind[i]=' ';
    ind[indLv]='\0';
    ::printf("%s %s %c %c\n",ind,typeSpecs[type].tag,begChar,endChar);
    
    for(const auto& subNode : subNodes)
      subNode.printf(indLv+1);
    
    delete[] ind;
  }
  
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
  
  constexpr operator bool() const
  {
    return type!=UNDEF;
  }
};

/////////////////////////////////////////////////////////////////

constexpr bool matchAllCharsBut(const char* list,
				const char& c)
{
  bool m=c!='\0';
  
  for(const char* li=list;*li!='\0' and m;li++)
    m&=c!=*li;
  
  return m;
}

/// Matches a specific char
constexpr bool match(const char*& str,
		     const char& c)
{
  const bool m=
    (*str==c);
  
  if(m)
    str++;
  
  return m;
}

constexpr RegexParserNode matchAndAddPossiblyOrredExpr(const char*& str);

constexpr RegexParserNode matchAndAddSubExpr(const char*& str)
{
  const char* probe=
    str;
  
  if(match(probe,'('))
    if(RegexParserNode s=
       matchAndAddPossiblyOrredExpr(probe);
       s)
      if(match(probe,')'))
	{
	  str=probe;
	  
	  return s;
	}
  
  return {RegexParserNode::UNDEF,{}};
}

constexpr RegexParserNode matchAndAddDot(const char*& str)
{
  using enum RegexParserNode::Type;
  
  if(match(str,'.'))
    return {CHAR,{},0,std::numeric_limits<int>::max()};
  else
    return {UNDEF,{}};
}

constexpr RegexParserNode matchEscapedChar(const char*& str)
{
  using enum RegexParserNode::Type;
  
  if(const char* tmp=str;
     match(tmp,'\\') and *tmp!='\0')
    {
      str+=2;
      printf("escaping: %c\n",*tmp);
      for(const auto& [c,r] : {std::make_pair('b','\b'),{'n','\n'},{'f','\f'},{'r','\r'},{'t','\t'}})
	if(*tmp==c)
	  return {CHAR,{},r,r+1};
      
      return {CHAR,{},*tmp,*tmp+1};
    }
  
  return {UNDEF,{}};
}

constexpr RegexParserNode matchAndAddCharExpr(const char*& str)
{
  using enum RegexParserNode::Type;
  
  if(RegexParserNode m=
     matchAndAddDot(str);
     m)
    return m;
  else if(RegexParserNode m=matchEscapedChar(str);
	  m)
    {
      printf("matched: %c %d-%d\n",m.begChar,m.begChar,m.endChar);
      return m;
    }
  else if(const char c=
	  *str;
	  matchAllCharsBut("|*+?()",c))
    {
      str++;
      
      return
	{CHAR,{},c,c+1};
    }
  else
    return
      {UNDEF,{}};
}

constexpr RegexParserNode matchAndAddExpr(const char*& str)
{
  if(RegexParserNode m=
     matchAndAddSubExpr(str);
     m)
    return m;
  else
    return matchAndAddCharExpr(str);
}

constexpr RegexParserNode matchAndAddPossiblyPostfixedExpr(const char*& str)
{
  using enum RegexParserNode::Type;
  
  if(RegexParserNode m=
     matchAndAddExpr(str);
     m)
    {
      if(match(str,'+'))
	return
	  {NONZERO,{std::move(m)}};
      else if(match(str,'?'))
	return
	  {OPT,{std::move(m)}};
      else if (match(str,'*'))
	return
	  {MANY,{std::move(m)}};
      else
	return
	  m;
    }
  else
    return {UNDEF,{}};
}

constexpr RegexParserNode matchAndAddPossiblyAndedExpr(const char*& str)
{
  RegexParserNode lhs=
    matchAndAddPossiblyPostfixedExpr(str);
  
  using enum RegexParserNode::Type;
  
  if(lhs)
    {
      if(RegexParserNode rhs=
	 matchAndAddPossiblyAndedExpr(str);
	 rhs)
	return
	  {AND,{std::move(lhs),std::move(rhs)}};
      else
	return lhs;
    }
  else
    return {UNDEF,{}};
}

constexpr RegexParserNode matchAndAddPossiblyOrredExpr(const char*& str)
{
  using enum RegexParserNode::Type;
  
  if(RegexParserNode lhs=
    matchAndAddPossiblyAndedExpr(str);
     lhs)
    {
      if(match(str,'|'))
	{
	  if(RegexParserNode rhs=
	     matchAndAddPossiblyAndedExpr(str);
	     rhs)
	    return
	      {OR,{std::move(lhs),std::move(rhs)}};
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
  const char* probe=str;
  
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
