#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <string_view>
#include <vector>

namespace pp::internal
{
  /// Print to terminal if not evaluated at compile time
  ///
  /// Forward declaration
  template <typename...Args>
  constexpr void diagnostic(Args&&...args);
  
  /// Namespace for tempative match functionalities
  namespace temptative
  {
    /// State of an action
    enum State : bool {UNACCEPTED,ACCEPTED};
    
    /// Store the number of tempatative actions, to properly indent the diagnostic
    inline size_t nNestedActions=0;
    
    /// When destroyed, performs the action unless the action is accepted
    template <typename F>
    struct Action
    {
      using enum State;
      
      /// Action performed at destruction if not accepted
      F undoer;
      
      /// State whether the action is accepted
      bool state;
      
      /// Construct taking a funtion
      constexpr Action(const char* descr,
		       F&& undoer,
		       const bool& constructionState=UNACCEPTED) :
	undoer(undoer),
	state(constructionState)
      {
	diagnostic("Starting ",descr,"\n");
	if(not std::is_constant_evaluated())
	  nNestedActions++;
      }
      
      /// Unaccept the action
      constexpr void unaccept()
      {
	state=UNACCEPTED;
      }
      
      /// Accept the action
      constexpr void accept()
      {
	state=ACCEPTED;
      }
      
      /// Destructor, undoing the action if not accepted
      constexpr ~Action()
      {
	if(state==UNACCEPTED)
	  undoer();
	if(not std::is_constant_evaluated())
	  nNestedActions--;
      }
      
      /// Cast to bool, returning the state
      constexpr operator bool() const
      {
	return (bool)state;
      }
    };
  }
  
  /// Emit the error. Can be run only at compile time, so the error will
  /// be prompted at compile time if invoked in a constexpr
  inline void errorEmitter(const char* str)
  {
    fprintf(stderr,"Error: %s\n",str);
    exit(1);
  }
  
  /// Print to terminal if not evaluated at compile time
  template <typename...Args>
  constexpr void diagnostic(Args&&...args)
  {
    if(not std::is_constant_evaluated())
      {
	for(size_t i=0;i<temptative::nNestedActions;i++)
	  std::cout<<"\t";
	((std::cout<<args),...);
      }
  }
  
  /// Makes a reduction of the whole vector \f, using the the function \f
  /// for unary or binary reductions.
  ///
  /// N.B: the function takes the reduction as a second argument, as
  /// it might be optional
  template <typename T,
	    typename F>
  constexpr auto reduce(const std::vector<T>& v,
			const F& f) -> decltype(f(std::declval<T>()))
  {
    if(v.size())
      {
	/// First element
	auto t=f(v.front());
	
	for(size_t i=1;i<v.size();i++)
	  t=f(v[i],t);
	
	return t;
      }
    else
      return {};
  }
  
  /// Returns the total number of elements of a vector of vectors
  template <typename T>
  constexpr size_t vectorOfVectorsTotalEntries(const std::vector<std::vector<T>>& v)
  {
    return reduce(v,
		  [](const std::vector<T>& s,
		     std::optional<size_t> x=std::nullopt)
		  {
		    if(not x)
		      x=0;
		    
		    return *x+s.size();
		  });
  }
  
  /// Constant type T if B is true, non const otherwise
  template <bool B,
	    typename T>
  using ConstIf=std::conditional_t<B,const T,T>;
  
  /// Implements static polymorphism via CRTP, while we wait for C++-23
  template <typename T>
  struct StaticPolymorphic
  {
    /// CRTP cast operator
    constexpr T& self()
    {
      return *(T*)this;
    }
    
    /// Const CRTP cast operator
    constexpr const T& self() const
    {
      return *(const T*)this;
    }
  };
  
  /// Possibly adds an element to a unique elements vector
  template <typename T>
  constexpr std::pair<bool,size_t> maybeAddToUniqueVector(std::vector<T>& v,
							  const T& x)
  {
    if(auto it=std::find(v.begin(),v.end(),x);it==v.end())
      {
	v.push_back(x);
	return {true,v.size()-1};
      }
    else
      return {false,std::distance(v.begin(),it)};
  }
  
  /////////////////////////////////////////////////////////////////
  
  /// Custom bitset
  struct BitSet
  {
    /// Number of bits
    size_t n;
    
    /// Stored data
    std::vector<char> data;
    
    /// Returns the size
    constexpr size_t size() const
    {
      return n;
    }
    
    /// Construct allowing n bits
    constexpr BitSet(const size_t& n) :
      n(n),
      data((n+7)/8,0)
    {
    }
    
    /// Set a given bit
    constexpr void set(const size_t& iEl,
		       const bool& b) &
    {
      const size_t i=iEl/8;
      const char j=iEl%8;
      char& f=data[i];
      const char mask=~(char(1)<<j);
      const char add=char(b)<<j;
      
      f=(f&mask)|add;
    }
    
    /// Access a given bit
    constexpr bool get(const size_t& iEl) const
    {
      const size_t i=iEl/8;
      const char j=iEl%8;
      const char& f=data[i];
      const char mask=char(1)<<j;
      
      return f&mask;
    }
    
    /// Subscribe a bit
    constexpr bool operator[](const size_t& iEl) const
    {
      return get(iEl);
    }
    
    constexpr size_t insert(const BitSet& oth)
    {
      size_t r=0;
      
      for(size_t i=0;i<data.size();i++)
	{
	  char c=data[i]^oth.data[i];
	  for(int i=0;i<8;i++)
	    r+=(c>>i)&1;
	  
	  data[i]|=oth.data[i];
	}
      
      return r;
    }
  };
  
  /////////////////////////////////////////////////////////////////
  
  /// Parameters to hold a vector of vectors of heterogeneous size, on the stack
  struct Stack2DVectorPars
  {
    /// Total number of entries
    size_t nEntries;
    
    /// Number of rows
    size_t nRows;
    
    constexpr bool isNull() const
    {
      return nRows==0;
    }
  };
  
  /// Holds a 2D table with rows of heterogeneous length into a single
  /// array, allocated on stack
  template <typename T,
	    Stack2DVectorPars Pars>
  struct Stack2DVector
  {
    /// Parameters to specify a row into the table
    struct RowPars
    {
      /// Begin of the row
      size_t begin;
      
      /// Row length
      size_t size;
    };
    
    /// Holds the data
    std::array<T,Pars.nEntries> data;
    
    /// Holds the parameters defining the rows positions
    std::array<RowPars,Pars.nRows> rowPars;
    
    /// Access element (row, col)
    constexpr const T& operator()(const size_t& row,
				  const size_t& col) const
    {
      return data[rowPars[row].begin+col];
    }
    
    /// Returns the length of a row
    constexpr const size_t& rowSize(const size_t& row) const
    {
      return rowPars[row].size;
    }
    
    /// Returns the total size
    constexpr const size_t size() const
    {
      return rowPars.size();
    }
    
    /// Fills the entries using the function getRow
    template <typename F>
    constexpr void fillWith(const F& getRow)
    {
      for(size_t iEntry=0,iRow=0;iRow<Pars.nRows;iRow++)
	{
	  rowPars[iRow].begin=iEntry;
	  
	  for(const T& entry : getRow(iRow))
	    data[iEntry++]=entry;
	  
	  rowPars[iRow].size=iEntry-rowPars[iRow].begin;
	}
    }
  };
  
  /////////////////////////////////////////////////////////////////
  
  ///Matches a single char condition
  static constexpr bool charMultiMatches(const char& c,
					 const char& m)
  {
    return c==m;
  }
  
  ///Matches either char of a string
  static constexpr bool charMultiMatches(const char& c,
					 const char* str)
  {
    while(*str!='\0')
      if(*(str++)==c)
	return true;
    
    return false;
  }
  
  /// Matches a range
  static constexpr bool charMultiMatches(const char& c,
					 const std::pair<char,char>& range)
  {
    return c>=range.first and c<range.second;
  }
  
  /// Matches either conditions of a tuple
  template <typename...Cond>
  static constexpr bool charMultiMatches(const char& c,
					 const std::tuple<Cond...>& conds)
  {
    return std::apply([c](const Cond&...cond)
    {
      return (charMultiMatches(c,cond) or ...);
    },conds);
  }
  
  /// Collect all info on char classes
  struct CharClasses
  {
    /// Lower alphabet chars
    static constexpr std::pair<char,char> lower{'a','z'+1};
    
    /// Capitalized alphabet chars
    static constexpr std::pair<char,char> upper{'A','Z'+1};
    
    /// Digits
    static constexpr std::pair<char,char> digit{'0','9'+1};
    
    /// Lower and upper chars
    static constexpr auto alpha=
      std::make_tuple(lower,upper);
    
    /// Lower and upper chars, and digits
    static constexpr auto alnum=
      std::make_tuple(alpha,digit);
    
    /// Elements which can be contained inside a word
    static constexpr auto word=
      std::make_tuple(alnum,'_');
    
    /// Blank spaces
    static constexpr auto blank=
      " \t";
    
    /// Control characters
    static constexpr auto cntrl=
      std::make_tuple(std::make_pair((char)0x01,(char)(0x1f+1)),std::make_pair((char)0x7f,(char)(0x7f+1)));
    
    /// Combination of alnum and punct
    static constexpr auto graph=
      std::make_pair((char)0x21,(char)(0x7e +1));
    
    /// Combination of graph and white space
    static constexpr auto print=
      std::make_pair((char)0x20,(char)(0x7e +1));
    
    /// All chars which refer to punctation
    static constexpr auto punct=
      "-!\"#$%&'()*+,./:;<=>?@[\\]_`{|}~";
    
    /// All spaces
    static constexpr auto space=
      " \t\r\n";
    
    /// Hex digit
    static constexpr auto xdigit=
      "0123456789abcdefABCDEF";
    
    /// Collect all classes in a tuple
    static constexpr auto classes=
      std::make_tuple(std::make_pair("[:alnum:]",alnum),
		      std::make_pair("[:word:]",word),
		      std::make_pair("[:alpha:]",alpha),
		      std::make_pair("[:blank:]",blank),
		      std::make_pair("[:cntrl:]",cntrl),
		      std::make_pair("[:digit:]",digit),
		      std::make_pair("[:graph:]",graph),
		      std::make_pair("[:lower:]",lower),
		      std::make_pair("[:print:]",print),
		      std::make_pair("[:punct:]",punct),
		      std::make_pair("[:space:]",space),
		      std::make_pair("[:upper:]",upper),
		      std::make_pair("[:xdigit:]",xdigit));
    
    /// Accessor to the tuple
    enum ClassId{ALNUM,WORD,ALPHA,BLANK,CNTRL,DIGIT,GRAPH,LOWER,PRINT,PUNCT,SPACE,UPPER,XDIGIT};
    
    /// Detect if the char is in the given class
    template <ClassId ID>
    static constexpr bool charIsInClass(const char& c)
    {
      return charMultiMatches(c,std::get<ID>(classes).second);
    }
  };
  
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
  
  /// Keep track of what matched
  struct Matching
  {
    /// Reference to the string view holding the data
    std::string_view ref;
    
    /// Returns an undoer, which rewinds the matcher if not accepted
    constexpr auto beginTemptativeMatch(const char* descr,
					const bool acceptedByDefault)
    {
      return temptative::Action(descr,
				[back=*this,
				 this]()
				{
				  diagnostic("not accepted, putting back ref \"",this->ref.begin(),"\" -> \"",back.ref.begin(),"\"\n");
				  *this=back;
				},acceptedByDefault);
    }
    
    /// Construct from a string view
    constexpr Matching(const std::string_view& in) :
      ref(in)
    {
    }
    
    /// Advance the reference
    constexpr void advance(const size_t& n=1)
    {
      ref.remove_prefix(n);
    }
    
    /// Match any char
    constexpr char matchAnyChar()
    {
      if(not ref.empty())
	{
	  const char c=
	    ref.front();
	  
	  diagnostic("accepted char: ",c,"\n");
	  advance();
	  
	  return c;
	}
      else
	return '\0';
    }
    
    /// Match a single char
    constexpr bool matchChar(const char& c)
    {
      diagnostic("trying to match char '",c,"'\n");
      
      const bool accepting=
	(not ref.empty()) and ref.starts_with(c);
      
      if(not ref.empty())
	diagnostic("  parsing char: ",ref.front(),"\n");
      else
	diagnostic("  no char can be parsed\n");
      diagnostic((accepting?"accepted":"not accepted")," (expected char: ",c,")\n");
      
      if(accepting)
	advance();
      
      return accepting;
    }
    
    /// Matches a possibly escaped char
    constexpr char matchPossiblyEscapedCharNotIn(const std::string_view& notIn)
    {
      diagnostic("Trying to match any char not in: ",notIn,"\n");
      
      if(const char c=matchCharNotIn(notIn))
	if(const char d=(c=='\\')?maybeEscape(matchAnyChar()):c)
	  {
	    diagnostic("  Matched : '",d,"'\n");
	    
	    return d;
	  }
      
      diagnostic("  Unable to match\n");
      
      return '\0';
    }
    
    /// Match string
    constexpr bool matchStr(std::string_view str)
    {
      /// Rewinds if not matched
      auto matchRes=
	beginTemptativeMatch("matchStr",true);
      
      diagnostic("Trying to match ",str,"\n");
      
      while(matchRes and not (str.empty() or ref.empty()))
	if(matchRes.state&=ref.starts_with(str.front()))
	  {
	    diagnostic("  matched ",str.front(),"\n");
	    
	    advance();
	    str.remove_prefix(1);
	  }
      
      matchRes.state&=str.empty();
      
      return matchRes;
    }
    
    /// Match a char if not in the filt list
    constexpr char matchCharNotIn(const std::string_view& filt)
    {
      if(not ref.empty())
	{
	  diagnostic("parsing char ",ref.front(),"\n");
	  
	  if(const char c=ref.front();filt.find_first_of(c)==filt.npos)
	    {
	      advance();
	      
	      diagnostic("accepted as not in the filter ",filt,"\n");
	      
	      return c;
	    }
	  else
	    diagnostic("not accepted as in the filter ",filt,"\n");
	}
      else
	diagnostic("not accepted as at the end\n");
      
      return '\0';
    }
    
    /// Match a char if it is in the list
    constexpr char matchAnyCharIn(const std::string_view& filt)
    {
      if(not ref.empty())
	{
	  /// Position of the char in the filt, npos if not present
	  const size_t pos=
	    filt.find_first_of(ref.front());
	  
	  if(pos!=filt.npos)
	    {
	      const char c=
		filt[pos];
	      
	      // diagnostic("matched char '",c,"'\n");
	      advance();
	      
	      return c;
	    }
	  
	  // diagnostic("not matched anything with \'",ref.front(),"\' in the filtering list \"",filt,"\"\n");
	}
      
      return '\0';
    }
    
    /// Matches a line of comment beginning with //
    constexpr bool matchLineComment()
    {
      /// Store the end of the line delimiter
      constexpr std::string_view lineEndIds=
	std::string_view("\n\r");
      
      /// Result to be returned
      bool res;
      
      if((res=matchStr("//")))
	{
	  // diagnostic("matched line comment\n");
	  
	  while(not ref.empty() and lineEndIds.find_first_of(ref.front())==lineEndIds.npos)
	    {
	      // diagnostic("matched ",ref.front()," in line comment\n");
	      advance();
	    }
	  
	  // diagnostic("matched line comment end\n");
	}
      // else
      //   if(not std::is_constant_evaluated())
      // 	if(not ref.empty())
      // 	  printf("not matched anything with %c as beginning of line comment\n",ref.front());
      
      return res;
    }
    
    /// Matches a block of text crossing lines if needed
    constexpr bool matchBlockComment()
    {
      /// Result to be returned
      bool res;
      
      if((res=matchStr("/*")))
	{
	  // diagnostic("matched beginning of block comment\n");
	  
	  while(not ref.empty() and not (res=(matchChar('*') and matchChar('/'))))
	    {
	      // diagnostic("discarding '",ref.front(),"'\n");
	      advance();
	    }
	  
	  // if(not std::is_constant_evaluated())
	  //   if(res)
	  //     printf("matched block comment end\n");
	}
      // else
      //   if(not std::is_constant_evaluated())
      // 	if(not ref.empty())
      // 	  printf("not matched %c as beginning of block comment\n",ref.front());
      
      return res;
    }
    
    /// Match whitespaces, line and block comments
    constexpr bool matchWhiteSpaceOrComments()
    {
      /// Result to be returned
      bool res=false;
      
      while(matchAnyCharIn(" \f\n\r\t\v") or matchLineComment() or matchBlockComment())
	res=true;
      
      if(res)
	diagnostic("matched whitespace or comments\n");
      
      return res;
    }
    
    /// Matches a literal or regex, introduced and finished by delim, with no line break
    constexpr std::string_view matchLiteralOrRegex(const char& delim)
    {
      /// Undo if not matched
      auto res=
	beginTemptativeMatch("matchLiteralOrRegex",false);
      
      if(std::string_view beg=ref;
	 ((not ref.empty())) and matchChar(delim))
	{
	  bool escaped{};
	  char c{};
	  
	  // diagnostic("Beginning of literal or regex, delimiter: ",delim,"\n");
	  
	  do
	    {
	      if(ref.empty() or matchAnyCharIn("\n\r"))
		errorEmitter("Unterminated literal or regex");
	      
	      escaped=(c=='\\');
	      c=matchAnyChar();
	      diagnostic(" matched \'",c,"\'\n");
	    }
	  while(escaped or c!=delim);
	  
	  if(beg.begin()+1==ref.begin()-1)
	    errorEmitter("Empty literal or regex");
	  
	  res.accept();
	  
	  return std::string_view{beg.begin()+1,ref.begin()-1};
	}
      
      return {};
    }
    
    /// Matches a literal, introduced and finished by ', with no line break
    constexpr std::string_view matchLiteral()
    {
      return matchLiteralOrRegex('\'');
    }
    
    /// Matches a regex, introduced and finished by ", with no line break
    constexpr std::string_view matchRegex()
    {
      return matchLiteralOrRegex('"');
    }
    
    /// Matches an identifier
    constexpr std::string_view matchId()
    {
      auto matchRes=
	this->beginTemptativeMatch("matchId",false);
      
      if(std::string_view beg=ref;
	 (not ref.empty()) and charMultiMatches(ref.front(),std::make_tuple(CharClasses::alpha,'_')))
	{
	  // diagnostic("Matched ",ref.front(),"\n");
	  advance();
	  while((not ref.empty()) and CharClasses::charIsInClass<CharClasses::WORD>(ref.front()))
	    {
	      // diagnostic("Matched ",ref.front(),"\n");
	      advance();
	    }
	  
	  matchRes.accept();
	  
	  // diagnostic("Matched: ",std::string_view{beg.begin(),ref.begin()},"\n");
	  
	  return std::string_view{beg.begin(),ref.begin()};
	}
      
      return {};
    }
    
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
    size_t tokId;
    
    /// Id of the node
    size_t nodeId;
    
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
      ::printf("%s %s, id: %zu, ",ind,typeSpecs[type].tag,nodeId);
      if(nullable)
	::printf("nullable ");
      for(const auto& [tag,vec] : {std::make_pair("firsts",firsts),{"lasts",lasts},{"follows",follows}})
	{
	  ::printf("%s: {",tag);
	  for(size_t i=0;const auto& v : vec)
	    ::printf("%s%zu",(i++==0)?"":",",v->nodeId);
	  ::printf("}, ");
	}
      switch(type)
	{
	case(CHAR):
	  ::printf("[%d - %d) = {",begChar,endChar);
	  for(char b=begChar;b<endChar;b++)
	    if(std::isprint(b))
	      ::printf("%c",b);
	  ::printf("}\n");
	  break;
	case(TOKEN):
	  ::printf("tok %zu\n",tokId);
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
      
      this->nodeId=id++;
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
			      const size_t tokId=0) :
      type(type),
      subNodes(subNodes),
      begChar(begChar),
      endChar(endChar),
      tokId(tokId),
      nodeId{0},
      nullable(false)
    {
    }
  };
  
  /////////////////////////////////////////////////////////////////
  
  /// Match two expressions joined by "|"
  ///
  /// Forward declaration
  constexpr std::optional<RegexParserNode> matchAndParsePossiblyOrredExpr(Matching& matchIn);
  
  /// Base char range operations
  template <typename T>
  struct BaseCharRanges :
    StaticPolymorphic<T>
  {
    /// Import the static polymorphism cast
    using StaticPolymorphic<T>::self;
    
    /// Insert a single char
    constexpr void set(const char& c)
    {
      self().set(std::make_pair(c,(char)(c+1)));
    }
    
    /// Insert a string
    constexpr void set(const char* str)
    {
      while(*str!='\0')
	self().set(*str++);
    }
    
    /// Insert a tuple
    template <typename...Head>
    constexpr void set(const std::tuple<Head...>& head)
    {
      std::apply([this](const Head&...head)
      {
	self().set(head...);
      },head);
    }
    
    /// Generic case
    template <typename...Args>
    constexpr void set(const Args&...args)
    {
      (self().set(args),...);
    }
  };
  
  /// Describes a range passed by the extrema
  constexpr std::string rangeDescribe(const char b,
				      const char e)
  {
    std::string res;
    auto printChar=
      [&res](const char c)
      {
	if(c>=32 and c<127)
	  res+=c;
	else
	  {
	    res+="\\";
	    for(char t=c,m=100;m;m/=10)
	      {
		res+=(char)((t/m)+48);
		t%=m;
	      }
	  }
      };
    
    if(e==b+1)
      {
	res="'";
	printChar(b);
	res+="'";
      }
    else
      {
	res="[";
	printChar(b);
	res+=";";
	printChar(e);
	res+=")";
      }
    
    return res;
  }
  
  /// Describes a range
  constexpr std::string rangeDescribe(const std::pair<char,char>& r)
  {
    return rangeDescribe(r.first,r.second);
  }
  
  /// Range of chars where consecutive ranges are not merged
  struct UnmergedCharRanges :
    BaseCharRanges<UnmergedCharRanges>
  {
    /// Delimiters of a range: begins (true) or just end(false) at
    /// the given char (depending on the bool)
    using RangeDel=
      std::pair<char,bool>;
    
    /// Delimiters of the range
    std::vector<RangeDel> ranges;
    
    /// Import the set method from base class
    using BaseCharRanges<UnmergedCharRanges>::set;
    
    /// Insert a range
    constexpr void set(const std::pair<char,char>& head)
    {
      /// Begin and end of the range
      const auto& [b,e]=
	head;
      
      /// Current range, at the beginning set at the first range delimiter
      auto cur=
	ranges.begin();
      
      /// Determine whether the range begins, when the end of
      /// the range will be inserted
      bool startNewRange=
	false;
      
      // Find where to insert the range begin
      while(cur!=ranges.end() and cur->first<b)
	startNewRange=(cur++)->second;
      
      // Insert the beginning of the range if not present
      if(cur==ranges.end() or cur->first!=b)
	cur=ranges.insert(cur,{b,true})+1;
      
      // Find where to insert the range end, marking that a
      // range must start at all intermediate delimiters
      while(cur!=ranges.end() and cur->first<e)
	{
	  startNewRange=cur->second;
	  cur++->second=true;
	}
      
      // Insert the end of the range end, if not present
      if(cur==ranges.end() or cur->first!=e)
	ranges.insert(cur,{e,startNewRange});
      
      diagnostic("  new range after inserting ",rangeDescribe(b,e),": \n");
      for(const auto& [d,isBeg] : ranges)
	diagnostic("    r: ",d," begins: ",(isBeg?"True":"False"),")\n");
      diagnostic("\n");
    }
    
    /// Loop on all ranges
    template <typename F>
    constexpr void onAllRanges(const F& f) const
    {
      for(const auto& [d,isBeg] : ranges)
	diagnostic("    r: ",d," begins: ",(isBeg?"True":"False"),"\n");
      
      for(size_t iRangeBeg=0;iRangeBeg+1<ranges.size();iRangeBeg++)
	{
	  /// Beginning of the range
	  const char& b=
	    ranges[iRangeBeg].first;
	  
	  /// End of the range
	  const char& e=
	    ranges[iRangeBeg+1].first;
	  
	  f(b,e);
	  
	  // If a new range does not start here, move on
	  if(not ranges[iRangeBeg+1].second)
	    iRangeBeg++;
	}
    }
  };
  
  /// Range of chars where consecutive ranges are not merged
  struct MergedCharRanges :
    BaseCharRanges<MergedCharRanges>
  {
    /// Delimiters of a range, stored as [beg=first,end=second)
    using RangeDel=
      std::pair<char,char>;
    
    /// Delimiters of the range
    std::vector<RangeDel> ranges;
    
    /// Import the set methods from base class
    using BaseCharRanges<MergedCharRanges>::set;
    
    /// Convert to the complementary range
    constexpr void negate()
    {
      /// Previous end, at the beginning is the first nonzero char
      char prevEnd=
	'\0'+1;
      
      /// Creates the negated ranges
      std::vector<RangeDel> negatedRanges;
      for(const auto& [b,e] : ranges)
	{
	  if(prevEnd!=b)
	    negatedRanges.push_back({prevEnd,b});
	  prevEnd=e;
	}
      
      // Add from last range to the end
      if(const char m=std::numeric_limits<char>::max();prevEnd<m)
	negatedRanges.push_back({prevEnd,m});
      
      diagnostic("range:\n");
      for(const auto& [b,e] : ranges)
	diagnostic(" ",rangeDescribe(b,e),"\n");
      
      diagnostic("negated range:\n");
      for(const auto& [b,e] : negatedRanges)
	diagnostic(" ",rangeDescribe(b,e),"\n");
      
      // Replaces the range with the negated one
      ranges=
	std::move(negatedRanges);
    }
    
    /// Insert a range
    constexpr void set(const std::pair<char,char>& head)
    {
      /// Begin and end of the range to be inserted
      const auto& [b,e]=
	head;
      
      diagnostic("Inserting ",rangeDescribe(head),"\n");
      
      /// Current range, at the beginning set at the first range delimiter
      auto cur=
	ranges.begin();
      
      // Find where to insert the range
      if(ranges.size())
	{
	  diagnostic("Skipping all ranges until finding one which ends after the beginning of that to be inserted\n");
	  while(cur!=ranges.end() and cur->second<b)
	    {
	      diagnostic("  Skipping ",rangeDescribe(*cur),"\n");
	      cur++;
	    }
	  
	  if(cur==ranges.end())
	    diagnostic("All existing ranges skipped\n");
	  else
	    diagnostic("Range ",rangeDescribe(*cur)," ends at ",cur->second," which is after ",b,"\n");
	}
      else
	diagnostic("No range present so far, will insert as a first range\n");
      
      /// Index of the position where to insert, useful for debug
      const size_t i=
	std::distance(ranges.begin(),cur);
      
      // Insert the beginning of the range if not present
      if(cur==ranges.end() or e<cur->first)
	{
	  diagnostic("Inserting ",rangeDescribe(head)," at ",i,"\n");
	  ranges.insert(cur,head);
	}
      else
	{
	  if(cur->first>b)
	    {
	      const char prev=cur->first;
	      cur->first=std::min(cur->first,b);
	      diagnostic("range ",i," ",rangeDescribe(prev,cur->second)," extended left to ",rangeDescribe(*cur),"\n");
	    }
	  
	  if(cur->second<e)
	    {
	      const char prev=cur->second;
	      cur->second=e;
	      diagnostic("range ",i," ",rangeDescribe(cur->first,prev)," extended right to ",rangeDescribe(*cur),"\n");
	      
	      while(cur+1!=ranges.end() and cur->second>=(cur+1)->first)
		{
		  cur->second=std::max(cur->second,(cur+1)->second);
		  diagnostic("extended right to ",rangeDescribe(*cur),"\n");
		  diagnostic("erasing ",rangeDescribe((cur+1)->first,(cur+1)->second),"\n");
		  cur=ranges.erase(cur+1)-1;
		}
	    }
	}
    }
    
    /// Loop on all ranges
    template <typename F>
    constexpr void onAllRanges(const F& f) const
    {
      for(const auto& [b,e] : ranges)
	f(b,e);
    }
  };
  
  /// Matches a set of chars in []
  constexpr std::optional<RegexParserNode> matchBracketExpr(Matching& matchIn)
  {
    /// Rewinds if not matched
    auto undoer=
      matchIn.beginTemptativeMatch("matchBracketExpr",false);
    
    if(matchIn.matchChar('['))
      {
	diagnostic("matched [, starting to match a bracket expression\n");
	
	/// Take whether the range is negated
	const bool negated=
	  matchIn.matchChar('^');
	
	if(negated)
	  diagnostic("negated range\n");
	
	/// List of matchable chars
	MergedCharRanges matchableChars;
	
	if(matchIn.matchChar('-'))
	  matchableChars.set('-');
	
	const auto matchClass=
	  [&matchableChars,
	   &matchIn](const auto& matchClass,
		     const auto& range,
		     const auto&...tail)
	  {
	    if(matchIn.matchStr(range.first))
	      {
		diagnostic("matched class ",range.first,"\n");
		
		matchableChars.set(range.second);
		
		return true;
	      }
	    else
	      if constexpr(sizeof...(tail))
		return matchClass(matchClass,tail...);
	      else
		return false;
	  };
	
	/// Take note of whether something has been matched
	bool matched=true;
	while(matched)
	  {
	    if(not std::apply([&matchClass](const auto&...classes)
	    {
	      return matchClass(matchClass,classes...);
	    },CharClasses::classes))
	      {
		diagnostic("matched no char class\n");
		
		if(const char b=matchIn.matchPossiblyEscapedCharNotIn("^]-"))
		  {
		    diagnostic("matched char ",b,"\n");
		    
		    /// Rewinds if not matched
		    auto rangeMatchState=
		      matchIn.beginTemptativeMatch("matchBracketExprRange",false);
		    
		    if(matchIn.matchChar('-'))
		      {
			diagnostic(" matched - to get char range\n");
			
			if(const char e=matchIn.matchPossiblyEscapedCharNotIn("^]-"))
			  {
			    diagnostic("  matched char range end ",e,"\n");
			    matchableChars.set(std::make_pair(b,e));
			    rangeMatchState.accept();
			  }
		      }
		    
		    if(not rangeMatchState)
		      {
			diagnostic("no char range end, single char\n");
			matchableChars.set(b);
		      }
		  }
		else
		  matched=false;
	      }
	  }
	
	if(matchIn.matchChar('-'))
	  matchableChars.set('-');
	
	if((undoer.state=matchIn.matchChar(']')))
	  {
	    if(negated)
	      matchableChars.negate();
	    
	    diagnostic("matched ]\n");
	    
	    /// Result to be returned, containing a nested list of OR nodes
	    std::optional<RegexParserNode> res;
	    
	    matchableChars.onAllRanges([&res](const char& b,
					      const char& e)
	    {
	      /// First create a detached node
	      auto tmp=
		RegexParserNode{RegexParserNode::Type::CHAR,{},b,e};
	      
	      if(res)
		res=RegexParserNode{RegexParserNode::Type::OR,{std::move(*res),std::move(tmp)}};
	      else
		res=std::move(tmp);
	    });
	    
	    return res;
	  }
      }
    
    return {};
  }
  
  /// Matches a subexpression
  constexpr std::optional<RegexParserNode> matchSubExpr(Matching& matchIn)
  {
    /// Rewinds if not matched
    auto undoer=
      matchIn.beginTemptativeMatch("matchSubExpr",false);
    
    if((undoer.state=matchIn.matchChar('(')))
      if(std::optional<RegexParserNode> s=matchAndParsePossiblyOrredExpr(matchIn))
	{
	  diagnostic("Looking at ')' at string: ",matchIn.ref,"\n");
	  if((undoer.state=s and matchIn.matchChar(')')))
	    return s;
	}
    diagnostic("not accepted\n");
    
    return {};
  }
  
  
  /// Matches any char but '\0'
  constexpr std::optional<RegexParserNode> matchDot(Matching& matchIn)
  {
    if(matchIn.matchChar('.'))
      return RegexParserNode{RegexParserNode::Type::CHAR,{},1,std::numeric_limits<char>::max()};
    
    return {};
  }
  
  /// Match a char including possible escape
  constexpr std::optional<RegexParserNode> matchAndParsePossiblyEscapedChar(Matching& matchIn)
  {
    if(const char c=matchIn.matchPossiblyEscapedCharNotIn("|*+?()"))
      return RegexParserNode{RegexParserNode::Type::CHAR,{},c,(char)(c+1)};
    
    return {};
  }
  
  /// Match an expression and possible postfix
  constexpr std::optional<RegexParserNode> matchAndParseExprWithPossiblePostfix(Matching& matchIn)
  {
    using enum RegexParserNode::Type;
    
    /// Result to be returned
    std::optional<RegexParserNode> m;
    
    if(not (m=matchBracketExpr(matchIn)))
      if(not (m=matchSubExpr(matchIn)))
	if(not (m=matchDot(matchIn)))
	  m=matchAndParsePossiblyEscapedChar(matchIn);
    
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
    
    if(auto undoer=matchIn.beginTemptativeMatch("orExprSecondPart",false);matchIn.matchChar('|'))
      if(std::optional<RegexParserNode> rhs=matchAndParsePossiblyAndedExpr(matchIn);(undoer.state=rhs.has_value()))
	return RegexParserNode{RegexParserNode::Type::OR,{std::move(*lhs),std::move(*rhs)}};
    
    return lhs;
  }
  
  /// Associate the regex to be matched and the index of the symbol to be passed
  struct RegexToken
  {
    /// Regex to be matched
    std::string_view str;
    
    /// Index of the recognized symbol
    size_t iSymbol;
  };
  
  /// Gets the parse tree from a list of regex
  constexpr std::optional<RegexParserNode> parseTreeFromRegex(const std::vector<RegexToken>& regexTokens,
							      const size_t pos=0)
  {
    using enum RegexParserNode::Type;
    
    if(pos<regexTokens.size())
      if(Matching match(regexTokens[pos].str);std::optional<RegexParserNode> t=matchAndParsePossiblyOrredExpr(match))
	if(t and match.ref.empty())
	  {
	    /// Result, to be returned if last to be matched
	    RegexParserNode res(AND,{std::move(*t),{TOKEN,{},'\0','\0',regexTokens[pos].iSymbol}});
	    
	    if(pos+1<regexTokens.size())
	      if(std::optional<RegexParserNode> n=parseTreeFromRegex(regexTokens,pos+1))
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
      ::printf(" stateFrom: %zu, ",iDStateFrom);
      
      if(end==beg+1)
	::printf("%c",beg);
      else
	::printf("[%c-%c)",beg,end);
      
      ::printf(", dState: %zu\n",nextDState);
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
    const size_t nTransitions;
    
    /// Detects if the machine is empty
    constexpr bool isNull() const
    {
      return nDStates==0 and nTransitions==0;
    }
  };
  
  /// Result of parsing
  struct ParseResult
  {
    /// Parsed string
    std::string_view str;
    
    /// Recognized token
    size_t iToken;
  };
  
  /// Base functionality of the regex parser
  template <typename T>
  struct BaseRegexParser :
    StaticPolymorphic<T>
  {
    /// Import the static polymorphism cast
    using StaticPolymorphic<T>::self;
    
    /// Parse a string
    constexpr std::optional<ParseResult> parse(std::string_view v) const
    {
      const auto matchBegin=v.begin();
      
      size_t dState=0;
      
      while(dState<self().dStates.size())
	{
	  diagnostic("\nEntering dState ",dState,"\n");
	  const char& c=
	    v.empty()?'\0':v.front();
	  
	  diagnostic("trying to match ",c,"\n");
	  
	  auto trans=self().transitions.begin()+self().dStates[dState].transitionsBegin;
	  diagnostic("First transition: ",self().dStates[dState].transitionsBegin,"\n");
	  while(trans!=self().transitions.end() and trans->iDStateFrom==dState and not((trans->beg<=c and trans->end>c)))
	    {
	      diagnostic("Ignored transition ",rangeDescribe(trans->beg,trans->end),"\n");
	      trans++;
	    }
	  
	  if(trans!=self().transitions.end() and trans->iDStateFrom==dState)
	    {
	      dState=trans->nextDState;
	      diagnostic("matched ",c," with trans ",trans->iDStateFrom," ",rangeDescribe(trans->beg,trans->end),", going to dState ",dState,"\n");
	      v.remove_prefix(1);
	    }
	  else
	    if(self().dStates[dState].accepting)
	      return ParseResult{std::string_view{matchBegin,v.begin()},self().dStates[dState].iToken};
	    else
	      dState=self().dStates.size();
	}
      
      return {};
    }
  };
  
  /// Dynamic regex parser
  struct DynamicRegexParser :
    BaseRegexParser<DynamicRegexParser>
  {
    /// States of the DFA
    std::vector<DState> dStates;
    
    /// Transitions among the states of the DFA
    std::vector<RegexMachineTransition> transitions;
    
    /// Default constructor
    constexpr DynamicRegexParser()
    {
    }
    
    /// Construct from parse tree
    constexpr DynamicRegexParser(RegexParserNode& parseTree)
    {
      createFromParseTree(parseTree);
    }
    
    /// Create from parse tree
    constexpr void createFromParseTree(RegexParserNode& parseTree)
    {
      parseTree.setAllIds();
      parseTree.setNullable();
      parseTree.setFirstsLasts();
      parseTree.setFollows();
      
      /////////////////////////////////////////////////////////////////
      
      //if(t and (probe==str+len or (len==0 and *probe=='\0')))
      if(not std::is_constant_evaluated())
	parseTree.printf();
      
      /// Label of the dState, given by the set of RegexParserNodes
      /// which the dState represents
      using DStateLabel=
	std::vector<RegexParserNode*>;
      
      /// Labels of the dState, set at the beginning to the firsts of the main node
      std::vector<DStateLabel> dStateLabels=
	{parseTree.firsts};
      
      /// List of the accepting dStates, which match the regex
      std::vector<std::pair<size_t,size_t>> acceptingDStates;
      
      for(size_t iDState=0;iDState<dStateLabels.size();iDState++)
	{
	  /// Delimiters of the range
	  UnmergedCharRanges ranges;
	  
	  for(const auto& f : dStateLabels[iDState])
	    ranges.set(std::make_pair(f->begChar,f->endChar));
	  
	  diagnostic("dState: {");
	  for(size_t i=0;const auto& n : dStateLabels[iDState])
	    diagnostic((i++==0)?"":",",n->nodeId);
	  diagnostic("}\n");
	  
	  // Loop on all ranges to determine to which dState it points,
	  // inserting a new dState if not present
	  ranges.onAllRanges([iDState,
			      &dStateLabels,
			      &acceptingDStates,
			      this](const char& b,
				    const char& e)
	  {
	    /// Label of the next dState
	    DStateLabel nextDState;
	    for(const auto& f : dStateLabels[iDState])
	      if(b>=f->begChar and e<=f->endChar)
		nextDState.insert(nextDState.end(),f->follows.begin(),f->follows.end());
	    
	    /// List of the recognized tokens
	    std::vector<size_t> recogTokens;
	    for(const auto& f : dStateLabels[iDState])
	      if(f->type==RegexParserNode::TOKEN)
		recogTokens.push_back(f->tokId);
	    
	    /// Check if two dStates differ, for some reason the std::vector comparison is not working with clang
	    constexpr auto dStateDiffers=
			[](const auto& a,
			   const auto& b)
			{
			  bool d=
			    a.size()!=b.size();
			  
			  for(size_t i=0;i<a.size() and not d;i++)
			    d|=a[i]!=b[i];
			  
			  return d;
			};
	    
	    /// Search the index of the next dState
	    size_t iNextDState=0;
	    while(iNextDState<dStateLabels.size() and dStateDiffers(dStateLabels[iNextDState],nextDState))
	      iNextDState++;
	    
	    diagnostic(" range ",rangeDescribe(b,e)," goes to state {");
	    for(size_t i=0;const auto& n : nextDState)
	      diagnostic((i++==0)?"":",",n->nodeId);
	    diagnostic("}, ",iNextDState,"\n");
	    
	    // Creates the next dState label if not existing
	    if(iNextDState==dStateLabels.size() and nextDState.size())
	      dStateLabels.push_back(nextDState);
	    
	    // if(recogTokens.size()>1)
	    //   errorEmitter("multiple token recognized");
	    
	    // if(recogTokens.size()==1 and (b!=e))
	    //   errorEmitter("token recognize when chars accepted");
	    
	    if(recogTokens.size()==0 and (b==e))
	      errorEmitter("token not recognized when chars not accepted");
	    
	    // Save the recognized token
	    if(recogTokens.size())
	      acceptingDStates.emplace_back(iDState,recogTokens.front());
	    
	    transitions.push_back({iDState,b,e,(b==e)?recogTokens.front():iNextDState});
	  });
	}
      
      /// Allocates the dStates
      dStates.resize(dStateLabels.size(),DState{.accepting=false,.iToken=0});
      
      // Count the number of transitions per dState
      std::vector<size_t> nTransitionsPerDState(dStates.size());
      for(const auto& [iDState,b,e,iNext] : transitions)
	nTransitionsPerDState[iDState]++;
      
      // Sets the beginning of the transitions for each dState
      for(size_t sum=0,iDState=0;iDState<dStates.size();iDState++)
	{
	  DState& dState=
	    dStates[iDState];
	  
	  dState.transitionsBegin=sum;
	  
	  sum+=nTransitionsPerDState[iDState];
	}
      
      // Mark whether the dState accepts
      for(const auto& [iDState,iToken] : acceptingDStates)
	{
	  dStates[iDState].accepting=true;
	  dStates[iDState].iToken=iToken;
	}
      
      if(not std::is_constant_evaluated())
	for(size_t iDState=0;iDState<dStateLabels.size();iDState++)
	  {
	    printf("dState %zu {",iDState);
	    for(size_t i=0;const auto& n : dStateLabels[iDState])
	      printf("%s%zu",(i++==0)?"":",",n->nodeId);
	    printf("} has the following transitions which start at %zu: \n",dStates[iDState].transitionsBegin);
	    
	    for(size_t iTransition=dStates[iDState].transitionsBegin;iTransition<transitions.size() and transitions[iTransition].iDStateFrom==iDState;iTransition++)
	      transitions[iTransition].printf();
	    
	    if(dStates[iDState].accepting)
	      printf(" accepting token %zu\n",dStates[iDState].iToken);
	  }
    }
    
    /// Gets the parameters needed to build the constexpr parser
    constexpr RegexMachineSpecs getSizes() const
    {
      return {.nDStates=dStates.size(),.nTransitions=transitions.size()};
    }
  };
  
  /// RegexParser storing the machine in fixed size tables
  template <RegexMachineSpecs Specs>
  struct ConstexprRegexParser :
    BaseRegexParser<ConstexprRegexParser<Specs>>
  {
    /// States of the machine
    std::array<DState,Specs.nDStates> dStates;
    
    /// Transitions among states
    std::array<RegexMachineTransition,Specs.nTransitions> transitions;
    
    /// Create from dynamic-sized parser
    constexpr ConstexprRegexParser(const DynamicRegexParser& oth)
    {
      for(size_t i=0;i<Specs.nDStates;i++)
	this->dStates[i]=oth.dStates[i];
      
      for(size_t i=0;i<Specs.nTransitions;i++)
	this->transitions[i]=oth.transitions[i];
    }
    
    /// Default constructor
    constexpr ConstexprRegexParser()=default;
  };
  
  /// Create parser from regexp
  template <RegexMachineSpecs RPS=RegexMachineSpecs{}>
  constexpr auto createParserFromRegex(const std::vector<RegexToken>& tokens)
  {
    /// Creates the parse tree
    std::optional<RegexParserNode> parseTree=
      parseTreeFromRegex(tokens);
    
    if(not parseTree)
      errorEmitter("Unable to parse the regex");
    
    if constexpr(RPS.isNull())
      return DynamicRegexParser(*parseTree);
    else
      return ConstexprRegexParser<RPS>(*parseTree);
  }
  
  /// Prepare a vector with tokens id starting from a variadic list
  template <typename...T>
  requires(std::is_same_v<char,T> and ...)
  constexpr std::vector<RegexToken> createRegexTokensFromStringList(const T*...str)
  {
    std::vector<RegexToken> regexTokens;
    
    for(size_t i=0;const char* const& s : {str...})
      regexTokens.emplace_back(s,i++);
    
    return regexTokens;
  }
  
  /// Create parser from regexp
  template <RegexMachineSpecs RPS=RegexMachineSpecs{},
    typename...T>
    requires(std::is_same_v<char,T> and ...)
    constexpr auto createParserFromRegex(const T*...str)
  {
    return createParserFromRegex<RPS>(createRegexTokensFromStringList(str...));
  }
  
  /// Estimates the parser size
  constexpr auto estimateRegexParserSize(const std::vector<RegexToken>& regexTokens)
  {
    return createParserFromRegex(regexTokens).getSizes();
  }
  
  /// Estimates the parser size
  template <typename...T>
  requires(std::is_same_v<char,T> and ...)
  constexpr auto estimateRegexParserSize(const T*...str)
  {
    return estimateRegexParserSize(createRegexTokensFromStringList(str...));
  }
  
  /////////////////////////////////////////////////////////////////
  
  /// A symbol of a grammar, basic information
  struct BaseGrammarSymbol
  {
    /// View on the string defining the symbol
    std::string_view name;
    
    /// Possible types of a symbol
    enum class Type{NULL_SYMBOL,TERMINAL_SYMBOL,NON_TERMINAL_SYMBOL,END_SYMBOL};
    
    /// Returns the tag associated to the type
    constexpr std::string_view typeTag() const
    {
      switch(type)
	{
	case Type::NULL_SYMBOL:
	  return "NULL";
	  break;
	case Type::TERMINAL_SYMBOL:
	  return "TERMINAL";
	  break;
	case Type::NON_TERMINAL_SYMBOL:
	  return "NON_TERMINAL";
	  break;
	case Type::END_SYMBOL:
	  return "END";
	  break;
	}
    }
    
    //// Type of this symbol
    Type type;
  };
  
  /// A symbol of a grammar, comprehensive information
  struct GrammarSymbol :
    BaseGrammarSymbol
  {
    /// Possible associativities of a symbol
    enum class Associativity{NONE,LEFT,RIGHT};
    
    /// Associativity of this symbol
    Associativity associativity;
    
    /// Precedence of this symbol
    size_t precedence;
    
    /// Store if this symbol has been used to assign explicitly the reference to others
    bool referredAsPrecedenceSymbol;
    
    /// Productions which reduce to this symbol
    std::vector<size_t> iProductions;
    
    /// Productions reachable by the rightmost derivation, from this symbol, by the first production symbol
    std::vector<size_t> iProductionsReachableByFirstSymbol;
    
    /// ermine if this symbol is nullable
    bool nullable;
    
    /// Determine the first elements of the subtree that must match something
    std::vector<size_t> firsts;
    
    /// Determine the following elements
    std::vector<size_t> follows;
    
    /// Construct the symbol
    constexpr GrammarSymbol(const std::string_view& name,
			    const Type& type) :
      BaseGrammarSymbol{name,type},
      associativity(Associativity::NONE),
      precedence(0),
      referredAsPrecedenceSymbol(false),
      nullable(false)
    {
    }
    
    /// Default constructor
    constexpr GrammarSymbol()=default;
  };
  
  /// A production rule of a grammar
  struct GrammarProduction
  {
    /// Symbol on the lhs of the production
    size_t iLhs;
    
    /// Symbols on the rhs of the production
    std::vector<size_t> iRhsList;
    
    /// Possible symbol specifiying the precedence of the production
    std::optional<size_t> precedenceSymbol;
    
    /// Name of the action to be accomplished when matching
    std::string_view action;
    
    /// Returns the precedence, or 0
    constexpr size_t precedence(const std::vector<GrammarSymbol>& symbols) const
    {
      if(precedenceSymbol)
	return symbols[precedenceSymbol.value()].precedence;
      else
	return 0;
    }
    
    /// Returns the production in a string
    constexpr inline std::string describe(const std::vector<GrammarSymbol>& symbols) const
    {
      /// Returned string
      std::string out;
      
      out+=symbols[iLhs].name;
      out+=" :";
      
      for(const auto& iRhs : iRhsList)
	{
	  out+=" ";
	  out+=symbols[iRhs].name;
	}
      
      return out;
    }
    
    /// Determine if nullable after the given position
    constexpr inline bool isNullableAfter(const std::vector<GrammarSymbol>& symbols,
					  size_t position) const
    {
      bool res=true;
      
      while(res and position<iRhsList.size())
	res&=symbols[iRhsList[position++]].nullable;
      
      return res;
    }
  };
  
  /////////////////////////////////////////////////////////////////
  
  /// Possible position in the grammar, representing a state
  struct GrammarItem
  {
    /// Index of the production
    size_t iProduction;
    
    /// Position of the dot
    size_t position;
    
    /// Returns the prouction listed in a string
    constexpr inline std::string describe(const std::vector<GrammarProduction>& productions,
					  const std::vector<GrammarSymbol>& symbols) const
    {
      const GrammarProduction& production=productions[iProduction];
      
      /// Returned string
      std::string out;
      
      out+=symbols[production.iLhs].name;
      out+=" :";
      
      for(size_t iIRhs=0,max=production.iRhsList.size();iIRhs<=max;iIRhs++)
	{
	  if(iIRhs==position)
	    out+=" . ";
	  
	  if(iIRhs<max)
	    {
	      out+=" ";
	      out+=symbols[production.iRhsList[iIRhs]].name;
	    }
	}
      
      return out;
    }
    auto operator<=>(const GrammarItem&) const = default;
  };
  
  /// State in the parser state machine
  struct GrammarState
  {
    /// Indices of the items describing the state
    std::vector<size_t> iItems;
    
    /// Search the passed item
    constexpr std::optional<size_t> findItem(const std::vector<GrammarItem>& items,
					     const GrammarItem& item) const
    {
      /// Poisition of the item, if found
      size_t iSearchItem=0;
      
      while(iSearchItem<iItems.size())
	if(const size_t iItem=iItems[iSearchItem];items[iItem]==item)
	  return iItem;
	else
	  iSearchItem++;
      
      return {};
    }
    
    /// Creates a goto state
    constexpr GrammarState createGotoState(const size_t& iSymbol,
					   std::vector<GrammarItem>& items,
					   const std::vector<GrammarProduction>& productions,
					   const std::vector<GrammarSymbol>& symbols) const
    {
      /// Returned gotostate
      GrammarState gotoState;
      
      for(const size_t& iItem : iItems)
	{
	  const GrammarItem& item=items[iItem];
	  if(item.position<productions[item.iProduction].iRhsList.size())
	    {
	      const size_t& iNextSymbol=productions[item.iProduction].iRhsList[item.position];
	      
	      auto add=
		[&gotoState,
		 &items](const size_t iProduction,
			 const size_t position)
		{
		  const auto [res,iAdded]=maybeAddToUniqueVector(items,{iProduction,position});
		  maybeAddToUniqueVector(gotoState.iItems,iAdded);
		};
	      
	      if(iSymbol==iNextSymbol)
		{
		  // diagnostic(" Reached symbol ",symbols[iSymbol].name,"\n");
		  add(item.iProduction,item.position+1);
		}
	      
	      for(const size_t& iProduction : symbols[iNextSymbol].iProductionsReachableByFirstSymbol)
		if(const GrammarProduction& production=productions[iProduction];production.iRhsList[0]==iSymbol)
		  {
		    // diagnostic(" Reached production ",describe(production)," whose first rhs is the symbol ",symbols[iSymbol].name,"\n");
		    add(iProduction,1);
		  }
	    }
	}
      
      return gotoState;
    }
    
    /// Adds the closure of the state
    constexpr inline void addClosure(std::vector<GrammarItem>& items,
				     const std::vector<GrammarProduction>& productions,
				     const std::vector<GrammarSymbol>& symbols)
    {
      for(size_t iIItem=0;iIItem<iItems.size();iIItem++)
	{
	  /// As we might be modifying items
	  auto item=
	    [&]()
	    {
	      return items[iItems[iIItem]];
	    };
	  
	  if(const std::vector<size_t>& iRhsList=productions[item().iProduction].iRhsList;item().position<iRhsList.size())
	    for(const size_t& iProduction : symbols[iRhsList[item().position]].iProductions)
	      {
		if(const size_t iItem=maybeAddToUniqueVector(items,{iProduction,0}).second;maybeAddToUniqueVector(iItems,iItem).first)
		  diagnostic("  Adding to the closure of \"",productions[item().iProduction].describe(symbols),"\" production: \"",productions[iProduction].describe(symbols),"\"\n");
	      }
	}
    }
    
    /// Returns a description of the state in a string
    constexpr inline std::string describe(const std::vector<GrammarItem>& items,
					  const std::vector<GrammarProduction>& productions,
					  const std::vector<GrammarSymbol>& symbols,
					  const std::string& pref="") const
    {
      /// Returned string
      std::string out;
      
      for(const size_t& iItem : iItems)
	out+=pref+"| "+items[iItem].describe(productions,symbols)+"\n";
      
      return out;
    }
    
    /// Threeway comparison
    auto operator<=>(const GrammarState& oth) const = default;
  };
  
  /// Transition in the parser state machine.
  struct GrammarTransition
  {
    /// Symbol that the transition is taken on.
    size_t iSymbol;
    
    /// State or production that the transition is taken or reduced to
    size_t iStateOrProduction;
    
    /// State whether a transition is a shift or reduction
    enum Type{SHIFT,REDUCE};
    
    /// Type of the current transition
    Type type;
    
    /// Returns a description of the transition in a string
    constexpr inline std::string describe(const std::vector<GrammarItem>& items,
					  const std::vector<GrammarProduction>& productions,
					  const std::vector<GrammarSymbol>& symbols,
					  const std::vector<GrammarState>& states) const
    {
      /// Returned string
      std::string out;
      
      out+="   \"";
      out+=symbols[iSymbol].name;
      out+="\" ";
      if(type==SHIFT)
	out+="transits to state: \n"+states[iStateOrProduction].describe(items,productions,symbols,"       ");
      else
	out+="induces a reduce transition using production: "+productions[iStateOrProduction].describe(symbols)+"\n";
      
      return out;
    }
    
    /// Gets a reduction transition
    static constexpr GrammarTransition getReduce(const size_t iSymbol,
						 const size_t iProduction)
    {
      return {.iSymbol=iSymbol,.iStateOrProduction=iProduction,.type=REDUCE};
    }
    
    /// Threeway comparison
    auto operator<=>(const GrammarTransition& oth) const = default;
  };
  
  /// Lookeahead
  struct Lookahead
  {
    /// Symbols of the lookahead
    BitSet symbolIs;
    
    /// List of lookaheads to which the lookahead propagates to
    std::vector<size_t> iPropagateToItems;
    
    ///Constructor
    constexpr Lookahead(const size_t& n) :
      symbolIs(n)
    {
    }
  };
  
  /// Specifications of the grammar
  struct GrammarSpecs
  {
    /// Number of symbols
    const size_t nSymbols;
    
    const Stack2DVectorPars productionPars;
    
    const size_t nItems;
    
    const Stack2DVectorPars stateItemsPars;
    
    const Stack2DVectorPars stateTransitionsPars;
    
    const RegexMachineSpecs regexMachinePars;
    
    /// Detects if the grammar is empty
    constexpr bool isNull() const
    {
      return
	nSymbols==0 and
	productionPars.isNull() and
	nItems==0 and
	stateItemsPars.isNull() and
	stateTransitionsPars.isNull() and
	regexMachinePars.isNull();
    }
  };
  
  /// Base functionality of the gramamr
  template <typename T>
  struct BaseGrammar :
    StaticPolymorphic<T>
  {
    /// Import the static polymorphism cast
    using StaticPolymorphic<T>::self;
  };
  
  /// Grammar with all the functions to create it
  struct Grammar
  {
    std::string_view name;
    
    std::vector<GrammarSymbol> symbols;
    
    size_t iStartSymbol{};
    
    size_t iEndSymbol{};
    
    size_t iErrorSymbol{};
    
    size_t iWhitespaceSymbol{};
    
    size_t currentPrecedence{};
    
    std::vector<GrammarProduction> productions;
    
    std::vector<RegexToken> whitespaceTokens;
    
    std::vector<GrammarItem> items;
    
    std::vector<GrammarState> stateItems;
    
    std::vector<std::vector<GrammarTransition>> stateTransitions;
    
    std::vector<Lookahead> lookaheads;
    
    DynamicRegexParser regexParser;
    
    /// Describes a production
    constexpr inline std::string describe(const GrammarProduction& production) const
    {
      return production.describe(symbols);
    }
    
    /// Describes an item
    constexpr inline std::string describe(const GrammarItem& item) const
    {
      return item.describe(productions,symbols);
    }
    
    /// Describes the state
    constexpr inline std::string describe(const GrammarState& state,
					  const std::string& pref="") const
    {
      return state.describe(items,productions,symbols,pref);
    }
    
    /// Describes a transition
    constexpr inline std::string describe(const GrammarTransition& transition) const
    {
      return transition.describe(items,productions,symbols,stateItems);
    }
    
    /// Finds or insert a symbol
    constexpr size_t insertOrFindSymbol(const std::string_view& name,
					const GrammarSymbol::Type& type)
    {
      GrammarSymbol* s;
      
      if(auto ref=
	 std::ranges::find_if(symbols,
			      [&name,
			       &type](const GrammarSymbol& s)
			      {
				return s.name==name and s.type==type;
			      });ref!=symbols.end())
	s=&(*ref);
      else
	s=&symbols.emplace_back(name,type);
      
      return std::distance(symbols.begin(),(std::vector<GrammarSymbol>::iterator)s);
    }
    
    constexpr std::optional<size_t> matchAndParseSymbol(Matching& m)
    {
      m.matchWhiteSpaceOrComments();
      
      if(m.matchStr("error"))
	return iErrorSymbol;
      else if(auto l=m.matchLiteral();not l.empty())
	return insertOrFindSymbol(l,GrammarSymbol::Type::TERMINAL_SYMBOL);
      else if(auto r=m.matchRegex();not r.empty())
	return insertOrFindSymbol(r,GrammarSymbol::Type::TERMINAL_SYMBOL);
      else if(auto i=m.matchId();not i.empty())
	return insertOrFindSymbol(i,GrammarSymbol::Type::NON_TERMINAL_SYMBOL);
      
      return std::nullopt;
    }
    
    constexpr bool matchAndParseAssociativityStatement(Matching& matchin)
    {
      /// Undo if not matching
      auto matchRes=
	matchin.beginTemptativeMatch("associativityStatement",false);
      
      using enum GrammarSymbol::Associativity;
      
      constexpr std::array<std::pair<std::string_view,GrammarSymbol::Associativity>,3>
		  possibleAssociativities{std::make_pair("%none",NONE),
					  std::make_pair("%left",LEFT),
					  std::make_pair("%right",RIGHT)};
      
      matchin.matchWhiteSpaceOrComments();
      
      size_t iAss=0;
      while(iAss<possibleAssociativities.size() and
	    not matchin.matchStr(possibleAssociativities[iAss].first))
	iAss++;
      
      if(iAss!=possibleAssociativities.size())
	{
	  diagnostic("Matched ",possibleAssociativities[iAss].first.begin()," associativity\n");
	  
	  GrammarSymbol::Associativity currentAssociativity=possibleAssociativities[iAss].second;
	  currentPrecedence++;
	  
	  while(auto m=matchAndParseSymbol(matchin))
	    {
	      GrammarSymbol& s=symbols[*m];
	      diagnostic("Matched symbol: \"",name,"\"\n");
	      
	      s.associativity=currentAssociativity;
	      s.precedence=currentPrecedence;
	    }
	  
	  matchin.matchWhiteSpaceOrComments();
	  
	  if((matchRes.state=matchin.matchChar(';')))
	    diagnostic("Matched associativity statement end\n");
	  else
	    errorEmitter("Unterminated associativity statement");
	}
      
      return matchRes;
    }
    
    constexpr bool matchAndParseProductionStatement(Matching& matchin)
    {
      auto matchRes=
	matchin.beginTemptativeMatch("productionStatement",false);
      
      matchin.matchWhiteSpaceOrComments();
      
      if(auto i=matchin.matchId();not i.empty())
	{
	  auto iLhs=
	    insertOrFindSymbol(i,GrammarSymbol::Type::NON_TERMINAL_SYMBOL);
	  diagnostic("Found lhs: ",symbols[iLhs].name,"\n");
	  
	  // Add the first symbol found as a starting reduction
	  if(productions.empty())
	    {
	      // Add the rule as a starting rule
	      productions.emplace_back(iStartSymbol,std::vector<size_t>{iLhs});
	      symbols[iStartSymbol].iProductions.emplace_back(0);
	    }
	  
	  matchin.matchWhiteSpaceOrComments();
	  if(matchin.matchChar(':'))
	    {
	      do
		{
		  /// Right hand side of the production
		  std::vector<size_t> iRhss;
		  matchin.matchWhiteSpaceOrComments();
		  while(std::optional<size_t> iMatchedSymbol=matchAndParseSymbol(matchin))
		    {
		      iRhss.push_back(*iMatchedSymbol);
		      matchin.matchWhiteSpaceOrComments();
		      diagnostic("Found rhs: ",symbols[*iMatchedSymbol].name,"\n");
		    }
		  
		  std::optional<size_t> iPrecedenceSymbol;
		  if(matchin.matchStr("%precedence"))
		    {
		      if((iPrecedenceSymbol=matchAndParseSymbol(matchin)))
			symbols[*iPrecedenceSymbol].referredAsPrecedenceSymbol=true;
		      else
			errorEmitter("Expected symbol from which to infer the precedence");
		      
		      matchin.matchWhiteSpaceOrComments();
		    }
		  
		  std::string_view action{};
		  if(matchin.matchChar('['))
		    {
		      matchin.matchWhiteSpaceOrComments();
		      
		      if(action=matchin.matchId();action.empty())
			errorEmitter("Expected identifier to be used as action");
		      
		      diagnostic("matched action: \"",action,"\"\n");
		      
		      matchin.matchWhiteSpaceOrComments();
		      if(not matchin.matchChar(']'))
			errorEmitter("Expected end of action ']'");
		      
		      matchin.matchWhiteSpaceOrComments();
		    }
		  
		  symbols[iLhs].iProductions.push_back(productions.size());
		  productions.emplace_back(iLhs,iRhss,iPrecedenceSymbol,action);
		  
		  diagnostic("ADDED production ",describe(productions.back()),"\n");
		}
	      while(matchin.matchChar('|'));
	      
	      if((matchRes.state=matchin.matchChar(';')))
		diagnostic("Found production statement end\n");
	    }
	}
      
      return matchRes;
    }
    
    constexpr bool matchAndParseWhitespaceStatement(Matching& matchin)
    {
      /// Undo if not matching
      auto matchRes=
	matchin.beginTemptativeMatch("whitespace statement",false);
      
      matchin.matchWhiteSpaceOrComments();
      
      if(matchin.matchStr("%whitespace"))
	{
	  diagnostic("Matched whitespace statement\n");
	  
	  matchin.matchWhiteSpaceOrComments();
	  
	  bool goon=true;;
	  while(goon)
	    {
	      auto l=matchin.matchRegex();
	      if(not l.empty())
		{
		  whitespaceTokens.emplace_back(l,iWhitespaceSymbol);
		  diagnostic("Matched regex ",l,"\n");
		  matchin.matchWhiteSpaceOrComments();
		}
	      else goon=false;
	    }
	  
	  matchRes.state=matchin.matchChar(';');
	}
      
      return matchRes;
    }
    
    /// Adds generic symbols to the symbols list
    constexpr void addGenericSymbols()
    {
      using enum GrammarSymbol::Type;
      
      for(const auto& [name,type,i] :
	    {std::make_tuple(".start",NON_TERMINAL_SYMBOL,&iStartSymbol),
	     {".end",END_SYMBOL,&iEndSymbol},
	     {".error",NULL_SYMBOL,&iErrorSymbol},
	     {".whitespace",NULL_SYMBOL,&iWhitespaceSymbol}})
	{
	  *i=symbols.size();
	  symbols.emplace_back(name,type);
	}
    }
    
    /// Matches the passed string into symbols, actions, productions, etc
    constexpr void parseTheGrammar(const std::string_view& str)
    {
      /// Embeds the string into a matcher
      Matching match(str);
      
      match.matchWhiteSpaceOrComments();
      
      if(auto id=match.matchId();not id.empty())
	{
	  name=id;
	  
	  diagnostic("Matched grammar: \"",id,"\", skipped to ",match.ref.begin(),"\n");
	  
	  match.matchWhiteSpaceOrComments();
	  
	  if(match.matchChar('{'))
	    {
	      diagnostic("Matched {\n");
	      
	      while(matchAndParseAssociativityStatement(match) or
		    matchAndParseWhitespaceStatement(match) or
		    matchAndParseProductionStatement(match))
		diagnostic("parsed some statement\n");
	      
	      match.matchWhiteSpaceOrComments();
	      if(not match.matchChar('}'))
		diagnostic("Unfinished grammar, reference is: \"",match.ref,"\"\n");
	      
	      match.matchWhiteSpaceOrComments();
	      if(not match.ref.empty())
		errorEmitter("Unfinished parsing!\n");
	      else
		diagnostic("Grammar parsing correctly ended\n");
	    }
	  else
	    errorEmitter("Empty grammar\n");
	}
      else
	errorEmitter("Unmatched id to name the grammar\n");
    }
    
    /// Performs some test of the grammar
    constexpr void checkTheGrammar()
    {
      // Check that all symbols are referenced at least once and defined
      for(const GrammarSymbol& s : symbols)
	if(s.type==GrammarSymbol::Type::NON_TERMINAL_SYMBOL and s.iProductions.empty() and not s.referredAsPrecedenceSymbol)
	  errorEmitter("Undefined symbol");
      
      /// Count of symbols usage as rhs or precedence
      std::vector<size_t> symbolsCount(symbols.size(),0);
      for(const GrammarProduction& production : productions)
	{
	  for(const size_t& r : production.iRhsList)
	    symbolsCount[r]++;
	  
	  if(auto p=production.precedenceSymbol)
	    symbolsCount[*p]++;
	}
      
      // Check that the symbols are used
      for(size_t iSymbol=0;iSymbol<symbols.size();iSymbol++)
	if(const std::array<size_t,4> filt{iStartSymbol,iEndSymbol,iErrorSymbol,iWhitespaceSymbol};
	   std::find(filt.begin(),filt.end(),iSymbol)==filt.end())
	  if(symbolsCount[iSymbol]==0)
	    {
	      diagnostic("Symbol ",symbols[iSymbol].name," ",iSymbol," ",iStartSymbol,"\n");
	      errorEmitter("Unreferenced symbol");
	    }
    }
    
    /// Remove the references to a symbol
    constexpr void replaceSymbolReference(size_t& iTargetSymbol,
					  const size_t& iReplacedSymbol,
					  const size_t& iReplacementSymbol)
    {
      // const size_t old=target;
      
      if(iTargetSymbol==iReplacedSymbol)
	iTargetSymbol=iReplacementSymbol;
      
      if(iTargetSymbol>iReplacedSymbol)
	iTargetSymbol--;
      
      // if(old!=target)
      //   diagnostic("before was ",old,"now is ",target,"\n");
    }
    
    /// Replaces all occurrence of the first symbol with that of the second, and remove it from the list
    constexpr void replaceAndRemoveSymbol(const size_t& iReplacedSymbol,
					  const size_t& iReplacementSymbol)
    {
      /// Checks that noth both symbols declared "what"
      const auto checkNotBothDeclared=
	[this,
	 iReplacedSymbol,
	 iReplacementSymbol](const size_t& replaced,
			     const size_t& replacement,
			     const char* what)
	{
	  if(replaced!=0 and replacement!=0)
	    errorEmitter((std::string("symbol ")+std::string(symbols[iReplacedSymbol].name)+std::string(" to be replaced by ")+std::string(symbols[iReplacementSymbol].name)+std::string(" but both have a declared ")+what).c_str());
	};
      
      const size_t& replacedPrecedence=symbols[iReplacedSymbol].precedence;
      size_t& replacementPrecedence=symbols[iReplacementSymbol].precedence;
      
      checkNotBothDeclared(replacedPrecedence,replacementPrecedence,"precedence");
      
      if(replacedPrecedence!=0 and replacementPrecedence!=0)
	errorEmitter((std::string("symbol ")+std::string(symbols[iReplacedSymbol].name)+std::string(" to be replaced by ")+std::string(symbols[iReplacementSymbol].name)+std::string(" but both have a declared precedence")).c_str());
      
      if(replacedPrecedence)
	replacementPrecedence=replacedPrecedence;
      
      const GrammarSymbol::Associativity& replacedAssociativity=symbols[iReplacedSymbol].associativity;
      GrammarSymbol::Associativity& replacementAssociativity=symbols[iReplacementSymbol].associativity;
      
      checkNotBothDeclared((size_t)replacedAssociativity,(size_t)replacementAssociativity,"associativity");
      
      if(replacedAssociativity!=GrammarSymbol::Associativity::NONE)
	replacementAssociativity=replacedAssociativity;
      
      /// Incapsulate the replacement of the symbol
      const auto action=
	[this,
	 iReplacedSymbol,
	 iReplacementSymbol](size_t& i)
	{
	  replaceSymbolReference(i,iReplacedSymbol,iReplacementSymbol);
	};
      
      for(GrammarProduction& p : productions)
	{
	  action(p.iLhs);
	  
	  for(size_t& iRhs : p.iRhsList)
	    action(iRhs);
	  
	  if(p.precedenceSymbol)
	    action(*p.precedenceSymbol);
	}
      
      symbols.erase(symbols.begin()+iReplacedSymbol);
    }
    
    /// Removes the given production
    constexpr void removeProduction(const size_t& iProduction)
    {
      diagnostic("Removing production: ",describe(productions[iProduction]),"\n");
      
      // Remove production from the list
      productions.erase(productions.begin()+iProduction);
      
      // Update the production references
      for(GrammarSymbol& s : symbols)
	for(size_t& jProduction : s.iProductions)
	  if(jProduction>iProduction)
	    jProduction--;
    }
    
    /// Replaces a non-terminal symbols that is actually named terminals
    constexpr bool removeOneRedundantProductionIfFound()
    {
      bool removed=false;
      
      size_t iSymbol=0;
      while(iSymbol<symbols.size() and not removed)
	{
	  if(iSymbol!=iErrorSymbol)
	    if(GrammarSymbol& symbol=symbols[iSymbol];symbol.iProductions.size()==1)
	      {
		const size_t /* don't take by reference! */iProduction=symbol.iProductions.front();
		
		if(const GrammarProduction& production=productions[iProduction];production.iRhsList.size()==1 and production.action.empty())
		  if(const size_t /* don't take by reference! */ iActualSymbol=production.iRhsList.front();symbols[iActualSymbol].type==GrammarSymbol::Type::TERMINAL_SYMBOL)
		    {
		      removed=true;
		      diagnostic("Symbol \"",symbol.name,"\" with precedence ",symbol.precedence,
				 " is an alias for the terminal: \"",symbols[iActualSymbol].name,"\" with precedence ",symbols[iActualSymbol].precedence,"\n");
		      
		      removeProduction(iProduction);
		      replaceAndRemoveSymbol(iSymbol,iActualSymbol);
		    }
	      }
	  
	  if(not removed)
	    iSymbol++;
	}
      
      return removed;
    }
    
    /// Performs grammar optimization
    constexpr void grammarOptimize()
    {
      const auto diag=
	[this](const char* tag)
	{
	  diagnostic("-----------------------------------\n");
	  diagnostic("list of productions ",tag," optimization:\n");
	  for(const GrammarProduction& production : productions)
	    diagnostic(describe(production),"\n");
	  diagnostic("list of symbols ",tag," optimization:\n");
	  for(const GrammarSymbol& s : symbols)
	    diagnostic("symbol ",s.name,"\n");
	  diagnostic("\n");
	  diagnostic("-----------------------------------\n");
	};
      
      diag("before");
      
      bool doneSomething;
      do
	{
	  doneSomething=false;
	  doneSomething|=removeOneRedundantProductionIfFound();
	}
      while(doneSomething);
      
      diag("after");
    }
    
    /// Computes the first elements
    constexpr void calculateFirsts()
    {
      // Calculate firsts
      diagnostic("-----------------------------------\n");
      
      for(size_t nAdded=1;nAdded;)
	{
	  nAdded=0;
	  for(size_t iS=0;iS<symbols.size();iS++)
	    {
	      /// \todo: loop only on non terminal, do the terminal part apart
	      
	      GrammarSymbol& s=symbols[iS];
	      diagnostic("Processing symbol ",s.name,"\n");
	      
	      if(s.type==GrammarSymbol::Type::NON_TERMINAL_SYMBOL)
		for(const size_t& iP : s.iProductions)
		  {
		    const GrammarProduction& p=productions[iP];
		    diagnostic("  Processing production ",iP,", lhs: ",symbols[p.iLhs].name," before added: ",nAdded,", rhs size: ",p.iRhsList.size(),"\n");
		    
		    bool nonNullableFound=false;
		    for(size_t iRhs=0;iRhs<p.iRhsList.size() and not nonNullableFound;iRhs++)
		      {
			const size_t iT=p.iRhsList[iRhs];
			nonNullableFound|=not symbols[iT].nullable;
			diagnostic("  Not at symbols end, adding ",symbols[iT].name,"\n");
			
			diagnostic("  Symbol ",symbols[iT].name," firsts:\n");
			for(const auto& iF : symbols[iT].firsts)
			  {
			    diagnostic("   ",symbols[iF].name,"\n");
			    
			    const bool isAdded=maybeAddToUniqueVector(s.firsts,iF).first;
			    nAdded+=isAdded;
			    
			    if(isAdded)
			      diagnostic("   added ",symbols[iF].name,"\n");
			  }
		      }
		    
		    if(not nonNullableFound)
		      {
			if(not s.nullable)
			  {
			    diagnostic("nullable changed\n");
			    nAdded++;
			  }
			s.nullable=true;
		      }
		  }
	      else
		nAdded+=maybeAddToUniqueVector(s.firsts,iS).first;
	      
	      diagnostic("  nadded: ",nAdded,"\n");
	    }
	  diagnostic("Finished looping on all symbols to find firsts, nAdded: ",nAdded,"\n");
	}
      
      // for(const GrammarSymbol& s : symbols)
      //   {
      // 	diagnostic("symbol ",s.name," firsts:\n");
      // 	for(const auto& iF : s.firsts)
      // 	  diagnostic("   ",symbols[iF].name,"\n");
      //   }
    }
    
    /// Computes the follow elements
    constexpr void calculateFollows()
    {
      diagnostic("-----------------------------------\n");
      
      symbols[iStartSymbol].follows.emplace_back(iEndSymbol);
      for(size_t nAdded=1;nAdded;)
	{
	  nAdded=0;
	  for(size_t iS=0;iS<symbols.size();iS++)
	    {
	      GrammarSymbol& s=symbols[iS];
	      // diagnostic("Processing symbol ",s.name,"\n");
	      
	      for(const size_t& iP : s.iProductions)
		{
		  const GrammarProduction& p=productions[iP];
		  // diagnostic("  Processing production ",iP,", lhs: ",symbols[p.lhs].name," before added: ",added,", rhs size: ",p.rhs.size(),"\n");
		  // for(const size_t& iS : p.rhs)
		  //   diagnostic("  ",symbols[iS].name," ",iS,"\n");
		  
		  bool nonNullableFound=false;
		  size_t iLastBeforeOut=p.iRhsList.size()-1;
		  for(size_t iRhs=p.iRhsList.size()-1;iRhs<p.iRhsList.size() and not nonNullableFound;iRhs--)
		    {
		      GrammarSymbol& curSymbol=symbols[p.iRhsList[iRhs]];
		      for(const auto& iF : s.follows)
			{
			  const bool isAdded=maybeAddToUniqueVector(curSymbol.follows,iF).first;
			  nAdded+=isAdded;
			  // if(isAdded)
			  //   diagnostic("   Added ",symbols[iF].name," from follows to follows of ",curSymbol.name,"\n");
			}
		      
		      nonNullableFound|=not curSymbol.nullable;
		      
		      iLastBeforeOut=iRhs;
		    }
		  
		  for(size_t iRhs=0;iRhs+1<p.iRhsList.size();iRhs++)
		    {
		      // diagnostic("checking previous iRhs ",iRhs," , ",symbols[p.rhs[iRhs]].name," and symbol iLastBeforeOut ",iLastBeforeOut," , ",symbols[p.rhs[iLastBeforeOut]].name,"\n");
		      for(const auto& iF : symbols[p.iRhsList[iLastBeforeOut]].firsts)
			{
			  const bool isAdded=maybeAddToUniqueVector(symbols[p.iRhsList[iRhs]].follows,iF).first;
			  nAdded+=isAdded;
			  // if(isAdded)
			  //   diagnostic("   Added ",symbols[iF].name," from firsts (",std::to_string(1+nonNullableFound),") of ",symbols[p.rhs[iLastBeforeOut]].name," to follows of ",symbols[p.rhs[iRhs]].name,"\n");
			}
		    }
		}
	    }
	  
	  diagnostic("Finished looping on all symbols to find follows, added something: ",nAdded,"\n");
	}
      
      // for(const GrammarSymbol& s : symbols)
      //   {
      // 	diagnostic("symbol ",s.name," follows:\n");
      // 	for(const auto& iF : s.follows)
      // 	  diagnostic("   ",symbols[iF].name,"\n");
      //   }
    }
    
    /// Set the precedence
    constexpr void setPrecedence()
    {
      diagnostic("-----------------------------------\n");
      for(auto& p : productions)
	{
	  diagnostic("Production \"",describe(p),"\"\n");
	  for(size_t iiRhs=p.iRhsList.size()-1;iiRhs<p.iRhsList.size() and not p.precedenceSymbol;iiRhs--)
	    {
	      const size_t iRhs=p.iRhsList[iiRhs];
	      diagnostic(" probing symbol ",iRhs," (\"",symbols[iRhs].name,"\"), type: ",(int)symbols[iRhs].type,"\n");
	      if(symbols[iRhs].type==GrammarSymbol::Type::TERMINAL_SYMBOL)
		{
		  p.precedenceSymbol=iRhs;
		  diagnostic(" precedence symbol: ",symbols[iRhs].name,"\n");
		}
	    }
	}
      
      // // Report the precedence
      // for(auto& p : productions)
      //   if(const auto& pp=p.precedenceSymbol)
      // 	diagnostic("Precedence symbol for production \"",describe(p),"\": ",symbols[*pp].name,"\n");
    }
    
    /// Pre-compute goto states to anticipate their additions
    constexpr void preComputeGotoStates()
    {
      diagnostic("-----------------------------------\n");
      
      // diagnostic("Productions--------\n");
      // for(const GrammarProduction& p : productions)
      //   diagnostic(describe(p),"\n");
      // diagnostic("Productions end--------\n");
      
      //int i=0;
      for(GrammarSymbol& s : symbols)
	{
	  std::vector<size_t> reachable;
	  
	  const auto lookForRachableProductionsOfSymbol=
	    [this,&reachable](const auto& self,
			      GrammarSymbol& curS)->void
	    {
	      for(const size_t& iP : curS.iProductions)
		if(const GrammarProduction& p=productions[iP];p.iRhsList.size())
		  {
		    /// Production reached by the first symbol, potentially included in the list
		    //for(size_t j=0;j<i;j++) diagnostic(" ");
		    diagnostic("testing production ",describe(productions[iP])," \n");
		    if(maybeAddToUniqueVector(reachable,iP).first)
		      {
			//for(size_t j=0;j<i;j++) diagnostic(" ");
			//i+=3;
			self(self,symbols[productions[iP].iRhsList.front()]);
			//i-=3;
		      }
		    else
		      {
			//for(size_t j=0;j<i;j++) diagnostic(" ");
			diagnostic(" Not inserting it\n");
		      }
		  }
	    };
	  
	  lookForRachableProductionsOfSymbol(lookForRachableProductionsOfSymbol,s);
	  
	  for(const size_t& iP : reachable)
	    {
	      diagnostic("->     actually inserting production ",describe(productions[iP])," which allows to reach ",s.name,", first symbol: ",symbols[productions[iP].iRhsList.front()].name,"\n");
	      s.iProductionsReachableByFirstSymbol.push_back(iP);
	    }
	}
      
      for(const GrammarSymbol& s : symbols)
	for(const size_t& iP :  s.iProductionsReachableByFirstSymbol)
	  diagnostic("Symbol \"",s.name,"\" can be reached through production \"",describe(productions[iP]),"\" whose first symbol is \"",symbols[productions[iP].iRhsList.front()].name,"\"\n");
    }
    
    /// Generates the states
    constexpr void generateStates()
    {
      diagnostic("-----------------------------------\n");
      
      stateItems.emplace_back(std::vector<size_t>{0});
      stateTransitions.resize(1);
      
      items.emplace_back(symbols[iStartSymbol].iProductions.front(),0);
      
      const size_t iStartState=0;
      stateItems[iStartState].addClosure(items,productions,symbols);
      
      diagnostic("Start state first production: ",describe(productions[symbols[iStartSymbol].iProductions.front()]),"\n");
      
      for(std::vector<size_t> iStates{0},iNextStates;iStates.size();iStates=iNextStates)
	{
	  iNextStates.clear();
	  
	  for(const size_t& iState : iStates)
	    for(size_t iSymbol=0;iSymbol<symbols.size();iSymbol++)
	      if(iSymbol!=iEndSymbol)
		if(const GrammarState gotoState=stateItems[iState].createGotoState(iSymbol,items,productions,symbols);gotoState.iItems.size())
		  {
		    /// Search the goto state in the list of states
		    const auto [inserted,iGotoState]=maybeAddToUniqueVector(stateItems,gotoState);
		    
		    if(inserted)
		      {
			iNextStates.push_back(iGotoState);
			stateTransitions.emplace_back();
		      }
		    
		    stateTransitions[iState].emplace_back(iSymbol,iGotoState);
		    diagnostic("Emplaced in state:\n",describe(stateItems[iState]));
		    diagnostic(" the transition mediated by symbol \"",symbols[iSymbol].name,"\" to state\n",describe(stateItems[iGotoState]),"\n");
		    
		  }
	}
      
      for(size_t iState=0;iState<stateItems.size();iState++)
	{
	  diagnostic("--\n");
	  
	  diagnostic("State:\n",describe(stateItems[iState]));
	  diagnostic("has ",stateTransitions[iState].size()," transitions:\n");
	  
	  if(stateTransitions[iState].size())
	    for(const GrammarTransition& t : stateTransitions[iState])
	      diagnostic(describe(t));
	}
      
      for(auto& s : stateItems)
	s.addClosure(items,productions,symbols);
    }
    
    /// Generates the spontaneous lookeaheads
    constexpr void generateSpontaneousLookahead()
    {
      diagnostic("-----------------------------------\n");
      
      lookaheads.resize(items.size(),symbols.size());
      diagnostic("Building the lookaheds for ",symbols.size()," symbols, read from lookaheads: ",lookaheads.front().symbolIs.n," nchars: ",lookaheads.front().symbolIs.data.size(),"\n");
      lookaheads[0].symbolIs.set(iEndSymbol,1);
      
      for(const GrammarState& state : stateItems)
	for(const size_t iItem : state.iItems)
	  {
	    const GrammarItem& item=items[iItem];
	    const size_t& iProduction=item.iProduction;
	    const GrammarProduction& production=productions[iProduction];
	    
	    diagnostic("Considering item ",describe(item),"\n");
	    
	    if(item.position<production.iRhsList.size())
	      {
		const size_t iSymbol=production.iRhsList[item.position];
		const GrammarSymbol& symbol=symbols[iSymbol];
		diagnostic(" at symbol: ",symbol.name,"\n");
		
		std::vector<size_t> toIns;
		for(size_t nonNullable=0,iOtherPosition=item.position+1;iOtherPosition<production.iRhsList.size() and nonNullable==0;iOtherPosition++)
		  {
		    const size_t iOtherSymbol=production.iRhsList[iOtherPosition];
		    const GrammarSymbol& otherSymbol=symbols[iOtherSymbol];
		    
		    for(const size_t& iFirst : otherSymbol.firsts)
		      toIns.push_back(iFirst);
		    
		    nonNullable+=not otherSymbol.nullable;
		  }
		
		for(const size_t& iOtherProduction : symbol.iProductions)
		  {
		    diagnostic(" Searching for production ",describe(productions[iOtherProduction]),"\n");
		    
		    for(const size_t iOtherItem : state.iItems)
		      if(items[iOtherItem]==GrammarItem{iOtherProduction,0})
			{
			  diagnostic("Adding to lookahead of item ",describe(items[iOtherItem])," the symbols: \n");
			  for(const size_t& iIns : toIns)
			    {
			      lookaheads[iOtherItem].symbolIs.set(iIns,1);
			      // for(const char& c : lookaheads[iOtherItem].data)
			      //   diagnostic(lookaheads[iOtherItem].data.size()," -> ",(int)c,"\n");
			      // diagnostic("\n");
			      diagnostic("  ",symbols[iIns].name,"\n");
			    }
			}
		  }
	      }
	  }
      
      diagnostic("---\n");
      for(size_t iItem=0;iItem<lookaheads.size();iItem++)
	{
	  diagnostic("Item ",describe(items[iItem])," contains the following lookahead:\n");
	  for(size_t iSymbol=0;iSymbol<symbols.size();iSymbol++)
	    if(lookaheads[iItem].symbolIs.get(iSymbol))
	      diagnostic("   ",symbols[iSymbol].name,"\n");
	  diagnostic("---\n");
	}
    }
    
    /// Generate goto items
    constexpr void generateGotoItems()
    {
      diagnostic("-----------------------------------\n");
      
      for(size_t iState=0;iState<stateItems.size();iState++)
	{
	  for(const GrammarTransition& transition : stateTransitions[iState])
	    for(const size_t& iItem : stateItems[iState].iItems)
	      {
		const GrammarItem& item=items[iItem];
		// const GrammarSymbol& symbol=symbols[transition.iSymbol];
		const GrammarProduction& production=productions[item.iProduction];
		if(production.iRhsList.size() and production.iRhsList[item.position]==transition.iSymbol)
		  maybeAddToUniqueVector(lookaheads[iItem].iPropagateToItems,*stateItems[transition.iStateOrProduction].findItem(items,{item.iProduction,item.position+1}));
	      }
	  
	  for(const size_t& iItem : stateItems[iState].iItems)
	    {
	      const GrammarItem& item=items[iItem];
	      const GrammarProduction& production=productions[item.iProduction];
	      
	      if(const size_t& position=item.position;
		 position<production.iRhsList.size() and production.isNullableAfter(symbols,position+1))
		for(const size_t& iOtherProduction : symbols[production.iRhsList[position]].iProductions)
		  {
		    if(const std::optional<size_t> maybeIGotoItem=stateItems[iState].findItem(items,{iOtherProduction,0}))
		      maybeAddToUniqueVector(lookaheads[iItem].iPropagateToItems,*maybeIGotoItem);
		  }
	    }
	}
      
      diagnostic("---\n");
      for(size_t iItem=0;iItem<lookaheads.size();iItem++)
	{
	  diagnostic("$ lookahead of symbols\n");
	  for(size_t iS=0;iS<symbols.size();iS++)
	    if(lookaheads[iItem].symbolIs.get(iS))
	      diagnostic("   ",symbols[iS].name,"\n");
	  diagnostic("has item ",describe(items[iItem])," propagates to:\n");
	  for(const size_t& iPropagateTo : lookaheads[iItem].iPropagateToItems)
	    diagnostic("   ",describe(items[iPropagateTo]),"\n");
	}
    }
    
    constexpr void propagateLookaheads()
    {
      // Propagates lookahead
      diagnostic("-----------------------------------\n");
      std::vector<size_t> iLookaheadsToInsert(lookaheads.size());
      std::iota(iLookaheadsToInsert.begin(),iLookaheadsToInsert.end(),0);
      for(std::vector<size_t> nextLookaheads;not iLookaheadsToInsert.empty();iLookaheadsToInsert.swap(nextLookaheads))
	{
	  nextLookaheads.clear();
	  
	  for(const size_t& iLookahead : iLookaheadsToInsert)
	    for(const Lookahead& lookahead=lookaheads[iLookahead];const size_t& iPropagateToItem : lookahead.iPropagateToItems)
	      {
		const auto print=
		  [this](const size_t &iItem)
		  {
		    std::string res;
		    res+="item \""+describe(items[iItem])+"\" containing the following symbols:\n";
		    for(size_t iS=0;iS<symbols.size();iS++)
		      if(lookaheads[iItem].symbolIs.get(iS))
			res+="  "+std::string(symbols[iS].name)+"\n";
		    
		    return res;
		  };
		
		diagnostic(" Lookahead ",print(iLookahead)," inserting into ",print(iPropagateToItem));
		
		const size_t n=lookaheads[iPropagateToItem].symbolIs.insert(lookahead.symbolIs);
		
		if(n) nextLookaheads.push_back(iPropagateToItem);
		
		diagnostic("inserted ",n," into ",describe(items[iPropagateToItem]),"\n\n");
	      }
	  
	  diagnostic("Next iteration\n");
	}
      
      diagnostic("---\n");
      for(size_t iItem=0;iItem<lookaheads.size();iItem++)
	{
	  diagnostic("Item ",describe(items[iItem])," contains the following lookahead:\n");
	  for(size_t iSymbol=0;iSymbol<symbols.size();iSymbol++)
	    if(lookaheads[iItem].symbolIs.get(iSymbol))
	      diagnostic("   ",symbols[iSymbol].name,"\n");
	  diagnostic("---\n");
	}
    }
    
    /// Inserts a reduce transition
    constexpr void insertReduceTransition(std::vector<GrammarTransition>& transitions,
					  const size_t& iSymbol,
					  const size_t& iProduction)
    {
      transitions.push_back(GrammarTransition::getReduce(iSymbol,iProduction));
      diagnostic("        inserting new reduce transitions\n");
    }
    
    /// Try to solve a shift/reduce conflict
    constexpr void dealWithShiftReduceConflict(GrammarTransition& transition,
					       const GrammarSymbol& symbol,
					       const size_t& iProduction)
    {
      const GrammarProduction& production=productions[iProduction];
      const size_t productionPrecedence=production.precedence(symbols);
      
      if(productionPrecedence==0 or symbol.precedence==0 or
	 (symbol.precedence==productionPrecedence and symbol.associativity==GrammarSymbol::Associativity::NONE))
	errorEmitter((std::string("shift/reduce conflict for '")+std::string(symbols[production.iLhs].name)+"' on '"+std::string(symbol.name)+
		      "' ought to transition: "+describe(transition)+"\nproduction precedence: "+std::to_string(productionPrecedence)+" symbol precedence: "+std::to_string(symbol.precedence)+" symbol associativity: "+std::to_string((int)symbol.associativity)).c_str());
      else
	if(productionPrecedence>symbol.precedence or (symbol.precedence==productionPrecedence and symbol.associativity==GrammarSymbol::Associativity::RIGHT))
	  {
	    diagnostic("overriding shift ",describe(transition));
	    transition.type=GrammarTransition::REDUCE;
	    transition.iStateOrProduction=iProduction;
	    diagnostic(" into reduce: ",describe(transition));
	  }
	else
	  {
	    diagnostic("leaving already esisting transition ",describe(transition)," given that ");
	    if(productionPrecedence<symbol.precedence)
	      diagnostic(" the production has precedence ",productionPrecedence," lesser than the symbol ",symbol.precedence);
	    else
	      diagnostic(" the production has the same precedence ",productionPrecedence," of the symbol ,which has associatity",(int)symbol.associativity,
			 " different from right (",(int)GrammarSymbol::Associativity::RIGHT);
	  }
    }
    
    /// Try to solve a reduce/reduce conflict
    constexpr void dealWithReduceReduceConflict(GrammarTransition& transition,
						const GrammarSymbol& symbol,
						const size_t& iProduction)
    {
      const GrammarProduction& production=productions[iProduction];
      const size_t productionPrecedence=production.precedence(symbols);
      
      if(const size_t& transitionPrecedence=productions[transition.iStateOrProduction].precedence(symbols);productionPrecedence==0 or transitionPrecedence==0 or
	 productionPrecedence==transitionPrecedence)
	errorEmitter((std::string("reduce/reduce conflict for '")+std::string(symbols[production.iLhs].name)+"' on '"+std::string(symbol.name)+
		      "' ought to transition: "+describe(transition)+"\nproduction precedence: "+std::to_string(productionPrecedence)+" transition precedence: "+std::to_string(transitionPrecedence)).c_str());
      else
	if(productionPrecedence>transitionPrecedence)
	  {
	    diagnostic("overriding reduce: ",describe(transition));
	    transition.iStateOrProduction=iProduction;
	    diagnostic(" into reduce: ",describe(transition));
	  }
	else
	  diagnostic("leaving already esisting transition ",describe(transition)," given that the production has precedence ",productionPrecedence," lesser than the transition ",transitionPrecedence);
    }
    
    /// Generate reduce or shift transitions
    constexpr void generateTransitions()
    {
      diagnostic("-----------------------------------\n");
      
      for(size_t iState=0;iState<stateItems.size();iState++)
	{
	  bool stateDescribed=0;
	  GrammarState& state=stateItems[iState];
	  
	  for(size_t iIItem=0;iIItem<stateItems[iState].iItems.size();iIItem++)
	    {
	      bool itemDescribed=0;
	      const size_t iItem=stateItems[iState].iItems[iIItem];
	      const GrammarItem& item=items[iItem];
	      const size_t& iProduction=item.iProduction;
	      const GrammarProduction& production=productions[iProduction];
	      
	      if(item.position>=production.iRhsList.size())
		for(size_t iSymbol=0;iSymbol<symbols.size();iSymbol++)
		  {
		    const GrammarSymbol& symbol=symbols[iSymbol];
		    
		    if(lookaheads[iItem].symbolIs.get(iSymbol))
		      {
			if(not stateDescribed)
			  {
			    diagnostic("State: \n",describe(state));
			    stateDescribed=true;
			  }
			
			if(not itemDescribed)
			  {
			    diagnostic("   in item ",describe(item),"\n     reduces:\n");
			    itemDescribed=true;
			  }
			
			diagnostic("      at symbol ",symbols[iSymbol].name,"\n");
			
			/// Position of the transition in the state
			size_t iTransition=0;
			std::vector<GrammarTransition>& transitions=stateTransitions[iState];
			while(iTransition<transitions.size() and transitions[iTransition].iSymbol!=iSymbol)
			  iTransition++;
			
			if(iTransition==transitions.size())
			  insertReduceTransition(transitions,iSymbol,iProduction);
			else
			  {
			    diagnostic("!!!!panic! state\n",describe(state)," has already transition:\n",describe(transitions[iTransition])," for symbol \'",symbol.name,"\'\n");
			    
			    if(GrammarTransition& transition=transitions[iTransition];transition.type==GrammarTransition::Type::SHIFT)
			      dealWithShiftReduceConflict(transition,symbol,iProduction);
			    else
			      dealWithReduceReduceConflict(transition,symbol,iProduction);
			  }
		      }
		  }
	    }
	}
    }
    
    /// Generate the regex parser
    constexpr void generateRegexParser()
    {
      std::vector<RegexToken> tokens=whitespaceTokens;
      
      for(size_t iSymbol=0;iSymbol<symbols.size();iSymbol++)
	if(symbols[iSymbol].type==GrammarSymbol::Type::TERMINAL_SYMBOL)
	  tokens.emplace_back(symbols[iSymbol].name,iSymbol);
      
      diagnostic("List of TERMINAL and whitespaces regex recognized by the grammar:\n");
      for(const auto& [name,iToken] : tokens)
	diagnostic("   ",name," -> ",iToken,"\n");
      
      regexParser=createParserFromRegex(tokens);
    }
    
    constexpr Grammar(const std::string_view& str) :
      currentPrecedence(0)
    {
      addGenericSymbols();
      parseTheGrammar(str);
      checkTheGrammar();
      grammarOptimize();
      
      calculateFirsts();
      calculateFollows();
      setPrecedence();
      preComputeGotoStates();
      generateStates();
      
      generateSpontaneousLookahead();
      generateGotoItems();
      propagateLookaheads();
      generateTransitions();
      
      generateRegexParser();
    }
    
    /// Gets the parameters needed to build the constexpr grammar
    constexpr GrammarSpecs getSizes() const
    {
      return
	{.nSymbols=symbols.size(),
	 .productionPars{.nEntries=reduce(productions,
					  [](const GrammarProduction& p,
					     std::optional<size_t> x=std::nullopt)
					  {
					    if(not x)
					      x=0;
					    
					    return *x+p.iRhsList.size()+1;
					  }),
	   .nRows=productions.size()},
	 .nItems=items.size(),
	 .stateItemsPars{.nEntries=reduce(stateItems,
					  [](const GrammarState& s,
					     std::optional<size_t> x=std::nullopt)
					  {
					    if(not x)
					      x=0;
					    
					    return *x+s.iItems.size();
					  }),
	   .nRows=stateItems.size()},
	 .stateTransitionsPars{.nEntries=vectorOfVectorsTotalEntries(stateTransitions),
			       .nRows=stateTransitions.size()},
	 .regexMachinePars=regexParser.getSizes()};
    }
  };
  
  /// Grammar in fixed size tables
  template <GrammarSpecs Specs>
  struct ConstexprGrammar :
    BaseGrammar<ConstexprGrammar<Specs>>
  {
    /// Symbols accepted by the grammar
    std::array<BaseGrammarSymbol,Specs.nSymbols> symbols;
    
    /// Index of the symbols representing a production, for each state
    Stack2DVector<size_t,Specs.productionPars> productionsData;
    
    /// Items representing the states, defined in term of index of production and position
    std::array<GrammarItem,Specs.nItems> items;
    
    /// Index of the items representing a state, for each state
    Stack2DVector<size_t,Specs.stateItemsPars> stateIItemsData;
    
    /// Transitions for each state, defined in terms of the symbol, the
    /// type (shift/reduce), and target state (if shift) or the production to be
    /// applied (if reduce)
    Stack2DVector<GrammarTransition,Specs.stateTransitionsPars> stateTransitionsData;
    
    ConstexprRegexParser<Specs.regexMachinePars> regexParser;
    
    static_assert(Specs.stateTransitionsPars.nRows==Specs.stateItemsPars.nRows,"number of rows for stateTransitions and stateItems do not match");
    
    /// Returns the number of states
    static constexpr size_t nStates()
    {
      return Specs.stateTransitionsPars.nRows;
    }
    
    /// Accessor to a production
    struct ProductionRef
    {
      /// Original grammar
      const ConstexprGrammar* g;
      
      /// Index of the production
      const size_t iProduction;
      
      /// Number of rhs symbols
      constexpr const size_t nRhs() const
      {
	return g->productionsData.rowSize(iProduction)-1;
      }
      
      /// Index of the lhs symbol
      constexpr const size_t& iLhs() const
      {
	return g->productionsData(iProduction,0);
      }
      
      /// Index of the iiRhs-th rhs symbol
      constexpr const size_t& iRhs(const size_t& iiRhs) const
      {
	return g->productionsData(iProduction,iiRhs+1);
      }
      
      /// Describes the production
      constexpr std::string describe() const
      {
	std::string out=(std::string)g->symbols[iLhs()].name+": ";
	
	for(size_t iiRhs=0;iiRhs<nRhs();iiRhs++)
	  out+=(std::string)g->symbols[iRhs(iiRhs)].name+" ";
	
	return out;
      }
    };
    
    /// Returns a reference to a production
    ProductionRef production(const size_t& iProduction) const
    {
      return {this,iProduction};
    }
    
    /// Reference to an item
    struct ItemRef
    {
      /// Underlying grammar
      const ConstexprGrammar* g;
      
      /// Index of the item
      const size_t iItem;
      
      /// Describes the item
      constexpr std::string describe()
      {
	/// Item referred
	const auto& item=g->items[iItem];
	
	/// Reference toi the production
	const ProductionRef p=g->production(item.iProduction);
	
	/// Resulting string
	std::string out=(std::string)g->symbols[p.iLhs()].name+": ";
	
	for(size_t iIRhs=0,max=p.nRhs();iIRhs<=max;iIRhs++)
	  {
	    if(iIRhs==item.position)
	      out+=" . ";
	    
	    if(iIRhs<max)
	      {
		out+=" ";
		out+=g->symbols[p.iRhs(iIRhs)].name;
	      }
	  }
	
	return out;
      }
    };
    
    /// Gets a reference to the iItem item of the grammar
    constexpr ItemRef item(const size_t& iItem) const
    {
      return {this,iItem};
    }
    
    /// Reference to a state
    struct StateRef
    {
      /// Underlying grammar
      const ConstexprGrammar* g;
      
      /// Index of the state
      const size_t iState;
      
      /// Gets the number of item
      constexpr const size_t nItems() const
      {
	return g->stateIItemsData.rowSize(iState);
      }
      
      /// Returns the index of the iiItem-th item
      constexpr const size_t& iItem(const size_t& iiItem) const
      {
	return g->stateIItemsData(iState,iiItem);
      }
      
      /// Returns a reference to the iiItem-th item
      constexpr ItemRef item(const size_t& iiItem) const
      {
	return g->item(iItem(iiItem));
      }
      
      /// Gets the number of item
      constexpr const size_t nTransitions() const
      {
	return g->stateTransitionsData.rowSize(iState);
      }
      
      /// Returns a reference to the iTransition-th transition
      constexpr const GrammarTransition& transition(const size_t& iTransition) const
      {
	return g->stateTransitionsData(iState,iTransition);
      }
      
      /// Describes the state
      constexpr std::string describe(const std::string& pref="") const
      {
	/// Returned string
	std::string out;
	
	for(size_t iiItem=0;iiItem<nItems();iiItem++)
	  out+=pref+"| "+item(iiItem).describe()+"\n";
	
	out+=pref+"\n"+pref+"accepting:\n";
	
	for(size_t iTransition=0;iTransition<nTransitions();iTransition++)
	  {
	    const GrammarTransition& t=transition(iTransition);
	    out+=pref+"   symbol \""+(std::string)g->symbols[t.iSymbol].name+" ";
	    if(t.type==GrammarTransition::Type::SHIFT)
	      out+=" SHIFTING to state #";
	    else
	      out+=" REDUCING with production #";
	    out+=std::to_string(t.iStateOrProduction)+"\n";
	  }
	
	return out;
      }
    };
    
    /// Returns a reference to the state
    constexpr StateRef state(const size_t& iState) const
    {
      return {this,iState};
    }
    
    /// Create from dynamic-sized grammar
    constexpr ConstexprGrammar(const Grammar& oth)
    {
      for(size_t iSymbol=0;iSymbol<Specs.nSymbols;iSymbol++)
	this->symbols[iSymbol]=(const BaseGrammarSymbol&)oth.symbols[iSymbol];
      
      productionsData.fillWith([&oth](const size_t& iProduction)
      {
	const GrammarProduction& p=oth.productions[iProduction];
	
	std::vector<size_t> res(p.iRhsList.size()+1);
	res[0]=p.iLhs;
	for(size_t iiRhs=0;iiRhs<p.iRhsList.size();iiRhs++)
	  res[iiRhs+1]=p.iRhsList[iiRhs];
	
	return res;
      });
      
      for(size_t iItem=0;iItem<Specs.nItems;iItem++)
	this->items[iItem]=oth.items[iItem];
      
      stateIItemsData.fillWith([&oth](const size_t& iState)->const std::vector<size_t>&{return oth.stateItems[iState].iItems;});
      
      stateTransitionsData.fillWith([&oth](const size_t& iState)->const std::vector<GrammarTransition>&{return oth.stateTransitions[iState];});
      
      regexParser=oth.regexParser;
    }
  };
}

namespace pp
{
  using namespace internal;
  
  /// Create grammar from string
  template <GrammarSpecs GS=GrammarSpecs{}>
  constexpr auto createGrammar(const std::string_view& str)
  {
    /// Creates the grammar
    Grammar grammar=str;
    
    if constexpr(GS.isNull())
      return grammar;
    else
      return ConstexprGrammar<GS>(grammar);
  }
  
  /// Estimates the grammar size
  constexpr GrammarSpecs estimateGrammarSize(const std::string_view& str)
  {
    return createGrammar(str).getSizes();
  }
  
  /// Create grammar from string provider, this way constexprness of the string survives
  constexpr auto createConstexprGrammar(const auto strProvider)
  {
    constexpr std::string_view str=strProvider();
    
    return createGrammar<estimateGrammarSize(str)>(str);
  } 
}
