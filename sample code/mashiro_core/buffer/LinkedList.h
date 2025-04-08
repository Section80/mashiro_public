#pragma once

#ifndef _LINKED_LIST_
#define _LINKED_LIST_

#include "MashiroGlobal.h"

struct Link
{
	Link();

	Link* pPrev;		// left? 
	Link* pNext;		// right? 
	void* pItem;

};

// pHead == pLast == nullptr: 비어있는 경우
// pHead == pLast != nullptr: 하나만 있는 경우
// pHead != pLast: 여러개인 경우
struct LinkedList
{
	LinkedList();

	int		count;
	Link*	pHead;
	Link*	pLast;

};
void	AppendToList(LinkedList* pList, Link* pLink);						// as last
void	PrependToList(LinkedList* pList, Link* pLink);						// as first
void	InsertNextToList(LinkedList* pList, Link* pPrev, Link* pLink);		// pPrev 뒤에 pLink를 넣는다. 
void	InsertPrevToList(LinkedList* pList, Link* pNext, Link* pLink);		// pNext 앞에 pLink를 넣는다. 
Link*	PopFirstFromList(LinkedList* pList);
Link*	PopLastFromList(LinkedList* pList);
void    UnlinkFromList(LinkedList* pList, Link* pLink);
void	MergeList(LinkedList* pSrc, LinkedList* pDest);						// pSource의 Link들을 전부 pDest로 옮긴다. 

// op: less (<)
// true: pLink1 is Less
// false: pLink2 is Less
// pItem을 직접 줄 수도 있지만, 
// Link 클래스를 확장하여 weight 값을 저장할 수도 있음으로
// Link pointer를 주도록 하였다. 
using SortFunc = bool(*)(Link* pLink1, Link* pLink2);

// 오름차순
// pHead: smallest
// pLast: biggest
struct SortedLinkedList : LinkedList
{
	SortedLinkedList(SortFunc sortFunc);

	SortFunc		sortFunc;

	// int				count;
	// Link*			pHead;
	// Link*			pLast;

};
void PushToSortedList(SortedLinkedList* pList, Link* pLink);		// 무식하게 iterate 한다. 
// 가장 작은 것을 pop할 때는 PopFirstFromList() 사용
// 가장 큰 것을 pop할 때는 PopLastFromList() 사용

// TODO: BinaryTree 구현? 

#endif // !_LINKED_LIST_
