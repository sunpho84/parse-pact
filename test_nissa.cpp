#include <cmath>
#include <parsePact.hpp>

#include <functional>
#include <set>
#include <map>
#include <variant>

using namespace pp;

using namespace std;

using namespace pp::internal;


template <CtString>
struct Action;

template <>
struct Action<"int">
{
  static constexpr int eval(const std::string_view& in)
  {
    int res=0;
    
    for(const char& c : in)
      {
	// if(c<'0' or c<'9')
	//   errorEmitter(c," not in the range [0-9]");
	res=res*10+c-'0';
      }
    
    return res;
  }
};

template <>
struct Action<"mul">
{
  static constexpr auto eval(const int& a,
			     const std::string_view& in,
			     const int& b)
  {
    return a*b;
  }
};

template <>
struct Action<"sum">
{
  static constexpr auto eval(const int& a,
			     const std::string_view& in,
			     const int& b)
  {
    return a+b;
  }
};

template <>
struct Action<"pow">
{
  static constexpr auto eval(const int& a,
			     const std::string_view& in,
			     const int& b)
  {
    int res=1;
    for(int i=0;i<b;i++)
      res*=a;
    
    return res;
  }
};

template <>
struct Action<"bracket">
{
  static constexpr const int& eval(const std::string_view& a,
			     const int& in,
			     const std::string_view& b)
  {
    return in;
  }
};

template <>
struct Action<"pp_maybeReturn">
{
  template <typename A>
  static constexpr decltype(auto) eval(A&& a)
  {
    return a;
  }
  
  static constexpr void eval()
  {
  }
};

template <FlattenedParseTreeNode...ParseTreeNodes>
struct Process;

template <>
struct Process<>
{
  static constexpr auto eval(std::nullptr_t)
  {
  }
};

template <FlattenedParseTreeNode Head,
	  FlattenedParseTreeNode...Tail>
struct Process<Head,
	       Tail...>
{
  template <typename Stack,
	    size_t...A,
	    size_t...B>
  static constexpr auto reduce(Stack&& stack,
			       const std::index_sequence<A...>&,
			       const std::index_sequence<B...>&)
  {
    if constexpr(not Head.txt().empty())
      {
	constexpr size_t txtSize=Head.txt().length()+1;
	constexpr CtString<txtSize> act=getCtString<Head.txtData>();
	
	using Res=
	  decltype(Action<act>::eval(std::get<B+sizeof...(A)>(stack)...));
	
	if constexpr(not std::is_same_v<Res,void>)
	    return Process<Tail...>::eval(std::get<A>(stack)...,Action<act>::eval(std::get<B+sizeof...(A)>(stack)...));
	else
	  {
	    Action<act>::eval(std::get<B+sizeof...(A)>(stack)...);
	    
	    return Process<Tail...>::eval(std::get<A>(stack)...,nullptr);
	  }
      }
    else
      if constexpr(sizeof...(Tail)==0)
	return Action<"pp_maybeReturn">::eval(std::get<B+sizeof...(A)>(stack)...);
      else
	return Process<Tail...>::eval(std::get<A>(stack)...,nullptr);
  }
  
  template <typename...Stack>
  static constexpr auto eval(Stack&&...stack)
  {
    if constexpr(Head.nSubNodes==0) error, now use isreduce
      {
	diagnostic("Shifting symbol \"",Head.txt(),"\n");
	
	return Process<Tail...>::eval(std::forward<Stack>(stack)...,Head.txt());
      }
    else
      return reduce(std::forward_as_tuple(std::forward<Stack>(stack)...),
		    std::make_index_sequence<sizeof...(Stack)-Head.nSubNodes>(),
		    std::make_index_sequence<Head.nSubNodes>());
  }
};

template <std::array A>
struct Processa
{
  static constexpr size_t N=A.size();
  
  template <size_t...I>
  static constexpr auto evala(const std::index_sequence<I...>&)
  {
    return Process<A[I]...>::eval();
  }
  
  static constexpr auto eval()
  {
    return evala(std::make_index_sequence<N>());
  }
};

using Var=
  std::variant<std::monostate,std::string,int,double>;

using Sym=
  std::variant<std::monostate,std::string,int,double>;

template <typename T>
const T& fetch(std::vector<Sym>& syms,
	       const size_t& i)
{
  if(const size_t n=syms.size();n<i)
    errorEmitter(n," symbols received, aksed symbol #",i);
  
  const T* s=
    std::get_if<T>(&syms[i]);
  
  if(not s)
    errorEmitter("symbol ",i," is not of the required type ",typeid(T).name());
  
  return *s;
}

int main()
{
  [[maybe_unused]]
  constexpr const char nissaGrammar[]=
    "nissa {"
    "   %whitespace \" *\";"
    "   %right \"=\";"
    "   %left \"\\+\";"
    "   %left \"\\-\";"
    "   %left \"\\*\";"
    "   document: document statement \";\" "
    "           | statement \";\" "
    "           | \"for\" \"\\(\" statement \";\" statement \";\" statement \"\\)\" statement \";\""
    "           ;"
    "   statement: lhs \"=\" statement [assign] "
    "            | lhs \"=\" expr [assign] "
    "            ;"
    "   lhs: id [return]"
    "      ;"
    "   expr: expr \"\\*\" expr [product]"
    "       | expr \"\\+\" expr [sum]"
    "       | expr \"\\-\" expr [sub]"
    "       | \"\\+\" expr [uplus]"
    "       | \"\\-\" expr [uminus]"
    "       | \"\\(\" expr \"\\)\" [bracket]"
    "       | id [rhsSub]"
    "       | str [return]"
    "       | int [return]"
    "       ;"
    "   id: \"[a-zA-Z_][a-zA-Z0-9_]*\" [getId]"
    "     ;"
    "   str: \"\\\"[^\\\"]*\\\"\" [storeString]"
    "      ;"
    "   int: \"[0-9]+\" [convToInt]"
    "      ;"
    "}";
  
  const auto g=createGrammar(nissaGrammar);
  
  constexpr auto nissa=createGrammar<nissaGrammar>();
  
  std::map<std::string,Var> varTable;
  
  std::vector<Sym> stack;
  
  std::map<std::string,std::function<Sym(std::vector<Sym>&)>> actions;
  actions["convToInt"]=
    [](std::vector<Sym>& syms)->Sym
  {
    return atoi(fetch<std::string>(syms,0).c_str());
  };
  
  actions["getId"]=
    [](std::vector<Sym>& syms)->Sym
  {
    return fetch<std::string>(syms,0);
  };
  
  actions["storeString"]=
    [](std::vector<Sym>& syms)->Sym
  {
    const std::string& tmp=
      fetch<std::string>(syms,0);
    
    return tmp.substr(1,tmp.length()-2);
  };
  
  actions["bracket"]=
    [](std::vector<Sym>& syms)->Sym
  {
    if(syms.size()!=3)
      errorEmitter("expecting exactly 3 symbols");
    
    return syms[1];
  };
  
  actions["return"]=
    [](std::vector<Sym>& syms)->Sym
  {
    if(syms.size()!=1)
      errorEmitter("expecting only 1 symbol");
    
    return syms[0];
  };
  
  actions["uplus"]=
    [](std::vector<Sym>& syms)->Sym
  {
    if(syms.size()!=2)
      errorEmitter("expecting 2 symbols");
    
    return std::visit([](const auto& op) -> Sym
    {
      if constexpr(Uplussable<decltype(op)>)
	return +op;
      else
	{
	  errorEmitter("unplussable types");
	  return std::monostate{};
	}
    },
      syms[1]);
    
    return {};
  };
  
  actions["uminus"]=
    [](std::vector<Sym>& syms)->Sym
  {
    if(syms.size()!=2)
      errorEmitter("expecting 2 symbols");
    
    return std::visit([](const auto& op) -> Sym
    {
      if constexpr(Uminusable<decltype(op)>)
	return -op;
      else
	{
	  errorEmitter("unplussable types");
	  return std::monostate{};
	}
    },
      syms[1]);
    
    return {};
  };
  
  actions["assign"]=
    [&varTable](std::vector<Sym>& syms)->Sym
  {
    if(syms.size()!=3)
      errorEmitter("expecting precisely 3 symbols");
    
    const Sym& rhs=
      syms[2];
    
    if(std::holds_alternative<std::monostate>(rhs))
      errorEmitter("while assigning, rhs has no type");
    
    Sym& lhs=
      varTable[fetch<std::string>(syms,0)];
    
    if(std::holds_alternative<std::monostate>(lhs) or lhs.index()==rhs.index())
      lhs=rhs;
    else
      errorEmitter("while assigning, lhs has a type with index different from rhs");
    
    return lhs;
  };
  
  actions["product"]=
    [](std::vector<Sym>& syms)->Sym
  {
    if(syms.size()!=3)
      errorEmitter("expecting precisely 3 symbols");
    
    return std::visit([](const auto& op1,
			 const auto& op2) -> Sym
    {
      if constexpr(Producible<decltype(op1),decltype(op2)>)
	return op1*op2;
      else
	{
	  errorEmitter("unproducible types");
	  return std::monostate{};
	}
    },
		      syms[0],
		      syms[2]);
    
    return {};
  };
  
  actions["sum"]=
    [](std::vector<Sym>& syms)->Sym
    {
    if(syms.size()!=3)
      errorEmitter("expecting precisely 3 symbols");
    
    return std::visit([](const auto& op1,
			 const auto& op2) -> Sym
    {
      if constexpr(Summable<decltype(op1),decltype(op2)>)
	return op1+op2;
      else
	{
	  errorEmitter("unsummable types");
	  return std::monostate{};
	}
    },
		      syms[0],
		      syms[2]);
    
    return {};
  };
  
  actions["sub"]=
    [](std::vector<Sym>& syms)->Sym
    {
    if(syms.size()!=3)
      errorEmitter("expecting precisely 3 symbols");
    
    return std::visit([](const auto& op1,
			 const auto& op2) -> Sym
    {
      if constexpr(Diffable<decltype(op1),decltype(op2)>)
	return op1-op2;
      else
	{
	  errorEmitter("unsubtractable types");
	  return std::monostate{};
	}
    },
		      syms[0],
		      syms[2]);
    
    return {};
  };
  
  actions["rhsSub"]=
    [&varTable](std::vector<Sym>& syms)->Sym
  {
    const std::string& name=
      fetch<std::string>(syms,0);
    
    const auto& v=
      varTable.find(name);
    
    if(v==varTable.end())
      errorEmitter("using uninitialized variable ",name);
    
    return v->second;
  };
  
  const auto pt=
    createParseTree(nissa,
		    "A=-1*+3; B=-5*(2-A); C=B*A; "
		    "D=\"ciao\"; E=F=D+\"dai\";");
  
  diagnostic("====================================\n");
  
  for(const auto& [txt,n] : pt)
    if(const std::string_view tmp{txt.first,txt.second};n==0)
      {
	diagnostic("Push string: ",tmp,"\n");
	stack.push_back((std::string)tmp);
      }
    else
      {
	diagnostic("Reducing ",n," symbols from stack of size ",stack.size(),"\n");
	
	if(tmp=="")
	  {
	    diagnostic(" (no action)\n");
	    stack.erase(stack.end()-n,stack.end());
	    stack.push_back(std::monostate{});
	  }
	else
	  {
	    diagnostic(" with action: \"",tmp,"\"\n");
	    
	    if(const auto af=
	       actions.find((std::string)tmp);
	       af==actions.end())
	      errorEmitter("action \"",tmp,"\" not registered");
	    else
	      {
		std::vector<Sym> syms{std::make_move_iterator(stack.end()-n),std::make_move_iterator(stack.end())};
		stack.erase(stack.end()-n,stack.end());
		diagnostic(" pushing returned symbol to stack\n");
		stack.push_back(af->second(syms));
	      }
	  }
	
	diagnostic(" new stack size: ",stack.size(),"\n");
      }
  
  for(const auto& [name,v] : varTable)
    std::visit([&name](const auto& v)
    {
      if constexpr(Streamable<decltype(v)>)
	diagnostic(name,"=",v,"\n");
      else
	errorEmitter("Unprintable type ",typeid(decltype(v)).name());
    },v);
  
  return 0;
}
