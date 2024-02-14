# A compile-time parser generator

The aim of this project is to provide a parser generator which produce
a lalr parser at compile time. This will allow to parse expressions at
compile time. 

The project is written in plain c++-20, has no dependency, and the
aim is to put everything in a single file. 

The parser is generated in two steps. At first step, the parser is
built in a non-constant sized structure, which is used to estimate the
required table sizes. On this basis, the needed fixed-size structure
parser is provided, and used to store the parser, which is built
again. The procedure is illustrated in the following example:

```c++
template <size_t N=0>
constexpr auto get(const std::string_view& str)
{
  if constexpr(N==0)
    return str.size();
  else
    {
      std::array<char,N> res;
      std::copy(str.begin(),str.end(),res.begin());
      return res;
    }
}

void test()
{
  constexpr char str[]="ciao!";
  constexpr auto strArr=get<get(str)>(str);
  static_assert(strArr[0]=='c');
}
```

Status: regex parser is so far.
