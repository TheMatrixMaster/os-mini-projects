#include <stdio.h>
#include <unistd.h>
#include "YAUThreads.h"


void testthread(threaddesc *arg)
{
        while (1)
        {
                printf("Hello from thread %d \n", getYAUThreadid(arg));
                sleep(1);

        }
 
}
 

void testthread_other(threaddesc *arg)
{
        while (1)
        {
                printf("Hello from Other thread %d \n", getYAUThreadid(arg));
                sleep(1);
        }
} 
 


int main()
{
	initYAUThreads();
	YAUSpawn( testthread );
	YAUSpawn( testthread );
	YAUSpawn( testthread );
	YAUSpawn( testthread );
	YAUSpawn( testthread_other );

	startYAUThreads(RR);
}
