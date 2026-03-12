#include <stdio.h>
#include <pthread.h>
// TODO Complete the assignment following the instructions in the assignment
// document. Refer to the Concurrency lecture notes in Canvas for examples.
// You may also find slides 50 and 51 from Ben Brewster's Concurrency slide
// deck to be helpful (you can find Brewster's slide decks in Canvas -> files).

struct thread_args{
    //Requires the mutex for locking, both conditions variables
    //for waiting and sending signals
    pthread_mutex_t* mutex;
    pthread_cond_t* my_cond_1;
    pthread_cond_t* my_cond_2;
    //My count to track count status
    //printstatus to communicate print status to producer
    int* my_count;
    int* printStatus;
    
};



//will write one or more thread functions to call

void* eater(void* args){
    printf( "CONSUMER THREAD CREATED\n" );
    int previousCount = 0;
    struct thread_args* casted_arg = (struct thread_args*) args;
    while( previousCount <= 9 ){
        pthread_mutex_lock(casted_arg->mutex); 
        printf( "CONSUMER: MUTEX LOCKED\n" );
        while( *(casted_arg->my_count) <= previousCount  ){
                printf( "CONSUMER: WAITING ON CONDTION VARIABLE 1\n" );
                pthread_cond_wait( casted_arg->my_cond_1, casted_arg->mutex );
            }
            //Previous count is used for the conditional statement and to set the while
            //loop condition basically, as well as to print the below
        printf( "Count updated: %d -> %d\n", previousCount, *(casted_arg->my_count) );
        previousCount++;
        pthread_cond_broadcast( casted_arg->my_cond_2 );
        printf( "CONSUMER: MUTEX UNLOCKED\n" );
        *(casted_arg->printStatus) = 1;
        pthread_mutex_unlock( casted_arg->mutex );
        }
}

    //below will wake up the signal which has the same
    //condtion variable its waiting on
    //pthread_cond_signal(&casted_args->cv);
int main() {
    int my_count = 0;
    int printStatus = 0;

    printf( "PROGRAM START\n" );
    pthread_mutex_t mutex; 
    pthread_mutex_init( &mutex, NULL );
    //The below will be what we check in a while loop to 
    //dictate when we wake up the waiting thread
    
    //ugly large block of declaration and initialization of 
    //variables/mutex and struct
    pthread_cond_t my_cond_1;
    pthread_cond_t my_cond_2;
    pthread_cond_init( &my_cond_1 , NULL );
    pthread_cond_init( &my_cond_2 , NULL );
    pthread_mutex_init( &mutex , NULL );
    pthread_t consumerThread;
    struct thread_args consumer;
    consumer.my_count = &my_count;
    consumer.mutex = &mutex;
    consumer.my_cond_1 = &my_cond_1;
    consumer.my_cond_2 = &my_cond_2;
    consumer.printStatus = &printStatus;
    //Create consumer thread
    pthread_create( &consumerThread, NULL, eater, &consumer );
    
    //Producer "thread" code
    while( my_count != 10 ){
        pthread_mutex_lock( &mutex );
        printf( "PRODUCER: MUTEX LOCKED\n" );
        my_count++;
        printf("PRODUCER: SIGNALING CONDITION VARIBLE 1\n");
        pthread_cond_broadcast(&my_cond_1);
        //while Consumer hasn't printed updated count
        while( printStatus != 1 ){
            printf( "PRODUCER: WAITING ON CONDTION VARIABLE 2\n" );
            pthread_cond_wait( &my_cond_2, &mutex );
        }
        printf( "PRODUCER: MUTEX UNLOCKED\n" );
        //reset status to indicate consumer has not yet printed
        //for last increment because mutex hasn't been unlocked yet
        printStatus = 0; 
        pthread_mutex_unlock( &mutex );
    //below is what we use to lock the threads while they are waiting/accessing
    //critical sections in the code    
    }

   //for cleaning up code preventing zomboid threads
   pthread_join(consumerThread, NULL);

   //code hygiene
   pthread_mutex_destroy(&mutex);
   pthread_cond_destroy( &my_cond_1 );
   pthread_cond_destroy( &my_cond_2 );


   printf( "PROGRAM END\n" );


   return 0;
}
