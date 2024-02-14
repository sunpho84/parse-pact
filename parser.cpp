#include <array>
#include <cctype>
#include <cstddef>

/// Returns the length of the string, excluding the terminator
constexpr size_t strLen(const char* str)
{
  size_t i=0;
  
  while(str[i]!='\0')
    i++;
  
  return i;
}

constexpr bool isEither(const char& c,
			const char* list)
{
  bool matched=false;
  
  size_t i=0;
  while(list[i]!='\0' and not matched)
    matched|=(c==list[i++]);
  
  return matched;
}

constexpr bool isTerminator(const char& c)
{
  return c=='\0';
}

constexpr const char spacesIds[]=" \n\t\f\r";

constexpr bool isSpace(const char& c)
{
  return isEither(c,spacesIds);
}

constexpr bool isNewLine(const char& c)
{
  return c=='\n';
}

constexpr bool isCarriageReturn(const char& c)
{
  return c=='\r';
}

struct Cursor
{
  /// Beginning of the string
  const char* const str;
  
  /// Length to be parsed, possibly smaller than the full length
  const size_t len;
  
  /// Beginning of the current line
  size_t lineBegPos;
  
  /// Number of the line
  size_t lineNo;
  
  /// Position in the string
  size_t pos;
  
  /// Construct from beginning and length
  constexpr Cursor(const char* str,
		   const size_t& len) :
    str(str),
    len(len),
    lineBegPos(0),
    lineNo(0),
    pos(0)
  {
  }
  
  constexpr const char& cur() const
  {
    return str[pos];
  }
  
  constexpr const char& operator()() const
  {
    return cur();
  }
  
  /// Construct taking a string and using up to the end
  constexpr Cursor(const char* str) :
    Cursor(str,strLen(str)+1)
  {
  }
  
  constexpr bool isTerminator() const
  {
    return ::isTerminator(cur());
  }
  
  constexpr bool isSpace() const
  {
    return ::isSpace(cur());
  }
  
  constexpr bool isNewLine() const
  {
    return ::isNewLine(cur());
  }
  
  constexpr bool isCarriageReturn() const
  {
    return ::isCarriageReturn(cur());
  }
  
  constexpr void advance()
  {
    if(pos<len)
      {
	if(isNewLine())
	  {
	    lineNo=0;
	    pos=0;
	  }
	else
	  pos++;
      }
  }
  
  constexpr void advance(const size_t& n)
  {
    for(size_t i=0;i<n;i++)
      advance();
  }
  
};

/// Parse the grammar
struct GrammarParser
{
  /// Result of the parsing
  struct ParseResult
  {
    bool validParsing;
    
    constexpr ParseResult() :
      validParsing(true)
    {
    }
  };
  
  constexpr bool matchWhiteSpace()
  {
    bool matched=false;
    
    while(pos!=len and isSpace(str[pos]))
      {
	matched=true;
	
	if(isNewLine(str[pos]))
	  advanceLineAndPos();
	else
	  advance();
      }
    
    return matched;
  }
  
  constexpr bool match(const char* what)
  {
    size_t posInWhat=0;
    
    while((not isTerminator(str[pos+posInWhat])) and (not isTerminator(what[posInWhat])) and str[pos+posInWhat]==what[posInWhat])
      posInWhat++;
    
    const bool matched=
      isTerminator(str[posInWhat]);
    
    if(matched)
      advance(posInWhat);
    
    return matched;
  }

  //metti str nascosta, rendi visibile tramite cursor(offset), metti advance a tenere il conto del numero di riga e posizione e carriagereturn, così puoi riaggiungere posinline, advance non avanza oltre len o \0
  
  constexpr void skipUpTo(const char& c)
  {
    while(not (isTerminator(str[pos]) or str[pos]==c))
      advance();
  }
  
  constexpr bool advanceIfMatch(const char& c)
  {
    if(pos==len)
      return false;
    else
      {
	const bool matched=
	  str[pos]==c;
	
	if(matched)
	  pos++;
	
	return matched;
      }
  }
  
  constexpr void advanceLineAndPos()
  {
    lineNo++;
    pos++;
    lineBegPos=pos;
  }
  
  constexpr bool skipIfMatchCarriageReturn()
  {
    const bool matched=
      isCarriageReturn(str[pos]);
    
    if(matched)
      advance(); ///Do not increase line number
    
    return matched;
  }
  
  constexpr bool skipIfMatchNewLine()
  {
    const bool matched=
      isNewLine(str[pos]);
    
    if(matched)
      {
	advanceLineAndPos();
	skipIfMatchCarriageReturn();
      }
    
    return matched;
  }
  
  constexpr bool matchLineComment()
  {
    const bool matched=
      match("//");
    
    if(matched)
      skipUpTo('\n');
    
    return matched;
  }
  
  constexpr bool matchBlockComment()
  {
    const bool matchedBegin=
      match("/*");
    
    if(matchedBegin)
      skipUpToEither("*\n");
    
    return matched;
  }
  
  constexpr bool matchWhiteSpaceAndComments()
  {
    bool matchedSomething;
    
    do
      {
	matchedSomething=false;
	matchedSomething|=matchWhiteSpace();
	matchedSomething|=matchLineComment();
	matchedSomething|=matchBlockComment();
      }
    while(matchedSomething);
    
    return true;
  }
  
  constexpr bool matchIdentifier()
  {
    matchWhitespaceAndComments();
  }
  
  /// Parse the string
  constexpr auto parse()
  {
    ParseResult result;
    
    bool& status=result.validParsing;
    
    if(status)
      result.validParsing=matchIdentifier();
      
  }
};

int main()
{
  GrammarParser parser("ciao");
  
  return 0;
}
