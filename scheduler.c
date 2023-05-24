#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include "list.h"
#include "so_scheduler.h"


#define FAIL_INIT_TID ((tid_t)0)

#define LOCKED  (0)
#define UNLOCKED (1)

#define INVALID_WAIT (-1)

#define INIT_FAIL   (-1)
#define INIT_SUCCESS    (0)

#define NEW_STATE (0)
#define READY_STATE (1)
#define RUNNING_STATE (2)
#define WAITING_STATE (3)
#define TERMINATED_STATE (4)

/* thread struct */
typedef struct thread{

    tid_t id;
    unsigned int quant;
    unsigned int priority;
    so_handler* handler;
    unsigned int state;
    sem_t sem;

}thread_t;

/* scheduler struct */
typedef struct scheduler{

    unsigned int quant;
    unsigned int io;
    struct thread_manager *thread_m;
    struct thread* running_thread;
    sem_t sem_end;

}scheduler_t;

/* helps manage and store threads */
typedef struct thread_manager{

    struct node *term_list;
    struct node *ready_list;
    unsigned int max_ready_prio;
    struct node *waiting_list;

}thread_manager_t;

/* declare scheduler */
struct scheduler *S = NULL;

/* insert in manager based on state */
void insert_thread(thread_manager_t* thread_m, thread_t* thread){
    /* create thread's cell */
    node_t *new_cell = alloc_cell(thread);
    list_t previous = NULL;
    unsigned int inserted = 0;

    /* insert in ready list */
    if (thread->state == READY_STATE){

        if(thread->priority > thread_m->max_ready_prio)
            thread_m->max_ready_prio = thread->priority;

        struct node* ready_l = thread_m->ready_list;

        if (ready_l == NULL){
            thread_m->ready_list = new_cell;
            return; 
        }

        for(list_t i = ready_l; i != NULL; previous = i, i = i->next){
            
            if( ((thread_t*)(i->data))->priority > thread->priority && previous != NULL ){
                previous->next = new_cell;
                new_cell->next = i;
                inserted = 1;
                break;
            }
            
            if( ((thread_t*)(i->data))->priority > thread->priority && previous == NULL){
                thread_m->ready_list = new_cell;
                new_cell->next = i;
                inserted = 1;
                break;
            }
        }
        if(!inserted){
            previous->next = new_cell;
            new_cell->next = NULL;
        }
        return;
    }

    /* insert in waiting list */
    if (thread->state == WAITING_STATE){
        
        list_t waiting_l = thread_m->waiting_list;
        if (waiting_l == NULL){
            thread_m->waiting_list = new_cell;
            return; 
        }
        
        for(list_t i = waiting_l; i != NULL; previous = i, i = i->next);
        previous->next = new_cell;
        return;
    }

    /* insert in terminated list */
    if (thread->state == TERMINATED_STATE){

        list_t term_l = thread_m->term_list;
        if(term_l == NULL){
            thread_m->term_list = new_cell;
            return;
        }

        for(list_t i = term_l; i != NULL; previous = i, i = i->next);
        previous->next = new_cell;
        return;
    }
}

/* removes thread from ready list */
void remove_thread_ready(unsigned int state){

    list_t i, previous = NULL;
    list_t ready_l = S->thread_m->ready_list;
    
    /* search and remove thread */
    for(i = ready_l; i != NULL; previous = i, i = i->next){
        if(((thread_t*)(i->data))->state == state){
            break;
        }
    }
    
    if(previous == NULL){
        S->thread_m->ready_list = S->thread_m->ready_list->next;
        ready_l->next = NULL;
        ready_l->data = NULL;
        free(ready_l);
        S->thread_m->max_ready_prio = 0;
        return;
    }
    previous->next = i->next;

    /* free old cell */
    i->next = NULL;
    i->data = NULL;
    free(i);

    return;
}

/* inserts threads waiting on IO in the ready list */
int wakeup_threads(unsigned int io){

    list_t waiting_list = S->thread_m->waiting_list;
    unsigned int woke_count = 0;
    if(waiting_list == NULL){
        return 0;
    }

    list_t i = waiting_list, previous = NULL;
    while(i != NULL){
        if(((thread_t*)(i->data))->quant == io){
            /* get thread redy */
            thread_t *woke_thread = (thread_t*)(i->data);
            woke_thread->quant = S->io;
            woke_thread->state = READY_STATE;
            /* insert in ready list */
            insert_thread(S->thread_m, woke_thread);
            woke_count++;
            /* remove from waiting list */
            if(previous == NULL){
                S->thread_m->waiting_list = S->thread_m->waiting_list->next;
                i->next = NULL;
                free(i);
                i = S->thread_m->waiting_list;
                continue;
            }
            previous->next = i->next;
            i->next = NULL;
            free(i);
            i = previous->next;
        }else{
            previous = i;
            i = i->next;
        }
    }
    return woke_count;
}

/* update max priority after remove */
void remake_max_prio(){

    if(S->thread_m->ready_list == NULL){
        S->thread_m->max_ready_prio = 0;
        return;
    }

    list_t i = S->thread_m->ready_list;
    while(i != NULL){
        S->thread_m->max_ready_prio = ((thread_t*)(i->data))->priority;
        i = i ->next;
    }
}

/* gets highest priority thread from ready list */
thread_t* get_next_thread(thread_manager_t* thread_m){
    
    if(thread_m->ready_list == NULL){
        return NULL;
    }
    
    for(list_t i = thread_m->ready_list; i != NULL; i = i->next){
        if(((thread_t*)(i->data))->priority == thread_m->max_ready_prio){
            return (thread_t*)(i->data);
        }
    }
    return NULL;
}

/* schedule function */
void schedule(){    
    
    thread_t* next_thread = get_next_thread(S->thread_m);
    
    /* run first thread */
    if(S->running_thread == NULL){        
        S->running_thread = next_thread;
        next_thread->state = RUNNING_STATE;
        remove_thread_ready(RUNNING_STATE);
        sem_post(&(S->running_thread->sem));
        return;
    }

    /* last thread alive */
    if(next_thread == NULL){
        
        /* all threads terminated, open so_end()'s semaphore */
        if(S->running_thread->state == TERMINATED_STATE){
            insert_thread(S->thread_m, S->running_thread);
            sem_post(&(S->sem_end));
            return; 
        }
        
        /* run it again */
        if(S->running_thread->quant == 0){
            S->running_thread->quant = S->quant;
        }
        return;
    }

    /* running thread blocked */
    if(S->running_thread->state == WAITING_STATE){

        thread_t* waiting = S->running_thread;
        /* use quant as a temporary placeholder for the I/O */
        insert_thread(S->thread_m, waiting);
        S->running_thread = next_thread;
        next_thread->state = RUNNING_STATE;

        remove_thread_ready(RUNNING_STATE);
        remake_max_prio();

        sem_post(&(S->running_thread->sem));
        sem_wait(&(waiting->sem));
        return;
    }


    /* running thread preempted */
    if(next_thread->priority > S->running_thread->priority || 
        S->running_thread->quant == 0 || S->running_thread->state == TERMINATED_STATE ){
    
        thread_t* preempted = S->running_thread;
        if(preempted->state != TERMINATED_STATE){
            preempted->state = READY_STATE;
        }

        preempted->quant = S->quant;

        /* insert preempted in terminated/ready list */
        insert_thread(S->thread_m, preempted);

        next_thread = get_next_thread(S->thread_m);
        S->running_thread = next_thread;
        next_thread->state = RUNNING_STATE;

        /* remake ready list */
        remove_thread_ready(RUNNING_STATE);
        remake_max_prio();

        /* if preempted terminated, let it die */
        if(preempted->state == TERMINATED_STATE){
            sem_post(&(preempted->sem));
        }
        sem_post(&(S->running_thread->sem));
        sem_wait(&(preempted->sem));
        return;
    }
}

/* initializes params of thread struct */
thread_t* newtherad_init(so_handler *handler, unsigned int priority){

    thread_t *new_thread = malloc(sizeof(struct thread));
    if(new_thread == NULL)
        return NULL;
    
    new_thread->handler = handler;
    new_thread->quant = S->quant;
    new_thread->priority = priority;
    new_thread->state = NEW_STATE;
    
    sem_init(&(new_thread->sem), 0, LOCKED);

    return new_thread;
}

/* initializes scheduler */
int so_init(unsigned int quant, unsigned int io){

    if(quant == 0 || io > SO_MAX_NUM_EVENTS)
        return INIT_FAIL; 

    if(S != NULL)
        return INIT_FAIL;
    
    S = malloc(sizeof(struct scheduler));
    if(S == NULL)   
        return INIT_FAIL;

    S->thread_m = malloc(sizeof(struct thread_manager));
    if(S->thread_m == NULL){
        free(S);
        S = NULL;
        return INIT_FAIL;
    }

    sem_init(&(S->sem_end), 0, UNLOCKED);

    S->thread_m->term_list = NULL;
    S->thread_m->waiting_list = NULL;
    S->thread_m->ready_list = NULL;
    S->thread_m->max_ready_prio = 0;

    S->running_thread = NULL;
    S->quant = quant;
    S->io = io;

    return INIT_SUCCESS;
}

/* creates new thread */
tid_t so_fork(so_handler *handler, unsigned int priority){

    if (handler == 0 || priority > SO_MAX_PRIO)
        return INVALID_TID;

    tid_t temp_id;
    thread_t *new_thread = newtherad_init(handler, priority);

    if(new_thread == NULL)
        return FAIL_INIT_TID;

    /* checks for first fork */
    if(S->running_thread != NULL){
        S->running_thread->quant--;
    }else{
        sem_wait(&(S->sem_end));
    }

    /* creates actual thread */
    pthread_create(&temp_id, NULL, start_thread, (void*)(new_thread));

    new_thread->id = temp_id;
    new_thread->state = READY_STATE;
    insert_thread(S->thread_m, new_thread);

    /* do RR reschedule */
    schedule();

    return new_thread->id;
}

/* blocks thread in waiting state */
int so_wait(unsigned int io){

    if(io >= S->io){
        return INVALID_WAIT;
    }
    /* uses quant as a temporary placeholder for the IO */
    S->running_thread->state = WAITING_STATE;
    S->running_thread->quant = io;

    /* do RR reschedule */
    schedule();

    return 0;
}

/* unblocks threads */
int so_signal(unsigned int io){

    if(io >= S->io){
        return INVALID_WAIT;
    }
    unsigned int woke_count = 0;
    S->running_thread->quant--;
    woke_count = wakeup_threads(io);

    /* do RR reschedule */
    schedule();

    return woke_count;
}

/* does nothing */
void so_exec(void){

    S->running_thread->quant--;

    /* do RR reschedule */
    schedule();
}

/* joins threads, destroys schduler */
void so_end(void){

    if(S == NULL){
        return;
    }

    /* wait for leave condition */
    sem_wait(&(S->sem_end));

    /* join all the threads and destroy the list*/
    if(S->thread_m->term_list != NULL){
        for(list_t i = S->thread_m->term_list; i != NULL; i = i->next){
            thread_t *temp = (thread_t*)(i->data);
            pthread_join(temp->id, NULL);
        }
        for(list_t i = S->thread_m->term_list; i != NULL;){
            list_t aux = i->next;
            thread_t *temp = (thread_t*)(i->data);
            sem_destroy(&(temp->sem));
            free(temp);
            free(i);
            i = aux;
        }
    }
    free(S->thread_m);        
    sem_destroy(&(S->sem_end));
    free(S);
    S = NULL;
    return;
}

/* thread routine function */
void *start_thread(void *arg){

    thread_t *thread = (thread_t*)arg;
    
    sem_wait(&(thread->sem));

    thread->handler(thread->priority);
    thread->state = TERMINATED_STATE;
    
    /* do RR reschedule */
    schedule();

    return NULL;
}
