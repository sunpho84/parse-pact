#include <cstdio>
#include <limits>
#include <vector>

template <typename T>
struct MyUniquePtr
{
  T* ptr;
  
  MyUniquePtr(const MyUniquePtr&) = delete;
  
  template <std::derived_from<T> U>
  constexpr MyUniquePtr(U* ptr=nullptr) :
    ptr(ptr)
  {
  }
  
  explicit constexpr MyUniquePtr(T* ptr=nullptr) :
    ptr(ptr)
  {
  }
  
  constexpr void maybeDelete()
  {
    if(ptr)
      {
	delete ptr;
	ptr=nullptr;
      }
  }
  
  constexpr explicit operator bool() const
  {
    return ptr!=nullptr;
  }
  
  template <std::derived_from<T> U>
  MyUniquePtr& operator=(U* oth)
  {
    maybeDelete();
    ptr=oth;
    
    return *this;
  }
  
  constexpr MyUniquePtr(MyUniquePtr&& oth) :
    ptr(oth.ptr)
  {
    oth.ptr=nullptr;
  }
  
  constexpr ~MyUniquePtr()
  {
    maybeDelete();
  }
};

struct RegexParserNode;

using RegexParserNodeUPtr=
  MyUniquePtr<RegexParserNode>;

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
  
  std::vector<RegexParserNodeUPtr> subNodes;
  
  constexpr void printf(const int& indLv=0)
  {
    char* ind=new char[indLv+1];
    for(int i=0;i<indLv;i++)
      ind[i]=' ';
    ind[indLv]='\0';
    ::printf("%s %s\n",ind,typeSpecs[type].tag);
    
    for(const auto& subNode : subNodes)
      subNode.ptr->printf(indLv+1);
    
    delete[] ind;
  }
  
  template <typename...T>
  requires(std::is_same_v<T,RegexParserNodeUPtr> and...)
  constexpr RegexParserNode(const Type& type,
			    T&&...a) :
    type(type)
  {
    (subNodes.emplace_back(std::move(a)),...);
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

constexpr RegexParserNodeUPtr matchAndAddCharExpr(const char*& str)
{
  const char c=
    *str;
  
  if(matchAllCharsBut("|*+?",c))
    {
      str++;
      return
	new RegexParserNode(RegexParserNode::Type::CHAR);
    }
  else
    return RegexParserNodeUPtr{nullptr};
}

constexpr bool match(const char*& str,
		     const char& c)
{
  const bool m=
    (*str==c);
  
  if(m)
    str++;
  return m;
}

constexpr RegexParserNodeUPtr matchAndAddPossiblyPostfixedExpr(const char*& str)
{
  if(RegexParserNodeUPtr m=
     matchAndAddCharExpr(str);
     m)
    {
      using enum RegexParserNode::Type;
      
      if(match(str,'+'))
	return
	  new RegexParserNode(NONZERO,std::move(m));
      else if(match(str,'?'))
	return
	  new RegexParserNode(OPT,std::move(m));
      else if (match(str,'*'))
	return
	  new RegexParserNode(MANY,std::move(m));
      else
	return
	  m;
    }
  else
    return RegexParserNodeUPtr{nullptr};
}

constexpr RegexParserNodeUPtr matchAndAddPossiblyAndedExpr(const char*& str)
{
  RegexParserNodeUPtr lhs=
    matchAndAddPossiblyPostfixedExpr(str);
  
  if(lhs)
    {
      RegexParserNodeUPtr rhs=
	matchAndAddPossiblyPostfixedExpr(str);
      if(rhs)
	return
	  new RegexParserNode(RegexParserNode::Type::AND,std::move(lhs),std::move(rhs));
      else
	return lhs;
    }
  else
    return RegexParserNodeUPtr(nullptr);
}

constexpr RegexParserNodeUPtr matchAndAddPossiblyOrredExpr(const char*& str)
{
  if(RegexParserNodeUPtr lhs=
    matchAndAddPossiblyAndedExpr(str);
     lhs)
    {
      if(match(str,'|'))
	{
	  if(RegexParserNodeUPtr rhs=
	     matchAndAddPossiblyAndedExpr(str);
	     rhs)
	    return
	      new RegexParserNode(RegexParserNode::Type::OR,std::move(lhs),std::move(rhs));
	  else
	    return lhs;
	}
      else
	return lhs;
    }
  else
    return RegexParserNodeUPtr(nullptr);
}

constexpr bool test(const char* str)
{
  RegexParserNodeUPtr t=
    matchAndAddPossiblyOrredExpr(str);
  
  t.ptr->printf();
  
  return t.ptr!=nullptr;
}

int main()
{
  const bool t=test("c|df?");
  
  printf("%d \n",t);
  
  return 0;
}
