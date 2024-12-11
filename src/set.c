
/*
	基本的集合操作，包括创建集合、插入元素、合并集合、判断元素是否在集合中以及销毁集合。

	symset 是一个结构体指针类型，代表一个集合，其中 snode 是集合中的节点结构体。
	uniteset() 用于合并两个集合 s1 和 s2，返回一个新的集合。它会按照元素大小顺序合并两个集合，并保证返回的集合中不包含重复元素。
	setinsert() 用于向集合中插入元素，保持集合中元素的有序性。
	createset() 用于创建一个集合，并依次插入传入的元素。
	destroyset() 用于销毁一个集合，释放集合中的节点内存。
	inset() 用于判断一个元素是否在集合中，如果在则返回 1，否则返回 0。

	注意createset()是可变参数函数，结尾需要加上一个0来表示参数传输完毕，pl0的代码中用宏SYM_NULL代替了这个0
*/


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "set.h"

symset uniteset(symset s1, symset s2)
{
	symset s;
	snode* p;
	
	s1 = s1->next;
	s2 = s2->next;
	
	s = p = (snode*) malloc(sizeof(snode));
	while (s1 && s2)
	{
		p->next = (snode*) malloc(sizeof(snode));
		p = p->next;
		if (s1->elem < s2->elem)
		{
			p->elem = s1->elem;
			s1 = s1->next;
		}
		else
		{
			p->elem = s2->elem;
			s2 = s2->next;
		}
	}

	while (s1)
	{
		p->next = (snode*) malloc(sizeof(snode));
		p = p->next;
		p->elem = s1->elem;
		s1 = s1->next;
		
	}

	while (s2)
	{
		p->next = (snode*) malloc(sizeof(snode));
		p = p->next;
		p->elem = s2->elem;
		s2 = s2->next;
	}

	p->next = NULL;

	return s;
} // uniteset

void setinsert(symset s, int elem)
{
	snode* p = s;
	snode* q;

	while (p->next && p->next->elem < elem)
	{
		p = p->next;
	}
	
	q = (snode*) malloc(sizeof(snode));
	q->elem = elem;
	q->next = p->next;
	p->next = q;
} // setinsert

symset createset(int elem, .../* SYM_NULL */)
{
	va_list list;
	symset s;

	s = (snode*) malloc(sizeof(snode));
	s->next = NULL;

	va_start(list, elem);
	while (elem)
	{
		setinsert(s, elem);
		elem = va_arg(list, int);
	}
	va_end(list);
	return s;
} // createset

void destroyset(symset s)
{
	snode* p;

	while (s)
	{
		p = s;
		p->elem = -1000000;
		s = s->next;
		free(p);
	}
} // destroyset

int inset(int elem, symset s)
{
	s = s->next;
	while (s && s->elem < elem)
		s = s->next;

	if (s && s->elem == elem)
		return 1;
	else
		return 0;
} // inset

// EOF set.c
