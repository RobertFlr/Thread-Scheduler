/*-- generic list structure implementation --*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifndef _LISTA_GENERICA_
#define _LISTA_GENERICA_

typedef struct node
{
  void* data;           
  struct node *next;  
} node_t, *list_t; 	/* tipurile Celula, Lista generice */


/* functii lista generica */
list_t alloc_cell(void* x);

int insert_list(list_t*, void*);  /*- inserare la inceput reusita sau nu (1/0) -*/

void distroy_list(list_t* list_address, void (*free_f)(void*)); /* distruge lista */

/* afiseaza elementele din lista, folosind o functie de Afișare element specific*/
void test_list(list_t, void (*free_element)(void*));

#endif