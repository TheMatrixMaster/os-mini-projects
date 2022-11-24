#include "queue.h"
#include <stdio.h>
#include <unistd.h>

#include <pthread.h>

int main() {
    
    int x = 1;
    int y = 2;
    int z = 3;

    /* Note: it is important that you do NOT move/copy this value
     * after the list has been initialized. Always refer to it by
     * reference/through a pointer indirection */
    struct queue q = queue_create();
    queue_init(&q);
    
    struct queue_entry *node = queue_new_node(&x);
    queue_insert_tail(&q, node);
    
    struct queue_entry *node2 = queue_new_node(&y);
    queue_insert_tail(&q, node2);
    
    struct queue_entry *node3 = queue_new_node(&z);
    queue_insert_tail(&q, node3);

    struct queue_entry *ptr = queue_pop_head(&q);
    while(ptr) {
	printf("popped %d\n", *(int*)ptr->data);
	
	queue_insert_tail(&q, ptr);
	usleep(1000 * 1000);
	
	ptr = queue_pop_head(&q);
    }
    
    return 0;
}
