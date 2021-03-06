//
// Created by sean on 29/07/19.
//

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <iostream>

#include "llvm/ADT/STLExtras.h"

class LexerParseException: public std::exception{
private:
    std::string msg;
public:
    explicit LexerParseException(const std::string& msg){
        this->msg = "Lexer Exception: " + msg;
    }

    const char* what() const noexcept override {
        return this->msg.c_str();
    }
};

enum Token{
    TOKEN_EOF = -1,

    // commands
    TOKEN_DEF = -2,
    TOKEN_EXT = -3,

    //PRIMARY
    TOKEN_IDENTIFIER = -4,
    TOKEN_NUMBER = -5
};

static std::string identifierStr;   // if current token is an identifier, this variable holds the name.
static double numVal; // if current token is a number, this variable holds the value.

/**
 * Get token from standard input.
 * @return if matches enum TOKEN return its value, otherwise return the ASCII of the character.
 */
static int getToken(){
    static int lastCharacter = ' ';

    while(isspace(lastCharacter))
        lastCharacter = getchar();

    if(isalpha(lastCharacter)){ // current token an identifier
        identifierStr = lastCharacter;
        while(isalpha((lastCharacter = getchar()))){
            identifierStr += lastCharacter;
        }
        if(identifierStr == "def") return TOKEN_DEF;
        else if(identifierStr == "extern") return TOKEN_EXT;
        return TOKEN_IDENTIFIER;
    }

    if(isdigit(lastCharacter) || lastCharacter == '.'){
        int dotCount = 0;
        if(lastCharacter == '.') ++dotCount;
        std::string numStr;
        numStr += lastCharacter;
        while(isdigit(lastCharacter = getchar()) || lastCharacter == '.'){
            if(lastCharacter == '.' && dotCount){
                throw LexerParseException("Incorrect format for numerical value, mutliple '.'");
            }
            numStr += lastCharacter;
        }
        numVal = strtod(numStr.c_str(), nullptr);
        return TOKEN_NUMBER;
    }

    if(lastCharacter == '#'){
        do{
            lastCharacter = getchar();
        }while (lastCharacter != EOF && lastCharacter != '\n' && lastCharacter != '\r');

        if(lastCharacter != EOF) return getToken();
    }

    if(lastCharacter == EOF) return TOKEN_EOF;

    int lastCharASCII = lastCharacter;
    lastCharacter = getchar();
    return lastCharASCII;
}

//===-----------------------------------------------
// Abstract syntax tree
//===-----------------------------------------------
namespace {
    class ExprAST{
    public:
        virtual ~ExprAST() = default;
    };

    class NumberExprAST: public ExprAST{
        double val;

    public:
        NumberExprAST(double val): val(val) {}
    };

    // class for variable like 'a'
    class VariableExprAST: public ExprAST{
        std::string variable;

    public:
        VariableExprAST(const std::string & variable): variable(std::move(variable)){}
    };

    class BinaryExprAST: public ExprAST{
        char Op;
        std::unique_ptr<ExprAST> LHS, RHS;

    public:
        BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs,
                std::unique_ptr<ExprAST> rhs) :
                Op(op), LHS(std::move(lhs)), RHS(std::move(rhs)) {}
    };

    // class for function call
    class CallExprAST: public ExprAST{
        std::string callee; // function name;
        std::vector<std::unique_ptr<ExprAST>> args;

    public:
        CallExprAST(const std::string callee,
                std::vector<std::unique_ptr<ExprAST>> args) : callee(callee), args(std::move(args)) {}
    };

    class ProtoTypeAST{
        std::string name;
        std::vector<std::string> args;

    public:
        ProtoTypeAST(const std::string &name,
                std::vector<std::string> args) : name(name), args(std::move(args)) {}
    };

    class FunctionAST{
        std::unique_ptr<ProtoTypeAST> prototype;
        std::unique_ptr<ExprAST> body;
    public:
        FunctionAST(std::unique_ptr<ProtoTypeAST> prototype, std::unique_ptr<ExprAST> body) :
        prototype(std::move(prototype)), body(std::move(body)) {}
    };
}

//===-----------------------------------------------------------
// Parser
//===-----------------------------------------------------------
static int curToken;
static int getNextToken(){
    curToken = getToken();
    return curToken;
}

std::map<char, int> binopPrecedence; // +, -, *, <

static int getTokenPrecedence(){
    if(!isascii(curToken)) return -1;
    int tokenPrecedence = binopPrecedence[curToken];
    if(tokenPrecedence <= 0) return -1;
    return tokenPrecedence;
}

std::unique_ptr<ExprAST> logError(const std::string msg){
    fprintf(stderr, "Error: %s\n", msg.c_str());
    return nullptr;
}

std::unique_ptr<ProtoTypeAST> logErrorP(const std::string msg){
    logError(msg);
    return nullptr;
}

static std::unique_ptr<ExprAST> parseExpression();

static std::unique_ptr<ExprAST> parseNumberExpr(){
    auto result =  std::make_unique<NumberExprAST>(numVal);
    getNextToken();
    return result;
}

static std::unique_ptr<ExprAST> parsePatheneseExpr(){
    getNextToken(); // eat (

    auto v = parseExpression();
    if(!v) return nullptr;
    if(curToken != ')') logError("Expect ')'");
    getNextToken(); // eat )
    return v;
}

static std::unique_ptr<ExprAST> parseIdentifierExpr(){
    std::string idName = identifierStr;

    getNextToken(); // move to next token

    // just an identifier, like a b
    if(curToken != '(') return std::make_unique<VariableExprAST>(idName);

    // enter the bracket parse
    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> args;
    if(curToken != ')'){ //a = (b)
        while (1){
            if(auto arg = parseExpression()){
                args.push_back(std::move(arg));
            }else return nullptr;

            if(curToken == ')') break;
            if(curToken != ',') logError("Expect ',' or ')' in argument list");
            getNextToken();
        }
    }
    getNextToken();
    return std::make_unique<CallExprAST>(idName, std::move(args));
}

static std::unique_ptr<ExprAST> parsePrimary(){
    switch (curToken){
        case TOKEN_IDENTIFIER:
            return parseIdentifierExpr();
        case TOKEN_NUMBER:
            return parseNumberExpr();
        case '(':
            return parsePatheneseExpr();
        default:
            return logError("Unknown token");
    }
}

//===----------------------------------------------------
// binary expression parsing
//===----------------------------------------------------
static std::unique_ptr<ExprAST> parseBinaryOpExpressionRHS(int, std::unique_ptr<ExprAST>);

/**
 * Parse costumized expressions
 * @return
 */
static std::unique_ptr<ExprAST> parseExpression(){
    std::unique_ptr<ExprAST> LHS = parsePrimary();
    if(!LHS) return nullptr;

    return parseBinaryOpExpressionRHS(0, std::move(LHS));
}

/**
 *  Create a binary operation AST.
 * @param opPrec    precedence of previous operator.
 * @param LHS  LHS AST of current binary operation
 * @return  BinaryOpExprAST pointer.
 */
static std::unique_ptr<ExprAST> parseBinaryOpExpressionRHS(int opPrec, std::unique_ptr<ExprAST> LHS){
    while (1){
        int curPrec = getTokenPrecedence(); // precedence of current operator.

        if(curPrec < opPrec) return LHS;    // This is the end of the expression. Current token is not an operator.

        int curOp = curToken;
        getNextToken(); // skip current operator.

        std::unique_ptr<ExprAST> RHS = parsePrimary();
        if(!RHS) return nullptr;

        int nextPrec = getTokenPrecedence();
        if(curPrec < nextPrec){
            RHS = parseBinaryOpExpressionRHS(curPrec + 1, std::move(RHS));
            if(!RHS) return nullptr;
        }

        LHS = std::make_unique<BinaryExprAST>(curOp, std::move(LHS), std::move(RHS));
    }
}

static std::unique_ptr<ProtoTypeAST> parsePrototype(){
    if(curToken != TOKEN_IDENTIFIER) return logErrorP("Expect a function name in prototype");
    std::string functionName = identifierStr;
    getNextToken();

    if(curToken != '(') return logErrorP("Expect '(' in prototype");
    std::vector<std::string> args;
    int cur  = getNextToken();
    while(cur == TOKEN_IDENTIFIER){
        args.push_back(identifierStr);
        cur = getNextToken();
    }
    if(curToken != ')') return logErrorP("Expect ')' matching '('");
    getNextToken();
    return std::make_unique<ProtoTypeAST>(functionName, std::move(args));
}

static std::unique_ptr<FunctionAST> parseDefinition(){
    getNextToken(); //skip token 'def'

    auto proto = parsePrototype();
    if(!proto) return nullptr;

    auto body = parseExpression();
    return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
}

static std::unique_ptr<ProtoTypeAST> parseExtern(){
    getNextToken(); // skip "extern"
    return parsePrototype();
}

static std::unique_ptr<FunctionAST> parseTopLevel(){
    if(auto body = parseExpression()){
        std::vector<std::string> v;
        auto proto = std::make_unique<ProtoTypeAST>("", std::move(v));
        return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
    }
    return nullptr;
}

static void definitionHandler(){
    if(parseDefinition()){
        fprintf(stderr, "Parsed a function definition.\n");
    }else if(curToken == ';') return;
    else getNextToken();
}

static void externHandler(){
    if(parseExtern()){
        fprintf(stderr, "Parsed a extern.\n");
    }else if(curToken == ';') return;
    else getNextToken();
}

static void topLevelHandler(){
    if(parseTopLevel())
        fprintf(stderr, "Parsed a top-level expression.\n");
    else getNextToken();
}

static void MainLoop(){
    while(1){
        fprintf(stderr, "Ready> ");
        getNextToken();
        switch (curToken){
            case TOKEN_EOF: return;
            case TOKEN_DEF:
                definitionHandler();
                break;
            case TOKEN_EXT:
                externHandler();
                break;
            case ';':
                continue;
            default:
                topLevelHandler();
                break;
        }
    }
}

int main(){
    binopPrecedence['<'] = 10;
    binopPrecedence['+'] = 20;
    binopPrecedence['-'] = 20;
    binopPrecedence['*'] = 40;
    std::cout << "Start> " << std::endl;
    MainLoop();
    return 0;
}
