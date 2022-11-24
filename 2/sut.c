#include "sut.h"
#include "queue.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ucontext.h>
#include <semaphore.h>

/*
TODOS:
- Fix nanosleep on c_exec thread
- Run a few extra tests
*/

typedef struct __threaddesc
{
	int threadid;
	char *threadstack;
	void *threadfunc;
    ucontext_t *threadcontext;
	struct __threaddesc *prev;
	struct __threaddesc *next;
} threaddesc;

threaddesc *cur_c_thread, *cur_i_thread;
threaddesc *dummythread, *tailthread;

int numthreads;

ucontext_t *i_exec_context, *c_exec_context;

struct queue readyQ, waitQ;
pthread_t c_exec_id, i_exec_id;
sem_t mutex, readyQmutex, waitQmutex;

void *c_exec()
{
    struct queue_entry *ptr;
    struct timespec quantum;

    quantum.tv_sec = 0;
    quantum.tv_nsec = 100000;

    while (true) {

        sem_wait(&readyQmutex);
        ptr = queue_pop_head(&readyQ);
        sem_post(&readyQmutex); 

        if (ptr) {
            // prevent premature thread cancellation
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

            // Get popped task and context swap with parent
            threaddesc *tdescptr = ((threaddesc*)ptr->data);
            cur_c_thread = tdescptr;

            // we can free the pointer memory to the queue node that we poppped
            free(ptr);
            ptr = NULL;

            // swap context to task we popped off
            swapcontext(c_exec_context, tdescptr->threadcontext);
        } 
        
        else if (numthreads == 0) {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            pthread_testcancel();
        }
        
        // else {
        //     // sleep c-exec for 100us before checking task queue again      
        //     printf("enter sleep\n");
        //     nanosleep(&quantum, NULL);
        //     printf("exit sleep\n");
        // }
    }
}

void *i_exec()
{
    struct queue_entry *ptr;

    while (true) {

        sem_wait(&waitQmutex);
        ptr = queue_pop_head(&waitQ);
        sem_post(&waitQmutex); 

        if (ptr) {
            // prevent premature thread cancellation
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

            // Get popped task and context swap with parent
            threaddesc *tdescptr = ((threaddesc*)ptr->data);
            cur_i_thread = tdescptr;

            // we can free the pointer memory to the queue node that we poppped
            free(ptr);
            ptr = NULL;

            // swap context to task we popped off
            swapcontext(i_exec_context, tdescptr->threadcontext);
        }

        else if (numthreads == 0) {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            pthread_testcancel();
        }
    }
}

void sut_init()
{
    // initialize variables
    numthreads = 0;
    cur_c_thread = NULL;
    cur_i_thread = NULL;
    tailthread = NULL;

    i_exec_context = (ucontext_t *) malloc(sizeof(ucontext_t));
    c_exec_context = (ucontext_t *) malloc(sizeof(ucontext_t));

    // initialize circular doubly linked list
    dummythread = (threaddesc*) malloc(sizeof(*dummythread));
    dummythread->threadid = -1;

    // create wait queue
    waitQ = queue_create();
    queue_init(&waitQ);
    
    // create ready queue
    readyQ = queue_create();
    queue_init(&readyQ);

    // initialize semaphore
    sem_init(&mutex, 0, 1);
    sem_init(&readyQmutex, 0, 1);
    sem_init(&waitQmutex, 0, 1);

    // create two kernel threads: one for handling compute tasks and one for handling I/O tasks
    pthread_create(&i_exec_id, NULL, i_exec, NULL);
    pthread_create(&c_exec_id, NULL, c_exec, NULL);
}

bool sut_create(sut_task_f fn)
{
    threaddesc *tdescptr;

    // check for max threads
    if (numthreads >= MAX_THREADS) {
        printf("FATAL: Maximum thread limit reached... creation failed!\n");
        return false;
    }

    // allocate size of threaddesc pointer 
    tdescptr = (threaddesc *) malloc(sizeof(*tdescptr));

    // allocate memory to pointers
    tdescptr->threadcontext = (ucontext_t *) malloc(sizeof(ucontext_t));
    tdescptr->threadstack = (char *) malloc(sizeof(char) * THREAD_STACK_SIZE);
    tdescptr->threadfunc = (void *) malloc(sizeof(*(tdescptr->threadfunc)));

	getcontext(tdescptr->threadcontext);
	tdescptr->threadid = numthreads;

	tdescptr->threadcontext->uc_stack.ss_sp = tdescptr->threadstack;
	tdescptr->threadcontext->uc_stack.ss_size = sizeof(char) * THREAD_STACK_SIZE;
	tdescptr->threadcontext->uc_link = 0;
	tdescptr->threadcontext->uc_stack.ss_flags = 0;

	tdescptr->threadfunc = fn;

	makecontext(tdescptr->threadcontext, fn, 1, tdescptr);

    // acquire lock to modify global variables between threads
    sem_wait(&mutex);

    if (tailthread == NULL) {
        tailthread = tdescptr;
        dummythread->next = tailthread;
        tailthread->prev = dummythread;
        tailthread->next = dummythread;
    } else {
        tdescptr->next = tailthread->next;
        tailthread->next = tdescptr;
        tdescptr->prev = tailthread;
        tailthread = tdescptr;
    }

    numthreads += 1;

    sem_post(&mutex);

    struct queue_entry *node = queue_new_node(tdescptr);

    sem_wait(&readyQmutex);
    queue_insert_tail(&readyQ, node);
    sem_post(&readyQmutex);

	return EXIT_SUCCESS;
}

void sut_yield()
{
    // get next context and swap by saving current context
    struct queue_entry *node = queue_new_node(cur_c_thread);

    sem_wait(&readyQmutex);
    queue_insert_tail(&readyQ, node);
    sem_post(&readyQmutex);

    swapcontext(cur_c_thread->threadcontext, c_exec_context);
}

void sut_exit()
{
    sem_wait(&mutex);

    // cut cur_c_thread out of the circular linked list
    cur_c_thread->prev->next = cur_c_thread->next;
    cur_c_thread->next->prev = cur_c_thread->prev;

    // get next context and swap without saving current context
    free(cur_c_thread);
    cur_c_thread = NULL;

    // update number of user threads
    numthreads -= 1;

    sem_post(&mutex);

    setcontext(c_exec_context);
}

int sut_open(char *fname)
{
    // swap context and put task in wait queue
    struct queue_entry *wnode = queue_new_node(cur_c_thread);

    sem_wait(&waitQmutex);
    queue_insert_tail(&waitQ, wnode);
    sem_post(&waitQmutex);

    swapcontext(cur_c_thread->threadcontext, c_exec_context);

    // when I/O thread (i_exec) grabs task, we read the file and save the descriptor
    FILE *fptr;
    int result;

    if (fptr = fopen(fname, "a+")) {
        result = fileno(fptr);
    } else {
        result = -1;
    }

    // we need to swap context in I/O thread and put this task back in the ready queue
    struct queue_entry *rnode = queue_new_node(cur_i_thread);

    sem_wait(&readyQmutex);
    queue_insert_tail(&readyQ, rnode);
    sem_post(&readyQmutex);

    swapcontext(cur_i_thread->threadcontext, i_exec_context);

    // once this thread is picked up again by c_exec thread, we return the desired value
    return result;
}

void sut_write(int fd, char *buf, int size)
{
    // swap context and put task in wait queue
    struct queue_entry *wnode = queue_new_node(cur_c_thread);

    sem_wait(&waitQmutex);
    queue_insert_tail(&waitQ, wnode);
    sem_post(&waitQmutex);

    swapcontext(cur_c_thread->threadcontext, c_exec_context);

    // write stream to file descriptor when i_exec grabs task
    int bytes_sent = 0;
    int remaining = size;
    while (remaining && bytes_sent != -1) {
        bytes_sent = write(fd, buf, remaining);
        remaining -= bytes_sent;
    }

    // swap context back to i_exec and place current thread in ready Q
    struct queue_entry *rnode = queue_new_node(cur_i_thread);

    sem_wait(&readyQmutex);
    queue_insert_tail(&readyQ, rnode);
    sem_post(&readyQmutex);

    swapcontext(cur_i_thread->threadcontext, i_exec_context);
}

void sut_close(int fd)
{
    // swap context and put task in wait queue
    struct queue_entry *wnode = queue_new_node(cur_c_thread);

    sem_wait(&waitQmutex);
    queue_insert_tail(&waitQ, wnode);
    sem_post(&waitQmutex);

    swapcontext(cur_c_thread->threadcontext, c_exec_context);

    // close file using file descriptor
    close(fd);

    // swap context back to i_exec and place current thread in ready Q
    struct queue_entry *rnode = queue_new_node(cur_i_thread);

    sem_wait(&readyQmutex);
    queue_insert_tail(&readyQ, rnode);
    sem_post(&readyQmutex);

    swapcontext(cur_i_thread->threadcontext, i_exec_context);
}

char *sut_read(int fd, char *buf, int size)
{
    // swap context and put task in wait queue
    struct queue_entry *wnode = queue_new_node(cur_c_thread);

    sem_wait(&waitQmutex);
    queue_insert_tail(&waitQ, wnode);
    sem_post(&waitQmutex);

    swapcontext(cur_c_thread->threadcontext, c_exec_context);

    // read contents of file descriptor into buffer
    int bytes_read = 0;
    int remaining = size;

    while ((bytes_read = read(fd, buf, size)) > 0) {
        remaining -= bytes_read;
    }

    // swap context back to i_exec and place current thread in ready Q
    struct queue_entry *rnode = queue_new_node(cur_i_thread);

    sem_wait(&readyQmutex);
    queue_insert_tail(&readyQ, rnode);
    sem_post(&readyQmutex);

    swapcontext(cur_i_thread->threadcontext, i_exec_context);

    // return the buffer when the context switches back in c_exec
    return buf;
}

void sut_shutdown()
{
    // wait for all tasks to terminate in both c_exec and i_exec
    pthread_cancel(i_exec_id);
    pthread_cancel(c_exec_id);

    // wait for threads to exit
    pthread_join(i_exec_id, NULL);
    pthread_join(c_exec_id, NULL);

    // free heap memory
    free(dummythread);
    free(tailthread);
    free(i_exec_context);
    free(c_exec_context);

    // destroy semaphore
    sem_destroy(&mutex);
    sem_destroy(&readyQmutex);
    sem_destroy(&waitQmutex);
}
