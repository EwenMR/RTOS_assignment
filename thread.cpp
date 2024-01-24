//compile with: g++ -lpthread thread.cpp -o thread

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <sched.h>

#define INNERLOOP 1000
#define OUTERLOOP 100
#define NPERIODICTASKS 3
#define NAPERIODICTASKS 1
#define NTASKS NPERIODICTASKS + NAPERIODICTASKS

//code of periodic tasks
void task1_code( );
void task2_code( );
void task3_code( );
//code of aperiodic task
void task4_code( );

//characteristic function of the thread, only for timing and synchronization
//periodic tasks
void *task1( void *);
void *task2( void *);
void *task3( void *);
//aperiodic task
void *task4( void *);

// Decalre custom functions
void do_nothing();
void scrivere(int fd, const char *msg);
int aprire_driver();
void open_write(const char *str);

// initialization of mutexes and conditions (only for aperiodic scheduling)
pthread_mutex_t mutex_task_4 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_task_4 = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mutex_semaphore = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t mutexattr_semaphore;


long int periods[NTASKS];
struct timespec next_arrival_time[NTASKS];
double WCET[NTASKS];
pthread_attr_t attributes[NTASKS];
pthread_t thread_id[NTASKS];
struct sched_param parameters[NTASKS];
int missed_deadlines[NTASKS];

int
main()
{	

  	// set task periods in nanoseconds
	//the first task has period 300 millisecond
	//the second task has period 500 millisecond
	//the third task has period 800 millisecond
  	periods[0]= 300000000; //in nanoseconds
  	periods[1]= 500000000; //in nanoseconds
  	periods[2]= 800000000; //in nanoseconds

  	//for aperiodic tasks we set the period equals to 0
  	periods[3]= 0; 

  	struct sched_param priomax;
  	priomax.sched_priority=sched_get_priority_max(SCHED_FIFO);
  	struct sched_param priomin;
  	priomin.sched_priority=sched_get_priority_min(SCHED_FIFO);

	// set the maximum priority to the current thread (you are required to be
  	// superuser). Check that the main thread is executed with superuser privileges
	// before doing anything else.

  	if (getuid() == 0){
    		if (pthread_setschedparam(pthread_self(),SCHED_FIFO,&priomax) != 0){
				perror("error setting scheduling parameters");
				exit(EXIT_FAILURE);
			}
	} else {
		fprintf(stderr, "You need root priveleges to set scheduling parameters\n Please switch to root user");
		exit(EXIT_FAILURE);
	}


  	// execute all tasks in standalone modality in order to measure execution times
  	// (use gettimeofday). Use the computed values to update the worst case execution
  	// time of each task.

	pthread_mutexattr_init(&mutexattr_semaphore);
	pthread_mutexattr_setprotocol(&mutexattr_semaphore, PTHREAD_PRIO_PROTECT);
	pthread_mutexattr_setprioceiling(&mutexattr_semaphore, priomin.sched_priority + NTASKS);
	pthread_mutex_init(&mutex_semaphore, &mutexattr_semaphore);


	// string that will be written on the file
	
 	
  	for (int i =0; i < NTASKS; i++)
    	{

		// initializa time_1 and time_2 required to read the clock
		struct timespec time_1, time_2;
		clock_gettime(CLOCK_REALTIME, &time_1);

		//we should execute each task more than one for computing the WCET
		//periodic tasks
 	     	if (i==0)
			task1_code();
      		if (i==1)
			task2_code();
      		if (i==2)
			task3_code();
      		
      		//aperiodic task
      		if (i==3)
			task4_code();

		clock_gettime(CLOCK_REALTIME, &time_2);


		// compute the Worst Case Execution Time (in a real case, we should repeat this many times under
		//different conditions, in order to have reliable values

      		WCET[i]= 1000000000*(time_2.tv_sec - time_1.tv_sec)
			       +(time_2.tv_nsec-time_1.tv_nsec);
      		printf("\nWorst Case Execution Time %d=%f \n", i, WCET[i]);
			fflush(stdout);
			char message[100];

    	}

    	// compute U
	double U = WCET[0]/periods[0]+WCET[1]/periods[1]+WCET[2]/periods[2];

    	// compute Ulub by considering the fact that we have harmonic relationships between periods
	// double Ulub = 1;
    	
	//if there are no harmonic relationships, use the following formula instead
	double Ulub = NPERIODICTASKS*(pow(2.0,(1.0/NPERIODICTASKS)) -1);


	
	//check the sufficient conditions: if they are not satisfied, exit  
  	if (U > Ulub)
    	{
      		printf("\n U=%lf Ulub=%lf Non schedulable Task Set", U, Ulub);
      		return(-1);
    	}
  	printf("\n U=%lf Ulub=%lf Scheduable Task Set", U, Ulub);
  	fflush(stdout);
	sleep(1);


  	// set the minimum priority to the current thread: this is now required because 
	//we will assign higher priorities to periodic threads to be soon created
	//pthread_setschedparam

  	if (getuid() == 0)
    		pthread_setschedparam(pthread_self(),SCHED_FIFO,&priomin);

  
  	// set the attributes of each task, including scheduling policy and priority
  	for (int i =0; i < NPERIODICTASKS; i++)
    	{
		//initializa the attribute structure of task i
      		pthread_attr_init(&(attributes[i]));

		//set the attributes to tell the kernel that the priorities and policies are explicitly chosen,
		//not inherited from the main thread (pthread_attr_setinheritsched) 
      		pthread_attr_setinheritsched(&(attributes[i]), PTHREAD_EXPLICIT_SCHED);
      
		// set the attributes to set the SCHED_FIFO policy (pthread_attr_setschedpolicy)
		pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);

		//properly set the parameters to assign the priority inversely proportional 
		//to the period
      		//parameters[i].sched_priority = priomin.sched_priority+NTASKS - i;
      		parameters[i].sched_priority = sched_get_priority_max(SCHED_FIFO) - i;

		//set the attributes and the parameters of the current thread (pthread_attr_setschedparam)
      		pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
    	}

 	// aperiodic tasks
  	for (int i =NPERIODICTASKS; i < NTASKS; i++)
    	{
      		pthread_attr_init(&(attributes[i]));
      		pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);

      		//set minimum priority (background scheduling)
      		parameters[i].sched_priority = 0;
      		pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
    	}


	//delare the variable to contain the return values of pthread_create	
  	int iret[NTASKS];

	//declare variables to read the current time
	struct timespec time_1;
	clock_gettime(CLOCK_REALTIME, &time_1);

  	// set the next arrival time for each task. This is not the beginning of the first
	// period, but the end of the first period and beginning of the next one. 
  	for (int i = 0; i < NPERIODICTASKS; i++)
    	{
		long int next_arrival_nanoseconds = time_1.tv_nsec + periods[i];
		//then we compute the end of the first period and beginning of the next one
		next_arrival_time[i].tv_nsec= next_arrival_nanoseconds%1000000000;
		next_arrival_time[i].tv_sec= time_1.tv_sec + next_arrival_nanoseconds/1000000000;
       		missed_deadlines[i] = 0;
    	}


	// create all threads(pthread_create)
  	iret[0] = pthread_create( &(thread_id[0]), &(attributes[0]), task1, NULL);
  	iret[1] = pthread_create( &(thread_id[1]), &(attributes[1]), task2, NULL);
  	iret[2] = pthread_create( &(thread_id[2]), &(attributes[2]), task3, NULL);
   	iret[3] = pthread_create( &(thread_id[3]), &(attributes[3]), task4, NULL);

  	// join all threads (pthread_join)
  	pthread_join( thread_id[0], NULL);
  	pthread_join( thread_id[1], NULL);
  	pthread_join( thread_id[2], NULL);

  	// set the next arrival time for each task. This is not the beginning of the first
	// period, but the end of the first period and beginning of the next one. 
  	
	for (int i = 0; i < NTASKS; i++)
    	{
      	printf ("\nMissed Deadlines Task %d=%d", i, missed_deadlines[i]);
		fflush(stdout);
    	}
  	exit(0);
}

// application specific task_1 code
void task1_code()
{
	const char *str_1 = "[1";
	const char *str_2 = "1]";

	open_write(str_1);

	do_nothing();

	open_write(str_2);
}

//thread code for task_1 (used only for temporization)
void *task1( void *ptr)
{
	// set thread affinity, that is the processor on which threads shall run
	cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

   	//execute the task one hundred times... it should be an infinite loop (too dangerous)
  	int i=0;
  	for (i=0; i < 100; i++)
    	{
		pthread_mutex_lock(&mutex_semaphore);
		task1_code();
		pthread_mutex_unlock(&mutex_semaphore);
		// sleep until the end of the current period (which is also the start of the
		// new one
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[0], NULL);

		// the thread is ready and can compute the end of the current period for
		// the next iteration
 		
		long int next_arrival_nanoseconds = next_arrival_time[0].tv_nsec + periods[0];
		next_arrival_time[0].tv_nsec= next_arrival_nanoseconds%1000000000;
		next_arrival_time[0].tv_sec= next_arrival_time[0].tv_sec + next_arrival_nanoseconds/1000000000;
    	}
	
}

void task2_code()
{
  	const char *str_1 = "[2";
	const char *str_2 = "2]";

	open_write(str_1);

	pthread_cond_signal(&cond_task_4);
	do_nothing();

	open_write(str_2);
}


void *task2( void *ptr )
{
	// set thread affinity, that is the processor on which threads shall run
	cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

	int i=0;
  	for (i=0; i < 100; i++)
    	{
			pthread_mutex_lock(&mutex_semaphore);
			task2_code();
			pthread_mutex_unlock(&mutex_semaphore);

			clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[1], NULL);
			long int next_arrival_nanoseconds = next_arrival_time[1].tv_nsec + periods[1];
			next_arrival_time[1].tv_nsec= next_arrival_nanoseconds%1000000000;
			next_arrival_time[1].tv_sec= next_arrival_time[1].tv_sec + next_arrival_nanoseconds/1000000000;
    	}
	
}

void task3_code()
{
	const char *str_1 = "[3";
	const char *str_2 = "3]";
	
	open_write(str_1);

	do_nothing();

	open_write(str_2);
}

void *task3( void *ptr)
{
	// set thread affinity, that is the processor on which threads shall run
	cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

	int i=0;
  	for (i=0; i < 100; i++)
    	{
			pthread_mutex_lock(&mutex_semaphore);
      		task3_code();
			pthread_mutex_unlock(&mutex_semaphore);

			clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[2], NULL);
			long int next_arrival_nanoseconds = next_arrival_time[2].tv_nsec + periods[2];
			next_arrival_time[2].tv_nsec= next_arrival_nanoseconds%1000000000;
			next_arrival_time[2].tv_sec= next_arrival_time[2].tv_sec + next_arrival_nanoseconds/1000000000;
    }

}

void task4_code()
{
  	const char *str_1 = "[4";
	const char *str_2 = "4]";

	open_write(str_1);

	do_nothing();

	open_write(str_2);
}



void *task4( void *)
{
	// set thread affinity, that is the processor on which threads shall run
	cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

	//add an infinite loop 
	while (1)
    	{
		// wait for the proper condition to be signalled	
		
		pthread_cond_wait(&cond_task_4, &mutex_task_4);
		task4_code();
	
	}
}







// FUNCTIONS TO MAKE THE CODE EASIER TO READ :)
// opening the driver module, writing a message and wasting time

// Function to write to a file
void scrivere(int fd, const char *msg){
	ssize_t size = write(fd, msg, sizeof(msg));
	printf("%s", msg);
	if (size == -1){
		perror("you don't know how to write properly");
		exit(1);
	}
}

// Function to open the driver module
int aprire_driver(){
	int driver_fd = open("/dev/simple", O_RDWR);
	if(driver_fd == -1){
		perror("Non sei come aprire il driver");
		exit(-1);
	}
	return driver_fd;
}

// Function to waste time, fare niente
void do_nothing(){
	double nada;
	for(int i =0; i < OUTERLOOP; i++){
		for(int j = 0; j < INNERLOOP; j++)
		nada = rand()*rand();
	}
}

void open_write(const char *str){
	int fd = aprire_driver();
	scrivere(fd, str);
	close(fd);
}


