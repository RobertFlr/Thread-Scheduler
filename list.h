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
} node_t, *list_t;

list_t alloc_cell(void* x);

#endif