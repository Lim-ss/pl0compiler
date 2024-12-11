#include <stdio.h>

#define NRW        11     // number of reserved words
#define TXMAX      500    // length of identifier table
#define MAXNUMLEN  14     // maximum number of digits in numbers
#define NSYM       10     // maximum number of symbols in array ssym and csym
#define MAXIDLEN   10     // length of identifiers

#define MAXADDRESS 32767  // maximum address
#define MAXLEVEL   32     // maximum depth of nesting block
#define CXMAX      500    // size of code array

#define MAXSYM     30     // maximum number of symbols  

#define STACKSIZE  1000   // maximum storage

#define MAXINS   8

enum symtype
{
	SYM_NULL,
	SYM_IDENTIFIER,
	SYM_NUMBER,
	SYM_PLUS,
	SYM_MINUS,
	SYM_TIMES,
	SYM_SLASH,
	SYM_ODD,
	SYM_EQU,
	SYM_NEQ,
	SYM_LES,
	SYM_LEQ,
	SYM_GTR,
	SYM_GEQ,
	SYM_LPAREN,
	SYM_RPAREN,
	SYM_COMMA,
	SYM_SEMICOLON,
	SYM_PERIOD,
	SYM_BECOMES,
    SYM_BEGIN,
	SYM_END,
	SYM_IF,
	SYM_THEN,
	SYM_WHILE,
	SYM_DO,
	SYM_CALL,
	SYM_CONST,
	SYM_VAR,
	SYM_PROCEDURE
};

enum idtype
{
	ID_CONSTANT, ID_VARIABLE, ID_PROCEDURE
};

enum opcode
{
	LIT, OPR, LOD, STO, CAL, INT, JMP, JPC
};

enum oprcode
{
	OPR_RET, OPR_NEG, OPR_ADD, OPR_MIN,
	OPR_MUL, OPR_DIV, OPR_ODD, OPR_EQU,
	OPR_NEQ, OPR_LES, OPR_LEQ, OPR_GTR,
	OPR_GEQ
};


typedef struct
{
	int f; // function code
	int l; // level
	int a; // displacement address
} instruction;

//////////////////////////////////////////////////////////////////////
char* err_msg[] =
{
/*  0 */    "",
/*  1 */    "Found ':=' when expecting '='.",
/*  2 */    "There must be a number to follow '='.",
/*  3 */    "There must be an '=' to follow the identifier.",
/*  4 */    "There must be an identifier to follow 'const', 'var', or 'procedure'.",
/*  5 */    "Missing ',' or ';'.",
/*  6 */    "Incorrect procedure name.",
/*  7 */    "Statement expected.",
/*  8 */    "Follow the statement is an incorrect symbol.",
/*  9 */    "'.' expected.",
/* 10 */    "';' expected.",
/* 11 */    "Undeclared identifier.",
/* 12 */    "Illegal assignment.",
/* 13 */    "':=' expected.",
/* 14 */    "There must be an identifier to follow the 'call'.",
/* 15 */    "A constant or variable can not be called.",
/* 16 */    "'then' expected.",
/* 17 */    "';' or 'end' expected.",
/* 18 */    "'do' expected.",
/* 19 */    "Incorrect symbol.",
/* 20 */    "Relative operators expected.",
/* 21 */    "Procedure identifier can not be in an expression.",
/* 22 */    "Missing ')'.",
/* 23 */    "The symbol can not be followed by a factor.",
/* 24 */    "The symbol can not be as the beginning of an expression.",
/* 25 */    "The number is too great.",
/* 26 */    "",
/* 27 */    "",
/* 28 */    "",
/* 29 */    "",
/* 30 */    "",
/* 31 */    "",
/* 32 */    "There are too many levels."
};

//////////////////////////////////////////////////////////////////////
char ch;         // last character read
int  sym;        // last symbol read
char id[MAXIDLEN + 1]; // last identifier read
int  num;        // last number read
int  cc;         // character count
int  ll;         // line length
int  kk;
int  err;
int  cx;         // index of current instruction to be generated.
int  level = 0;	 //block的嵌套层级，变量(var)或过程(procedure)通过enter函数加进符号表(table)时需要写入level，以便于运行调用时根据level之差进行静态链回溯寻找变量(或过程)的存放位置
int  tx = 0;	 //table index,符号表栈的栈指针，每次加入新符号时，先++再填入，所以指向的是上一个加入的符号(初始时指向0位置除外)

char line[80];

instruction code[CXMAX];

/*
	下面四个数组都是给函数getsym()用的
*/

/*用于逐个和读到的词进行比较，判断是普通标识符还是保留的关键字*/
char* word[NRW + 1] =
{
	"", /* place holder */
	"begin", "call", "const", "do", "end","if",
	"odd", "procedure", "then", "var", "while"
};

/*识别出关键字后，用于对应填入关键字的类型(sym)，和上面的word数组配合使用*/
int wsym[NRW + 1] =
{
	SYM_NULL, SYM_BEGIN, SYM_CALL, SYM_CONST, SYM_DO, SYM_END,
	SYM_IF, SYM_ODD, SYM_PROCEDURE, SYM_THEN, SYM_VAR, SYM_WHILE
};

/*识别出运算符，用于对应填入运算符的类型(sym)，和下面的csym数组搭配使用*/
int ssym[NSYM + 1] =
{
	SYM_NULL, SYM_PLUS, SYM_MINUS, SYM_TIMES, SYM_SLASH,
	SYM_LPAREN, SYM_RPAREN, SYM_EQU, SYM_COMMA, SYM_PERIOD, SYM_SEMICOLON
};

//下面这两个数组有些情况下会影响编译(好像是某些编译器会当成不可修改的数组)
//我用vs编译会出问题，用vscode的code runner编译倒是能跑
//奇怪的报错 Error	C2065	'mnemonic': undeclared identifier
//干脆移到了pl0.c中，这样应该无论怎样都能跑起来了

/*用于逐个和读到的字符进行比较，判断是哪种运算符*/
//char csym[NSYM + 1] =
//{
//	' ', '+', '-', '*', '/', '(', ')', '=', ',', '.', ';'
//};
//
//char* mnemonic[MAXINS] =
//{
//	"LIT", "OPR", "LOD", "STO", "CAL", "INT", "JMP", "JPC"
//};

 /*
 	table的一行
 */
typedef struct
{
	char name[MAXIDLEN + 1];
	int  kind; //枚举 ID_CONSTANT, ID_VARIABLE, ID_PROCEDURE
	int  value;
} comtab;

comtab table[TXMAX];

/*
	table的一行，和comtab存储长度相同，平时以comtab结构体的形式存储，
	当kind为 ID_VARIABLE 或 ID_PROCEDURE 时，强制类型转换为mask解读
	其实这么做只是为了省点空间，把{comtab.value}和{mask.level，mask.address}存在了同一个偏移位置
	所以查看table的内容时，如果是ID_VARIABLE 或 ID_PROCEDURE需要(mask*)&table[i]这样子查看，否则只能看到没有意义的数值
*/
typedef struct
{
	char  name[MAXIDLEN + 1];
	int   kind; 
	short level;
	short address;
} mask; 

FILE* infile;

// EOF PL0.h
