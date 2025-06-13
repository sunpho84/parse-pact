#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <memory>
#include <parsePact.hpp>

#include <functional>
#include <set>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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

struct Sub
{
  template <typename A,
	    typename B>
  static auto eval(const A& a,
		   const B& b) -> decltype(a-b)
  {
    return a-b;
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

using SubNode=
  BinOpNode<Sub>;

using ProdNode=
  BinOpNode<Prod>;

struct AssignNode;

struct ForNode;

struct IfNode;

struct ASTNodesNode;

using ASTNode=
  std::variant<ASTNodesNode,
	       ForNode,
	       IfNode,
	       SymNode,
	       UplusNode,
	       UminusNode,
	       SumNode,
	       SubNode,
	       ProdNode,
	       ValueNode,
	       AssignNode>;

struct ASTNodesNode
{
  std::vector<std::unique_ptr<ASTNode>> subNodes;
};

using Value=
  std::variant<std::monostate,std::string,int,double>;

struct AssignNode
{
  std::string name;
  
  std::unique_ptr<ASTNode> rhs;
};

struct ForNode
{
  std::vector<std::unique_ptr<ASTNode>> subNodes;
};

struct IfNode
{
  std::vector<std::unique_ptr<ASTNode>> subNodes;
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

using VarTable=
  std::unordered_map<std::string,Value>;

struct VarTables
{
  std::deque<VarTable> stack;
  
  void push()
  {
    stack.emplace_front();
  }
  
  void pop()
  {
    stack.pop_front();
  }
  
  std::optional<Value*> find(const std::string& name)
  {
    for(auto& leaf : stack)
      if(auto it=leaf.find(name);it!=leaf.end())
	return &it->second;
    
    return {};
  }
  
  Value& operator[](const std::string& name)
  {
    if(auto f=find(name))
      return **f;
    else
      return stack.front()[name];
  }
  
  void print()
  {
    size_t i=0;
    for(VarTable& varTable : stack)
      {
	diagnostic("----- ",i++," -----\n");
	for(const auto& [name,v] : varTable)
	  std::visit([&name](const auto& v)
	  {
	    if constexpr(Streamable<decltype(v)>)
	      diagnostic(name,"=",v,"\n");
	    else
	      errorEmitter("Unprintable type ",typeid(decltype(v)).name());
	  },v);
      }
  }
};

VarTables varTables;

struct Evaluator
{
  Value operator()(const ValueNode& valueNode)
  {
    return valueNode.value;
  }
  
  Value operator()(const AssignNode& assignNode)
  {
    Value& v=varTables[assignNode.name];
    
    return v=std::visit(*this,*assignNode.rhs);
    
    return v;
  }
  
  Value operator()(const IfNode& ifNode)
  {
    varTables.push();
    
    if(std::visit([](const auto& v)
	{
	  if constexpr(std::is_convertible_v<decltype(v),bool>)
	    return (bool)v;
	  else
	    errorEmitter("Cannot convert the type to bool");
	  
	  return false;
	},std::visit(*this,*ifNode.subNodes[0])))
      std::visit(*this,*ifNode.subNodes[1]);
    else
      std::visit(*this,*ifNode.subNodes[2]);
    
    varTables.pop();
    
    return std::monostate{};
  }
  
  Value operator()(const ForNode& forNode)
  {
    varTables.push();
    
    for(std::visit(*this,*forNode.subNodes[0]);
    	std::visit([](const auto& v)
	{
	  if constexpr(std::is_convertible_v<decltype(v),bool>)
	    return (bool)v;
	  else
	    errorEmitter("Cannot convert the type to bool");
	  
	  return false;
	},std::visit(*this,*forNode.subNodes[1]));
	std::visit(*this,*forNode.subNodes[2]))
      std::visit(*this,*forNode.subNodes[3]);
    
    varTables.pop();
    
    return std::monostate{};
  }
  
  Value operator()(const ASTNodesNode& astNodesNode)
  {
    varTables.push();
    
    for(const std::unique_ptr<ASTNode>& astNode : astNodesNode.subNodes)
      std::visit(*this,*astNode);
    
    if(varTables.stack.size()>1)
      varTables.pop();
    
    return std::monostate{};
  }
  
  Value operator()(const SymNode& symNode)
  {
    const auto v=
      varTables.find(symNode.name);
    
    if(not v.has_value())
      errorEmitter("using uninitialized variable ",symNode.name);
    
    return **v;
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
T& fetch(std::vector<std::unique_ptr<ASTNode>>& subNodes,
	       const size_t& i)
{
  if(const size_t n=subNodes.size();n<i)
    errorEmitter(n," nodes received, aksed for node #",i);
  
  T* s=
    std::get_if<T>(&*subNodes[i]);
  
  if(not s)
    errorEmitter("subNode ",i," is not of the required type ",typeid(T).name());
  
  return *s;
}

void c()
{
  const char cGrammar[]=
    "c {"
    "   %whitespace \"[ |\\n\\t]+\";"
    "   %none lowerThanElse;"
    "   %none \"else\";"
    "   %left \",\";"
    "   %right \"=\";"
    "   %right \"\\+=\";"
    "   %right \"-=\";"
    "   %right \"\\*=\";"
    "   %right \"/=\";"
    "   %left \"\\|\\|\";"
    "   %left \"&&\";"
    "   %left \"==\";"
    "   %left \"!=\";"
    "   %left \"<\";"
    "   %left \"<=\";"
    "   %left \">\";"
    "   %left \">=\";"
    "   %left \"\\+\";"
    "   %left \"-\";"
    "   %left \"\\*\";"
    "   %left \"/\";"
    "   %right \"!\";"
    "   %left \"--\";"
    "   %left \"\\+\\+\";"
    "   %none FUNCTION_CALL;"
    ""
    "   statement : expression_statement"
    "             | compound_statement"
    "             | forStatement"
    "             | ifStatement"
    "             | function_call compound_statement"
    "             ;"
    "   forStatement: \"for\" \"\\(\" forInit \";\" forCheck \";\" forIncr \"\\)\" statement [forStatement]"
    "               ;"
    "   forInit: expression [return]"
    "          |"
    "          ;"
    "   forCheck: expression [return]"
    "          |"
    "          ;"
    "   forIncr: expression [return]"
    "          |"
    "          ;"
    "   ifStatement: \"if\" \"\\(\" expression \"\\)\" statement %precedence lowerThanElse [ifStatement]"
    "              | \"if\" \"\\(\" expression \"\\)\" statement \"else\" statement [ifElseStatement]"
    "              ;"
    "    compound_statement : \"{\" statements \"}\""
    "                       ;"
    "    statements : "
    "               | statements statement"
    "               ;"
    "    expression_statement : expression \";\""
    "                           ;"
    "    expression : logical_or_expression"
    "               | unary_expression assignment_operator expression"
    "               ;"
    "    logical_or_expression : logical_and_expression"
    "                           | logical_or_expression \"\\|\\|\" logical_and_expression"
    "                           ;"
    "    logical_and_expression : equality_expression"
    "                            | logical_and_expression \"&&\" equality_expression"
    "                            ;"
    "    equality_expression : relational_expression"
    "                          | equality_expression \"==\" relational_expression"
    "                          | equality_expression \"!=\" relational_expression"
    "                          ;"
    "    relational_expression : additive_expression %precedence \"<=\""
    "                           | relational_expression \"<\" additive_expression"
    "                           | relational_expression \">\" additive_expression"
    "                           | relational_expression \"<=\" additive_expression"
    "                           | relational_expression \">=\" additive_expression"
    "                           ;"
    "    additive_expression : multiplicative_expression"
    "                          | additive_expression \"\\+\" multiplicative_expression"
    "                          | additive_expression \"-\" multiplicative_expression"
    "                          ;"
    "    multiplicative_expression : unary_expression"
    "                                | multiplicative_expression \"\\*\" unary_expression"
    "                                | multiplicative_expression \"/\" unary_expression"
    "                                | multiplicative_expression \"%\" unary_expression"
    "                                ;"
    "    unary_expression : postfix_expression"
    "                       | \"\\+\" unary_expression"
    "                       | \"-\" unary_expression"
    "                       | \"!\" unary_expression"
    "                       ;"
    "    postfix_expression : primary_expression"
    "                         | postfix_expression \"\\+\\+\""
    "                         | postfix_expression \"--\""
    "                         ;"
    "    argument_expressions : expression"
    "                         | argument_expressions \",\" expression"
    "                         ;"
    "    primary_expression : identifier"
    "                        | integer_constant"
    "                        | character_constant"
    "                        | floating_constant"
    "                        | string"
    "                        | \"\\(\" expression \"\\)\""
    "                        | function_call"
    "                        ;"
    "    function_call : identifier \"\\(\" argument_expressions \"\\)\" %precedence FUNCTION_CALL"
    "                  ;"
    "    assignment_operator : \"=\""
    "                         | \"\\*=\""
    "                         | \"/=\""
    "                         | \"\\+=\""
    "                         | \"-=\""
    "                         ;"
    "    identifier : \"[a-zA-Z_][a-zA-Z0-9_]*\""
    "              ;"
    "    integer_constant : \"[0-9]+\""
    "                     ;"
    "    character_constant : \"'[^']*'\""
    "                       ;"
    "    floating_constant : \"[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?\""
    "                      ;"
    "    string : \"\\\"[^\\\"]*\\\"\""
    "           ;"
    "}";
  
  const char simpleCGrammar[]=
    "c {"
    "   %whitespace \" +\";"
    ""
    "    expression : identifier [RETURN]"
    "               | \"\\(\" expression \"\\)\" [BRACKET]"
    "               | function_call [RETURN]"
    "               ;"
    "    function_call : identifier \"\\(\" argument_expressions \"\\)\"[CALL]"
    "                  ;"
    "    argument_expressions : [NO_EXPRESSION]"
    "                         |expression [FIRST_EXPRESSION]"
    "                         | argument_expressions \",\" expression [PUSH_EXPRESSION]"
    "                         ;"
    "    identifier : \"[a-zA-Z_][a-zA-Z0-9_]*\" [IDENTIFIER]"
    "              ;"
    "}";
  
  const auto c=createGrammar(cGrammar);
  
  diagnostic("Grammar info, nstates: ",c.states.size(),"\n");
  diagnostic("Lexer info, nDstates: ",c.regexMatcher.dStates.size(),"\n");
  
  std::vector<char> ext;
  if(const char* path="/home/francesco/trastulli/parse-pact/example";
     std::filesystem::exists(path)){
    
    const size_t exs=std::filesystem::file_size(path);
    ext.resize(exs+1);
    if(FILE* file=fopen(path,"r"))
      {
	if(const size_t n=fread(&ext[0],1,exs,file);n!=exs)
	  errorEmitter("expected ",exs," obtained ",n);
	ext[exs]='\0';
	fclose(file);
      }
    else
      errorEmitter("unable to read ",path);
  }
  else
    errorEmitter("file ",path," does not exists");
  
  auto pt=
    createParseTree(c,&ext[0]);
  
  for(const auto& [txt,isReduce,n] : pt)
    if(const std::string_view tmp{txt.first,txt.second};not isReduce)
	diagnostic("Push string: ",tmp,"\n");
    else
      diagnostic("Reducing ",n," symbols from stack with action ",tmp,"\n");
}

int main()
{
  c();

  return 0;
  
  // ASTNode a=AssignNode{.name="A",
  //   .rhs=std::make_unique<ASTNode>(SumNode{.op1=std::make_unique<ASTNode>(ValueNode(5)),
  // 					   .op2=std::make_unique<ASTNode>(ValueNode(-3))})};
  
  // Evaluator ev;
  // std::visit(ev,a);
  
  // using namespace pp;
  // using namespace pp::internal;
  
  // it will be convenient to keep track of this https://cs.wmich.edu/~gupta/teaching/cs4850/sumII06/The%20syntax%20of%20C%20in%20Backus-Naur%20form.htm
  
  [[maybe_unused]]
  constexpr const char nissaGrammar[]=
	      "nissa {"
	      "   %whitespace \" *\";"
	      "   %right \"=\";"
	      "   %left \"\\+\";"
	      "   %left \"\\-\";"
	      "   %left \"\\*\";"
	      "   %none lowerThanElse;"
	      "   %none \"else\";"
	      "   statements: statements statement [appendStatement]"
	      "             | [createStatements]"
	      "             ;"
	      "   statement: exprStatement [return]"
	      "            | forStatement [return]"
	      "            | ifStatement [return]"
	      "            | \"{\" statements \"}\" [return1]"
	      "            ;"
	      "   exprStatement: expr \";\" [exprStatement]"
	      "                | \";\""
	      "                ;"
	      "   forStatement: \"for\" \"\\(\" forInit \";\" forCheck \";\" forIncr \"\\)\" statement [forStatement]"
	      "               ;"
	      "   forInit: expr [return]"
	      "          |"
	      "          ;"
	      "   forCheck: expr [return]"
	      "          |"
	      "          ;"
	      "   forIncr: expr [return]"
	      "          |"
	      "          ;"
	      "   ifStatement: \"if\" \"\\(\" expr \"\\)\" statement %precedence lowerThanElse [ifStatement]"
	      "              | \"if\" \"\\(\" expr \"\\)\" statement \"else\" statement [ifElseStatement]"
	      "              ;"
	      "   lhs: id [return]"
	      "      ;"
	      "   expr: assignExpr [return]"
	      "       | expr \"\\*\" expr [product]"
	      "       | expr \"\\+\" expr [sum]"
	      "       | expr \"\\-\" expr [sub]"
	      "       | \"\\+\" expr [uplus]"
	      "       | \"\\-\" expr [uminus]"
	      "       | \"\\(\" expr \"\\)\" [bracket]"
	      "       | id [rhsSub]"
	      "       | str [return]"
	      "       | int [return]"
	      "       ;"
	      "   assignExpr: lhs \"=\" expr [assign] "
	      "             ;"
	      "   id: \"[a-zA-Z_][a-zA-Z0-9_]*\" [return]"
	      "     ;"
	      "   str: \"\\\"[^\\\"]*\\\"\" [storeString]"
	      "      ;"
	      "   int: \"[0-9]+\" [convToInt]"
	      "      ;"
	      "}";
  
  const auto nissa=createGrammar(nissaGrammar);
  
  //constexpr auto nissa=createGrammar<nissaGrammar>();
  
  auto pt=
    createParseTree(nissa,
		    "A=-1*+3; B=-5*(2-A); if(0)C=B*A; else T=11;"
		    "D=\"ciao\";"
		    "for(i=0;i-1;i=i+1) for(j=0;j-2;j=j+1){D=D+\" \"+D;}");
  
  std::string_view r("return");
  pt.back().txtData=std::make_pair(&*r.begin(),&*r.end());
  
  diagnostic("====================================\n");
  
  std::vector<std::unique_ptr<ASTNode>> stack;
  std::map<std::string,std::function<std::unique_ptr<ASTNode>(std::vector<std::unique_ptr<ASTNode>>&)>> actions;
  
  actions["createStatements"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=0)
	errorEmitter("expecting 0 symbols");
      
      return std::make_unique<ASTNode>(ASTNodesNode{});
    };
  
  actions["exprStatement"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=2)
	errorEmitter("expecting exactly 2 symbols");
      
      return std::move(subNodes[0]);
    };
  
  actions["forStatement"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=9)
	errorEmitter("expecting 9 symbols, obtained ",subNodes.size());
      
      std::unique_ptr<ASTNode> res=std::make_unique<ASTNode>(ForNode());
      for(const int& i : {2,4,6,8})
	std::get_if<ForNode>(&*res)->subNodes.emplace_back(std::move(subNodes[i]));
      
      return res;
    };
  
  actions["ifStatement"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=5)
	errorEmitter("expecting 5 symbols, obtained ",subNodes.size());
      
      std::unique_ptr<ASTNode> res=std::make_unique<ASTNode>(IfNode());
      for(const int& i : {2,4})
	std::get_if<IfNode>(&*res)->subNodes.emplace_back(std::move(subNodes[i]));
      
      std::get_if<IfNode>(&*res)->subNodes.emplace_back(std::make_unique<ASTNode>(ASTNodesNode()));
      
      return res;
    };
  
  actions["ifElseStatement"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=7)
	errorEmitter("expecting 7 symbols, obtained ",subNodes.size());
      
      std::unique_ptr<ASTNode> res=std::make_unique<ASTNode>(IfNode());
      for(const int& i : {2,4,6})
	std::get_if<IfNode>(&*res)->subNodes.emplace_back(std::move(subNodes[i]));
      
      return res;
    };
  
  actions["appendStatement"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=2)
	errorEmitter("expecting exactly 2 symbol");
      
      std::unique_ptr<ASTNode> res=std::move(subNodes[0]);
      
      ASTNodesNode* l=std::get_if<ASTNodesNode>(&*(res));
      
      if(l==nullptr)
	errorEmitter("first argument is not a list of statement");
      
      l->subNodes.emplace_back(std::move(subNodes[1]));
      
      return res;
    };
  
  actions["return"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=1)
	errorEmitter("expecting only 1 symbol");
      
      return std::move(subNodes[0]);
    };
  
  actions["return1"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()<2)
	errorEmitter("expecting only 2 symbols");
      
      return std::move(subNodes[1]);
    };
  
  actions["convToInt"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      ValueNode& v=fetch<ValueNode>(subNodes,0);
      
      std::string* s=std::get_if<std::string>(&v.value);
      
      if(s==nullptr)
	errorEmitter("expecting std::string, obtained other type");
      
      return std::make_unique<ASTNode>(ValueNode{atoi(s->c_str())});
    };
  
  actions["uplus"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
  {
    if(subNodes.size()!=2)
      errorEmitter("expecting 2 symbols");
    
    return std::make_unique<ASTNode>(UplusNode{.op=std::move(subNodes[1])});
  };
  
  actions["uminus"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=2)
	errorEmitter("expecting 2 symbols");
      
      return std::make_unique<ASTNode>(UminusNode{.op=std::move(subNodes[1])});
    };
  
  actions["sum"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=3)
	errorEmitter("expecting precisely 3 symbols");
      
      return std::make_unique<ASTNode>(SumNode{.op1=std::move(subNodes[0]),
					       .op2=std::move(subNodes[2])});
    };
  
  actions["sub"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=3)
	errorEmitter("expecting precisely 3 symbols");
      
      return std::make_unique<ASTNode>(SubNode{.op1=std::move(subNodes[0]),
					       .op2=std::move(subNodes[2])});
    };
  
  actions["product"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
    {
      if(subNodes.size()!=3)
	errorEmitter("expecting precisely 3 symbols");
      
      return std::make_unique<ASTNode>(ProdNode{.op1=std::move(subNodes[0]),
					       .op2=std::move(subNodes[2])});
    };
  
  actions["assign"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
  {
    if(subNodes.size()!=3)
      errorEmitter("expecting precisely 3 symbols");
    
    ValueNode& lhs=
      fetch<ValueNode>(subNodes,0);
    
    const std::string* n=
      std::get_if<std::string>(&lhs.value);
    
    if(n==nullptr)
      errorEmitter("lhs not of string type");
    
    return std::make_unique<ASTNode>(AssignNode{.name=*n,.rhs=std::move(subNodes[2])});
  };
  
  actions["bracket"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
  {
    if(subNodes.size()!=3)
      errorEmitter("expecting precisely 3 symbols");
    
    return std::move(subNodes[1]);
  };
  
  actions["storeString"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
  {
    if(subNodes.size()!=1)
      errorEmitter("expecting precisely 1 symbols");
      
    const ValueNode& nameNode=
      fetch<ValueNode>(subNodes,0);
    
    const std::string* name=
      std::get_if<std::string>(&nameNode.value);
    
    if(name==nullptr)
      errorEmitter("symbol containing the string not of string type");
    
    return std::make_unique<ASTNode>(ValueNode{.value=name->substr(1,name->length()-2)});
  };
  
  actions["rhsSub"]=
    [](std::vector<std::unique_ptr<ASTNode>>& subNodes)->std::unique_ptr<ASTNode>
  {
      if(subNodes.size()!=1)
	errorEmitter("expecting precisely 1 symbols");
      
    const ValueNode& nameNode=
      fetch<ValueNode>(subNodes,0);
    
    const std::string* name=
      std::get_if<std::string>(&nameNode.value);
    
    if(name==nullptr)
      errorEmitter("symbol containing the name of the expanding symbol not of string type");
    
    return std::make_unique<ASTNode>(SymNode{.name=*name});
  };
  
  for(const auto& [txt,isReduce,n] : pt)
    if(const std::string_view tmp{txt.first,txt.second};not isReduce)
      {
	diagnostic("Push string: ",tmp,"\n");
	stack.push_back(std::make_unique<ASTNode>(ValueNode(std::string(tmp))));
      }
    else
      {
	diagnostic("Reducing ",n," symbols from stack of size ",stack.size(),"\n");
	
	if(tmp=="")
	  {
	    diagnostic(" (no action)\n");
	    stack.erase(stack.end()-n,stack.end());
	    stack.push_back(std::make_unique<ASTNode>(ValueNode(std::monostate{})));
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
		std::vector<std::unique_ptr<ASTNode>> subNodes{std::make_move_iterator(stack.end()-n),std::make_move_iterator(stack.end())};
		stack.erase(stack.end()-n,stack.end());
		diagnostic(" pushing returned symbol to stack\n");
		stack.push_back(af->second(subNodes));
	      }
	  }
	
	diagnostic(" new stack size: ",stack.size(),"\n");
      }
  
  Evaluator e;
  
  std::visit(e,*stack[0]);
  
  diagnostic("FINALE\n");
  varTables.print();
  
  return 0;
}
