#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

/// Print to terminal if not evaluated at compile time
template <typename...Args>
constexpr void diagnostic(Args&&...args);

namespace Temptative
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
      for(size_t i=0;i<Temptative::nNestedActions;i++)
	std::cout<<"\t";
      ((std::cout<<args),...);
    }
}

/// Implements static polymorphysm via CRTP, while we wait for C++-23
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
    return Temptative::Action(descr,
			      [back=*this,
			       this]()
    {
       diagnostic("not accepted, putting back ref ",this->ref.begin(),"->",back.ref.begin(),"\n");
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
    const bool accepting=
      (not ref.empty()) and ref.starts_with(c);
    
    if(not ref.empty())
      diagnostic("parsing char: ",ref.front(),", ");
    diagnostic((accepting?"accepted":"not accepted"),", expected char: ",c,"\n");
    
    if(accepting)
      advance();
    
    return accepting;
  }
  
  /// Matches a possibly escaped char
  constexpr char matchPossiblyEscapedCharNotIn(const std::string_view& notIn)
  {
    if(const char c=matchCharNotIn(notIn))
      if(const char d=(c=='\\')?maybeEscape(matchAnyChar()):c)
	return d;
    
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
	  // diagnostic("matched ",str.front(),"\n");
	  
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

/// Base char range operations
template <typename T>
struct BaseCharRanges :
  StaticPolymorphic<T>
{
  /// Import the static polymorphysm cast
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
    
    diagnostic("  new range after inserting [",b,";",e,"): \n");
    for(const auto& [b,e] : ranges)
      diagnostic("    r: [",b,";",e,")\n");
    diagnostic("\n");
  }
  
  /// Loop on all ranges
  template <typename F>
  constexpr void onAllRanges(const F& f) const
  {
    for(const auto& [b,e] : ranges)
      diagnostic("    r: [",b,";",e,")\n");
    
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
      diagnostic(" [",b,";",e,")\n");
    diagnostic("negated range:\n");
    for(const auto& [b,e] : negatedRanges)
      diagnostic(" [",b,";",e,")\n");
    
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
    
    diagnostic("Considering [",b,";",e,")\n");
    
    /// Current range, at the beginning set at the first range delimiter
    auto cur=
      ranges.begin();
    
    // Find where to insert the range
    while(cur!=ranges.end() and cur->second<b)
      cur++;
    
    /// Index of the position where to insert, useful for debug
    const size_t i=
      std::distance(ranges.begin(),cur);
    
    // Insert the beginning of the range if not present
    if(cur==ranges.end() or cur->second<b)
      {
	diagnostic("Inserting [",b,";",e,") at ",i,"\n");
	ranges.insert(cur,{b,e});
      }
    else
      {
	if(cur->first>b)
	  {
	    diagnostic("range ",i," [",cur->first,",",cur->second,"%d) extended left to ");
	    cur->first=std::min(cur->first,b);
	    diagnostic("[",cur->first,",",cur->second,")\n");
	  }
	
	if(cur->second<e)
	  {
	    diagnostic("range ",i,"[",cur->first,",",cur->second,") extended right to ");
	    cur->second=e;
	    diagnostic("[",cur->first,",",cur->second,")\n");
	    while(cur+1!=ranges.end() and cur->second>=(cur+1)->first)
	      {
		cur->second=std::max(cur->second,(cur+1)->second);
		diagnostic("extended right to [",cur->first,",",cur->second,")\n");
		diagnostic("erasing [",(cur+1)->first,",",(cur+1)->second,")\n");
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
      /// Take whether the range is negated
      const bool negated=
	matchIn.matchChar('^');
	
      diagnostic("matched [\n");
      
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

/// Base functionality of the regex parser
template <typename T>
struct BaseRegexParser :
  StaticPolymorphic<T>
{
  /// Import the static polymorphysm cast
  using StaticPolymorphic<T>::self;
  
  /// Parse a string
  constexpr std::optional<size_t> parse(std::string_view v) const
  {
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
	    diagnostic("Ignored transition [",trans->beg,",",trans->end,")\n");
	    trans++;
	  }
	
	if(trans!=self().transitions.end() and trans->iDStateFrom==dState)
	  {
	    dState=trans->nextDState;
	    diagnostic("matched ",c," with trans ",trans->iDStateFrom," [",trans->beg,",",trans->end,"), going to dState ",dState,"\n");
	    v.remove_prefix(1);
	  }
	else
	  if(self().dStates[dState].accepting)
	    return {self().dStates[dState].iToken};
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
  
  /// Construct from parse tree
  constexpr DynamicRegexParser(RegexParserNode& parseTree)
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
	  diagnostic((i++==0)?"":",",n->id);
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
	  std::vector<int> recogTokens;
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
	  
	  diagnostic(" range [",b,",",e,") goes to state {");
	  for(size_t i=0;const auto& n : nextDState)
	    diagnostic((i++==0)?"":",",n->id);
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
	    printf("%s%zu",(i++==0)?"":",",n->id);
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
    return {.nDStates=dStates.size(),.nTranstitions=transitions.size()};
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
  std::array<RegexMachineTransition,Specs.nTranstitions> transitions;
  
  /// Create from dynamic-sized parser
  constexpr ConstexprRegexParser(const DynamicRegexParser& oth)
  {
    for(size_t i=0;i<Specs.nDStates;i++)
      this->dStates[i]=oth.dStates[i];
    for(size_t i=0;i<Specs.nTranstitions;i++)
      this->transitions[i]=oth.transitions[i];
  }
};

/// Create parser from regexp
template <RegexMachineSpecs RPS=RegexMachineSpecs{},
	  typename...T>
requires(std::is_same_v<char,T> and ...)
constexpr auto createParserFromRegex(const T*...str)
{
  /// Creates the parse tree
  std::optional<RegexParserNode> parseTree=
    parseTreeFromRegex(str...);
  
  if(not parseTree)
    errorEmitter("Unable to parse the regex");
  
  if constexpr(RPS.isNull())
    return DynamicRegexParser(*parseTree);
  else
    return ConstexprRegexParser<RPS>(*parseTree);
}

/// Create parser from regexp
template <typename...T>
requires(std::is_same_v<char,T> and ...)
constexpr auto estimateRegexParserSize(const T*...str)
{
  return createParserFromRegex(str...).getSizes();
}

/////////////////////////////////////////////////////////////////

struct GrammarSymbol;

struct GrammarProduction
{
  GrammarSymbol* lhs;
  
  std::vector<GrammarSymbol*> rhs;
  
  const GrammarSymbol* precedenceSymbol;
  
  std::string_view action;
};

struct GrammarSymbol
{
  std::string_view name;
  
  enum class Type{NULL_SYMBOL,TERMINAL_SYMBOL,NON_TERMINAL_SYMBOL,END_SYMBOL};
  
  Type type;
  
  enum class Associativity{NONE,LEFT,RIGHT};
  
  Associativity associativity;
  
  size_t precedence;
  
  GrammarSymbol* precedenceSymbol;
  
  bool referredAsPrecedenceSymbol;
  
  /// Id of the grammar symbol
  size_t id;
  
  /// Productions which reduce to this symbol
  std::vector<GrammarProduction*> productions;
};

struct RegexToken
{
  std::string_view str;
  
  size_t iSymbol;
};

struct Grammar
{
  std::string_view name;
  
  std::vector<GrammarSymbol> symbols;
  
  size_t iStartSymbol;
  
  size_t iEndSymbol;
  
  size_t iErrorSymbol;
  
  size_t iWhitespaceSymbol;
  
  size_t currentPrecedence;
  
  std::vector<GrammarProduction> productions;
  
  std::vector<RegexToken> whitespaceTokens;
  
  constexpr GrammarSymbol* insertOrFindSymbol(const std::string_view& name,
					      const GrammarSymbol::Type& type)
  {
    if(auto ref=
       std::ranges::find_if(symbols,
			    [&name](const GrammarSymbol& s)
			    {
			      return s.name==name;
			    });ref!=symbols.end())
      return &(*ref);
    else
      return &symbols.emplace_back(name,type);
  }
  
  constexpr GrammarSymbol* matchAndParseSymbol(Matching& m)
  {
    m.matchWhiteSpaceOrComments();
    
    if(m.matchStr("error"))
      return &symbols[iErrorSymbol];
    else if(auto l=m.matchLiteral();not l.empty())
      return insertOrFindSymbol(l,GrammarSymbol::Type::TERMINAL_SYMBOL);
    else if(auto r=m.matchRegex();not r.empty())
      return insertOrFindSymbol(r,GrammarSymbol::Type::TERMINAL_SYMBOL);
    else if(auto i=m.matchId();not i.empty())
      return insertOrFindSymbol(i,GrammarSymbol::Type::NON_TERMINAL_SYMBOL);
				
    return nullptr;
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
	    diagnostic("Matched symbol: ",std::quoted(m->name),"\n");
	    
	    m->associativity=currentAssociativity;
	    m->precedence=currentPrecedence;
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
	auto lhs=
	  insertOrFindSymbol(i,GrammarSymbol::Type::NON_TERMINAL_SYMBOL);
	diagnostic("Found lhs: ",lhs->name,"\n");
	
	// Add the first symbol found as a starting reduction
	if(productions.empty())
	  {
	    productions.emplace_back(&symbols[iStartSymbol],std::vector<GrammarSymbol*>{lhs},nullptr,std::string_view{});
	    symbols[iStartSymbol].productions.emplace_back(&productions.back());
	  }
	
	matchin.matchWhiteSpaceOrComments();
	if(matchin.matchChar(':'))
	  {
	    do
	      {
		/// Right hand side of the production
		std::vector<GrammarSymbol*> rhs;
		matchin.matchWhiteSpaceOrComments();
		while(GrammarSymbol* matchedSymbol=matchAndParseSymbol(matchin))
		  {
		    rhs.push_back(matchedSymbol);
		    matchin.matchWhiteSpaceOrComments();
		    diagnostic("Found rhs: ",matchedSymbol->name,"\n");
		  }
		
		GrammarSymbol* precedenceSymbol;
		if(matchin.matchStr("%precedence"))
		  {
		    if((precedenceSymbol=matchAndParseSymbol(matchin)))
		      precedenceSymbol->referredAsPrecedenceSymbol=true;
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
		    
		    diagnostic("matched action: ",std::quoted(action),"\n");
		    
		    matchin.matchWhiteSpaceOrComments();
		    if(not matchin.matchChar(']'))
		      errorEmitter("Expected end of action ']'");
		    
		    matchin.matchWhiteSpaceOrComments();
		  }
		
		productions.emplace_back(lhs,rhs,precedenceSymbol,action);
		lhs->productions.push_back(&productions.back());
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
  
  constexpr Grammar(const std::string_view& str) :
    currentPrecedence(0)
  {
    using enum GrammarSymbol::Type;
    
    // Adds generic symboles
    for(const auto& [name,type,i] :
	  {std::make_tuple(".start",NON_TERMINAL_SYMBOL,&iStartSymbol),
	   {".end",END_SYMBOL,&iEndSymbol},
	   {".error",NULL_SYMBOL,&iErrorSymbol},
	   {".whitespace",NULL_SYMBOL,&iWhitespaceSymbol}})
      {
	*i=symbols.size();
	symbols.emplace_back(name,type,GrammarSymbol::Associativity::NONE,0);
      }
    
    Matching match(str);
    
    match.matchWhiteSpaceOrComments();
    
    if(auto id=match.matchId();not id.empty())
      {
	name=id;
	
	diagnostic("Matched grammar: ",std::quoted(id),", skipped to ",match.ref.begin(),"\n");
	
	match.matchWhiteSpaceOrComments();
	
	if(match.matchChar('{'))
	  {
	    diagnostic("Matched {\n");
	    
	    while(matchAndParseAssociativityStatement(match) or
		  matchAndParseWhitespaceStatement(match) or
		  matchAndParseProductionStatement(match))
	      diagnostic("parsed\n");
	    
	    if(not match.matchChar('}'))
	      diagnostic("Unfinished grammar\n");
	    
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
    
    /////////////////////////////////////////////////////////////////
    
    /// Count of symbols usage
    std::vector<size_t> symbolsCount(symbols.size(),0);
    
    // Number the symbol and perform several checks
    for(size_t iSymbol=0;iSymbol<symbols.size();iSymbol++)
      {
	/// Take a reference to the symbol
	GrammarSymbol& s=symbols[iSymbol];
	
	// Number the symbol
	s.id=iSymbol;
	
	// Check that all symbols are referenced at least once and defined
	if(s.type==NON_TERMINAL_SYMBOL and s.productions.empty() and not s.referredAsPrecedenceSymbol)
	  errorEmitter("Undefined symbol");
	
	// Count the symbol usage
	for(const GrammarProduction& production : productions)
	  {
	    for(const GrammarSymbol* r : production.rhs)
	      symbolsCount[r->id]++;
	    //symbolsCount[production.lhs->id]++;
	    if(auto p=production.precedenceSymbol)
	      symbolsCount[p->id]++;
	  }
      }
    
    for(size_t iSymbol=0;iSymbol<symbols.size();iSymbol++)
      if(const std::array<size_t,4> filt{iStartSymbol,iEndSymbol,iErrorSymbol,iWhitespaceSymbol};
	 std::find(filt.begin(),filt.end(),iSymbol)!=filt.end())
	if(symbolsCount[iSymbol]==0)
	  errorEmitter("Unreferenced symbol");
  }
};
