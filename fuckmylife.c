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

#define NEW_STATE	(0)
#define READY_STATE (1)
#define RUNNING_STATE (2)
#define WAITING_STATE (3)
#define TERMINATED_STATE (4)

/* structure for thread */
typedef struct thread{
    tid_t id;
    unsigned int quant;
    unsigned int priority;
    so_handler* handler;
    unsigned int state;
    sem_t sem;
}thread_t;



typedef struct scheduler{
    
    unsigned int quant;
    
    unsigned int io;
    /* pointer to (hopefully) the thread_manager that contains all the READY/WAITING/TERMINATED threads */
    struct thread_manager *thread_m;
    
    struct thread* running_thread;
    
    unsigned int running;

    sem_t sem_end;
}scheduler_t;



typedef struct thread_manager{
    struct node *term_list;
    struct node *ready_list;
    unsigned int max_ready_prio;
    struct node *waiting_list;
    struct node *terminated;

}thread_manager_t;

/* initialize basic stuff */
struct scheduler *S = NULL;

void insert_thread(thread_manager_t* thread_m, thread_t* thread){
    /* allocates new list cell */
    node_t *new_cell = alloc_cell(thread);
    list_t previous = NULL;
    unsigned int inserted = 0;
    /*  */
    if (thread->state == READY_STATE){
        /* checks to update the max priority amongst all the threads in the ready list*/
        if(thread->priority > thread_m->max_ready_prio){
            thread_m->max_ready_prio = thread->priority;
        }
        /* insert in ready list */
        struct node* ready_l = thread_m->ready_list;
        if (ready_l == NULL){
            thread_m->ready_list = new_cell;
            return; 
        }
        for(list_t i = ready_l; i != NULL; previous = i, i = i->next){
            /* insert before a thread with higher priority */
            if( ((thread_t*)(i->data))->priority > thread->priority && previous != NULL ){
                previous->next = new_cell;
                new_cell->next = i;
                inserted = 1;
                break;
            }
            /* instert at the beginign of list */
            if( ((thread_t*)(i->data))->priority > thread->priority && previous == NULL){
                /* 'yOu wONt uSe gLoBAl VarIabLes So YoU aLsO hAvE to pAsS tHe LisT aDdrEss' - prof. SD */
                thread_m->ready_list = new_cell;
                new_cell->next = i;
                inserted = 1;
                break;
            }
            /*the list should look like this : */

            /* |ready_th_1| -> |ready_th_2| -> |ready_th_3| -> |ready_th_4| -> |ready_th_5| ->|NULL|  */
            /*       |               |               |               |               |                */
            /*  (prio = 0)      (prio = 1)      (prio = 1)      (prio = 3)      (prio = 5)            */

            /*  max_ready_prio = 5 */

            /* the idea is that when building the list, new threads go to the end of the list segment */
            /* that stores the elements with the same priority as the ones being inserted, and when   */
            /* extracting from the list the thread that is picked up first is the one at the begining */
            /* of said segment */
        }
        if(!inserted){
            printf("did powerful insert\n");
            previous->next = new_cell;
            new_cell->next = NULL;
        }
        return;
    }

    if (thread->state == WAITING_STATE){
        /* insert in waiting list */
        list_t waiting_l = thread_m->waiting_list;
        if (waiting_l == NULL){
            thread_m->waiting_list = new_cell;
            return; 
        }
        /* insert in no particular order since all the threads will end up on the ready list eventually */
        for(list_t i = waiting_l; i != NULL; previous = i, i = i->next);
        previous->next = new_cell;
        return;
    }

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

/* removes only the thread based on given state */
/* There can only be 1 thread in the ready list */
/*  with it's state != READY_STATE */
void remove_thread_ready(unsigned int state){
    list_t i, previous = NULL;
    list_t ready_l = S->thread_m->ready_list;
    /* iteraters tourg the list until it find the thread to be removed */
    for(i = ready_l; i != NULL; previous = i, i = i->next){
        if(((thread_t*)(i->data))->state == state){
            break;
        }
    }
    /* 1st thread in the list removal case */
    if(previous == NULL){
        S->thread_m->ready_list = S->thread_m->ready_list->next;
        ready_l->next = NULL;
        ready_l->data = NULL;
        free(ready_l);
        S->thread_m->max_ready_prio = 0;
        return;
    }
    /* from 2nd -> last thread removal case */
    previous->next = i->next;
    /* destorys thread's cell */
    i->next = NULL;
    i->data = NULL;
    free(i);
    /* restore the max priority in the ready list */
    /* all done :D */
    return;
}

/* inserts all the threads waiting on the given IO in the ready list */
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
            /* insert it in the ready list */
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

thread_t* get_next_thread(thread_manager_t* thread_m){
    /* if thread list is empty exit function */
    if(thread_m->ready_list == NULL){
        return NULL;
    }
    /* return a pointer to the 1st thread with the higherst possible priority */
    for(list_t i = thread_m->ready_list; i != NULL; i = i->next){
        if(((thread_t*)(i->data))->priority == thread_m->max_ready_prio){
            return (thread_t*)(i->data);
        }
    }
    return NULL;
}

/* after each 'instruction' checks if running_thread is preempted */
/* if yes, blocks it allowing another thread to run instead */
void schedule(){    
    printf("\n\ncucubau\n\n");
    printf("redy list : ");
    for(list_t i = S->thread_m->ready_list; i != NULL; i = i->next){
        printf("%d_",((thread_t*)(i->data))->priority);
    }
    printf("\n\n");
    printf("\nList's max prio : %d\n",S->thread_m->max_ready_prio);
    thread_t* next_thread = get_next_thread(S->thread_m);

    if(S->running_thread != NULL)
    printf("running one : %u\n",S->running_thread->priority);
    else 
    printf("running one is NULL\n");
    
    if(S->running_thread == NULL){        
        S->running_thread = next_thread;
        next_thread->state = RUNNING_STATE;
        remove_thread_ready(RUNNING_STATE);
        /* 1st thread starts execution */
        sem_post(&(S->running_thread->sem));
        return;
    }
    /* in this case only one thread exists in the system (ready_list is empty) */
    if(next_thread == NULL){
        /* check if it's terminated */
        if(S->running_thread->state == TERMINATED_STATE){
            // printf("THIS ID is terminated : %lu\n",(long unsigned int)(S->running_thread->id));
            insert_thread(S->thread_m, S->running_thread);
            printf("i sent you to terminate and free\n");
            /* all threads are terminated, wait for so_end() call*/
            sem_post(&(S->sem_end));
            return; 
        }
        /* if it's not, reschedule the same thread */
        /* reset it's remaining time is necessarry */
        if(S->running_thread->quant == 0){
            S->running_thread->quant = S->quant;
        }
        return;
    }

    /* the running thread got blocked waiting for an I/O */
    if(S->running_thread->state == WAITING_STATE){
        thread_t* waiting = S->running_thread;
        /* uses quant as a temporary placeholder for the I/O id */
        /* the thread is waiting on */
        insert_thread(S->thread_m, waiting);
        S->running_thread = next_thread;
        next_thread->state = RUNNING_STATE;

        remove_thread_ready(RUNNING_STATE);

        remake_max_prio();

        sem_post(&(S->running_thread->sem));
        sem_wait(&(waiting->sem));
        return;
    }


    /* checks if running_thread got preempted ( it's quantum expired / terminated ) */
    /* (and it also isn't the only thread in the system) */
    if(next_thread->priority > S->running_thread->priority || 
        S->running_thread->quant == 0 || S->running_thread->state == TERMINATED_STATE ){
    
        thread_t* preempted = S->running_thread;
        if(preempted->state != TERMINATED_STATE){
            preempted->state = READY_STATE;
        }
        // }else printf("gotcha\n");

        preempted->quant = S->quant;

        insert_thread(S->thread_m, preempted);
        if(preempted->state == TERMINATED_STATE){
            printf("inserted terminated\n");
        }

        next_thread = get_next_thread(S->thread_m);
        S->running_thread = next_thread;
        next_thread->state = RUNNING_STATE;

        // printf("PREEMPTED ID : %lu\n",(long unsigned int)(preempted->id));
        // printf("NEW_RUNNING ID : %lu\n",(long unsigned int)(S->running_thread->id));

        /* removes the thread in the running state from ready list */
        remove_thread_ready(RUNNING_STATE);

        remake_max_prio();

        /* if the thread terminates and calls schedule (in that order), let it die */
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
    /* allocates memory for the new thread */
    thread_t *new_thread = malloc(sizeof(struct thread));
    if(new_thread == NULL)
        return NULL;
    /* fill in default values */
    new_thread->handler = handler;
    new_thread->quant = S->quant;
    new_thread->priority = priority;
    new_thread->state = NEW_STATE;
     /* initializes semaphore as locked */
    sem_init(&(new_thread->sem), 0, LOCKED);
    return new_thread;
}

int so_init(unsigned int quant, unsigned int io){

    if(quant <= 0 || io > SO_MAX_NUM_EVENTS)
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
    S->running = 1;

    S->running_thread = NULL;
    S->quant = quant;
    S->io = io;

    return INIT_SUCCESS;
}

tid_t so_fork(so_handler *handler, unsigned int priority){
    if (handler == 0 || priority > SO_MAX_PRIO)
        return INVALID_TID;

    tid_t temp_id;
    thread_t *new_thread = newtherad_init(handler, priority);

    if(new_thread == NULL)
        return FAIL_INIT_TID;

    /* checks if it's fork's first call */
    /* locks the end sempaphore when the first thread enters the system */
    if(S->running_thread != NULL){
        S->running_thread->quant--;
    }else{
        sem_wait(&(S->sem_end));
    }
    /* creates actual thread using api */
    pthread_create(&temp_id, NULL, start_thread, (void*)(new_thread));

    new_thread->id = temp_id;
    new_thread->state = READY_STATE;
    insert_thread(S->thread_m, new_thread);
    /* do RR reschedule if necessary */
    schedule();
    return new_thread->id;
}

int so_wait(unsigned int io){

    if(io >= S->io){
        return INVALID_WAIT;
    }
    /* uses quant as a temporary placeholder for the I/O's */
    /* id the thread is waiting on */
    S->running_thread->state = WAITING_STATE;
    S->running_thread->quant = io;
    schedule();
    return 0;
}

int so_signal(unsigned int io){
    if(io >= S->io){
        return INVALID_WAIT;
    }
    unsigned int woke_count = 0;
    S->running_thread->quant--;
    woke_count = wakeup_threads(io);
    schedule();
    return woke_count;
}

void so_exec(void){
    S->running_thread->quant--;
    schedule();
}

void so_end(void){
    if(S == NULL){
        return;
    }
    sem_wait(&(S->sem_end));
    /* join all the threads and destroy the list*/
    if(S->thread_m->term_list != NULL){
        for(list_t i = S->thread_m->term_list; i != NULL; i = i->next){
            thread_t *temp = (thread_t*)(i->data);
            pthread_join(temp->id, NULL);
            printf(" {joined} ");
        }
        printf("\n");
        /* destroy all the threads */
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
    printf(" freed all\n");
}

void *start_thread(void *arg){
    thread_t *thread = (thread_t*)arg;
    /* signals thread entering function */
    sem_wait(&(thread->sem));
    thread->handler(thread->priority);
    thread->state = TERMINATED_STATE;
    /* put in terminated pool */
    schedule();
    return NULL;
}