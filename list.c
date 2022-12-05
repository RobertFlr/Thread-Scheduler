/*-- generic list functions --*/
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

int insert_list(list_t* list_address, void* element_address)
{
	list_t aux = alloc_cell(element_address);
	if(!aux)
	    return 0;

	aux->next = *list_address;
	*list_address = aux;

	return 1;
}

void destroy_list(list_t* list_address, void (*free_element)(void*))
{
	list_t aux;
	while(*list_address != NULL)
    	{
        	aux = *list_address;
        	if (!aux)
            	return;

        	free_element(aux->data);
        	*list_address = aux->next;
        	free(aux);
    	}
}


void test_list(list_t list, void (*out)(void*))
{
        if(!list) {
           printf("empty list\n");
           return;
        }

	printf("list:[");
	for(; list; list = list->next) 
		out(list->data);
    
	printf("]\n");
}