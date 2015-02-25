#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <sys/netmgr.h>
#include <sys/neutrino.h>

#include "TCBScheduler.h"
#include "TCBThread.h"

using namespace std;

#define MY_PULSE_CODE   _PULSE_CODE_MINAVAIL

// Used to tune the code to the board
int iterationsPerSecondCounter = 0;  // This is a free running counter and
                                      // should only be used for board tunning.
bool keepRunning = true;
pthread_mutex_t timimgMutex;

// Function declarations
bool calculateComputationTime (int channelID, int &iterationsPerSecond);
void* measureTime( void* arg );

typedef union {
        struct _pulse   pulse;

        /* your other message structures would go
           here too */
} my_message_t;

int main(int argc, char *argv[]) {
	std::cout << "Welcome to the QNX Momentics IDE" << std::endl;

	   // Put in a tuning factor to allow the OS time to run.
	   // 80% works on a 486.
		const double tuningFactor = 0.8;

	   int channelID = 0;
	   int iterationsPerSecond = 0;

	   TaskParam userInput;
	   vector <TaskParam> threadConfigs;

	   // Set up a message channel for this process.
	   // You'll use this for getting messages back
	   // from the scheduler and for figuring out how may loop iterations in 1 second.
	   channelID = ChannelCreate(0);

	   calculateComputationTime (channelID, iterationsPerSecond);

	   // tune iterationsPerSecond so that the OS has time to run without blowing
	   // deadlines.
	   iterationsPerSecond = iterationsPerSecond * tuningFactor;

	   cout << __FUNCTION__ << " tuned iterationsPerSecond "  << iterationsPerSecond << endl;

	   // Howard's code will go here after we've tuned the computation time.
	   userInput.configComputeTimems = 100;
	   userInput.configPeriodms = 300;
	   userInput.configDeadlinems = 200;
	   threadConfigs.push_back(userInput);

	   userInput.configComputeTimems = 200;
	   userInput.configPeriodms = 500;
	   userInput.configDeadlinems = 400;
	   threadConfigs.push_back(userInput);

	   userInput.configComputeTimems = 100;
	   userInput.configPeriodms = 1000;
	   userInput.configDeadlinems = 900;
	   threadConfigs.push_back(userInput);

	  TCBScheduler scheduler (threadConfigs, iterationsPerSecond);

	  scheduler.run();

	   sleep (3);

	   scheduler.setSimTime(5);

	   scheduler.startSim();

		// Debug code ignore this.
/*
	   TCBThread testThread(100, 200, 150, iterationsPerSecond, 1);

	   testThread.suspend ();

	   testThread.run();

	   sleep(1);

	   testThread.resume();

	   sleep(1);

	   testThread.stop();

	   testThread.WaitForInternalThreadToExit();
*/
	   sleep (10);
	   cout << __FUNCTION__ << " done "<< endl;

	return EXIT_SUCCESS;
}

bool calculateComputationTime (int channelID, int &iterationsPerSecond)
{
	bool timingMutexLocked = false;


    struct sigevent         event;
	struct itimerspec       itime;
	my_message_t            msg;
	int                     rcvid;
	timer_t                 timer_id;
	pthread_t thread_tid;



	// hurt to set this again just in case the code changes.
	iterationsPerSecond = 0;

	   // gotta initialize my mutex before starting.
		pthread_mutex_init(&timimgMutex, NULL);

		// lock the damn thing so that the timing thread doesn't start until we're ready.
		pthread_mutex_lock(&timimgMutex);
		timingMutexLocked = true;


		// create a timing thread.
		if(pthread_create(&thread_tid, NULL, &measureTime, NULL ))
		{
		   cout << __FUNCTION__ << " Holy crap we couldn't create a thread!!" << endl;
		   return false;
		}

	   event.sigev_notify = SIGEV_PULSE;
	   event.sigev_coid = ConnectAttach(ND_LOCAL_NODE, 0,
			   	   	   	   	   	   	   channelID,
	                                    _NTO_SIDE_CHANNEL, 0);
	   event.sigev_priority = getprio(0);
	   event.sigev_code = MY_PULSE_CODE;
	   timer_create(CLOCK_REALTIME, &event, &timer_id);

	   itime.it_value.tv_sec = 1;
	   /* 500 million nsecs = .5 secs */
	   itime.it_value.tv_nsec = 000000000;
	   itime.it_interval.tv_sec = 1;
	   /* 500 million nsecs = .5 secs */
	   itime.it_interval.tv_nsec = 000000000;
	   timer_settime(timer_id, 0, &itime, NULL);

	   /*
	    * As of the timer_settime(), we will receive our pulse
	    * in 1.5 seconds (the itime.it_value) and every 1.5
	    * seconds thereafter (the itime.it_interval)
	    */

	   while (keepRunning == true) {
	       rcvid = MsgReceive(channelID, &msg, sizeof(msg), NULL);
	       if (rcvid == 0) { /* we got a pulse */
	            if (msg.pulse.code == MY_PULSE_CODE) {
	            	cout << __FUNCTION__ << "we got a pulse from our timer\n" << endl;
	            	if(timingMutexLocked)
	            	{
	            		cout << __FUNCTION__ << "unlocking the mutex\n" << endl;
	            		pthread_mutex_unlock(&timimgMutex);
	            		timingMutexLocked = false;
	            	}
	            	else if (keepRunning == true)
	            	{
	            		// kill the thread by stopping it and then setting the
	            		// keep running flag to false
	            		pthread_mutex_lock(&timimgMutex);
	            		keepRunning = false;

	            		// Turns out our numbers were off by 200% we need to capture
	            		// the value of the counter at this point to ensure accuracy.
	            		iterationsPerSecond = iterationsPerSecondCounter;
	            		pthread_mutex_unlock(&timimgMutex);

	            		cout << __FUNCTION__ << " iterationsPerSecond 1 "  << iterationsPerSecond << endl;
	            	}
	            } /* else other pulses ... */
	       } /* else other messages ... */
	   }

	   // Please dont' touch this I don't quite know what it will do.
//	    if (ConnectDetach(channelID) == -1) {
//	        printf("Timer: Error in ConnectDetach\n");
//	    }

		// kill the periodic timer.
		if (timer_delete(timer_id) == -1)
		{
		   cout << "Timer: Error in timer_delete()" << endl;
		}


		cout << __FUNCTION__ << " waiting for thread to join " << endl;

		if(pthread_join(thread_tid, NULL))
		{
			cout << __FUNCTION__  << "Could not join thread. "" << endl";
		}

		//cout << __FUNCTION__ << " iterationsPerSecond 2" << iterationsPerSecond << endl;

		cout << __FUNCTION__ << " end " << endl;;
   return true;
}

void* measureTime( void* arg )
{
	cout << __FUNCTION__ << " begin " << endl;

    while(keepRunning == true) {
        pthread_mutex_lock( &timimgMutex );
        ++iterationsPerSecondCounter;
        pthread_mutex_unlock( &timimgMutex );
    }

    cout << __FUNCTION__ << " end " << endl;

    return NULL;
}

