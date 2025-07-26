#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <parsePact.hpp>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

using namespace pp::internal;

template <typename T,
	  typename V>
T& unvariant(V& v)
{
  T* u=std::get_if<T>(&v);
  if(u==nullptr)
    errorEmitter("expecting ",typeid(T).name(),", obtained other type");
  
  return *u;
}

std::string variantInnerTypeName(const auto& v)
{
  return std::visit([](const auto& x)
  {
    return typeid(decltype(x)).name();
  },v);
}

template <typename...Ts>
struct Overload :
  Ts...
{
    using Ts::operator()...;
    
    template <typename T>
    void operator()(const T& arg) const
    {
      errorEmitter("Unhandled type in overload variant visitor: ",typeid(arg).name());
    }
};

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

struct IdNode;

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

struct Uref
{
  template <typename A>
  static A eval(const A& a)
  {
    errorEmitter("Not meant to be really evaluated... yet");
    
    return a;
  }
};

using UplusNode=
  UnOpNode<Uplus>;

using UminusNode=
  UnOpNode<Uminus>;

using UrefNode=
  UnOpNode<Uref>;

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

struct FuncDefNode;

struct FuncCallNode;

struct IfNode;

struct ASTNodesNode;

using ASTNode=
  std::variant<ASTNodesNode,
	       ForNode,
	       IfNode,
	       IdNode,
	       UplusNode,
	       UminusNode,
	       UrefNode,
	       SumNode,
	       SubNode,
	       ProdNode,
	       FuncDefNode,
	       FuncCallNode,
	       ValueNode,
	       AssignNode>;

using FunctionArgs=
  std::map<std::string,std::tuple<bool,std::shared_ptr<ASTNode>>>;

struct ASTNodesNode
{
  std::vector<std::shared_ptr<ASTNode>> subNodes;
};

struct Environment;

struct Function
{
  std::shared_ptr<FunctionArgs> args;
  
  std::shared_ptr<ASTNode> body;
  
  Environment* env;
};

using Value=
  std::variant<std::monostate,std::string,int,double,Function>;

struct AssignNode
{
  std::shared_ptr<ASTNode> lhs;
  
  std::shared_ptr<ASTNode> rhs;
};

struct ForNode
{
  std::vector<std::shared_ptr<ASTNode>> subNodes;
};

struct FuncDefNode
{
  std::string name;
  
  std::shared_ptr<FunctionArgs> args;
  
  std::shared_ptr<ASTNode> body;
};

struct FuncCallNode
{
  std::string name;
  
  std::vector<std::shared_ptr<ASTNode>> args;
};

struct IfNode
{
  std::vector<std::shared_ptr<ASTNode>> subNodes;
};

struct IdNode
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
  std::shared_ptr<ASTNode> op;
};

template <typename T>
struct BinOpNode
{
  std::shared_ptr<ASTNode> op1;
  
  std::shared_ptr<ASTNode> op2;
};

struct Environment
{
  Environment* parent;
  
  std::unordered_map<std::string,std::shared_ptr<Value>> varTable;
  
  std::shared_ptr<Value> find(const std::string& name)
  {
    if(auto it=varTable.find(name);it!=varTable.end())
      return it->second;
    else
      if(parent)
	return parent->find(name);
      else
	return {};
  }
  
  Value& operator[](const std::string& name)
  {
    if(auto f=find(name))
      return *f;
    else
      return *(varTable[name]=std::make_shared<Value>());
  }
  
  void print(const size_t& i=0)
  {
    diagnostic("----- ",i," -----\n");
    for(const auto& [name,v] : varTable)
      std::visit([&name](const auto& v)
      {
	if constexpr(Streamable<decltype(v)>)
	  diagnostic(name,"=",v,"\n");
	else
	  diagnostic(name," of unprintable type: ",typeid(decltype(v)).name(),"\n");
      },*v);
    
    if(parent)
      parent->print(i+1);
  }
  
  Environment(Environment* parent=nullptr) :
    parent(parent)
  {
  }
};

std::map<std::string,Function> functionsTable;

struct Evaluator
{
  Environment env;
  
  Value operator()(const ValueNode& valueNode)
  {
    return valueNode.value;
  }
  
  Value operator()(const FuncDefNode& funcDefNode)
  {
    const std::string& name=
      funcDefNode.name;
    
    if(env.find(name))
      errorEmitter("Redefining a function which has a name already defined");
    
    env[name]=Function{.args=funcDefNode.args,.body=funcDefNode.body,.env=&env};
    
    return std::monostate{};
  }
  
  Value operator()(const FuncCallNode& funcCallNode)
  {
    const std::string& fName=
      funcCallNode.name;
    
    if(auto nf=env.find(fName))
      if(Function* f=std::get_if<Function>(&*nf))
	{
	  Evaluator subev{&env};
	  for(const std::shared_ptr<ASTNode>& ap : funcCallNode.args)
	    {
	      AssignNode& a=
		unvariant<AssignNode>(*ap);
	      
	      const std::string& aName=
		unvariant<IdNode>(*a.lhs).name;
	      
	      if(auto it=f->args->find(aName);it==f->args->end())
		errorEmitter("trying to pass argument ",aName," not expected by the function ",fName);
	      else
		if(const bool& isRef=std::get<bool>(it->second))
		  if(IdNode* id=std::get_if<IdNode>(&*a.rhs))
		    if(std::shared_ptr<Value> eid=env.find(id->name))
		      {
			diagnostic("Getting par \"",aName,"\" by ref\n");
			subev.env.varTable[aName]=eid;
		      }
		    else
		      errorEmitter("undefined symbol \"",id->name,"\" when passing argument \"",aName,"\" to function \"",fName,"\"");
		  else
		    errorEmitter("argument \"",aName,"\" of function \"",fName,"\" expects an id as a parameter (pass by reference)");
		else
		  subev.env.varTable.try_emplace(aName,std::make_shared<Value>(std::visit(*this,*a.rhs)));
	    }
	  
	  // diagnostic("Calling function, specified arguments:\n");
	  // subev.env.print();
	  
	  // Put possible default pars
	  for(auto& [aName,pars] : *f->args)
	    if(not subev.env.varTable.contains(aName))
	      if(const std::shared_ptr<ASTNode>& optDef=std::get<1>(pars))
		  subev.env[aName]=std::visit(*this,*optDef);
	      else
		errorEmitter("parameter \"",aName,"\" with no default value unspecified when calling the function \"",fName,"\"");
	    else
	      {}
	  
	  return std::visit(subev,*f->body);
	}
      else
	errorEmitter("Variable ",fName," is of type ",variantInnerTypeName(*nf)," not a ",typeid(Function).name());
    else
      errorEmitter("unable to find function: \"",fName,"\"");
    
    return std::monostate{};
  }
  
  Value operator()(const AssignNode& assignNode)
  {
    IdNode* s=
      std::get_if<IdNode>(&*assignNode.lhs);
    
    if(s==nullptr)
      errorEmitter("lhs of assign node is not an identifier");
    
    Value& v=env[s->name];
    
    return v=std::visit(*this,*assignNode.rhs);
    
    return v;
  }
  
  Value operator()(const IfNode& ifNode)
  {
    Evaluator subev{&env};
    
    if(std::visit([](const auto& v)
	{
	  if constexpr(std::is_convertible_v<decltype(v),bool>)
	    return (bool)v;
	  else
	    errorEmitter("Cannot convert the type to bool");
	  
	  return false;
	},std::visit(subev,*ifNode.subNodes[0])))
      std::visit(subev,*ifNode.subNodes[1]);
    else
      std::visit(subev,*ifNode.subNodes[2]);
    
    return std::monostate{};
  }
  
  Value operator()(const ForNode& forNode)
  {
    Evaluator subev{&env};
    
    for(std::visit(*this,*forNode.subNodes[0]);
    	std::visit([](const auto& v)
	{
	  if constexpr(std::is_convertible_v<decltype(v),bool>)
	    return (bool)v;
	  else
	    errorEmitter("Cannot convert the type to bool");
	  
	  return false;
	},std::visit(subev,*forNode.subNodes[1]));
	std::visit(subev,*forNode.subNodes[2]))
      std::visit(subev,*forNode.subNodes[3]);
    
    return std::monostate{};
  }
  
  Value operator()(const ASTNodesNode& astNodesNode)
  {
    /// Creates a subev only if there is already one above
    Evaluator* subev=this;
    if(env.parent!=nullptr)
      subev=new Evaluator{&env};
    
    for(const std::shared_ptr<ASTNode>& astNode : astNodesNode.subNodes)
      std::visit(*subev,*astNode);
    
    if(env.parent!=nullptr)
      delete subev;
    
    return std::monostate{};
  }
  
  Value operator()(const IdNode& symNode)
  {
    const auto v=
      env.find(symNode.name);
    
    if(not v)
      errorEmitter("using uninitialized variable ",symNode.name);
    
    return *v;
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
T& fetch(std::vector<std::shared_ptr<ASTNode>>& subNodes,
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

template <typename T>
struct ParseTreeExecutor
{
  using StackEl=
    std::shared_ptr<T>;
  
  using Stack=
    std::vector<StackEl>;
  
  std::map<std::string,std::function<StackEl(Stack&)>> actions;
  
  StackEl execParseTree(const std::vector<FlattenedParseTreeNode>& pt) const
  {
    Stack stack;
    
    for(size_t iPt=0;iPt<pt.size();iPt++)
      {
	const auto& [txt,isReduce,n]=pt[iPt];
	
	if(const std::string_view tmp{txt.first,txt.second};not isReduce)
	  {
	    diagnostic("Push string: ",tmp,"\n");
	    stack.push_back(std::make_shared<T>(ValueNode(std::string(tmp))));
	  }
	else
	  {
	    diagnostic("Reducing ",n," symbols from stack of size ",stack.size(),"\n");
	    
	    diagnostic("Stack types:\n");
	    for(const auto& s : stack)
	      diagnostic(" ",variantInnerTypeName(*s),"\n");
	    
	    if(tmp=="")
	      {
		diagnostic(" (no action)\n");
		if(iPt+1==pt.size())
		  diagnostic(" last reduction, avoiding remove from the stack\n");
		else
		  {
		    stack.erase(stack.end()-n,stack.end());
		    stack.push_back(std::make_shared<T>(ValueNode(std::monostate{})));
		  }
	      }
	    else
	      {
		diagnostic(" with action: \"",tmp,"\"\n");
		
		auto actionNameEnd=tmp.begin();
		while(actionNameEnd!=tmp.end() and not (*actionNameEnd=='('))
		  actionNameEnd++;
		
		std::string_view action{tmp.begin(),actionNameEnd};
		std::vector<size_t> argsIdList;
		if(actionNameEnd!=tmp.end())
		  {
		    diagnostic("action \"",tmp,"\" has args, action name: ",action,"\n");
		    
		    auto c=
		      actionNameEnd+1;
		    
		    while((*c)!=')' and c!=tmp.end())
		      {
			size_t i=0;
			while(*c!=',' and *c!=')' and c!=tmp.end())
			  {
			    if(*c<'0' or *c>'9')
			      errorEmitter("while parsing action \"",tmp,"\" matched \"",std::string_view{tmp.begin(),c},"\" and then unexpected symbol \'",*c,"'");
			    
			    i=i*10+(*c-'0');
			    c++;
			  }
			
			if(i>=n)
			  errorEmitter("while parsing action \"",tmp,"\" encountered argument ",i," greater than the number of parsed symbols, ",n);
			argsIdList.push_back(i);
			
			if(*c!=')')
			  c++;
		      }
		    
		    if(c==tmp.end())
		      errorEmitter("while parsing action ",action," matched \"",std::string_view{tmp.begin(),c},"\" but not the matching ')'");
		  }
		else
		  {
		    argsIdList.resize(n);
		    std::iota(argsIdList.begin(),argsIdList.end(),0);
		  }
		
		if(const auto af=
		   actions.find((std::string)action);
		   af==actions.end())
		  errorEmitter("action \"",action,"\" not registered");
		else
		  {
		    Stack subNodes(argsIdList.size());
		    for(size_t i=0;i<argsIdList.size();i++)
		      {
			const size_t iStack=stack.size()-n+argsIdList[i];
			subNodes[i]=stack[iStack];
			diagnostic(" passing as argument ",i," the stack symbol ",iStack,", of type ",variantInnerTypeName(*subNodes[i])," to stack\n");
		      }
		    
		    stack.erase(stack.end()-n,stack.end());
		    if(std::shared_ptr<ASTNode> p=af->second(subNodes))
		      {
			diagnostic(" pushing returned symbol, of type ",variantInnerTypeName(*p)," to stack\n");
			stack.push_back(p);
		      }
		    else
		      diagnostic(" no returned symbol\n");
		  }
	      }
	    
	    diagnostic(" new stack size: ",stack.size(),"\n");
	    
	    diagnostic("Stack types:\n");
	    for(const auto& s : stack)
	      diagnostic(" ",variantInnerTypeName(*s),"\n");
	  }
      }
    
    if(const size_t n=stack.size();n!=1)
      errorEmitter("stack size is ",n," should contain precisely 1 element");
    
    return std::move(stack.front());
  }
};

// void createParsableExample(const Grammar& g)
// {
//   std::deque<size_t> iStates;
//   iStates.push_back(0);
  
//   std::mt19937_64 gen(123224);
//   std::deque<size_t> next;
//   std::vector<std::pair<size_t,size_t>> past;
//   std::vector<std::pair<std::deque<size_t>,std::vector<size_t>>> pathWay;
  
//   while(iStates.size()!=2 or iStates[0]!=g.iStartSymbol or iStates[1]!=g.iEndSymbol)
//     {
//       diagnostic("iState: {");
//       for(const size_t& i : iStates)
// 	diagnostic(i,",");
//       diagnostic("}\n");
      
//       const int iState=iStates.back();
//       const GrammarState& state=g.states[iState];
//       diagnostic(" cur state: ",iState,"\n",g.describe(state),"\n");
      
//       diagnostic("next: {");
//       for(const size_t& i : next)
// 	diagnostic(g.symbols[i].name,",");
//       diagnostic("}\n");
      
//       std::vector<size_t> iTerminalTrans;
//       for(size_t iTrans=0;iTrans<g.transitionsOfStates[iState].size();iTrans++)
// 	{
// 	  const auto& [iSymbol,iProductionOrState,transType]=g.transitionsOfStates[iState][iTrans];
	  
// 	  const GrammarSymbol& symbol=
// 	    g.symbols[iSymbol];
	  
// 	  diagnostic(symbol.typeTag()," Symbol: ",symbol.name," ");
	  
// 	  if(symbol.type==GrammarSymbol::Type::TERMINAL_SYMBOL)
// 	    {
// 	      const size_t n=(g.transitionsOfStates[iState][iTrans].type==GrammarTransition::REDUCE)?100:1;
// 	      for(size_t i=0;i<n;i++)
// 		iTerminalTrans.push_back(iTrans);
// 	    }
	  
// 	  if(transType==GrammarTransition::SHIFT)
// 	    diagnostic("trans to state ",iProductionOrState,"\n");
// 	  else
// 	    diagnostic(" reduces with production: ",g.describe(g.productions[iProductionOrState]),"\n");
// 	}
      
//       if(next.empty())
// 	{
// 	  diagnostic("Needs to invent a symbol\n");
	  
// 	  diagnostic("trans list:");
// 	  for(const size_t& iTrans : iTerminalTrans)
// 	    diagnostic(" ",iTrans);
// 	  diagnostic("\n");
	  
// 	  if(iTerminalTrans.size())
// 	    {
// 	      size_t iPath=0;
// 	      while(iPath<pathWay.size() and pathWay[iPath].first!=iStates)
// 		iPath++;
	      
// 	      if(iPath==pathWay.size())
// 		pathWay.emplace_back(iStates,std::vector<size_t>{});
	      
// 	      for(const auto& iForbiddenSymbols : pathWay.back().second)
// 		for(int iTrans=0;iTrans<iTerminalTrans.size();iTrans++)
// 		  if(g.transitionsOfStates[iState][iTerminalTrans[iTrans]].iSymbol==iForbiddenSymbols)
// 		    {
// 		      diagnostic("Evicting \"",g.symbols[iForbiddenSymbols].name,"\" from trans list\n");
// 		      iTerminalTrans.erase(iTerminalTrans.begin()+iTrans);
// 		      iTrans--;
// 		    }
	      
// 	      diagnostic("n accepted terminals: ",iTerminalTrans.size(),"\n");
	      
// 	      const size_t iiTrans=
// 		std::uniform_int_distribution<>(0,iTerminalTrans.size()-1)(gen);
// 	      diagnostic("iiTrans: ",iiTrans,"\n");
	      
// 	      const size_t iTrans=
// 		iTerminalTrans[iiTrans];
	      
// 	      const auto& [iSymbol,iProductionOrState,transType]=g.transitionsOfStates[iState][iTrans];
// 	      diagnostic("Invented symbol ",g.symbols[iSymbol].name,"\n");
// 	      next.push_back(iSymbol);
// 	    }
// 	  else
// 	    if(g.nTransitionsOfState(iState)==1 and g.transitionsOfStates[iState][0].iSymbol==g.iEndSymbol)
// 	      next.push_back(g.iEndSymbol);
// 	    else
// 	      errorEmitter("ajjj");
// 	}
//       else
// 	diagnostic("No need to invent a symbol\n");
      
//       const GrammarSymbol& symbol=g.symbols[next.front()];
//       diagnostic("Next symbol: ",symbol.name,"\n");
      
//       bool found{};
//       for(size_t iTransition=0;iTransition<g.transitionsOfStates[iState].size() and not found;iTransition++)
// 	if(const GrammarTransition& transition=g.transitionsOfStates[iState][iTransition];transition.iSymbol==next.front())
// 	  {
// 	    const size_t iSP=transition.iStateOrProduction;
// 	    if(transition.type==GrammarTransition::SHIFT)
// 	      {
// 		iStates.push_back(iSP);
// 		diagnostic("Shifting to state ",iSP,"\n");
// 		const size_t n=next.front();
// 		next.pop_front();
		
// 		if(g.symbols[n].type==GrammarSymbol::Type::TERMINAL_SYMBOL)
// 		  past.emplace_back(n,pathWay.size()-1);
// 	      }
// 	    else
// 	      {
// 		const GrammarProduction& production=g.productions[iSP];
// 		for(size_t i=0;i<production.nRhs();i++)
// 		  iStates.pop_back();
// 		diagnostic("Reducing with production: ",g.describe(production),"\n");
// 		next.push_front(production.iLhs());
// 	      }
	    
// 	    found=true;
// 	  }
      
//       if(not found)
// 	{
// 	  diagnostic("Forbidding generation of symbol \"",g.symbols[next.front()].name,"\" at last pathway\n");
// 	  iStates=pathWay.back().first;
// 	  pathWay.back().second.push_back(next.front());
// 	  next.clear();
// 	}
      
//       for(const auto& [symbol,stateGen] : past)
// 	diagnostic(g.symbols[symbol].name,"{",stateGen,"} ");
//       diagnostic("\n");
//       diagnostic("----------\n");
//     }
  
// }

void a()
{
  static constexpr const char actGrammar[]=
    "actGrammar {"
    "   %whitespace \" +\";"
    "   action : identifier [actionNoList]"
    "          | identifier \"\\(\" integers \"\\)\" [actionWithList]"
    "          ;    "
    "   identifier : \"[a-zA-Z_][a-zA-Z0-9_]*\" [return]"
    "              ;"
    "   integers : [noIntegers]"
    "            | someIntegers [return]"
    "            ;"
    "   someIntegers : integer [firstInteger]"
    "                | someIntegers \",\" integer [appendInteger]"
    "                ;"
    "   integer : \"[0-9]+\" [convToInt]"
    "                    ;"
    "}";
  
  const auto act=createGrammar(actGrammar);
  diagnostic("actStates: ",act.states.size(),"\n");
  diagnostic("regex dstates: ",act.regexMatcher.dStates.size(),"\n");
  
  {
    const auto pt=createParseTree(act,"ciao(1,2,3)");
    for(const auto& [txt,isReduce,n] : pt)
    if(const std::string_view tmp{txt.first,txt.second};not isReduce)
      {
	diagnostic("Push string: ",tmp,"\n");
      }
    else
      {
	diagnostic("Reducing ",n," symbols\n");
	
	if(tmp=="")
	  {
	    diagnostic(" (no action)\n");
	  }
	else
	  {
	    diagnostic(" with action: \"",tmp,"\"\n");
	  }
      }
  }
}

void c()
{
  
  const char cGrammar[]=
    "c {"
    "   %whitespace \"( |\\n|\\t)+\";"
    "   %none lowerThanElse;"
    "   %none \"else\";"
    "   %left \",\";"
    "   %right \"=\";"
    "   %right \"\\+=\";"
    "   %right \"\\-=\";"
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
    "   %left \"\\-\";"
    "   %left \"\\*\";"
    "   %left \"/\";"
    "   %right \"!\";"
    "   %left \"\\-\\-\";"
    "   %left \"\\+\\+\";"
    "   %none FUNCTION_CALL;"
    ""
    "   statement : expression_statement [return]"
    "             | compound_statement [return]"
    "             | forStatement [return]"
    "             | ifStatement [return]"
    "             | functionDefinition [return]"
    "             ;"
    "   functionDefinition: \"fun\" identifier \"\\(\" functionDefinitionArgs \"\\)\" compound_statement [funcDef(1,3,5)]"
    "                     ;"
    "   functionDefinitionArgs: [createStatements]"
    "                         | functionDefinitionArgs functionDefinitionArg  [appendStatement]"
    "                         | functionDefinitionArgs \",\" functionDefinitionArg [appendStatement(0,2)]"
    "                         ;"
    "   functionDefinitionArg: identifier [return]"
    "                        | assign_expression [return]"
    "                        | unary_reference [return]"
    "                        ;"
    "   forStatement: \"for\" \"\\(\" forInit \";\" forCheck \";\" forIncr \"\\)\" statement [forStatement]"
    "               ;"
    "   forInit: expression [return]"
    "          |"
    "          ;"
    "   forCheck: expression [return]"
    "           |"
    "           ;"
    "   forIncr: expression [return]"
    "          |"
    "          ;"
    "   ifStatement: \"if\" \"\\(\" expression \"\\)\" statement %precedence lowerThanElse [ifStatement]"
    "              | \"if\" \"\\(\" expression \"\\)\" statement \"else\" statement [ifElseStatement]"
    "              ;"
    "    compound_statement : \"{\" statements \"}\" [return(1)]"
    "                       ;"
    "    statements : [createStatements]"
    "               | statements statement [appendStatement]"
    "               ;"
    "    expression_statement : expression \";\" [return(0)]"
    "                           ;"
    "    expression : logical_or_expression [return]"
    "               | assign_expression [return]"
    "               | identifier \"\\*=\" expression [unaryProdAssign]"
    "               | identifier \"/=\" expression [unaryDivAssign]"
    "               | identifier \"\\+=\" expression [unarySumAssign]"
    "               | identifier \"\\-=\" expression [unaryDiffAssign]"
    "               ;"
    "    assign_expression: identifier \"=\" expression [unaryAssign(0,2)]"
    "                     ;"
    "    logical_or_expression : logical_and_expression [return]"
    "                          | logical_or_expression \"\\|\\|\" logical_and_expression [binayOr]"
    "                          ;"
    "    logical_and_expression : equality_expression [return]"
    "                           | logical_and_expression \"&&\" equality_expression [binaryAnd]"
    "                           ;"
    "    equality_expression : relational_expression [return]"
    "                        | equality_expression \"==\" relational_expression [binaryComparison]"
    "                        | equality_expression \"!=\" relational_expression [binaryInequality]"
    "                        ;"
    "    relational_expression : additive_expression %precedence \"<=\" [return]"
    "                          | relational_expression \"<\" additive_expression [binarySmaller]"
    "                          | relational_expression \">\" additive_expression [binaryGreater]"
    "                          | relational_expression \"<=\" additive_expression [binarySmallerEqual]"
    "                          | relational_expression \">=\" additive_expression [binaryGreaterEqual]"
    "                          ;"
    "    additive_expression : multiplicative_expression [return]"
    "                        | additive_expression \"\\+\" multiplicative_expression [binarySum]"
    "                        | additive_expression \"\\-\" multiplicative_expression [binaryDiff(0,2)]"
    "                        ;"
    "    multiplicative_expression : unary_expression [return]"
    "                              | multiplicative_expression \"\\*\" unary_expression [binaryProd(0,2)]"
    "                              | multiplicative_expression \"/\" unary_expression [binaryDiv]"
    "                              | multiplicative_expression \"%\" unary_expression [binaryModule]"
    "                              ;"
    "    unary_expression : postfix_expression [return]"
    "                     | \"\\+\" unary_expression [unaryPlus(1)]"
    "                     | \"\\-\" unary_expression [unaryMinus(1)]"
    "                     | unary_reference [return]"
    "                     | \"!\" unary_expression [unaryNot]"
    "                     ;"
    "    unary_reference : \"&\" unary_expression [unaryReference(1)]"
    "                    ;"
    "    postfix_expression : primary_expression [return]"
    "                       | postfix_expression \"\\+\\+\" [postfixIncrement]"
    "                       | postfix_expression \"\\-\\-\" [postfixDecrement]"
    "                       ;"
    "    function_call_arguments : assign_expression [firstFuncCallArg]"
    "                            | function_call_arguments \",\" assign_expression [appendFuncCallArg(0,2)]"
    "                            ;"
    "    primary_expression : identifier [return]"
    "                       | integer_constant [return]"
    "                       | floating_constant [return]"
    "                       | string [return]"
    "                       | \"\\(\" expression \"\\)\" [return(1)]"
    "                       | function_call [return(0)]"
    "                       ;"
    "    function_call : identifier \"\\(\" function_call_arguments \"\\)\" %precedence FUNCTION_CALL [funcCall(0,2)] "
    "                  ;"
    "    identifier : \"[a-zA-Z_][a-zA-Z0-9_]*\" [convToId]"
    "               ;"
    "    integer_constant : \"[0-9]+\" [convToInt]"
    "                     ;"
    "    floating_constant : \"[0-9]+(\\.[0-9]+)?((e|E)(\\+|\\-)?[0-9]+)?\" [convToFloat]"
    "                      ;"
    "    string : \"\\\"[^\\\"]*\\\"\" [return]"
    "           ;"
    "}";
  
  const auto c=createGrammar(cGrammar);
  
  for(size_t iState=0;iState<c.states.size();iState++)
    {
      const GrammarState& state=c.states[iState];
      
      diagnostic("--\n");
      
      diagnostic("State ",iState,":\n",c.describe(state));
      diagnostic("has ",c.transitionsOfStates[iState].size()," transitions:\n");
      
      for(const GrammarTransition& t : c.transitionsOfStates[iState])
	diagnostic(c.describe(t));
    }
  
  // std::vector<std::vector<size_t>> arriveToStateFrom(c.states.size());
  // for(size_t iState=0;iState<c.states.size();iState++)
  //   for(size_t iTrans=0;iTrans<c.transitionsOfStates[iState].size();iTrans++)
  //     if(const GrammarTransition& t=c.transitionsOfStates[iState][iTrans];t.type==GrammarTransition::SHIFT)
  // 	arriveToStateFrom[t.iStateOrProduction].emplace_back(iState);
  
  // for(size_t iState=0;iState<c.states.size();iState++)
  //   if(arriveToStateFrom[iState].size()==1)
  //     for(size_t iTrans=0;iTrans<c.transitionsOfStates[iState].size();iTrans++)
  // 	{
  // 	  const GrammarTransition& t=c.transitionsOfStates[iState][iTrans];
  // 	  if(t.type==GrammarTransition::REDUCE and c.productions[t.iStateOrProduction].nRhs()==1 and c.action(t.iStateOrProduction)=="return")
  // 	    diagnostic("State ",iState,
  // 		       " can be reached only by state ",arriveToStateFrom[iState].front(),
  // 		       " and when receiving symbol ",c.symbols[t.iSymbol].name,
  // 		       " has a trivial reduce: from symbol ",c.symbols[c.productions[t.iStateOrProduction].iRhsList.front()].name,
  // 		       " to symbol ",c.symbols[c.productions[t.iStateOrProduction].iLhs()].name,
  // 		       "\n");
  // 	  if(t.type==GrammarTransition::SHIFT and c.transitionsOfStates[arriveToStateFrom[iState].front()].size()==1 and c.transitionsOfStates[iState].size()==1)
  // 	    diagnostic("State ",iState,
  // 		       " can be reached only by state ",arriveToStateFrom[iState].front(),
  // 		       " and has only 1 transitions\n");
	  
  // 	  //trivialProductions.emplace_back(iState,iTrans);
  //     }
  
  // for(size_t iState=0;iState<c.states.size();iState++)
  //   {
  //     diagnostic("Arrives to state ",iState," from ",arriveToStateFrom[iState].size()," transitions, ");
  //     for(const auto& [iFrom,iTrans] : arriveToStateFrom[iState])
  // 	{
  // 	  // c.state(iState).iItems.size();
  // 	  const GrammarTransition& t=c.transitionsOfStates[iState][iTrans];
  // 	  diagnostic(iFrom,"{",c.symbols[t.iSymbol].name," ",,"} ");
  //     diagnostic("\n");
  //   }
  
  
  
  diagnostic("Grammar info, nstates: ",c.states.size(),"\n");
  diagnostic("Lexer info, nDstates: ",c.regexMatcher.dStates.size(),"\n");
  
  
  
  // createParsableExample(c);
  // return ;
  
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
  
  ParseTreeExecutor<ASTNode> ptExecutor;
  
#define ENSURE_N_SYMBOLS(N)			\
  if(subNodes.size()!=N)			\
    errorEmitter("expecting " #N " symbols")
  
#define PROVIDE_ACTION_WITH_N_SYMBOLS(NAME,				\
				      N,				\
				      BODY...)				\
  ptExecutor.actions[NAME]=						\
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes) -> std::shared_ptr<ASTNode> \
    {									\
      ENSURE_N_SYMBOLS(N);						\
      									\
      BODY;								\
    }
  
  PROVIDE_ACTION_WITH_N_SYMBOLS("createStatements",0,return std::make_shared<ASTNode>(ASTNodesNode{}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("appendStatement",2,
				
				if(ASTNodesNode* l=std::get_if<ASTNodesNode>(&*(subNodes[0]));l==nullptr)
				  errorEmitter("first argument is not a list of statement: ",std::visit([](const auto& x){return typeid(decltype(x)).name();},*(subNodes[0])));
				else
				  l->subNodes.emplace_back(subNodes[1]);
				return subNodes[0]);
  
  PROVIDE_ACTION_WITH_N_SYMBOLS("return",1,return subNodes[0]);
  PROVIDE_ACTION_WITH_N_SYMBOLS("convToInt",1,
				ValueNode& v=fetch<ValueNode>(subNodes,0);
				std::string* s=std::get_if<std::string>(&v.value);
				if(s==nullptr)
				  errorEmitter("expecting std::string, obtained other type");
				return std::make_shared<ASTNode>(ValueNode{atoi(s->c_str())}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("unaryMinus",1,return std::make_shared<ASTNode>(UminusNode{.op=subNodes[0]}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("unaryPlus",1,return std::make_shared<ASTNode>(UplusNode{.op=subNodes[0]}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("unaryReference",1,return std::make_shared<ASTNode>(UrefNode{.op=subNodes[0]}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("binaryProd",2,return std::make_shared<ASTNode>(ProdNode{.op1=subNodes[0],.op2=subNodes[1]}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("binaryDiff",2,return std::make_shared<ASTNode>(SubNode{.op1=subNodes[0],.op2=subNodes[1]}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("unaryAssign",2,return std::make_shared<ASTNode>(AssignNode{.lhs=subNodes[0],.rhs=subNodes[1]}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("firstFuncCallArg",1,return std::make_shared<ASTNode>(ASTNodesNode{.subNodes{subNodes[0]}}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("appendFuncCallArg",2,fetch<ASTNodesNode>(subNodes,0).subNodes.push_back(subNodes[1]);return subNodes[0]);
  PROVIDE_ACTION_WITH_N_SYMBOLS("funcCall",2,return std::make_shared<ASTNode>(FuncCallNode{.name=fetch<IdNode>(subNodes,0).name,.args=fetch<ASTNodesNode>(subNodes,1).subNodes}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("convToId",1,return std::make_shared<ASTNode>(IdNode{.name=unvariant<std::string>(fetch<ValueNode>(subNodes,0).value)}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("funcDefArg",1,return std::make_shared<ASTNode>(IdNode{.name=unvariant<std::string>(fetch<ValueNode>(subNodes,0).value)}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("funcDef",3,
				const std::string name=fetch<IdNode>(subNodes,0).name;
				if(auto it=functionsTable.find(name);it!=functionsTable.end())
				  functionsTable.erase(it);
				
				std::shared_ptr<ASTNode> body=std::make_shared<ASTNode>(std::move(fetch<ASTNodesNode>(subNodes,2)));
				
				std::shared_ptr<FunctionArgs> args=std::make_shared<FunctionArgs>();
				
				for(std::shared_ptr<ASTNode>& a : fetch<ASTNodesNode>(subNodes,1).subNodes)
				  std::visit(Overload{
				      [&args](const IdNode& ass)
				      {
					args->try_emplace(ass.name,false,nullptr);
				      },
					[&args](const AssignNode& ass)
					{
					  args->try_emplace(unvariant<IdNode>(*ass.lhs).name,false,ass.rhs);
					},
					[&args](const UrefNode& ref)
					{
					  args->try_emplace(unvariant<IdNode>(*ref.op).name,true,nullptr);
					}},*a);
				
				return std::make_shared<ASTNode>(FuncDefNode{name,args,body});
				);
  
  diagnostic("Executing the parse tree to generate the AST\n");
  
  std::shared_ptr<ASTNode> r=ptExecutor.execParseTree(pt);
  
  diagnostic("Evaluating the AST\n");
  
  Evaluator ev;
  diagnostic(" base node: ",variantInnerTypeName(*r),"\n");
  std::visit(ev,*r);
  ev.env.print();
}

int main()
{
  c();
  
  return 0;
  
  // ASTNode a=AssignNode{.name="A",
  //   .rhs=std::make_shared<ASTNode>(SumNode{.op1=std::make_shared<ASTNode>(ValueNode(5)),
  // 					   .op2=std::make_shared<ASTNode>(ValueNode(-3))})};
  
  // Evaluator ev;
  // std::visit(ev,a);
  
  // using namespace pp;
  // using namespace pp::internal;
  
  // it will be convenient to keep track of this https://cs.wmich.edu/~gupta/teaching/cs4850/sumII06/The%20syntax%20of%20C%20in%20Backus-Naur%20form.htm
  
  [[maybe_unused]]
  constexpr const char nissaGrammar[]=
	      "nissa {"
	      "   %whitespace \" +\";"
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
  
  std::vector<std::shared_ptr<ASTNode>> stack;
  std::map<std::string,std::function<std::shared_ptr<ASTNode>(std::vector<std::shared_ptr<ASTNode>>&)>> actions;
  
  actions["createStatements"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=0)
	errorEmitter("expecting 0 symbols");
      
      return std::make_shared<ASTNode>(ASTNodesNode{});
    };
  
  actions["exprStatement"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=2)
	errorEmitter("expecting exactly 2 symbols");
      
      return std::move(subNodes[0]);
    };
  
  actions["forStatement"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=9)
	errorEmitter("expecting 9 symbols, obtained ",subNodes.size());
      
      std::shared_ptr<ASTNode> res=std::make_shared<ASTNode>(ForNode());
      for(const int& i : {2,4,6,8})
	std::get_if<ForNode>(&*res)->subNodes.push_back(subNodes[i]);
      
      return res;
    };
  
  actions["ifStatement"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=5)
	errorEmitter("expecting 5 symbols, obtained ",subNodes.size());
      
      std::shared_ptr<ASTNode> res=std::make_shared<ASTNode>(IfNode());
      for(const int& i : {2,4})
	std::get_if<IfNode>(&*res)->subNodes.emplace_back(std::move(subNodes[i]));
      
      std::get_if<IfNode>(&*res)->subNodes.emplace_back(std::make_shared<ASTNode>(ASTNodesNode()));
      
      return res;
    };
  
  actions["ifElseStatement"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=7)
	errorEmitter("expecting 7 symbols, obtained ",subNodes.size());
      
      std::shared_ptr<ASTNode> res=std::make_shared<ASTNode>(IfNode());
      for(const int& i : {2,4,6})
	std::get_if<IfNode>(&*res)->subNodes.emplace_back(std::move(subNodes[i]));
      
      return res;
    };
  
  actions["appendStatement"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=2)
	errorEmitter("expecting exactly 2 symbol");
      
      std::shared_ptr<ASTNode> res=std::move(subNodes[0]);
      
      ASTNodesNode* l=std::get_if<ASTNodesNode>(&*(res));
      
      if(l==nullptr)
	errorEmitter("first argument is not a list of statement");
      
      l->subNodes.emplace_back(std::move(subNodes[1]));
      
      return res;
    };
  
  actions["return"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=1)
	errorEmitter("expecting only 1 symbol");
      
      return std::move(subNodes[0]);
    };
  
  actions["return1"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()<2)
	errorEmitter("expecting only 2 symbols");
      
      return std::move(subNodes[1]);
    };
  
  actions["convToInt"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      ValueNode& v=fetch<ValueNode>(subNodes,0);
      
      std::string* s=std::get_if<std::string>(&v.value);
      
      if(s==nullptr)
	errorEmitter("expecting std::string, obtained other type");
      
      return std::make_shared<ASTNode>(ValueNode{atoi(s->c_str())});
    };
  
  actions["uplus"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  {
    if(subNodes.size()!=2)
      errorEmitter("expecting 2 symbols");
    
    return std::make_shared<ASTNode>(UplusNode{.op=std::move(subNodes[1])});
  };
  
  actions["uminus"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=2)
	errorEmitter("expecting 2 symbols");
      
      return std::make_shared<ASTNode>(UminusNode{.op=std::move(subNodes[1])});
    };
  
  actions["sum"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=3)
	errorEmitter("expecting precisely 3 symbols");
      
      return std::make_shared<ASTNode>(SumNode{.op1=std::move(subNodes[0]),
					       .op2=std::move(subNodes[2])});
    };
  
  actions["sub"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=3)
	errorEmitter("expecting precisely 3 symbols");
      
      return std::make_shared<ASTNode>(SubNode{.op1=std::move(subNodes[0]),
					       .op2=std::move(subNodes[2])});
    };
  
  actions["product"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
    {
      if(subNodes.size()!=3)
	errorEmitter("expecting precisely 3 symbols");
      
      return std::make_shared<ASTNode>(ProdNode{.op1=std::move(subNodes[0]),
					       .op2=std::move(subNodes[2])});
    };
  
  // actions["assign"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  // {
  //   if(subNodes.size()!=3)
  //     errorEmitter("expecting precisely 3 symbols");
    
  //   ValueNode& lhs=
  //     fetch<ValueNode>(subNodes,0);
    
  //   const std::string* n=
  //     std::get_if<std::string>(&lhs.value);
    
  //   if(n==nullptr)
  //     errorEmitter("lhs not of string type");
    
  //   return std::make_shared<ASTNode>(AssignNode{.name=*n,.rhs=std::move(subNodes[2])});
  // };
  
  actions["bracket"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  {
    if(subNodes.size()!=3)
      errorEmitter("expecting precisely 3 symbols");
    
    return std::move(subNodes[1]);
  };
  
  actions["storeString"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  {
    if(subNodes.size()!=1)
      errorEmitter("expecting precisely 1 symbols");
      
    const ValueNode& nameNode=
      fetch<ValueNode>(subNodes,0);
    
    const std::string* name=
      std::get_if<std::string>(&nameNode.value);
    
    if(name==nullptr)
      errorEmitter("symbol containing the string not of string type");
    
    return std::make_shared<ASTNode>(ValueNode{.value=name->substr(1,name->length()-2)});
  };
  
  actions["rhsSub"]=
    [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  {
      if(subNodes.size()!=1)
	errorEmitter("expecting precisely 1 symbols");
      
    const ValueNode& nameNode=
      fetch<ValueNode>(subNodes,0);
    
    const std::string* name=
      std::get_if<std::string>(&nameNode.value);
    
    if(name==nullptr)
      errorEmitter("symbol containing the name of the expanding symbol not of string type");
    
    return std::make_shared<ASTNode>(IdNode{.name=*name});
  };
  
  for(const auto& [txt,isReduce,n] : pt)
    if(const std::string_view tmp{txt.first,txt.second};not isReduce)
      {
	diagnostic("Push string: ",tmp,"\n");
	stack.push_back(std::make_shared<ASTNode>(ValueNode(std::string(tmp))));
      }
    else
      {
	diagnostic("Reducing ",n," symbols from stack of size ",stack.size(),"\n");
	
	if(tmp=="")
	  {
	    diagnostic(" (no action)\n");
	    stack.erase(stack.end()-n,stack.end());
	    stack.push_back(std::make_shared<ASTNode>(ValueNode(std::monostate{})));
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
		std::vector<std::shared_ptr<ASTNode>> subNodes{std::make_move_iterator(stack.end()-n),std::make_move_iterator(stack.end())};
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
  
  return 0;
}
