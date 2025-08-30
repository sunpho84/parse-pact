#include <cmath>
#include <cstddef>
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

using Time=
  std::chrono::time_point<std::chrono::high_resolution_clock>;

Time take_time()
{
  return std::chrono::high_resolution_clock::now();
}

double time_diff_with_now(const Time& start)
{
  return std::chrono::duration_cast<std::chrono::microseconds>(take_time()-start).count()/1e6;
}

#define N_VARIADIC_ARGS(ARGS...)			\
  {std::tuple_size_v<decltype(std::make_tuple(ARGS))>}

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

std::string unescapeString(const std::string& in)
{
  std::string out;
  
  bool esc=false;
  
  for(size_t i=1;i<in.length()-1;i++)
    if(esc)
      {
	switch(in[i])
	  {
	  case 'b':out+='\b';break;
	  case 'n':out+='\n';break;
	  case 'f':out+='\f';break;
	  case 'r':out+='\r';break;
	  case 't':out+='\t';break;
	  default:out+=in[i];
	  }
	esc=false;
      }
    else
      if(in[i]=='\\')
	esc=true;
      else
	out+=in[i];
  
  if(esc)
    errorEmitter("String ended while escaping");
  
  return out;
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
concept Unotsable=
requires(Op op)
{
  !op;
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

#define DEFINE_UN_NODE_OP(NAME,OP1,OP2)			\
    struct U ## NAME					\
  {							\
    template <typename A>				\
    static auto eval(A&& a) -> decltype(OP1 a OP2)	\
    {							\
      return OP1 a OP2;					\
    }							\
  }

#define DEFINE_UN_NODE(NAME,OP1,OP2)			\
  DEFINE_UN_NODE_OP(NAME,OP1,OP2);			\
  							\
  using U ## NAME ## Node=				\
    UnOpNode<U ## NAME>

DEFINE_UN_NODE(Not,!,);
DEFINE_UN_NODE(Plus,+,);
DEFINE_UN_NODE(Minus,-,);

#undef DEFINE_UN_NODE

template <typename T>
struct PostfixVariateNode;

#define DEFINE_POSTFIX_NODE(NAME,OP1,OP2)			\
  DEFINE_UN_NODE_OP(Postfix ## NAME,OP1,OP2);			\
  								\
  using UPostfix ## NAME ## Node=				\
    PostfixVariateNode<UPostfix ## NAME>

DEFINE_POSTFIX_NODE(Increment,,++);
DEFINE_POSTFIX_NODE(Decrement,,--);

struct URef
{
  template <typename A>
  static A eval(const A& a)
  {
    errorEmitter("Not meant to be really evaluated... yet");
    
    return a;
  }
};

using URefNode=
  UnOpNode<URef>;

/////////////////////////////////////////////////////////////////

template <typename T>
struct BinOpNode;

#define DEFINE_BIN_NODE(NAME,OP)			\
  struct NAME						\
  {							\
    template <typename A,				\
	      typename B>				\
    static auto eval(const A& a,			\
		     const B& b) -> decltype(a OP b)	\
    {							\
      return a OP b;					\
    }							\
  };							\
							\
  using NAME ## Node=					\
    BinOpNode<NAME>

DEFINE_BIN_NODE(Sum,+);
DEFINE_BIN_NODE(Diff,-);
DEFINE_BIN_NODE(Prod,*);
DEFINE_BIN_NODE(Div,/);
DEFINE_BIN_NODE(Mod,%);
DEFINE_BIN_NODE(Smaller,<);
DEFINE_BIN_NODE(Greater,>);
DEFINE_BIN_NODE(SmallerEqual,<=);
DEFINE_BIN_NODE(GreaterEqual,>=);
DEFINE_BIN_NODE(Compare,==);
DEFINE_BIN_NODE(Inequal,!=);
DEFINE_BIN_NODE(Or,or);
DEFINE_BIN_NODE(And,and);

#undef DEFINE_BIN_NODE

struct AssignNode;

struct ForNode;

struct FuncDefNode;

struct FuncCallNode;

struct SubscribeNode;

struct ReturnNode;

struct IfNode;

struct ASTNodesNode;

using ASTNode=
  std::variant<ASTNodesNode,
	       ForNode,
	       IfNode,
	       IdNode,
	       UPlusNode,
	       UMinusNode,
	       UNotNode,
	       UPostfixIncrementNode,
	       UPostfixDecrementNode,
	       URefNode,
	       SumNode,
	       DiffNode,
	       ProdNode,
	       DivNode,
	       ModNode,
	       SmallerNode,
	       GreaterNode,
	       SmallerEqualNode,
	       GreaterEqualNode,
	       CompareNode,
	       OrNode,
	       AndNode,
	       InequalNode,
	       FuncDefNode,
	       FuncCallNode,
	       SubscribeNode,
	       ReturnNode,
	       ValueNode,
	       AssignNode>;

using FunctionArgs=
  std::map<std::string,std::tuple<bool,std::shared_ptr<ASTNode>>>;

struct ASTNodesNode
{
  std::vector<std::shared_ptr<ASTNode>> subNodes;
};

struct Environment;

struct Function;

struct HostFunction;

struct ValuesList;

using Value=
  std::variant<std::monostate,std::string,int,double,Function,HostFunction,ValuesList>;

struct ValuesList
{
  std::vector<Value> data;
  
  Value& operator[](const int& i);
};

struct Evaluator;

struct Function
{
  std::shared_ptr<FunctionArgs> args;
  
  std::shared_ptr<ASTNode> body;
  
  Environment* env;
};

struct HostFunction :
  std::function<Value(std::vector<Value>&)>
{
};

ValuesList operator+(const ValuesList& a,
		     const ValuesList& b)
{
  ValuesList res;
    
  res.data.reserve(a.data.size()+b.data.size());
  
  for(const std::vector<Value>* v : {&a.data,&b.data})
    for(const Value& vi : *v)
      res.data.push_back(vi);
  
  return res;
}

struct Evaluator;

std::ostream& operator<<(std::ostream& os,
			 const ValuesList& v)
{
  os<<"list(";
  for(size_t i=0;i<v.data.size();i++)
    std::visit([i,&os](const auto& v)
    {
      if(i)
	os<<",";
      if constexpr(Streamable<decltype(v)>)
	os<<v;
      else os<<"(unprintable)";
    },v.data[i]);
  os<<")";
  
  return os;
}

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
  std::shared_ptr<ASTNode> fetch;
  
  std::vector<std::shared_ptr<ASTNode>> args;
};

struct SubscribeNode
{
  std::shared_ptr<ASTNode> base;
  
  std::shared_ptr<ASTNode> subscr;

  template <typename E>
  int getIndex(E&& e) const;
};

struct ReturnNode
{
  std::shared_ptr<ASTNode> arg;
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
struct PostfixVariateNode
{
  std::shared_ptr<ASTNode> op;
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

/////////////////////////////////////////////////////////////////

template <typename E>
int SubscribeNode::getIndex(E&& e) const
{
  Value s=std::visit(std::forward<E>(e),*subscr);
  int* _i=std::get_if<int>(&s);
  
  if(not _i)
    errorEmitter("index of subscrition is not an integer but is of type: ",variantInnerTypeName(s));
  
  const int i=*_i;
  if(i<0)
    errorEmitter("Trying to subscribe with negative index ",i);
  
  return i;
}

Value& ValuesList::operator[](const int& i)
{
  if(const size_t m=data.size();i>=m)
    errorEmitter("Trying to subscribe a vector of size ",m," using index ",i);
  
  return data[i];
}

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
  
  Value& at(const std::string& name)
  {
    std::shared_ptr<Value> f=find(name);
    
    if(f==nullptr)
      errorEmitter("Trying to use uninitialized variable ",name);
    
    return *f;
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
    
    env[name]=
      Function{.args=funcDefNode.args,
      .body=funcDefNode.body,
      .env=&env};
    
    return std::monostate{};
  }
  
  Value operator()(const FuncCallNode& funcCallNode)
  {
    /// Function to call
    Value ff=
      std::visit(*this,*funcCallNode.fetch);
    
    if(const IdNode* id=std::get_if<IdNode>(&*funcCallNode.fetch))
      diagnostic("Calling function \"",id->name,"\n");
    else
      diagnostic("Calling anonymous function \n");
    
    if(Function* f=std::get_if<Function>(&ff))
      {
	Evaluator subev{&env};
	for(const std::shared_ptr<ASTNode>& ap : funcCallNode.args)
	  {
	    AssignNode& a=
	      unvariant<AssignNode>(*ap);
	    
	    const std::string& aName=
	      unvariant<IdNode>(*a.lhs).name;
	    
	    if(auto it=f->args->find(aName);it==f->args->end())
	      errorEmitter("trying to pass argument ",aName," not expected by the function");
	    else
	      if(const bool& isRef=std::get<bool>(it->second))
		if(IdNode* id=std::get_if<IdNode>(&*a.rhs))
		  if(std::shared_ptr<Value> eid=env.find(id->name))
		    {
		      diagnostic("Getting par \"",aName,"\" by ref\n");
		      subev.env.varTable[aName]=eid;
		    }
		  else
		    errorEmitter("undefined symbol \"",id->name,"\" when passing argument \"",aName,"\" to function");
		else
		  errorEmitter("argument \"",aName,"\" expects an id as a parameter (pass by reference)");
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
		errorEmitter("parameter \"",aName,"\" with no default value unspecified when calling the function");
	  else
	    {}
	
	return std::visit(subev,*f->body);
      }
    else
      if(HostFunction* hf=std::get_if<HostFunction>(&ff))
	{
	  std::vector<Value> evArgs;
	  for(const std::shared_ptr<ASTNode>& ap : funcCallNode.args)
	    evArgs.emplace_back(std::visit(*this,*ap));
	  
	  return (*hf)(evArgs);
	}
      else
	errorEmitter("Variable is of type ",variantInnerTypeName(ff)," not a ",typeid(Function).name());
    
    return std::monostate{};
  }
  
  
  Value operator()(const SubscribeNode& subscribeNode)
  {
    const int i=subscribeNode.getIndex(*this);
    
    std::shared_ptr<Value> v;
    if(IdNode* idNode=std::get_if<IdNode>(&*subscribeNode.base))
      {
	v=env.find(idNode->name);
	
	if(v==nullptr)
	  errorEmitter("Trying to use uninitialized variable ",idNode->name);
      }
    else
      v=std::make_shared<Value>(std::visit(*this,*subscribeNode.base));
    
    if(ValuesList* vl=std::get_if<ValuesList>(&*v))
      return (*vl)[i];
    else
      errorEmitter("trying to subscribe non-list");
    
    return {};
  }

  Value operator()(const ReturnNode& returnNode)
  {
    return std::visit(*this,*returnNode.arg);
  }
  
  Value operator()(const AssignNode& assignNode)
  {
    auto getLhs=
      [this](auto&& getLhs,
	 auto&& node)->Value*
      {
	if(IdNode* s=
	   std::get_if<IdNode>(&node))
	  return &env[s->name];
	  
	if(SubscribeNode* s=
	   std::get_if<SubscribeNode>(&node))
	  {
	    Value* base=getLhs(getLhs,*s->base);
	    
	    if(ValuesList* vl=std::get_if<ValuesList>(base))
	      return &(*vl)[s->getIndex(*this)];
	    else
	      errorEmitter("base of the subscription is not an lhs, but of type ",variantInnerTypeName(*base));
	    
	    return {};
	  }
	
	errorEmitter("Trying to use expression of type ",variantInnerTypeName(node)," as a lhs");
	
	return {};
      };
    
    auto& lhs=*getLhs(getLhs,*assignNode.lhs);
    
    lhs=std::visit(*this,*assignNode.rhs);
    
    return lhs;
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
      if(ifNode.subNodes.size()>=2)
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
    std::shared_ptr<Evaluator> _subev;
    Evaluator *subev=this;
    if(env.parent!=nullptr)
      {
	_subev=std::make_shared<Evaluator>(&env);
	subev=_subev.get();
      }
    
    for(auto& n : astNodesNode.subNodes)
      {
	if(std::get_if<ReturnNode>(&*n))
	  return std::visit(*subev,*n);
	else
	  std::visit(*subev,*n);
      }
    
    return {};
  }
  
  Value operator()(const IdNode& idNode)
  {
    return env.at(idNode.name);
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
  Value operator()(const PostfixVariateNode<T>& node)
  {
    const std::string& name=
      unvariant<IdNode>(*node.op).name;
    
    std::shared_ptr<Value> v=env.find(name);
    if(v!=nullptr)
      return
	std::visit([](auto& r) -> Value
	{
	  if constexpr(requires {T::eval(r);})
	    return T::eval(r);
	  else
	    {
	      errorEmitter("asking to postfix-variate type ",typeid(r).name()," not supporting postfix");
	      return std::monostate{};
	    }
	},*v);
    else
      {
	errorEmitter("asking to postfix-variate inexisting id ",name);
	return std::monostate{};
      }
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


void c()
{
#define BARE_WHITESPACES			\
  "( |\\n|\\t)+"
  
#define CPP_COMMENT				\
  "//[^\\n]+\\n"
  
#define C_COMMENT				\
  "/\\*([^\\*]|\\*[^/])+\\*/"
    
    const char cGrammar[]=
    "c {"
    "   %whitespace \"" BARE_WHITESPACES "|" CPP_COMMENT "|" C_COMMENT "\";"
    "   %none lowerThanElse;"
    "   %none \"else\";"
    "   %left \"\\(\";"
    "   %left \"\\)\";"
    "   %none BRACKETS;"
    "   %left \",\";"
    "   %right \"=\";"
    "   %right \"\\+=\";"
    "   %right \"\\-=\";"
    "   %right \"\\*=\";"
    "   %right \"/=\";"
    "   %left \"\\|\\||or\";"
    "   %left \"&&|and\";"
    "   %left \"==\";"
    "   %left \"!=\";"
    "   %left \"<\";"
    "   %left \"<=\";"
    "   %left \">\";"
    "   %left \">=\";"
    "   %left \"\\+\";"
    "   %left \"\\-\";"
    "   %left \"%\";"
    "   %left \"\\*\";"
    "   %left \"/\";"
    "   %right UNARY_ARITHMETIC;"
    "   %right \"&\";"
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
    "             | \"return\" expression_statement [funcReturn(1)]"
    "             ;"
    "   functionDefinition: \"fun\" identifier \"\\(\" functionDefinitionArgs \"\\)\" compound_statement [funcDef(1,3,5)]"
    "                     ;"
    "   functionDefinitionArgs: [createStatements]"
    "                         | functionDefinitionArgs functionDefinitionArg  [appendStatement]"
    "                         | functionDefinitionArgs \",\" functionDefinitionArg [appendStatement(0,2)]"
    "                         ;"
    "   functionDefinitionArg: identifier [return]"
    // "                        | assign_expression [return]" //reduce/reduce conflict
    "                        | unary_reference [return]"
    "                        ;"
    "   forStatement: \"for\" \"\\(\" forInit \";\" forCheck \";\" forIncr \"\\)\" statement [forStatement(2,4,6,8)]"
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
    "   ifStatement: \"if\" \"\\(\" expression \"\\)\" statement %precedence lowerThanElse [ifStatement(2,4)]"
    "              | \"if\" \"\\(\" expression \"\\)\" statement \"else\" statement [ifElseStatement(2,4,6)]"
    "              ;"
    "    compound_statement : \"{\" statements \"}\" [return(1)]"
    "                       ;"
    "    statements : [createStatements]"
    "               | statements statement [appendStatement]"
    "               ;"
    "    expression_statement : expression \";\" [return(0)]"
    "                         ;"
    "    expression : postfix_expression %precedence \"\\+\\+\" [return(0)]"
    "               | expression \"\\+\" expression [binarySum(0,2)]"
    "               | expression \"\\-\" expression [binaryDiff(0,2)]"
    "               | expression \"\\*\" expression [binaryProd(0,2)]"
    "               | expression \"/\" expression [binaryDiv(0,2)]"
    "               | expression \"%\" expression [binaryMod(0,2)]"
    "               | \"\\+\" expression %precedence UNARY_ARITHMETIC [unaryPlus(1)]"
    "               | \"\\-\" expression %precedence UNARY_ARITHMETIC [unaryMinus(1)]"
    "               | unary_reference [return]"
    "               | \"!\" expression [unaryNot(1)]"
    "               | expression \"<\" expression [binarySmaller(0,2)]"
    "               | expression \">\" expression [binaryGreater(0,2)]"
    "               | expression \"<=\" expression [binarySmallerEqual(0,2)]"
    "               | expression \">=\" expression [binaryGreaterEqual(0,2)]"
    "               | expression \"==\" expression [binaryCompare(0,2)]"
    "               | expression \"!=\" expression [binaryInequal(0,2)]"
    "               | expression \"&&|and\" expression [binaryAnd(0,2)]"
    "               | expression \"\\|\\||or\" expression [binaryOr(0,2)]"
    "               | assign_expression [return]"
    "               | identifier \"\\*=\" expression [unaryProdAssign]"
    "               | identifier \"/=\" expression [unaryDivAssign]"
    "               | identifier \"\\+=\" expression [unarySumAssign]"
    "               | identifier \"\\-=\" expression [unaryDiffAssign]"
    "               ;"
    "    postfix_expression : primary_expression [return(0)]"
    "                       | postfix_expression \"\\+\\+\" [unaryPostfixIncrement(0)]"
    "                       | postfix_expression \"\\-\\-\" [unaryPostfixDecrement(0)]"
    "                       | postfix_expression \"\\(\" \"\\)\" %precedence FUNCTION_CALL [emptyFuncCall(0)] "
    "                       | postfix_expression \"\\(\" expressions_list \"\\)\" %precedence FUNCTION_CALL [funcCall(0,2)] "
    "                       | postfix_expression \"\\[\" expression \"\\]\" [subscribe(0,2)] "
    "                       ;"
    "    primary_expression : identifier [return]"
    "                       | \"[0-9]+\" [convToInt]"
    "                       | \"([0-9]+(\\.[0-9]*)?|(\\.[0-9]+))((e|E)(\\+|\\-)?[0-9]+)?\" [convToFloat]"
    "                       | \"\\\"[^\\\"]*\\\"\" [convToStr]"
    "                       | \"\\(\" expression \"\\)\" %precedence BRACKETS [return(1)]"
    "                       ;"
    "    assign_expression : postfix_expression \"=\" expression %precedence \"=\" [unaryAssign(0,2)]"
    "                      ;"
    // "    lhs : identifier [getPtrOfId]"
    // "        | lhs \"[\" expression \"]\" [subscribe(0,2)]"
    // "        ;"
    "    unary_reference : \"&\" expression [unaryReference(1)]"
    "                    ;"
    "    expressions_list : expression [firstExprOfList]"
    "                     | expressions_list \",\" expression [appendExprToList(0,2)]"
    "                     ;"
    "    identifier : \"[a-zA-Z_][a-zA-Z0-9_]*\" [convToId]"
    "               ;"
    "}";
  
  // verbose=true;
  auto gCreateMoment=take_time();
  const auto c=createGrammar(cGrammar);
  std::cout<<"Time to create the grammar: "<<time_diff_with_now(gCreateMoment)<<"\n";
  
  for(size_t iState=0;iState<c.states.size();iState++)
    {
      const GrammarState& state=c.states[iState];
      
      std::cout<<"--\n";
      
      std::cout<<"State "<<iState<<":\n"<<c.describe(state);
      std::cout<<"has "<<c.transitionsOfStates[iState].size()<<" transitions:\n";
      
      for(const GrammarTransition& t : c.transitionsOfStates[iState])
	std::cout<<c.describe(t);
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
  
  
  
  // diagnostic("Grammar info, nstates: ",c.states.size(),"\n");
  // diagnostic("Lexer info, nDstates: ",c.regexMatcher.dStates.size(),"\n");
  
  
  
  // createParsableExample(c);
  // return ;
  
  ParseTreeExecutor<ASTNode> ptExecutor;
  
#define ENSURE_N_SYMBOLS(N)			\
  if(subNodes.size()!=N)			\
    errorEmitter("expecting " #N " symbols, obtained ",subNodes.size())
  
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
  PROVIDE_ACTION_WITH_N_SYMBOLS("convToInt",1,return std::make_shared<ASTNode>(ValueNode{atoi(unvariant<std::string>(fetch<ValueNode>(subNodes,0).value).c_str())}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("convToFloat",1,return std::make_shared<ASTNode>(ValueNode{strtod(unvariant<std::string>(fetch<ValueNode>(subNodes,0).value).c_str(),nullptr)}));
  
#define PROVIDE_UN_ACTION(NAME)					\
  PROVIDE_ACTION_WITH_N_SYMBOLS("unary" #NAME,1,return std::make_shared<ASTNode>(U ## NAME ## Node{.op=subNodes[0]}))
  
  PROVIDE_UN_ACTION(Plus);
  PROVIDE_UN_ACTION(Minus);
  PROVIDE_UN_ACTION(Not);
  PROVIDE_UN_ACTION(PostfixIncrement);
  PROVIDE_UN_ACTION(PostfixDecrement);
  
#undef PROVIDE_UN_ACTION
  PROVIDE_ACTION_WITH_N_SYMBOLS("unaryReference",1,return std::make_shared<ASTNode>(URefNode{.op=subNodes[0]}));

#define PROVIDE_BIN_ACTION(NAME)					\
  PROVIDE_ACTION_WITH_N_SYMBOLS("binary" #NAME,2,return std::make_shared<ASTNode>(NAME ## Node{.op1=subNodes[0],.op2=subNodes[1]}))
  
  PROVIDE_BIN_ACTION(Sum);
  PROVIDE_BIN_ACTION(Diff);
  PROVIDE_BIN_ACTION(Prod);
  PROVIDE_BIN_ACTION(Div);
  PROVIDE_BIN_ACTION(Mod);
  PROVIDE_BIN_ACTION(Smaller);
  PROVIDE_BIN_ACTION(Greater);
  PROVIDE_BIN_ACTION(SmallerEqual);
  PROVIDE_BIN_ACTION(GreaterEqual);
  PROVIDE_BIN_ACTION(Compare);
  PROVIDE_BIN_ACTION(Inequal);
  PROVIDE_BIN_ACTION(Or);
  PROVIDE_BIN_ACTION(And);

#undef PROVIDE_BIN_ACTION
  
  PROVIDE_ACTION_WITH_N_SYMBOLS("unaryAssign",2,return std::make_shared<ASTNode>(AssignNode{.lhs=subNodes[0],.rhs=subNodes[1]}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("firstExprOfList",1,return std::make_shared<ASTNode>(ASTNodesNode{.subNodes{subNodes[0]}}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("appendExprToList",2,fetch<ASTNodesNode>(subNodes,0).subNodes.push_back(subNodes[1]);return subNodes[0]);
  PROVIDE_ACTION_WITH_N_SYMBOLS("emptyFuncCall",1,return std::make_shared<ASTNode>(FuncCallNode{.fetch=subNodes[0],.args{}}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("funcCall",2,return std::make_shared<ASTNode>(FuncCallNode{.fetch=subNodes[0],.args=fetch<ASTNodesNode>(subNodes,1).subNodes}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("funcReturn",1,return std::make_shared<ASTNode>(ReturnNode{.arg=subNodes[0]}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("subscribe",2,return std::make_shared<ASTNode>(SubscribeNode{.base=subNodes[0],.subscr=subNodes[1]}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("convToId",1,return std::make_shared<ASTNode>(IdNode{.name=unvariant<std::string>(fetch<ValueNode>(subNodes,0).value)}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("convToStr",1,return std::make_shared<ASTNode>(ValueNode{unescapeString(unvariant<std::string>(fetch<ValueNode>(subNodes,0).value))}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("ifStatement",2,return std::make_shared<ASTNode>(IfNode{.subNodes{subNodes}}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("ifElseStatement",3,return std::make_shared<ASTNode>(IfNode{.subNodes{subNodes}}));
  PROVIDE_ACTION_WITH_N_SYMBOLS("forStatement",4,return std::make_shared<ASTNode>(ForNode{.subNodes{subNodes}}));
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
					[&args](const URefNode& ref)
					{
					  args->try_emplace(unvariant<IdNode>(*ref.op).name,true,nullptr);
					}},*a);
				
				return std::make_shared<ASTNode>(FuncDefNode{name,args,body});
				);
  
  /////////////////////////////////////////////////////////////////
  
  std::vector<char> ext;
  if(const char* path="/home/francesco/trastulli/parse-pact/example";
     std::filesystem::exists(path))
    {
      const size_t exs=
	std::filesystem::file_size(path);
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
  
  /////////////////////////////////////////////////////////////////
  
  std::cout<<"Creating the parse tree\n";
  
  auto ptCreate=take_time();
  auto pt=
    createParseTree(c,&ext[0]);
  std::cout<<"Time to create the parse tree: "<<time_diff_with_now(ptCreate)<<"\n";
  
  /////////////////////////////////////////////////////////////////
  
  std::cout<<"Creating the AST\n";
  
  auto astCreate=take_time();
  std::shared_ptr<ASTNode> r=
    ptExecutor.execParseTree(pt);
  std::cout<<"Time to build the AST: "<<time_diff_with_now(astCreate)<<"\n";
  
  /////////////////////////////////////////////////////////////////
  
  std::cout<<"Preparing the evaluator\n";
  
  Evaluator ev;
  
  ev.env["M_PI"]=M_PI;
  
  ev.env["print"]=
    HostFunction{
    [](std::vector<Value>& args)->Value
    {
      for(Value& arg : args)
	std::visit([](const auto& v)
      {
	if constexpr(Streamable<decltype(v)>)
	  std::cout<<v;
	else
	  std::cout<<"unprintable type: "<<typeid(decltype(v)).name()<<"\n";
      },arg);
      
      return {};
    }};
  
  ev.env["list"]=
    HostFunction{
    [](std::vector<Value>& args)->Value
    {
      return ValuesList{args};
    }};
  
#define REGISTER_ARGLESS_HOST_FUNCTION(NAME)	\
  ev.env[#NAME]=				\
    HostFunction{				\
    [](std::vector<Value>& args)->Value		\
    {						\
      return NAME();				\
    }}
  
#define REGISTER_HOST_FUNCTION(NAME,ARGS...)				\
  ev.env[#NAME]=							\
    HostFunction{							\
    [](std::vector<Value>& args)->Value					\
    {									\
      constexpr size_t N=						\
	N_VARIADIC_ARGS(ARGS);						\
    									\
    const size_t n=							\
      args.size();							\
    									\
    if(N!=n)								\
      errorEmitter("trying to call function ",#NAME,			\
		   " which expects ",N," args with ",n);		\
									\
      return std::visit([](auto&&...args) ->Value			\
    {									\
      if constexpr(requires {NAME(std::forward<decltype(args)>(args)...);}) \
	if constexpr(std::is_same_v<void,decltype(NAME(std::forward<decltype(args)>(args)...))>) \
	  NAME(std::forward<decltype(args)>(args)...);			\
	else								\
	  return NAME(std::forward<decltype(args)>(args)...);		\
      else								\
	errorEmitter("Trying to call ",#NAME,				\
		     " function with impossible args ",typeid(args).name()...); \
      									\
      return std::monostate{};						\
    }									\
	,ARGS);								\
    }}
  
  REGISTER_ARGLESS_HOST_FUNCTION(rand);
  
  REGISTER_HOST_FUNCTION(srand,args[0]);
  REGISTER_HOST_FUNCTION(sqrt,args[0]);
  REGISTER_HOST_FUNCTION(exp,args[0]);
  REGISTER_HOST_FUNCTION(sin,args[0]);
  REGISTER_HOST_FUNCTION(cos,args[0]);
  REGISTER_HOST_FUNCTION(tan,args[0]);
  REGISTER_HOST_FUNCTION(pow,args[0],args[1]);
  
  {
    using namespace std;
    REGISTER_HOST_FUNCTION(to_string,args[0]);
  }
  
  /////////////////////////////////////////////////////////////////
  
  std::cout<<"Evaluating the AST\n";
  
  auto t=take_time();
  std::visit(ev,*r);
  std::cout<<"Time to run the ast: "<<time_diff_with_now(t)<<"\n";
  
  /////////////////////////////////////////////////////////////////
  
  std::cout<<"Environment: \n";
  
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
  
  // [[maybe_unused]]
  // constexpr const char nissaGrammar[]=
  // 	      "nissa {"
  // 	      "   %whitespace \" +\";"
  // 	      "   %right \"=\";"
  // 	      "   %left \"\\+\";"
  // 	      "   %left \"\\-\";"
  // 	      "   %left \"\\*\";"
  // 	      "   %none lowerThanElse;"
  // 	      "   %none \"else\";"
  // 	      "   statements: statements statement [appendStatement]"
  // 	      "             | [createStatements]"
  // 	      "             ;"
  // 	      "   statement: exprStatement [return]"
  // 	      "            | forStatement [return]"
  // 	      "            | ifStatement [return]"
  // 	      "            | \"{\" statements \"}\" [return1]"
  // 	      "            ;"
  // 	      "   exprStatement: expr \";\" [exprStatement]"
  // 	      "                | \";\""
  // 	      "                ;"
  // 	      "   forStatement: \"for\" \"\\(\" forInit \";\" forCheck \";\" forIncr \"\\)\" statement [forStatement]"
  // 	      "               ;"
  // 	      "   forInit: expr [return]"
  // 	      "          |"
  // 	      "          ;"
  // 	      "   forCheck: expr [return]"
  // 	      "          |"
  // 	      "          ;"
  // 	      "   forIncr: expr [return]"
  // 	      "          |"
  // 	      "          ;"
  // 	      "   ifStatement: \"if\" \"\\(\" expr \"\\)\" statement %precedence lowerThanElse [ifStatement]"
  // 	      "              | \"if\" \"\\(\" expr \"\\)\" statement \"else\" statement [ifElseStatement]"
  // 	      "              ;"
  // 	      "   lhs: id [return]"
  // 	      "      ;"
  // 	      "   expr: assignExpr [return]"
  // 	      "       | expr \"\\*\" expr [product]"
  // 	      "       | expr \"\\+\" expr [sum]"
  // 	      "       | expr \"\\-\" expr [sub]"
  // 	      "       | \"\\+\" expr [uplus]"
  // 	      "       | \"\\-\" expr [uminus]"
  // 	      "       | \"\\(\" expr \"\\)\" [bracket]"
  // 	      "       | id [rhsSub]"
  // 	      "       | str [return]"
  // 	      "       | int [return]"
  // 	      "       ;"
  // 	      "   assignExpr: lhs \"=\" expr [assign] "
  // 	      "             ;"
  // 	      "   id: \"[a-zA-Z_][a-zA-Z0-9_]*\" [return]"
  // 	      "     ;"
  // 	      "   str: \"\\\"[^\\\"]*\\\"\" [storeString]"
  // 	      "      ;"
  // 	      "   int: \"[0-9]+\" [convToInt]"
  // 	      "      ;"
  // 	      "}";
  
  // const auto nissa=createGrammar(nissaGrammar);
  
  // //constexpr auto nissa=createGrammar<nissaGrammar>();
  
  // auto pt=
  //   createParseTree(nissa,
  // 		    "A=-1*+3; B=-5*(2-A); if(0)C=B*A; else T=11;"
  // 		    "D=\"ciao\";"
  // 		    "for(i=0;i-1;i=i+1) for(j=0;j-2;j=j+1){D=D+\" \"+D;}");
  
  // std::string_view r("return");
  // pt.back().txtData=std::make_pair(&*r.begin(),&*r.end());
  
  // diagnostic("====================================\n");
  
  // std::vector<std::shared_ptr<ASTNode>> stack;
  // std::map<std::string,std::function<std::shared_ptr<ASTNode>(std::vector<std::shared_ptr<ASTNode>>&)>> actions;
  
  // actions["createStatements"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=0)
  // 	errorEmitter("expecting 0 symbols");
      
  //     return std::make_shared<ASTNode>(ASTNodesNode{});
  //   };
  
  // actions["exprStatement"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=2)
  // 	errorEmitter("expecting exactly 2 symbols");
      
  //     return std::move(subNodes[0]);
  //   };
  
  
  // actions["ifStatement"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=5)
  // 	errorEmitter("expecting 5 symbols, obtained ",subNodes.size());
      
  //     std::shared_ptr<ASTNode> res=std::make_shared<ASTNode>(IfNode());
  //     for(const int& i : {2,4})
  // 	std::get_if<IfNode>(&*res)->subNodes.emplace_back(std::move(subNodes[i]));
      
  //     std::get_if<IfNode>(&*res)->subNodes.emplace_back(std::make_shared<ASTNode>(ASTNodesNode()));
      
  //     return res;
  //   };
  
  // actions["ifElseStatement"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=7)
  // 	errorEmitter("expecting 7 symbols, obtained ",subNodes.size());
      
  //     std::shared_ptr<ASTNode> res=std::make_shared<ASTNode>(IfNode());
  //     for(const int& i : {2,4,6})
  // 	std::get_if<IfNode>(&*res)->subNodes.emplace_back(std::move(subNodes[i]));
      
  //     return res;
  //   };
  
  // actions["appendStatement"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=2)
  // 	errorEmitter("expecting exactly 2 symbol");
      
  //     std::shared_ptr<ASTNode> res=std::move(subNodes[0]);
      
  //     ASTNodesNode* l=std::get_if<ASTNodesNode>(&*(res));
      
  //     if(l==nullptr)
  // 	errorEmitter("first argument is not a list of statement");
      
  //     l->subNodes.emplace_back(std::move(subNodes[1]));
      
  //     return res;
  //   };
  
  // actions["return"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=1)
  // 	errorEmitter("expecting only 1 symbol");
      
  //     return std::move(subNodes[0]);
  //   };
  
  // actions["return1"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()<2)
  // 	errorEmitter("expecting only 2 symbols");
      
  //     return std::move(subNodes[1]);
  //   };
  
  // actions["convToInt"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     ValueNode& v=fetch<ValueNode>(subNodes,0);
      
  //     std::string* s=std::get_if<std::string>(&v.value);
      
  //     if(s==nullptr)
  // 	errorEmitter("expecting std::string, obtained other type");
      
  //     return std::make_shared<ASTNode>(ValueNode{atoi(s->c_str())});
  //   };
  
  // actions["uplus"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  // {
  //   if(subNodes.size()!=2)
  //     errorEmitter("expecting 2 symbols");
    
  //   return std::make_shared<ASTNode>(UplusNode{.op=std::move(subNodes[1])});
  // };
  
  // actions["uminus"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=2)
  // 	errorEmitter("expecting 2 symbols");
      
  //     return std::make_shared<ASTNode>(UminusNode{.op=std::move(subNodes[1])});
  //   };
  
  // actions["sum"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=3)
  // 	errorEmitter("expecting precisely 3 symbols");
      
  //     return std::make_shared<ASTNode>(SumNode{.op1=std::move(subNodes[0]),
  // 					       .op2=std::move(subNodes[2])});
  //   };
  
  // actions["sub"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=3)
  // 	errorEmitter("expecting precisely 3 symbols");
      
  //     return std::make_shared<ASTNode>(SubNode{.op1=std::move(subNodes[0]),
  // 					       .op2=std::move(subNodes[2])});
  //   };
  
  // actions["product"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  //   {
  //     if(subNodes.size()!=3)
  // 	errorEmitter("expecting precisely 3 symbols");
      
  //     return std::make_shared<ASTNode>(ProdNode{.op1=std::move(subNodes[0]),
  // 					       .op2=std::move(subNodes[2])});
  //   };
  
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
  
  // actions["bracket"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  // {
  //   if(subNodes.size()!=3)
  //     errorEmitter("expecting precisely 3 symbols");
    
  //   return std::move(subNodes[1]);
  // };
  
  // actions["storeString"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  // {
  //   if(subNodes.size()!=1)
  //     errorEmitter("expecting precisely 1 symbols");
      
  //   const ValueNode& nameNode=
  //     fetch<ValueNode>(subNodes,0);
    
  //   const std::string* name=
  //     std::get_if<std::string>(&nameNode.value);
    
  //   if(name==nullptr)
  //     errorEmitter("symbol containing the string not of string type");
    
  //   return std::make_shared<ASTNode>(ValueNode{.value=name->substr(1,name->length()-2)});
  // };
  
  // actions["rhsSub"]=
  //   [](std::vector<std::shared_ptr<ASTNode>>& subNodes)->std::shared_ptr<ASTNode>
  // {
  //     if(subNodes.size()!=1)
  // 	errorEmitter("expecting precisely 1 symbols");
      
  //   const ValueNode& nameNode=
  //     fetch<ValueNode>(subNodes,0);
    
  //   const std::string* name=
  //     std::get_if<std::string>(&nameNode.value);
    
  //   if(name==nullptr)
  //     errorEmitter("symbol containing the name of the expanding symbol not of string type");
    
  //   return std::make_shared<ASTNode>(IdNode{.name=*name});
  // };
  
  // for(const auto& [txt,isReduce,n] : pt)
  //   if(const std::string_view tmp{txt.first,txt.second};not isReduce)
  //     {
  // 	diagnostic("Push string: ",tmp,"\n");
  // 	stack.push_back(std::make_shared<ASTNode>(ValueNode(std::string(tmp))));
  //     }
  //   else
  //     {
  // 	diagnostic("Reducing ",n," symbols from stack of size ",stack.size(),"\n");
	
  // 	if(tmp=="")
  // 	  {
  // 	    diagnostic(" (no action)\n");
  // 	    stack.erase(stack.end()-n,stack.end());
  // 	    stack.push_back(std::make_shared<ASTNode>(ValueNode(std::monostate{})));
  // 	  }
  // 	else
  // 	  {
  // 	    diagnostic(" with action: \"",tmp,"\"\n");
	    
  // 	    if(const auto af=
  // 	       actions.find((std::string)tmp);
  // 	       af==actions.end())
  // 	      errorEmitter("action \"",tmp,"\" not registered");
  // 	    else
  // 	      {
  // 		std::vector<std::shared_ptr<ASTNode>> subNodes{std::make_move_iterator(stack.end()-n),std::make_move_iterator(stack.end())};
  // 		stack.erase(stack.end()-n,stack.end());
  // 		diagnostic(" pushing returned symbol to stack\n");
  // 		stack.push_back(af->second(subNodes));
  // 	      }
  // 	  }
	
  // 	diagnostic(" new stack size: ",stack.size(),"\n");
  //     }
  
  // Evaluator e;
  
  // std::visit(e,*stack[0]);
  
  // diagnostic("FINALE\n");
  
  // return 0;
}
