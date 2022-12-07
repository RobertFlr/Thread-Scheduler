/*-- generic list --*/
#include "list.h"

list_t alloc_cell(void* x)
{
	list_t aux;
  	aux = (list_t)malloc(sizeof(node_t));
   	if(!aux) return NULL;
   	
	aux->data = x;
	aux->next = NULL;
	return aux;
}