// pl0 compiler source code

/*
	这整个项目的函数分为几大类：
	1.有关set的(主要在set.c里定义)包括destroyset(),inset()等,以及test()：
	这些都是用来做错误恢复功能的，用于遇到错误的时候跳过那一段，继续分析后面的源码，如果源码没有错的话，
	或者没想要做很好的错误提示，不用管这些函数，包括很多LL1的递归分析函数的参数都是一个set，也不用管，
	实验的任务里面没有涉及到要扩展错误恢复的功能，而且这部分我读了很久都没搞懂它的逻辑，感觉很乱，
	直接不理这些操作就可以，就算全删掉都是可以正常分析正确的源码的，只是源码出错的时候可能会崩或者进入无限循环。

	2.LL1的递归分析函数：
	包括block()、condition()、constdeclaration()、expression()、term()、factor()、
	statement()、vardeclaration()
	这些函数基本每个都对应一个语法的非终结式(除了最后一个对应的是一个变量名)
	，里面主要做的就是根据语法去匹配符号，递归调用其他非终结式，并执行相关的语义动作(比如将变量名加入符号表，或者生成指令)
	这些函数前面我都写了注释，标注了对应的语法产生式
	需要注意有些地方其实和pdf上画的不太一样，比如有些地方pdf上写着是识别逗号，但其实分号也可以，不过这个不重要

	3.编译分析时的一些其他函数：
	操作符号表(table)的相关函数：enter()、position()
	读字符读符号相关函数：getch()、getsym()
	生成指令，打印生成的指令：gen()、listcode()
	报错:error()

	4.解释运行时用到的函数:
	interpret()、base()

	整个项目的全局变量分为几大类：(大部分定义在PL0.h中，小部分定义在pl0.c中)
	1."真正有意义"的全局变量：table[](符号表)，code[](生成的指令)，tx(符号表的栈指针)，cx(指令的栈指针)、level(当前嵌套层级)
	err(error数量)、sym(上一个读到的符号)

	2.预定义的词、符号分类：比如wsym[],ssym[],csym[],word[]，用来做比较，判断某个词、符号属不属于某种类别

	3.没什么用的全局变量：比如cc、ll、kk这种，基本就是只在很小的范围内用，比如读入文件里的字符时用，很多时候这些函数都不写返回值，
	而是设置一个全局变量用来作为返回值的存放位置，就是这些没什么用的全局变量的来历，不过虽然没什么用，还是不能删，毕竟确实用到了

*/

#pragma warning(disable:4996)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "PL0.h"
#include "set.h"

//下面这两个数组本来是在PL0.h里面的，由于奇怪的编译错误，所以移到了这里，注释见头文件中原本的位置
char csym[NSYM + 1] =
{
	' ', '+', '*', '/', '(', ')', '=', ',', '.', ';'
};

char* mnemonic[MAXINS] =
{
	"LIT", "OPR", "LOD", "STO", "CAL", "INT", "JMP", "JPZ", "DCAL", "MOV",
	"LEA", "LODA", "STOA", "JNZ", "RET", "PRN"
};

//////////////////////////////////////////////////////////////////////
// print error message.
void error(int n)
{
	int i;

	printf("      ");
	for (i = 1; i <= cc - 1; i++)
		printf(" ");
	printf("^\n");
	printf("Error %3d: %s\n", n, err_msg[n]);
	err++;
	printf("compile stop\n");
	exit(0);
} // error

//////////////////////////////////////////////////////////////////////
/*
	读入一个除了空字符(空格、制表、回车)外的字符到全局变量ch
	如果之前没有读到到一半的行，直接把一整行字符存入line中，通过全局变量ch返回第一个字符
	否则返回下一个字符
	是否已经有行读到一半通过cc和ll判断，ll是上一次读到的整行的长度，cc是已经返回的字符的长度
*/
void getch(void)
{
	if (cc == ll)
	{
		if (feof(infile))
		{
			printf("\nPROGRAM INCOMPLETE\n");
			exit(1);
		}
		ll = cc = 0;
		printf("%5d  ", cx);
		while ( (!feof(infile)) // added & modified by alex 01-02-09
			    && ((ch = getc(infile)) != '\n'))
		{
			printf("%c", ch);
			line[++ll] = ch;
		} // while
		printf("\n");
		line[++ll] = ' ';
	}
	ch = line[++cc];
} // getch

//////////////////////////////////////////////////////////////////////
// gets a symbol from input stream.
/*
	读入一个symbol(普通的标识符、保留的关键字、数、运算符)
	通过全局变量sym返回这个symbol
*/
void getsym(void)
{
	int i, k;
	char a[MAXIDLEN + 1];

	while (ch == ' '||ch == '\t')
		getch();

	if (isalpha(ch))
	{ // symbol is a reserved word or an identifier.
		k = 0;
		do
		{
			if (k < MAXIDLEN)
				a[k++] = ch;
			getch();
		}
		while (isalpha(ch) || isdigit(ch));
		a[k] = 0;
		strcpy(id, a);
		word[0] = id;
		i = NRW;
		while (strcmp(id, word[i--]));
		if (++i)
			sym = wsym[i]; // symbol is a reserved word
		else
			sym = SYM_IDENTIFIER;   // symbol is an identifier
	}
	else if (isdigit(ch))
	{ // symbol is a number.
		k = num = 0;
		sym = SYM_NUMBER;
		do
		{
			num = num * 10 + ch - '0';
			k++;
			getch();
		}
		while (isdigit(ch));
		if (k > MAXNUMLEN)
			error(25);     // The number is too great. //这里指的是数字位数过多
	}
	else if (ch == ':')
	{
		getch();
		if (ch == '=')
		{
			sym = SYM_BECOMES; // :=
			getch();
		}
		else
		{
			sym = SYM_NULL;       // illegal?
		}
	}
	else if (ch == '>')
	{
		getch();
		if (ch == '=')
		{
			sym = SYM_GEQ;     // >=
			getch();
		}
		else
		{
			sym = SYM_GTR;     // >
		}
	}
	else if (ch == '<')
	{
		getch();
		if (ch == '=')
		{
			sym = SYM_LEQ;     // <=
			getch();
		}
		else if (ch == '>')
		{
			sym = SYM_NEQ;     // <>
			getch();
		}
		else
		{
			sym = SYM_LES;     // <
		}
	}
	else if (ch == '-')
	{
		getch();
		if (ch == '>')
		{
			sym = SYM_ARROW;  // -> 新增
			getch();
		}
		else
		{
			sym = SYM_MINUS;  // -
		}
	}
	else 
	{ // other tokens
	/* 
		剩下的这些运算符不像上面那样单独一个else if是因为上面的那些需要看两个字符做判断(比如<=和<)
		剩下的都是看一个字符就能知道的类型的，直接对比预先定义好的数组中的字符就能判断类型
	*/
		i = NSYM;
		csym[0] = ch;
		while (csym[i--] != ch);
		if (++i)
		{
			sym = ssym[i];
			getch();
		}
		else
		{
			printf("Fatal Error: Unknown character.\n");
			exit(1);
		}
	}
} // getsym

//////////////////////////////////////////////////////////////////////
// generates (assembles) an instruction.生成指令加入code数组中，cx指示下一条指令的位置
void gen(int x, int y, int z)
{
	if (cx > CXMAX)
	{
		printf("Fatal Error: Program too long.\n");
		exit(1);
	}
	code[cx].f = x;
	code[cx].l = y;
	code[cx++].a = z;
} // gen

//////////////////////////////////////////////////////////////////////
// tests if error occurs and skips all symbols that do not belongs to s1 or s2.
// 虽然叫函数test，但是不能删掉，因为它还有带有错误恢复功能(编译出现error时，跳到下一个同步点继续分析)
void test(symset s1, symset s2, int n)
{
	symset s;

	if (! inset(sym, s1))
	{
		error(n);
		s = uniteset(s1, s2);
		while(! inset(sym, s))
			getsym();
		destroyset(s);
	}
} // test

//////////////////////////////////////////////////////////////////////

int dx;  // data allocation index //下一个分配的临时变量(下一个加入到符号表table中的var)相对于base的偏移，每个block中初始化为3，这是因为0,1,2三个位置分别存了三个内部变量SL,DL,RA(静态链,动态链,返回地址)
int argument_location; //下一个分配的参数相对于bp的偏移

// enter object(constant, variable or procedre) into table.
/*
	将常量(const)、变量(var)或过程(procedure)加入符号表(table)
	存放的信息分别为
	const    : kind, value
	var      : kind, level, address
	procedure: kind, level, address
	其中 procedure 的 address 不是在enter函数中就直接存的，是在block()的开头存的
*/
void enter(int kind)
{
	tx++;
	strcpy(table[tx].name, id);
	table[tx].kind = kind;
	switch (kind)
	{
	case ID_CONSTANT:
		if (num > MAXADDRESS)//这里指的是数字的大小过大，但是不知道为什么是MAXADDRESS(可能是立即数不能超过最大地址？)
		{
			error(25); // The number is too great. 
			num = 0;
		}
		table[tx].value = num;
		break;
	case ID_VARIABLE:
		table[tx].level = level;
		table[tx].address = dx++;
		break;
	case ID_PROCEDURE:
		table[tx].level = level;
		//table[tx].address不是在创建符号表条目时填入的
		break;
	case ID_ARGUMENT_VARIABLE:
		table[tx].level = level + 1;//参数属于子函数的层次，但是是在父函数中enter的
		table[tx].address = argument_location--;
		break;
	case ID_ARGUMENT_PROCEDURE:
		table[tx].level = level + 1;//参数属于子函数的层次，但是是在父函数中enter的
		table[tx].address = argument_location; //这里的address指的是这个参数在栈中相对bp的位置
		argument_location -= 2;//前一个位置(-0)存作为参数的函数的起始指令地址，后一个位置(-1)存它的静态链
	} // switch
} // enter

//////////////////////////////////////////////////////////////////////
// locates identifier in symbol table.获得指定符号在符号表中的位置(寻找变量、常量或过程)
int position(char* id)
{
	int i;
	strcpy(table[0].name, id);
	i = tx + 1;
	while (strcmp(table[--i].name, id) != 0);
	return i;
} // position

//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——constdeclaration
	格式为ident = number
	语义动作：将该常量名加入符号表
*/
void constdeclaration()
{
	if (sym == SYM_IDENTIFIER)
	{
		getsym();
		if (sym == SYM_EQU || sym == SYM_BECOMES)
		{
			if (sym == SYM_BECOMES)
				error(1); // Found ':=' when expecting '='.
			getsym();
			if (sym == SYM_NUMBER)
			{
				enter(ID_CONSTANT);
				getsym();
			}
			else
			{
				error(2); // There must be a number to follow '='.
			}
		}
		else
		{
			error(3); // There must be an '=' to follow the identifier.
		}
	} else	error(4);
	 // There must be an identifier to follow 'const', 'var', or 'procedure'.
} // constdeclaration

//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——constdeclaration
	格式为ident
	语义动作：将该变量名加入符号表
*/
void vardeclaration(void)
{
	if (sym == SYM_IDENTIFIER)
	{
		enter(ID_VARIABLE);
		getsym();
	}
	else
	{
		error(4); // There must be an identifier to follow 'const', 'var', or 'procedure'.
	}
} // vardeclaration

//////////////////////////////////////////////////////////////////////
void listcode(int from, int to)
{
	int i;
	
	printf("\n");
	for (i = from; i < to; i++)
	{
		printf("%5d %s\t%d\t%d\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
	}
	printf("\n");
} // listcode

//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——factor (对应pdf里的因子) <>表示可有可无
		ident						 生成LIT指令(常量时)或者根据level差生成LOD指令(变量时)
	或	number						 生成LIT指令
	或	(expression)				 
	或	-factor						 生成OPR指令(负)
	语义动作：
		对应分析式后面
*/
void factor(symset fsys)
{
	void expression(symset fsys);
	int i;
	symset set;
	
	test(facbegsys, fsys, 24); // The symbol can not be as the beginning of an expression.

	if (inset(sym, facbegsys))
	{
		if (sym == SYM_IDENTIFIER)
		{
			if ((i = position(id)) == 0)
			{
				error(11); // Undeclared identifier.
			}
			else
			{
				switch (table[i].kind)
				{
				case ID_CONSTANT:
					gen(LIT, 0, table[i].value);
					break;
				case ID_VARIABLE:
					gen(LOD, level - table[i].level, table[i].address);
					break;
				case ID_PROCEDURE:
					error(21); // Procedure identifier can not be in an expression.
					break;
				case ID_ARGUMENT_VARIABLE:
					gen(LOD, level - table[i].level, table[i].address);
					break;
				case ID_ARGUMENT_PROCEDURE:
					error(21); // Procedure identifier can not be in an expression.
					break;
				} // switch
			}
			getsym();
		}
		else if (sym == SYM_NUMBER)
		{
			if (num > MAXADDRESS)
			{
				error(25); // The number is too great.
				num = 0;
			}
			gen(LIT, 0, num);
			getsym();
		}
		else if (sym == SYM_LPAREN)
		{
			getsym();
			set = uniteset(createset(SYM_RPAREN, SYM_NULL), fsys);
			expression(set);
			destroyset(set);
			if (sym == SYM_RPAREN)
			{
				getsym();
			}
			else
			{
				error(22); // Missing ')'.
			}
		}
		else if(sym == SYM_MINUS) // UMINUS,  Expr -> '-' Expr
		{  
			 getsym();
			 factor(fsys);
			 gen(OPR, 0, OPR_NEG);
		}
		//test(fsys, createset(SYM_LPAREN, SYM_NULL), 23);
	} // if
} // factor

//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——term (对应pdf里的项) <>表示可有可无
		factor *(或/) factor *(或/)...... factor

	语义动作：
		生成OPR指令(乘或除)
*/
void term(symset fsys)
{
	int mulop;
	symset set;
	
	set = uniteset(fsys, createset(SYM_TIMES, SYM_SLASH, SYM_NULL));
	factor(set);
	while (sym == SYM_TIMES || sym == SYM_SLASH)
	{
		mulop = sym;
		getsym();
		factor(set);
		if (mulop == SYM_TIMES)
		{
			gen(OPR, 0, OPR_MUL);
		}
		else
		{
			gen(OPR, 0, OPR_DIV);
		}
	} // while
	destroyset(set);
} // term

//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——expression (对应pdf里的表达式) <>表示可有可无
		term +(或-) term +(或-)...... term

	语义动作：
		生成OPR指令(加或减)
*/
void expression(symset fsys)
{
	int addop;
	symset set;

	set = uniteset(fsys, createset(SYM_PLUS, SYM_MINUS, SYM_NULL));
	
	term(set);
	while (sym == SYM_PLUS || sym == SYM_MINUS)
	{
		addop = sym;
		getsym();
		term(set);
		if (addop == SYM_PLUS)
		{
			gen(OPR, 0, OPR_ADD);
		}
		else
		{
			gen(OPR, 0, OPR_MIN);
		}
	} // while

	destroyset(set);
} // expression

//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——condition (对应pdf里的条件) <>表示可有可无
		odd expression
	或	expression =(或<> < > <= >=) expression

	语义动作：
		生成不同的OPR指令
*/
void condition(symset fsys)
{
	int relop;
	symset set;

	if (sym == SYM_ODD)
	{
		getsym();
		expression(fsys);
		gen(OPR, 0, 6);
	}
	else
	{
		set = uniteset(relset, fsys);
		expression(set);
		destroyset(set);
		if (! inset(sym, relset))
		{
			error(20);
		}
		else
		{
			relop = sym;
			getsym();
			expression(fsys);
			switch (relop)
			{
			case SYM_EQU:
				gen(OPR, 0, OPR_EQU);
				break;
			case SYM_NEQ:
				gen(OPR, 0, OPR_NEQ);
				break;
			case SYM_LES:
				gen(OPR, 0, OPR_LES);
				break;
			case SYM_GEQ:
				gen(OPR, 0, OPR_GEQ);
				break;
			case SYM_GTR:
				gen(OPR, 0, OPR_GTR);
				break;
			case SYM_LEQ:
				gen(OPR, 0, OPR_LEQ);
				break;
			} // switch
		} // else
	} // else
} // condition

//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——statement (对应pdf里的语句) <>表示可有可无
		ident:=expression							  根据level差,生成STO指令
	或	call ident 									  根据level差,生成CAL指令
	或	begin statement;......statement end			  
	或	if condition then statement	<else statement>  生成JPZ指令(有回填步骤)
	或	while condition do statement				  生成JPZ指令(有回填步骤)，生成JMP指令
	或	do statement while condition				  新增
	或	return <expression>							  新增
	或	print <expression>							  新增
	语义动作：
		对应分析式后面
*/
void statement(symset fsys)
{
	int i, cx1, cx2;
	symset set1, set;

	if (sym == SYM_IDENTIFIER)
	{ // variable assignment
		if (! (i = position(id)))
		{
			error(11); // Undeclared identifier.
		}
		else if (table[i].kind != ID_VARIABLE && table[i].kind != ID_ARGUMENT_VARIABLE)
		{
			error(12); // Illegal assignment.
			i = 0;
		}
		getsym();
		if (sym == SYM_BECOMES)
		{
			getsym();
		}
		else
		{
			error(13); // ':=' expected.
		}
		expression(fsys);
		if (i)
		{
			gen(STO, level - table[i].level, table[i].address);
		}
	}
	else if (sym == SYM_CALL)
	{ // procedure call
		getsym();
		if (sym != SYM_IDENTIFIER)
		{
			error(14); // There must be an identifier to follow the 'call'.
		}
		else
		{
			i = position(id);
			getsym();
			if (! i)
			{
				error(11); // Undeclared identifier.
			}
			if (table[i].kind == ID_PROCEDURE || table[i].kind == ID_ARGUMENT_PROCEDURE)
			{
				if (sym == SYM_LPAREN)
				{
					getsym();
				}
				else
				{
					error(35);//'(' expected.
				}
				instruction temp_instruction[20];//临时的指令，最终需要倒序生成,因为之前已经限定最多十个参数，所以最多20条指令
				int temp_instruction_num = 0;
				for (int n = 0;n < table[i].argument.argumentNum;n++) //顺序生成临时参数入栈指令，以便后面倒序生成
				{
					if (table[i].argument.argumentType[n] == 0) //匹配一个var或number参数
					{
						if(sym == SYM_IDENTIFIER)
						{
							int j = position(id);
							if (!j)
							{
								error(11); // Undeclared identifier.
							}
							if (table[j].kind == ID_VARIABLE || table[j].kind == ID_ARGUMENT_VARIABLE)
							{
								//传参
								temp_instruction[temp_instruction_num].f = LOD;
								temp_instruction[temp_instruction_num].l = level - table[j].level;
								temp_instruction[temp_instruction_num].a = table[j].address;
								temp_instruction_num++;
							}
							else
							{
								error(37);//argument no match.
							}
						}
						else if(sym == SYM_NUMBER)
						{
							//传参
							temp_instruction[temp_instruction_num].f = LIT;
							temp_instruction[temp_instruction_num].l = 0;
							temp_instruction[temp_instruction_num].a = num;
							temp_instruction_num++;
						}
						else
						{
							error(36);//argument must be an identifier.
						}
					}
					else //匹配一个procedure参数
					{
						if (sym != SYM_IDENTIFIER)
						{
							error(36);//argument must be an identifier.
						}
						else
						{
							int j = position(id);
							if (!j)
							{
								error(11); // Undeclared identifier.
							}
							if (table[j].kind == ID_PROCEDURE)
							{	
								//传参，procedure的起始地址，编译时已知
								temp_instruction[temp_instruction_num].f = LIT;
								temp_instruction[temp_instruction_num].l = 0;
								temp_instruction[temp_instruction_num].a = table[j].address;
								temp_instruction_num++;
								//传参，根据层差取得procedure的静态链，即其父函数的bp
								temp_instruction[temp_instruction_num].f = LEA;
								temp_instruction[temp_instruction_num].l = level - table[j].level;
								temp_instruction[temp_instruction_num].a = 0;
								temp_instruction_num++;
							}
							else if (table[j].kind == ID_ARGUMENT_PROCEDURE)
							{
								//传参，起始地址
								temp_instruction[temp_instruction_num].f = LOD;
								temp_instruction[temp_instruction_num].l = level - table[j].level;
								temp_instruction[temp_instruction_num].a = table[j].address;
								temp_instruction_num++;
								//传参，静态链
								temp_instruction[temp_instruction_num].f = LOD;
								temp_instruction[temp_instruction_num].l = level - table[j].level;
								temp_instruction[temp_instruction_num].a = table[j].address - 1;
								temp_instruction_num++;
							}
							else
							{
								error(37);//argument no match.
							}
							getsym();
							if (sym == SYM_LPAREN)
							{
								getsym();
							}
							else
							{
								error(35);//'(' expected.
							}
							if (sym == SYM_RPAREN)
							{
								//getsym();
							}
							else
							{
								error(34);//')' expected.
							}
						}
					}
					getsym();
					if (n != table[i].argument.argumentNum - 1)
					{
						if (sym == SYM_COMMA)
						{
							getsym();
						}
						else
						{
							error(38);//',' expected.
						}
					}
				}
				for (int n = temp_instruction_num - 1;n >= 0;n--)
				{
					gen(temp_instruction[n].f, temp_instruction[n].l, temp_instruction[n].a);//倒序生成入栈指令，因为参数在栈中是反着排序的
				}
				if (sym == SYM_RPAREN)
				{
					getsym();
				}
				else
				{
					error(34);//')' expected.
				}
				if (table[i].kind == ID_PROCEDURE)
				{
					gen(CAL, level - table[i].level, table[i].address);
				}
				else //table[i].kind == ID_ARGUMENT_PROCEDURE
				{
					gen(LOD, level - table[i].level, table[i].address - 1); //静态链
					gen(LOD, level - table[i].level, table[i].address); //起始指令地址
					gen(DCAL, 0, 0);
				}
				int argumentStackSpace = 0;//统计参数所占的栈空间，便于后续归还
				for (int n = 0;n < table[i].argument.argumentNum;n++)
				{
					if (table[i].argument.argumentType[n] == 0) //var
					{
						argumentStackSpace += 1;
					}
					else
					{
						argumentStackSpace += 2;
					}
				}
				if (sym == SYM_ARROW) //带返回值的函数
				{
					if (table[i].returnvalve != 1)
					{
						error(42);//procedure has no return valve
					}
					getsym();
					if (sym == SYM_IDENTIFIER)
					{
						int return_var = position(id);
						if (!return_var)
						{
							error(11); // Undeclared identifier.
						}
						if (table[return_var].kind != ID_VARIABLE && table[return_var].kind != ID_ARGUMENT_VARIABLE)
						{
							error(43); //a variable type expected after ->.
						}
						gen(INT, 0, -argumentStackSpace); //释放参数占用的栈空间
						gen(MOV, argumentStackSpace+1, 2);//移动返回值到top+2，返回值一开始位于原本的bp中(即SL的位置)
						gen(LEA, level - table[return_var].level, table[return_var].address);//加载返回地址到top+1
						gen(INT, 0, 1);//将top指针移动到返回值位置
						gen(STOA, 0, 0);//存返回值到返回地址中
						getsym();
					}
					else
					{
						error(43); //a variable type expected after ->.
					}
				}
				else
				{
					gen(INT, 0, -argumentStackSpace); //释放参数占用的栈空间
				}
			}
			else
			{
				error(15); // A constant or variable can not be called. 
			}
		}
	} 
	else if (sym == SYM_IF)
	{ // if statement
		getsym();
		set1 = createset(SYM_THEN, SYM_DO, SYM_NULL);
		set = uniteset(set1, fsys);
		condition(set);
		destroyset(set1);
		destroyset(set);
		if (sym == SYM_THEN)
		{
			getsym();
		}
		else
		{
			error(16); // 'then' expected.
		}
		cx1 = cx;
		gen(JPZ, 0, 0); //false入口，目标地址待定
		statement(fsys);
		if (sym == SYM_ELSE)
		{
			getsym();
			cx2 = cx;
			gen(JMP, 0, 0); //出口，目标地址待定
			code[cx1].a = cx; //回填false入口目标地址
			statement(fsys);
			code[cx2].a = cx;
		}
		else
		{
			code[cx1].a = cx; //回填出口目标地址
		}
		
	}
	else if (sym == SYM_BEGIN)
	{ // block
		getsym();
		set1 = createset(SYM_SEMICOLON, SYM_END, SYM_NULL);
		set = uniteset(set1, fsys);
		statement(set);
		while (sym == SYM_SEMICOLON || inset(sym, statbegsys))
		{
			if (sym == SYM_SEMICOLON)
			{
				getsym();
			}
			else
			{
				error(10);
			}
			statement(set);
		} // while
		destroyset(set1);
		destroyset(set);
		if (sym == SYM_END)
		{
			getsym();
		}
		else
		{
			error(17); // ';' or 'end' expected.
		}
	}
	else if (sym == SYM_WHILE)
	{ // while statement
		cx1 = cx;
		getsym();
		set1 = createset(SYM_DO, SYM_NULL);
		set = uniteset(set1, fsys);
		condition(set);
		destroyset(set1);
		destroyset(set);
		cx2 = cx;
		gen(JPZ, 0, 0); //目标地址待定
		if (sym == SYM_DO)
		{
			getsym();
		}
		else
		{
			error(18); // 'do' expected.
		}
		statement(fsys);
		gen(JMP, 0, cx1);
		code[cx2].a = cx; //回填目标地址
	}
	else if (sym == SYM_DO)
	{ 
		// do while 新增
		getsym();
		cx1 = cx;
		statement(fsys);
		if (sym == SYM_WHILE)
		{
			getsym();
		}
		else
		{
			error(40);//'while' expected.
		}
		set = createset(SYM_NULL);//空集合，用于应付传参要求的，这里我已经不打算做错误恢复处理了
		condition(set);
		destroyset(set);
		gen(JNZ, 0, cx1);
	}
	else if (sym == SYM_RETURN)
	{
		//return 新增
		getsym();
		if (sym == SYM_IDENTIFIER || sym == SYM_NUMBER || sym == SYM_MINUS || sym == SYM_LPAREN) //带返回值返回,条件是下一个sym属于First(expression)
		{
			expression(fsys);
			gen(STO, 0, 0);//直接将expression的值存到当前bp的位置(约定的返回值位置)
			gen(RET, 0, 0);
		}
		else //直接返回
		{
			gen(RET, 0, 0);
		}
	}
	else if (sym == SYM_PRINT)
	{
		//print 新增
		getsym();
		if (sym == SYM_IDENTIFIER || sym == SYM_NUMBER || sym == SYM_MINUS || sym == SYM_LPAREN) //带返回值返回,条件是下一个sym属于First(expression)
		{
			expression(fsys);
			gen(PRN, 0, 0);//直接将expression的值打印并出栈销毁
		}
		else //直接返回
		{
			error(44);//expression expected after print.
		}
	}

	//test(fsys, phi, 19);
} // statement

//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——type_list 新增
		argument_list : (x y,x y,x y,....)
		x : var 或 procedure
		y : type_list 或 空
	语义动作：

	修改argument的子argument
*/
void type_list(arg* argument)
{
	getsym();
	if (sym == SYM_RPAREN) //空参数列表
	{
		getsym();
		return;
	}
	//非空参数列表
	while (1)
	{
		if (sym == SYM_VAR)
		{
			getsym();
			argument->argumentType[argument->argumentNum] = 0;
			argument->argumentNum++;
		}
		else if (sym == SYM_PROCEDURE)
		{
			argument->argumentType[argument->argumentNum] = 1;
			argument->child_arg[argument->argumentNum] = (arg*)malloc(sizeof(arg));
			argument->child_arg[argument->argumentNum]->argumentNum = 0;//通过malloc分配的argument不会自动初始化为默认值
			getsym();
			if (sym == SYM_LPAREN)
			{
				type_list(argument->child_arg[argument->argumentNum]);
			}
			argument->argumentNum++;
		}
		else
		{
			error(39);//'var' or 'procedure' expected in the type list.
		}
		if (sym != SYM_COMMA)
		{
			break;
		}
		else
		{
			getsym();
		}
	}
	if (sym == SYM_RPAREN)
	{
		getsym();
	}
	else
	{
		error(34); // ')' expected.
	}
}

//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——argument_list 新增
		argument_list : (ident x,ident x,ident x,....)
		x : type_list 或 空
	语义动作：

	enter参数到符号表，修改procedure表项的argument
*/
void argument_list(arg* argument)
{
	getsym();
	if (sym == SYM_RPAREN) //空参数列表
	{
		getsym();
		return;
	}
	//非空参数列表
	argument_location = -1;//第一个参数在 bp - 1 的位置
	while (1)
	{
		if (sym == SYM_IDENTIFIER)
		{
			getsym();
			if (sym != SYM_LPAREN) // var参数
			{
				enter(ID_ARGUMENT_VARIABLE);
				argument->argumentType[argument->argumentNum] = 0;
				argument->argumentNum++;
			}
			else // procedure参数
			{
				enter(ID_ARGUMENT_PROCEDURE);
				argument->argumentType[argument->argumentNum] = 1;
				argument->child_arg[argument->argumentNum] = (arg*)malloc(sizeof(arg));
				argument->child_arg[argument->argumentNum]->argumentNum = 0;//通过malloc分配的argument不会自动初始化为默认值
				type_list(argument->child_arg[argument->argumentNum]);
				table[tx].argument = *(argument->child_arg[argument->argumentNum]);
				argument->argumentNum++;
			}
		}
		else
		{
			error(33);//identifier expected in the argument list.
		}
		if (sym != SYM_COMMA)
		{
			break;
		}
		else
		{
			getsym();
		}
	}
	if (sym == SYM_RPAREN)
	{
		getsym();
	}
	else
	{
		error(34); // ')' expected.
	}
}


//////////////////////////////////////////////////////////////////////
/*
	LL1分析程序——block (对应pdf里的程序体) <>表示可有可无
		<const constdeclaration(,或;)......constdeclaration;>
		<var vardeclaration(,或;)......vardeclaration;>
		<procedure ident<(....)><->var>;block;>
		statement
	语义动作：
		生成JMP指令
		将常量、变量、子程序的声明加入符号表(如果有的话)
		生成INT指令（分析statement之前）
		生成返回指令（分析statement之后）

	参数procedure_tx对应本block的procedure条目在table中的位置
*/
void block(symset fsys, int procedure_tx)
{
	int cx0; // initial code index
	int block_dx;//进入分析子block之前把父block的dx存起来，分析完子block后再取回，dx的作用具体见dx的注释
	int savedTx;//进入分析子block之前把父block的tx存起来，分析完子block后再取回，这么做是因为子块中新增的table条目出了子块就无效了
	symset set1, set;

	dx = 3;
	/*
		下面这两行代码的解释：将下一条要产生的指令的位置存入符号表中对应本block的procedure条目的address中，也就是给符号表中的procedure填入它第一条指令的位置
		如果本block是主函数，则存入符号表第0项的address中，table的第0项默认是空的，新加入的项从第1项开始，所以不会被覆盖（其实可以把第0项看成是主函数的procedure项，只不过里面只填了address）
		如果本block不是主函数，则进入block的分析函数之前一定已经在符号表中创建了procedure的项，且刚好就是上一条，但是还没存入address，此时存入
	*/
	table[procedure_tx].address = cx;
	/*
		block产生的第一条指令一定是无条件跳转指令：跳转到该block对应的过程的地址，这里跳转到0只是占位，后面会修改
		这么做是因为block中可能里面声明了子block，那么子block的指令会先于本blcok的指令生成
		而符号表中block对应的procedure只存了该block的起始指令位置，这些指令是包括子block的，子block的指令是需要在call时才运行的
		为了能正确定位到本block的指令而不是子block的指令，需要在分析子block结束后(如果有的话)把第一条跳转指令的目标地址改成下一条指令的地址
		从而程序运行时，调用函数时可以先从符号表找起始指令位置，然后再跳转到正确位置
		最终生成的block指令的大致顺序如下
		指令位置		指令
		a			JMP   0   a+n
		a+1			......(属于子block)
		a+2			......(属于子block)
					......(属于子block)
		a+n			INT...(本blcok的指令)
		......		......
	*/
	gen(JMP, 0, 0); 
	if (level > MAXLEVEL)
	{
		error(32); // There are too many levels.
	}
	do
	{
		if (sym == SYM_CONST)
		{ // constant declarations
			getsym();
			do
			{
				constdeclaration();
				while (sym == SYM_COMMA)
				{
					getsym();
					constdeclaration();
				}
				if (sym == SYM_SEMICOLON)
				{
					getsym();
				}
				else
				{
					error(5); // Missing ',' or ';'.
				}
			}
			while (sym == SYM_IDENTIFIER);
		} // if

		if (sym == SYM_VAR)
		{ // variable declarations
			getsym();
			do
			{
				vardeclaration();
				while (sym == SYM_COMMA)
				{
					getsym();
					vardeclaration();
				}
				if (sym == SYM_SEMICOLON)
				{
					getsym();
				}
				else
				{
					error(5); // Missing ',' or ';'.
				}
			}
			while (sym == SYM_IDENTIFIER);
		} // if
		while (sym == SYM_PROCEDURE)
		{ // procedure declarations
			block_dx = dx;//save dx before handling procedure call!
			

			int next_procedure_tx;
			getsym();
			if (sym == SYM_IDENTIFIER)
			{
				enter(ID_PROCEDURE);
				next_procedure_tx = tx;
				getsym();
			}
			else
			{
				error(4); // There must be an identifier to follow 'const', 'var', or 'procedure'.
			}

			savedTx = tx;//需要放到procedure条目enter之后

			if (sym == SYM_LPAREN) //带参数的过程
			{
				argument_list(&table[tx].argument);
			}

			if (sym == SYM_ARROW) //带返回值
			{
				getsym();
				if (sym == SYM_VAR)
				{
					getsym();
					table[next_procedure_tx].returnvalve = 1;
				}
				else
				{
					error(41);//'var' expected after ->.
				}
			}

			if (sym == SYM_SEMICOLON)
			{
				getsym();
			}
			else
			{
				error(5); // Missing ',' or ';'.
			}

			level++;
			//savedTx = tx;这句移动到前面了，因为前面enter了参数，子模块分析结束时时也需要删去
			set1 = createset(SYM_SEMICOLON, SYM_NULL);
			set = uniteset(set1, fsys);
			block(set,next_procedure_tx);
			destroyset(set1);
			destroyset(set);
			tx = savedTx;
			level--;

			if (sym == SYM_SEMICOLON)
			{
				getsym();
				set1 = createset(SYM_IDENTIFIER, SYM_PROCEDURE, SYM_NULL);
				set = uniteset(statbegsys, set1);
				test(set, fsys, 6);
				destroyset(set1);
				destroyset(set);
			}
			else
			{
				error(5); // Missing ',' or ';'.
			}
			dx = block_dx; //restore dx after handling procedure call!
		} // while
		set1 = createset(SYM_IDENTIFIER, SYM_NULL);
		set = uniteset(statbegsys, set1);
		test(set, declbegsys, 7);
		destroyset(set1);
		destroyset(set);
	}
	while (inset(sym, declbegsys));

	code[table[procedure_tx].address].a = cx;//修改block的第一条指令(JMP)的跳转目标，详见前面的注释
	table[procedure_tx].address = cx;//修改符号表关于本block的procedure的address，避免了每次都需要经过第一个jmp指令跳转(主函数避免不了)
	cx0 = cx;
	gen(INT, 0, dx);//生成指令：分配dx大小的运行栈空间
	set1 = createset(SYM_SEMICOLON, SYM_END, SYM_NULL);
	set = uniteset(set1, fsys);
	statement(set);
	destroyset(set1);
	destroyset(set);
	//gen(OPR, 0, OPR_RET); // return
	gen(RET, 0, 0);
	test(fsys, phi, 8); // test for error: Follow the statement is an incorrect symbol.
	listcode(cx0, cx);
} // block

//////////////////////////////////////////////////////////////////////
//根据层次差，进行0次、一次或多次的静态连回溯，找到对应层次的基址
int base(int stack[], int currentLevel, int levelDiff)
{
	int b = currentLevel;
	
	while (levelDiff--)
		b = stack[b];
	return b;
} // base

//////////////////////////////////////////////////////////////////////
// interprets and executes codes.
void interpret()
{
	int pc;        // program counter
	int stack[STACKSIZE];
	int top;       // top of stack
	int b;         // program, base, and top-stack register 基址寄存器
	instruction i; // instruction register

	printf("Begin executing PL/0 program.\n");

	pc = 0;
	b = 0; //基址寄存器，指向当前SL
	top = 0; //栈中0, 1, 2三个位置分别存三个内部变量SL, DL, RA(静态链,动态链,返回地址),但是栈空间应该由被调函数开辟，所以即使提前在栈上存入了三个量，top仍然初始化为0
	stack[1] = stack[2] = stack[3] = 0;
	do
	{
		if (top > STACKSIZE)
		{
			printf("stack overflow\n");
			exit(-1);
		}
		i = code[pc++];
		printf("%s\t%d\t%d\n", mnemonic[i.f], i.l, i.a);//新增打印信息，用于输出指令执行的顺序
		switch (i.f)
		{
		case LIT:
			stack[++top] = i.a;
			break;
		case RET: //新增，因为一开始返回用OPR很奇怪，不过一开始的方式并没有删除，仍然可以通过OPR返回
			top = b - 1;
			pc = stack[top + 3];
			b = stack[top + 2];
			break;
		case OPR:
			switch (i.a) // operator
			{
			case OPR_RET:
				top = b - 1;
				pc = stack[top + 3];
				b = stack[top + 2];
				break;
			case OPR_NEG:
				stack[top] = -stack[top];
				break;
			case OPR_ADD:
				top--;
				stack[top] += stack[top + 1];
				break;
			case OPR_MIN:
				top--;
				stack[top] -= stack[top + 1];
				break;
			case OPR_MUL:
				top--;
				stack[top] *= stack[top + 1];
				break;
			case OPR_DIV:
				top--;
				if (stack[top + 1] == 0)
				{
					fprintf(stderr, "Runtime Error: Divided by zero.\n");
					fprintf(stderr, "Program terminated.\n");
					continue;
				}
				stack[top] /= stack[top + 1];
				break;
			case OPR_ODD:
				stack[top] %= 2;
				break;
			case OPR_EQU:
				top--;
				stack[top] = stack[top] == stack[top + 1];
				break;
			case OPR_NEQ:
				top--;
				stack[top] = stack[top] != stack[top + 1];
				break;
			case OPR_LES:
				top--;
				stack[top] = stack[top] < stack[top + 1];
				break;
			case OPR_GEQ:
				top--;
				stack[top] = stack[top] >= stack[top + 1];
				break;
			case OPR_GTR:
				top--;
				stack[top] = stack[top] > stack[top + 1];
				break;
			case OPR_LEQ:
				top--;
				stack[top] = stack[top] <= stack[top + 1];
				break;
			} // switch
			break;
		case LOD:
			stack[++top] = stack[base(stack, b, i.l) + i.a];
			break;
		case LEA: //新增，取变量的地址
			stack[++top] = base(stack, b, i.l) + i.a;
			break;
		case LODA: //新增，读取某地址的值
			stack[top] = stack[stack[top]];
			break;
		case STOA: //新增，存入某值到某个地址
			stack[stack[top - 1]] = stack[top];
			top -= 2;
			break;
		case MOV: //新增，将栈中某值复制到另一处，栈指针不移动
			stack[top + i.a] = stack[top + i.l];
			break;
		case STO:
			stack[base(stack, b, i.l) + i.a] = stack[top];
			//printf("  :=  %d\n", stack[top]);//打印单独改到PRN指令中
			top--;
			break;
		case CAL:
			stack[top + 1] = base(stack, b, i.l);
			// generate new block mark
			stack[top + 2] = b;
			stack[top + 3] = pc;
			b = top + 1;
			pc = i.a;
			break;
		case DCAL: //新增，动态指定静态链和返回地址版本的CAL，调用前先在栈顶按顺序存静态链和返回地址
			//stack[top - 1] = stack[top - 1]; //静态链SL
			stack[top + 1] = pc; //返回地址
			pc = stack[top]; //起始地址
			stack[top] = b; //动态链DL
			top--;
			b = top;
			break;
		case INT:
			top += i.a;
			break;
		case JMP:
			pc = i.a;
			break;
		case JPZ:
			if (stack[top] == 0)
				pc = i.a;
			top--;
			break;
		case JNZ:
			if (stack[top] != 0)
				pc = i.a;
			top--;
			break;
		case PRN:
			printf("print: %d\n", stack[top]);
			top--;
			break;
		} // switch
	}
	while (pc);

	printf("End executing PL/0 program.\n");
} // interpret

//////////////////////////////////////////////////////////////////////
void main ()
{
	FILE* hbin;
	char s[80];
	int i;
	symset set, set1, set2;

	//printf("Please input source file name: "); // get file name to be compiled
	//scanf("%s", s);
	//if ((infile = fopen(s, "r")) == NULL)
	//{
	//	printf("File %s can't be opened.\n", s);
	//	exit(1);
	//}
	if ((infile = fopen("input/5.txt", "r")) == NULL)
	{
		printf("File %s can't be opened.\n", s);
		exit(1);
	}

	/*
		这几行都是创建一些集合，注意SYM_NULL是写在末尾表示没有参数了，不会进入集合中
		这些集合都是用来进行错误诊断时，跳过错误部分用的
	*/

	//空集
	phi = createset(SYM_NULL);
	//关系运算符集合
	relset = createset(SYM_EQU, SYM_NEQ, SYM_LES, SYM_LEQ, SYM_GTR, SYM_GEQ, SYM_NULL);
	
	// create begin symbol sets
	//声明(declaration)的First集合
	declbegsys = createset(SYM_CONST, SYM_VAR, SYM_PROCEDURE, SYM_NULL);
	//语句(statement)的First集合
	statbegsys = createset(SYM_BEGIN, SYM_CALL, SYM_IF, SYM_WHILE, SYM_NULL);
	//因子(factor)的First集合
	facbegsys = createset(SYM_IDENTIFIER, SYM_NUMBER, SYM_LPAREN, SYM_MINUS, SYM_NULL);

	err = cc = cx = ll = 0; // initialize global variables
	ch = ' ';
	kk = MAXIDLEN;

	getsym();

	table[0].kind = table[0].level = table[0].value = table[0].address = -1;//用于debug时提醒该条目仅用于比较，不用于存储符号

	set1 = createset(SYM_PERIOD, SYM_NULL); //FOLLOW集增加{.}
	set2 = uniteset(declbegsys, statbegsys); //同步符号集增加{const,var,procedure,begin,call,if,while}
	set = uniteset(set1, set2);
	block(set,0);
	destroyset(set1);
	destroyset(set2);
	destroyset(set);
	destroyset(phi);
	destroyset(relset);
	destroyset(declbegsys);
	destroyset(statbegsys);
	destroyset(facbegsys);

	if (sym != SYM_PERIOD)
		error(9); // '.' expected.
	if (err == 0)
	{
		hbin = fopen("hbin.txt", "w");
		for (i = 0; i < cx; i++)
			fwrite(&code[i], sizeof(instruction), 1, hbin);
		fclose(hbin);
	}
	printf("ALL instruction:");
	listcode(0, cx);//列出完整指令
	if (err == 0)
		interpret();
	else
		printf("There are %d error(s) in PL/0 program.\n", err);
	
} // main

//////////////////////////////////////////////////////////////////////
// eof pl0.c
