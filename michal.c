// FIRST ASSIGNMENT RTOS - Galvagni Gianluca s5521188

// APPLICATION:
// <1> Design an application with 3 threads J1, J2, J3, whose periods are 300ms, 500ms, and 800ms, 
//     plus an aperiodic thread J4 in background which is triggered by J2.
// <2> The threads shall just "waste time," as we did in the exercise with threads.
// <3> Design a simple driver with only open, close, and write system calls.
// <4> During its execution, every task 
//     (a) opens the special file associated with the driver;
//     (b) writes to the driver its identifier plus open square brackets (i.e., [1, [2, [3, or [4);
//     (c) close the special files;
//     (d) performs operations (i.e., wasting time);
//     (e) performs (i)(ii) and (iii) again to write to the driver its identifier, 
//         but with closed square brackets (i.e., 1], 2], 3] or 4]).
// <5> The write system call writes on the kernel log the string received from the thread. 
//     A typical output of the system, when reading the kernel log, can be the following [11][2[11]2][3[11]3][4]. 
//     This sequence clearly shows that some threads can be preempted by other threads 
//     (if this does not happen, try to increase the computational time of longer tasks).
// <6> Finally, modify the code of all tasks to use semaphores. 
//     Every thread now protects all its operations (i) to (v) with a semaphore,which prevents other tasks from preempting. 
//     Specifically, use semaphores with a priority ceiling access protocol.  

// COMPILE WITH: g++ -lpthread first_RTOS.c -o first_RTOS

//-------------------------------------LIBRARIES----------------------------------------
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <sched.h>

//-------------------------------GLOBAL VARIABLES------------------------------------------
#define PERIOD_1 300000000  // 300ms in nanoseconds
#define PERIOD_2 500000000  // 500ms in nanoseconds
#define PERIOD_3 800000000  // 800ms in nanoseconds

#define PERIODIC_TASKS 3
#define APERIODIC_TASKS 1
#define TASKS PERIODIC_TASKS + APERIODIC_TASKS

#define INNERLOOP 1000
#define OUTERLOOP 2000

// choose the numbers of ciclic for each task
#define NTASK1 100
#define NTASK2 200
#define NTASK3 300
#define NTASK4 400

// INIZIALIZATION OF PERIODIC TASKS
int task1_code();
int task2_code();
int task3_code();

// INIZIALIZATION OF APERIODIC TASKS
int task4_code();

// CARATERISTICS FUNCTIONS OF PERIODIC TRHEADS
void *task1(void *);
void *task2(void *);
void *task3(void *);

// CARATERISTICS FUNCTIONS OF APERIODIC TRHEADS
void *task4(void *);

// INIZIALIZATION OF MUTEX
pthread_mutex_t mutex_task4 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_task4 = PTHREAD_COND_INITIALIZER;

// UNCOMMENT to use the priority ceiling mutex
/*
// set the priority ceiling of the mutex
pthread_mutex_t mutex_semaphore = PTHREAD_MUTEXATTR_INITIALIZER;

// set the priority ceiling of the mutex
pthread_mutexattr_t mutex_attr_semaphore;
*/

// PERIODIC TASKS CREATION
long int periods[TASKS];

struct timespec next_activation[TASKS];
double wcet[TASKS];
pthread_attr_t attr[TASKS];
pthread_t thread[TASKS];
struct sched_param param[TASKS];

// ---------------------------------MAIN----------------------------------------------------

int main()
{
    // Set the task periods in nanoseconds
    periods[0] = PERIOD_1;
    periods[1] = PERIOD_2;
    periods[2] = PERIOD_3;

    // for aperiority task we set the period to 0
    periods[3] = 0;

    // set the max and min priority
    struct sched_param priomax;
    priomax.sched_priority = sched_get_priority_max(SCHED_FIFO);
    struct sched_param priomin;
    priomin.sched_priority = sched_get_priority_min(SCHED_FIFO); 

    if (geteuid() == 0)
    {     
        // Set the scheduling parameters of the threads
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &priomax);
    }
    else
    {
        printf("You must be root to run this program\n");
        exit(-1);
    }

    // open the special file associated with the driver
    int fd;
    if((fd = open("/dev/mydevice", O_RDWR)) < 0)
    {
        printf("Error opening the device file (main)\n");
        exit(-1);
    }

    // UNCOMMENT the following lines to using the priority ceiling mutex
    /*
    // initialize the mutex attributes
    pthread_mutexattr_init(&mutex_attr_semaphore);

    // set the priority ceiling protocol
    pthread_mutexattr_setprotocol(&mutex_attr_semaphore, PTHREAD_PRIO_PROTECT);

    // set the priority ceiling of the mutex to the max priority
    pthread_mutexattr_setprioceiling(&mutex_attr_semaphore, priomax.sched_priority + TASKS);

    // initialize the mutex
    pthread_mutex_init(&mutex_task4, &mutex_attr_semaphore);
    */

   // string to be written on the file
    char string[100];

    // execute the periodic tasks and the aperiodic task in background
    int i;
    for(i=0; i<TASKS; i++){
        
        // initialize the time_1 and time_2 variables required to read the execution time of the tasks
        struct timespec time_1, time_2;
        clock_gettime(CLOCK_REALTIME, &time_1);
        
        // CREATE THREADS
        if(i==0){
            task1_code();
        }
        else if(i==1){
            task2_code();
        }
        else if(i==2){
            task3_code();
        }

        // APERIODIC TASK
        else if(i==3){
            task4_code();
        }

        // read the execution time of the tasks
        clock_gettime(CLOCK_REALTIME, &time_2);

        // compute the worst case execution time of the tasks
        wcet[i] = (time_2.tv_sec - time_1.tv_sec) * 1000000000 + (time_2.tv_nsec - time_1.tv_nsec);

        // write the wcet on the file
        sprintf(string, "Worst Case Execution Time of Task %d: %f", i+1, wcet[i]);
        if(write(fd, string, strlen(string)+1) != strlen(string)+1)
        {
            printf("Error writing the file(main)\n");
            exit(-1);
        }
    }

    // COMPUTE THE DEADLINE OF THE TASKS
    double U = wcet[0]/periods[0] + wcet[1]/periods[1] + wcet[2]/periods[2];

    // COMPUTE THE DEADLINE OF THE APERIODIC TASK
    double Ulub = (pow(2, (1.0/PERIODIC_TASKS)) - 1) * PERIODIC_TASKS;

    // check if the system is schedulable
    if(U > Ulub)
    {
        printf("The system is not schedulable: \n");
        sprintf(string, "U = %lf, Ulub = %lf", U, Ulub);

        if(write(fd, string, strlen(string)+1) != strlen(string)+1)
        {
            printf("Error writing the file\n");
            exit(-1);
        }
        return -1;
    }
    else
    {
        printf("The system is schedulable: \n");
        printf("U = %lf, Ulub = %lf\n", U, Ulub);
        sprintf(string, "U = %lf, Ulub = %lf", U, Ulub);
        
        if(write(fd, string, strlen(string)+1) != strlen(string)+1)
        {
            printf("Error writing the file\n");
            exit(-1);
        }
    }

    // CLOSE THE FILE
    close(fd);

    sleep(5);

    // SET THE PRIORITY OF THE THREADS with the min priority 
    if (geteuid() == 0)
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &priomin);

    // SET THE PERIODIC THREADS
    for(i=0; i<PERIODIC_TASKS; i++){

        // set the scheduling policy
        pthread_attr_init(&attr[i]);

        // set the scheduling policy
        pthread_attr_setinheritsched(&attr[i], PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr[i], SCHED_FIFO);

        // set the priority of the threads
        param[i].sched_priority = priomin.sched_priority + TASKS - i;

        // set the scheduling parameters of the threads
        pthread_attr_setschedparam(&attr[i], &param[i]);
    }

    // SET THE APERIODIC THREAD
    for(i=PERIODIC_TASKS; i<TASKS; i++){

        // set the scheduling policy
        pthread_attr_init(&attr[i]);

        // set the scheduling policy
        pthread_attr_setschedpolicy(&attr[i], SCHED_FIFO);

        // set the priority of the threads
        param[i].sched_priority = 0;

        // set the scheduling parameters of the threads
        pthread_attr_setschedparam(&attr[i], &param[i]);
    }

    // DECLARE a VARIABLE to store the return value of the pthread_create function
    int iret[TASKS];

    // DECLARE a VARIABLE to read the current time
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);

    // SET the next activation time of the periodic tasks
    for(i=0; i<PERIODIC_TASKS; i++){

        // time in nanoseconds add the period to the current time
        long int next_arrival_nanoseconds = time.tv_nsec + periods[i];

        // end of the period and start of the new period
        next_activation[i].tv_sec = time.tv_sec + next_arrival_nanoseconds / 1000000000;
        next_activation[i].tv_nsec = next_arrival_nanoseconds % 1000000000;
    }

    printf("Start of the execution of the tasks\n");
    fflush(stdout);
    
    // CREATE the THREADS
    iret[0] = pthread_create(&thread[0], &attr[0], task1, NULL);
    printf("Thread 1 created\n");
    iret[1] = pthread_create(&thread[1], &attr[1], task2, NULL);
    printf("Thread 2 created\n");
    iret[2] = pthread_create(&thread[2], &attr[2], task3, NULL);
    printf("Thread 3 created\n");
    iret[3] = pthread_create(&thread[3], &attr[3], task4, NULL);
    printf("Thread 4 created\n");
              
    // JOIN the THREADS
    for(i=0; i<PERIODIC_TASKS; i++){

        // join the threads
        pthread_join(thread[i], NULL);
        printf("Thread %d joined\n", i+1);
    }

    printf("End of the execution of the tasks\n");
    fflush(stdout);

    // UNNCOMMENT the following lines to destroy the mutex
    /*

    // DESTROY the MUTEX
    pthread_mutex_destroy(&mutex_task4); 
    */
    exit(0);
}

// FUNCTION to WASTE TIME
double waste_time(int val){
    int i, j;
    double waste;
    for(i=0; i<OUTERLOOP * val; i++){
        for(j=0; j<INNERLOOP; j++){
            waste = rand() * rand();
        }
    }
    return waste;
}

// TASK 1
int task1_code()
{
    // DECLARE a string to write on the file
    const char *string_i;
    const char *string_j;

    // DECLARE a length of the string
    int length_i, length_j;

    // FILE DESCRIPTOR
    int fd;

    // OPEN the FILE
    if((fd = open("/dev/mydevice", O_RDWR)) < 0)
    {
        printf("Error opening the device file(task1)\n");
        exit(-1);
    }

    // WRITE on the FILE
    string_i = " 1[ ";
    length_i = strlen(string_i) + 1;
    if(write(fd, string_i, length_i) != length_i)
    {
        printf("Error writing the file\n");
        exit(-1);
    }

    // CLOSE the FILE
    close(fd);

    // WASTE TIME
    double waste = waste_time(1);

    // OPEN the FILE
    if((fd = open("/dev/mydevice", O_RDWR)) < 0)
    {
        printf("Error opening the device file\n");
        exit(-1);
    }

    // WRITE on the FILE
    string_j = " ]1 ";
    length_j = strlen(string_j) + 1;
    if(write(fd, string_j, length_j) != length_j)
    {
        printf("Error writing the file\n");
        exit(-1);
    }
    
    // CLOSE the FILE
    close(fd);

    return 0;
}

// TEMPORIZZATION TASK 1
void *task1(void *ptr)
{
    // SET THREAD AFFINITY to CPU 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);

    // EXECUTE the TASK 1 a lot of times... it should be an infinite loop (but too dangerous)
    for(int i=0; i<NTASK1; i++){

        // UNCOMMENT the following lines to lock the mutex
        /*
        // TAKE the MUTEX
        pthread_mutex_lock(&mutex_task4);
        */

        // EXECUTE the TASK 1
        if(task1_code()){
            printf("Error executing the task 1\n");
            fflush(stdout);

            // UNCOMMENT the following lines to use the mutex
            /*
            // RELEASE the MUTEX
            pthread_mutex_unlock(&mutex_task4);
            */
            return NULL;
        }

        // UNCOMMENT the following lines to use the mutex
        /*
        // RELEASE the MUTEX
        pthread_mutex_unlock(&mutex_task4);
        */

        // WAIT until the next activation time
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_activation[0], NULL);

        // SET the next activation time
        long int next_arrival_nanoseconds = next_activation[0].tv_nsec + periods[0];
        next_activation[0].tv_nsec = next_arrival_nanoseconds % 1000000000;
        next_activation[0].tv_sec = next_activation[0].tv_sec + next_arrival_nanoseconds / 1000000000;
    }
    return NULL;
}

// TASK 2
int task2_code()
{
    // DECLARE a string to write on the file
    const char *string_i;
    const char *string_j;

    // DECLARE a length of the string
    int length_i, length_j;

    // FILE DESCRIPTOR
    int fd;

    // OPEN the FILE
    if((fd = open("/dev/mydevice", O_RDWR)) < 0)
    {
        printf("Error opening the device file(task2)\n");
        exit(-1);
    }

    // WRITE on the FILE
    string_i = " 2[ ";
    length_i = strlen(string_i) + 1;
    if(write(fd, string_i, length_i) != length_i)
    {
        printf("Error writing the file\n");
        exit(-1);
    }
    
    // CLOSE the FILE
    close(fd);

    // WASTE TIME
    double wasted = waste_time(2);

    // OPEN the FILE
    if((fd = open("/dev/mydevice", O_RDWR)) < 0)
    {
        printf("Error opening the device file\n");
        exit(-1);
    }

    // if wasted (random number) is 0 then the aperiodic task is executed
    if(wasted == 0){
        const char *string_4 = ":ex(4)";
        int length_4 = strlen(string_4) + 1;
        if(write(fd, string_4, length_4) != length_4)
        {
            printf("Error writing the file\n");
            exit(-1);
        }

        pthread_cond_signal(&cond_task4);
    }

    // WRITE on the FILE
    string_j = " ]2 ";
    length_j = strlen(string_j) + 1;
    if(write(fd, string_j, length_j) != length_j)
    {
        printf("Error writing the file\n");
        exit(-1);
    }
   
    // CLOSE the FILE
    close(fd);

    return 0;
}

// TEMPORIZZATION TASK 2
void *task2(void *ptr)
{
    // SET THREAD AFFINITY to CPU 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);

    // EXECUTE the TASK 2 a lot of times... it should be an infinite loop (but too dangerous)
    for(int i=0; i<NTASK2; i++){
        
        // UNCOMMENT the following lines to use the mutex
        /*
        // TAKE the MUTEX
        pthread_mutex_lock(&mutex_task4);
        */

        // EXECUTE the TASK 2
        if(task2_code()){
            printf("Error executing the task 2\n");
            fflush(stdout);

            // UNCOMMENT the following lines to use the mutex
            /*
            // RELEASE the MUTEX
            pthread_mutex_unlock(&mutex_task4);
            */                                                              
            return NULL;
        }                                                                       

        // UNCOMMENT the following lines to use the mutex
        /*
        // RELEASE the MUTEX
        pthread_mutex_unlock(&mutex_task4);
        */

        // WAIT until the next activation time
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_activation[1], NULL);
        long int next_arrival_nanoseconds = next_activation[1].tv_nsec + periods[1];
        next_activation[1].tv_nsec = next_arrival_nanoseconds % 1000000000;                                                                                                                                           
        next_activation[1].tv_sec = next_activation[1].tv_sec + next_arrival_nanoseconds / 1000000000;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
    }
    return NULL;
}

// TASK 3
int task3_code()
{
    // DECLARE a string to write on the file
    const char *string_i;
    const char *string_j;

    // DECLARE a length of the string
    int length_i, length_j;

    // FILE DESCRIPTOR
    int fd;

    // OPEN the FILE
    if((fd = open("/dev/mydevice", O_RDWR)) < 0)
    {
        printf("Error opening the device file(task3)\n");
        exit(-1);
    }

    // WRITE on the FILE
    string_i = " 3[ ";
    length_i = strlen(string_i) + 1;
    if(write(fd, string_i, length_i) != length_i)
    {
        printf("Error writing the file\n");
        exit(-1);
    }
   
    // CLOSE the FILE
    close(fd);

    // WASTE TIME
    double wasted = waste_time(4);

    // OPEN the FILE
    if((fd = open("/dev/mydevice", O_RDWR)) < 0)
    {
        printf("Error opening the device file\n");
        exit(-1);
    }

    // WRITE on the FILE
    string_j = " ]3 ";
    length_j = strlen(string_j) + 1;
    if(write(fd, string_j, length_j) != length_j)
    {
        printf("Error writing the file\n");
        exit(-1);
    }
    
    // CLOSE the FILE
    close(fd);

    return 0;
}

// TEMPORIZZATION TASK 3
void *task3(void *ptr)
{
    // SET THREAD AFFINITY to CPU 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);

    // EXECUTE the TASK 3 a lot of times... it should be an infinite loop (but too dangerous)
    for(int i=0; i<NTASK3; i++){
        
        // UNCOMMENT the following lines to use the mutex
        /*
        // TAKE the MUTEX
        pthread_mutex_lock(&mutex_task4);
        */

        // EXECUTE the TASK 3
        if(task3_code()){
            printf("Error executing the task 3\n");
            fflush(stdout);

            // UNCOMMENT the following lines to use the mutex
            /*
            // RELEASE the MUTEX
            pthread_mutex_unlock(&mutex_task4);
            */                                                              
            return NULL;
        }                                                                       

        // UNCOMMENT the following lines to use the mutex
        /*
        // RELEASE the MUTEX
        pthread_mutex_unlock(&mutex_task4);
        */

        // WAIT until the next activation time
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_activation[2], NULL);
        long int next_arrival_nanoseconds = next_activation[2].tv_nsec + periods[2];                                                                                                                                            
        next_activation[2].tv_nsec = next_arrival_nanoseconds % 1000000000;       
        next_activation[2].tv_sec = next_activation[2].tv_sec + next_arrival_nanoseconds / 1000000000;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
    }
    return NULL;
}

// TASK 4
int task4_code()
{
    // DECLARE a string to write on the file
    const char *string_i;
    const char *string_j;

    // DECLARE a length of the string
    int length_i, length_j;

    // FILE DESCRIPTOR
    int fd;

    // OPEN the FILE
    if((fd = open("/dev/mydevice", O_RDWR)) < 0)
    {
        printf("Error opening the device file(task4)\n");
        exit(-1);
    }

    // WRITE on the FILE
    string_i = " 4[ ";
    length_i = strlen(string_i) + 1;
    if(write(fd, string_i, length_i) != length_i)
    {
        printf("Error writing the file\n");
        exit(-1);
    }


    // CLOSE the FILE
    close(fd);

    // WASTE TIME
    waste_time(1);

    // OPEN the FILE
    if((fd = open("/dev/mydevice", O_RDWR)) < 0)
    {
        printf("Error opening the device file\n");
        exit(-1);
    }

    // WRITE on the FILE
    string_j = " ]4 ";
    length_j = strlen(string_j) + 1;
    if(write(fd, string_j, length_j) != length_j)
    {
        printf("Error writing the file\n");
        exit(-1);
    }
   
    // CLOSE the FILE
    close(fd);

    return 0;
}

// TEMPORIZZATION TASK 4
void *task4(void *ptr)
{
    // SET THREAD AFFINITY to CPU 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);

    // INFINITE LOOP
    while(1){
        
        // UNCOMMENT the following lines to use the mutex
        /*
        // TAKE the MUTEX
        pthread_mutex_lock(&mutex_task4);
        */

       pthread_cond_wait(&cond_task4, &mutex_task4);

        // EXECUTE the TASK 4
        if(task4_code()){
            printf("Error executing the task 4\n");
            fflush(stdout);

            // UNCOMMENT the following lines to use the mutex
            /*
            // RELEASE the MUTEX
            pthread_mutex_unlock(&mutex_task4);
            */                                                              
            return NULL;
        }                                                                       

        // UNCOMMENT the following lines to use the mutex
        /*
        // RELEASE the MUTEX
        pthread_mutex_unlock(&mutex_task4);
        */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      
    }   
}