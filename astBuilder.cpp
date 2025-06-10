#include <cmath>
#include <memory>
#include <parsePact.hpp>

#include <functional>
#include <set>
#include <map>
#include <unordered_map>
#include <variant>

using namespace pp::internal;

template <typename Op1,
	  typename Op2>
concept Producible=
requires(Op1 op1,
	 Op2 op2)
{
  op1*op2;
};

template <typename Op1,
	  typename Op2>
concept Diffable=
requires(Op1 op1,
	 Op2 op2)
{
  op1-op2;
};

template <typename Op1,
	  typename Op2>
concept Summable=
requires(Op1 op1,
	 Op2 op2)
{
  op1+op2;
};

template <typename Op>
concept Uplussable=
requires(Op op)
{
  +op;
};

template <typename Op>
concept Uminusable=
requires(Op op)
{
  -op;
};

template <typename Op>
concept Streamable=
requires(Op op)
{
  std::cout<<op;
};

/////////////////////////////////////////////////////////////////

struct SymNode;

struct ValueNode;

template <typename T>
struct UnOpNode;

struct Uplus
{
  template <typename A>
  static auto eval(const A& a) -> decltype(+a)
  {
    return +a;
  }
};

struct Uminus
{
  template <typename A>
  static auto eval(const A& a) -> decltype(-a)
  {
    return -a;
  }
};

using UplusNode=
  UnOpNode<Uplus>;

using UminusNode=
  UnOpNode<Uminus>;

/////////////////////////////////////////////////////////////////

template <typename T>
struct BinOpNode;

struct Sum
{
  template <typename A,
	    typename B>
  static auto eval(const A& a,
		   const B& b) -> decltype(a+b)
  {
    return a+b;
  }
};

struct Prod
{
  template <typename A,
	    typename B>
  static auto eval(const A& a,
		   const B& b) -> decltype(a*b)
  {
    return a*b;
  }
};

using SumNode=
  BinOpNode<Sum>;

using ProdNode=
  BinOpNode<Prod>;

struct AssignNode;

using ASTNode=
  std::variant<SymNode,
	       UplusNode,
	       UminusNode,
	       SumNode,
	       ProdNode,
	       ValueNode,
	       AssignNode>;

using Value=
  std::variant<std::monostate,std::string,int,double>;

struct AssignNode
{
  std::string name;
  
  std::unique_ptr<ASTNode> rhs;
};

struct SymNode
{
  std::string name;
};

struct ValueNode
{
  Value value;
};

template <typename T>
struct UnOpNode
{
  std::unique_ptr<ASTNode> op;
};

template <typename T>
struct BinOpNode
{
  std::unique_ptr<ASTNode> op1;
  
  std::unique_ptr<ASTNode> op2;
};

std::unordered_map<std::string,Value> vars;

struct Evaluator
{
  Value operator()(const ValueNode& valueNode)
  {
    return valueNode.value;
  }
  
  Value operator()(const AssignNode& assignNode)
  {
    vars[assignNode.name]=std::visit(*this,*assignNode.rhs);
    
    return vars[assignNode.name];
  }
  
  Value operator()(const SymNode& symNode)
  {
    return vars[symNode.name];
  }
  
  template <typename T>
  Value operator()(const UnOpNode<T>& unOpNode)
  {
    const auto r=
      std::visit(*this,*unOpNode.op);
    
    return std::visit([](const auto& r) -> Value
		      
    {
      if constexpr(requires {T::eval(r);})
	return T::eval(r);
      else
	{
	  errorEmitter("un-unopable ",typeid(T).name()," with type ",typeid(r).name());
	  
	  return std::monostate{};
	}
    },r);
  }
  
  template <typename T>
  Value operator()(const BinOpNode<T>& binOpNode)
  {
    const auto r1=
      std::visit(*this,*binOpNode.op1);
    
    const auto r2=
      std::visit(*this,*binOpNode.op2);
      
    return std::visit([](const auto& r1,
			 const auto& r2) -> Value
    {
      if constexpr(requires {T::eval(r1,r2);})
	return T::eval(r1,r2);
      else
	{
	  errorEmitter("uncombinable ",typeid(T).name()," with types ",typeid(r1).name()," ",typeid(r2).name());
	  
	  return std::monostate{};
	}
    },r1,r2);
  }
};

template <typename T>
T& fetch(std::vector<ASTNode>& subNodes,
	       const size_t& i)
{
  if(const size_t n=subNodes.size();n<i)
    errorEmitter(n," nodes received, aksed for node #",i);
  
  T* s=
    std::get_if<T>(&subNodes[i]);
  
  if(not s)
    errorEmitter("subNode ",i," is not of the required type ",typeid(T).name());
  
  return *s;
}


int main()
{
  ASTNode a=AssignNode{.name="A",
    .rhs=std::make_unique<ASTNode>(SumNode{.op1=std::make_unique<ASTNode>(ValueNode(5)),
					   .op2=std::make_unique<ASTNode>(ValueNode(-3))})};
  
  Evaluator ev;
  std::visit(ev,a);
  
  for(auto& [t,v] : vars)
    std::visit([t](const auto& v)
    {
      if constexpr(not std::is_same_v<std::decay_t<decltype(v)>,std::monostate>)
	diagnostic(t," ",v,"\n");
    },v);
  
  // using namespace pp;
  // using namespace pp::internal;
  
  [[maybe_unused]]
  constexpr const char nissaGrammar[]=
    "nissa {"
    "   %whitespace \" *\";"
    "   %right \"=\";"
    "   %left \"\\+\";"
    "   %left \"\\-\";"
    "   %left \"\\*\";"
    "   document: document statement"
    "           | statement"
    "           ;"
    "   statement: exprStatement \";\" "
    "            | forStatement"
    "            ;"
    "   forStatement: \"for\" \"\\(\" exprStatement \";\" exprStatement \";\" exprStatement \"\\)\" statement"
    "               ;"
    "   exprStatement: lhs \"=\" exprStatement [assign] "
    "                | lhs \"=\" expr [assign] "
    "                ;"
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
    "   id: \"[a-zA-Z_][a-zA-Z0-9_]*\" [return]"
    "     ;"
    "   str: \"\\\"[^\\\"]*\\\"\" [storeString]"
    "      ;"
    "   int: \"[0-9]+\" [convToInt]"
    "      ;"
    "}";
  
  const auto nissa=createGrammar(nissaGrammar);
  
  //constexpr auto nissa=createGrammar<nissaGrammar>();
  
  const auto pt=
    createParseTree(nissa,
		    "A=-1*+3; B=-5*(2-A); C=B*A; "
		    "D=\"ciao\"; for(E=F=D+\"dai\";E=F;E=E) G=E;");
  
  diagnostic("====================================\n");
  
  std::vector<ASTNode> stack;
  std::map<std::string,std::function<ASTNode(std::vector<ASTNode>&)>> actions;
  
  actions["return"]=
    [](std::vector<ASTNode>& subNodes)->ASTNode
    {
      if(subNodes.size()!=1)
	errorEmitter("expecting only 1 symbol");
      
      return std::move(subNodes[0]);
    };
  
  actions["convToInt"]=
    [](std::vector<ASTNode>& subNodes)->ASTNode
    {
      ValueNode& v=fetch<ValueNode>(subNodes,0);
      
      std::string* s=std::get_if<std::string>(&v.value);
      
      if(s==nullptr)
	errorEmitter("expecting std::string, obtained other type");
      
      return ValueNode{atoi(s->c_str())};
    };
  
  actions["uplus"]=
    [](std::vector<ASTNode>& subNodes)->ASTNode
  {
    if(subNodes.size()!=2)
      errorEmitter("expecting 2 symbols");
    
    return UplusNode{.op=std::make_unique<ASTNode>(std::move(subNodes[1]))};
  };
  
  actions["uminus"]=
    [](std::vector<ASTNode>& subNodes)->ASTNode
    {
      if(subNodes.size()!=2)
	errorEmitter("expecting 2 symbols");
      
      return UminusNode{.op=std::make_unique<ASTNode>(std::move(subNodes[1]))};
    };
  
  actions["product"]=
    [](std::vector<ASTNode>& subNodes)->ASTNode
    {
      if(subNodes.size()!=3)
	errorEmitter("expecting precisely 3 symbols");
      
      return ProdNode{.op1=std::make_unique<ASTNode>(std::move(subNodes[0])),
		      .op2=std::make_unique<ASTNode>(std::move(subNodes[2]))};
    };
  
  actions["assign"]=
    [](std::vector<ASTNode>& subNodes)->ASTNode
  {
    if(subNodes.size()!=3)
      errorEmitter("expecting precisely 3 symbols");
    
    ValueNode& lhs=
      fetch<ValueNode>(subNodes,0);
    
    std::string* n=std::get_if<std::string>(&lhs.value);
    
    if(;n==nullptr)
      errorEmitter("lhs not of string type");
    
    return AssignNode{.name=*n,.rhs=std::make_unique<ASTNode>(std::move(subNodes[2]))};
  };
  
  for(const auto& [txt,n] : pt)
    if(const std::string_view tmp{txt.first,txt.second};n==0)
      {
	diagnostic("Push string: ",tmp,"\n");
	stack.push_back(ValueNode(std::string(tmp)));
      }
    else
      {
	diagnostic("Reducing ",n," symbols from stack of size ",stack.size(),"\n");
	
	if(tmp=="")
	  {
	    diagnostic(" (no action)\n");
	    stack.erase(stack.end()-n,stack.end());
	    stack.push_back(ValueNode(std::monostate{}));
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
		std::vector<ASTNode> subNodes{std::make_move_iterator(stack.end()-n),std::make_move_iterator(stack.end())};
		stack.erase(stack.end()-n,stack.end());
		diagnostic(" pushing returned symbol to stack\n");
		stack.push_back(af->second(subNodes));
	      }
	  }
	
	diagnostic(" new stack size: ",stack.size(),"\n");
      }
  
  // for(const auto& [name,v] : varTable)
  //   std::visit([&name](const auto& v)
  //   {
  //     if constexpr(Streamable<decltype(v)>)
  // 	diagnostic(name,"=",v,"\n");
  //     else
  // 	errorEmitter("Unprintable type ",typeid(decltype(v)).name());
  //   },v);
  
  return 0;
}
