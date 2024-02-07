#include <array>
#include <cstdio>
#include <limits>

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
  virtual constexpr const char* getTag() const=0;
  
  virtual constexpr size_t getNSubNodes() const=0;
  
  virtual constexpr const RegexParserNodeUPtr& getSubNode(const size_t& i) const=0;
  
  constexpr virtual ~RegexParserNode()
  {
  }
};

constexpr void printf(const RegexParserNodeUPtr& p,
	    const int& indLv=0)
{
  char* ind=new char[indLv+1];
  for(int i=0;i<indLv;i++)
    ind[i]=' ';
  ind[indLv]='\0';
  ::printf("%s %s\n",ind,p.ptr->getTag());
  
  for(size_t i=0;i<p.ptr->getNSubNodes();i++)
    printf(p.ptr->getSubNode(i),indLv+1);
  
  delete[] ind;
}

struct RegexParserOrNode :
  RegexParserNode
{
  std::array<RegexParserNodeUPtr,2> subNodes;
  
  constexpr size_t getNSubNodes() const
  {
    return 2;
  }
  
  constexpr const RegexParserNodeUPtr& getSubNode(const size_t& i) const
  {
    return subNodes[i];
  }
  
  constexpr const char* getTag() const
  {
    return "|";
  }
  
  constexpr RegexParserOrNode()=default;
  
  constexpr RegexParserOrNode(RegexParserNodeUPtr&& a,
			      RegexParserNodeUPtr&& b) :
    subNodes({std::move(a),std::move(b)})
  {
  }
};

struct RegexParserAndNode :
  RegexParserNode
{
  std::array<RegexParserNodeUPtr,2> subNodes;
  
  constexpr size_t getNSubNodes() const
  {
    return 2;
  }
  
  constexpr const RegexParserNodeUPtr& getSubNode(const size_t& i) const
  {
    return subNodes[i];
  }
  
  constexpr const char* getTag() const
  {
    return "&";
  }
  
  constexpr RegexParserAndNode(RegexParserNodeUPtr&& a,
			       RegexParserNodeUPtr&& b) :
    subNodes({std::move(a),std::move(b)})
  {
  }
};

struct RegexParserOptionalNode :
  RegexParserNode
{
  std::array<RegexParserNodeUPtr,1> subNodes;
  
  constexpr size_t getNSubNodes() const
  {
    return 1;
  }
  
  constexpr const RegexParserNodeUPtr& getSubNode(const size_t& i) const
  {
    return subNodes[i];
  }
  
  constexpr const char* getTag() const
  {
    return "?";
  }
  
  constexpr RegexParserOptionalNode(RegexParserNodeUPtr&& a) :
    subNodes{std::move(a)}
  {
  }
};

struct RegexParserManyNode :
  RegexParserNode
{
  std::array<RegexParserNodeUPtr,1> subNodes;
  
  constexpr size_t getNSubNodes() const
  {
    return 1;
  }
  
  constexpr const RegexParserNodeUPtr& getSubNode(const size_t& i) const
  {
    return subNodes[i];
  }
  
  constexpr const char* getTag() const
  {
    return "*";
  }
  
  constexpr RegexParserManyNode(RegexParserNodeUPtr&& a) :
    subNodes{std::move(a)}
  {
  }
};

struct RegexParserNonZeroNode :
  RegexParserNode
{
  std::array<RegexParserNodeUPtr,1> subNodes;
  
  constexpr size_t getNSubNodes() const
  {
    return 1;
  }
  
  constexpr const RegexParserNodeUPtr& getSubNode(const size_t& i) const
  {
    return subNodes[i];
  }
  
  constexpr const char* getTag() const
  {
    return "+";
  }
  
  constexpr RegexParserNonZeroNode(RegexParserNodeUPtr&& a) :
    subNodes{std::move(a)}
  {
  }
};

struct RegexParserCharsNode :
  RegexParserNode
{
  int begin;
  
  int end;
  
  std::array<RegexParserNodeUPtr,0> subNodes;
  
  constexpr size_t getNSubNodes() const
  {
    return 0;
  }
  
  constexpr const RegexParserNodeUPtr& getSubNode(const size_t& i) const
  {
    return subNodes[i];
  }
  
  constexpr const char* getTag() const
  {
    return "<char>";
  }
  
  constexpr RegexParserCharsNode() :
    begin(0),
    end(std::numeric_limits<int>::max())
  {
  }
  
  constexpr RegexParserCharsNode(const int& begin,
		   const int& end) :
    begin(begin),
    end(end)
  {
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
	new RegexParserCharsNode(c,c+1);
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
      if(match(str,'+'))
	return
	  new RegexParserNonZeroNode(std::move(m));
      else if(match(str,'?'))
	return
	  new RegexParserOptionalNode(std::move(m));
      else if (match(str,'*'))
	return
	  new RegexParserManyNode(std::move(m));
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
	  new RegexParserAndNode(std::move(lhs),std::move(rhs));
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
	      new RegexParserOrNode(std::move(lhs),std::move(rhs));
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
  
  printf(t);
  
  return t.ptr!=nullptr;
}

int main()
{
  const bool t=test("c|df?");
  
  printf("%d \n",t);
  
  return 0;
}
