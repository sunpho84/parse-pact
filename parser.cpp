#include <cstdio>
#include <utility>

/////////////////////////////////////////////////////////////////

/// String container
template <size_t N>
struct FixedLengthString
{
  /// Storage for the char sequence
  char str[N];
  
  /// Length
  static constexpr size_t length=N;
  
  /// Gets reference to the i-th char
  constexpr char& operator[](const size_t& i)
  {
    return str[i];
  }
  
  /// Gets const reference to the i-th char
  constexpr const char& operator[](const size_t& i) const
  {
    return str[i];
  }
  
  /// Default constructor
  constexpr FixedLengthString() :
    str{}
  {
  }
  
  /// Construct taking the beginning of a string, and at most N char
  constexpr FixedLengthString(const char* beg)
  {
    size_t i;
    
    for(i=0;i<N-1 and beg[i]!='\0';i++)
      str[i]=beg[i];
    
    while(i<N)
      str[i++]='\0';
  }
  
  /// Construct taking a list of string literals
  template <size_t...R>
  constexpr FixedLengthString(const char (&..._str)[R])
  {
    size_t i=0;
    
    for(auto [s,r]: {std::make_pair(_str,R)...})
      for(size_t j=0;j<r-1 and i<N-1;j++)
	if(s[j]!='\0')
	  str[i++]=s[j];
    
    while(i<N)
      str[i++]='\0';
  }
};

/// Literal operator to create a fixed time string
template <FixedLengthString FS>
constexpr auto operator""_fs()
{
  return FS;
}

/// Deduce guide for the length of a fixed-length string
template <size_t...R>
FixedLengthString(const char (&..._str)[R])->
  FixedLengthString<(R+...)-sizeof...(R)+1>;

/////////////////////////////////////////////////////////////////

int main()
{
  return 0;
}
