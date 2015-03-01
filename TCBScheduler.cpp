/*
 * TCBScheduler.cpp
 *
 *  Created on: Feb 18, 2015
 *      Author: hxr5656
 */

#include "TCBScheduler.h"
#include "Common.h"

#include <errno.h>
#include <iostream.h>
#include <unistd.h>

// For trace events.
#include <sys/neutrino.h>
#include <sys/trace.h>

using namespace std;

TCBScheduler::TCBScheduler(std::vector <TaskParam>& threadConfigs,int runTime, TCBScheduler::SchedulingStrategy selectedStrategy, int iterationsPerSecond)
: fromSchedmq(0),
  toSchedmq(0),
  running (true),
  simRunning(false),
  simTimeSec(0),
  strategy(UNDEFINED)
{
	// configure the threads and add them to the scheduler
	for(unsigned int i=0; i < threadConfigs.size(); ++i)
	{
		TCBThreads.push_back(TCBThread(threadConfigs[i].configComputeTimems,
				                      threadConfigs[i].configPeriodms,
				                      threadConfigs[i].configDeadlinems,
				                      iterationsPerSecond, i));
	}
}

int TCBScheduler::getSimTime()
{
	return simTimeSec;
}

// this is where we do all the work.
void  TCBScheduler::InternalThreadEntry()
{
 cout << __FUNCTION__  << " started " << endl;

    int currentSimTimems = 0;
    unsigned int numberOfThreadsStarted = 0;   // Lets me know when to send simulationInitialize
    bool threadScheduled = false;

    // set the name of the thread for tracing
 	std::string name = "TCBScheduler";
	pthread_setname_np(_thread, name.c_str());

 	std::string toSchedmsgQueueName = "toTCBSchedulerMsgQueue";
 	std::string fromSchedmsgQueueName = "fromTCBSchedulerMsgQueue";

	char buffer[MAXMSGSIZE + 1] = {0};

	// Create the message queues
	struct mq_attr attr;
	   attr.mq_flags = 0;
	   attr.mq_maxmsg = 10;
	   attr.mq_msgsize = MAXMSGSIZE;
	   attr.mq_curmsgs = 0;

	if ((fromSchedmq = mq_open(fromSchedmsgQueueName.c_str(), O_CREAT|O_RDWR, 0666, &attr)) == -1)
	{
		cout << __FUNCTION__  << fromSchedmsgQueueName << " was not created "
			 << strerror( errno ) << endl;
	}

	if ((toSchedmq = mq_open(toSchedmsgQueueName.c_str(), O_CREAT|O_RDWR, 0666, &attr)) == -1)
	{
		cout << __FUNCTION__  << toSchedmsgQueueName << " was not created "
			 << strerror( errno ) << endl;
	}

//	cout << __FUNCTION__  << " TCBThreads.size() " << TCBThreads.size() << endl;
	//  Start up the threads
	for(unsigned int i = 0; i < TCBThreads.size(); ++i)
	{
		TCBThreads[i].run( );
	};

	// Set the priority 1 level higher than main or the threads to be run.
	pthread_setschedprio(pthread_self(), 11);

	timespec nextWakeupTime;

	TCBThread* runingTCBThread = 0;

	// Have the timer periodically wake up just so we know it's alive.
	clock_gettime(CLOCK_REALTIME, &nextWakeupTime);

	// I'm using this to increment the currentSimTimems and set the timer.
	const int simTimeIncrementms = 50;

	while (running)
	{
		if(mq_timedreceive( toSchedmq, buffer, MAXMSGSIZE, NULL,  &nextWakeupTime ) > 0 )
		{
//			cout << __FUNCTION__  << " We got a message " << endl;
			// The only messages we get are message structs so cast the buffer to
			// a message struct.
			MsgStruct * message = reinterpret_cast<MsgStruct *> (buffer);

			if (message->messageType == MSG_TCBTHRINITIALIZED)
			{
			    // One of our threads finished initializing!
				// lock the semaphore on it.
				cout << __FUNCTION__  << " We got a MSG_TCBTHRINITIALIZED message from thread " << message->threadNumber << endl;

				TCBThreads[message->threadNumber].suspend();
				TCBThreads[message->threadNumber].startNewComputePeriod ();

				++numberOfThreadsStarted;

				// Once all the threads are initialized let user know.
				if(numberOfThreadsStarted == TCBThreads.size())
				{
					// Send a start message to the thread to kick off the simulation.
					MsgStruct schedulerInitializedMessage;
					schedulerInitializedMessage.messageType = MSG_SCHEDULARINITIALIZED;

					if (mq_send(fromSchedmq, reinterpret_cast<char*>(&schedulerInitializedMessage), sizeof(MsgStruct), 0) < 0)
					{
						cout << __FUNCTION__  << " Error schedular initialized message "
									 << strerror( errno ) << endl;
					}
				}
			}
			else if (message->messageType == MSG_STARTSIM)
			{
				cout << __FUNCTION__  << " We got a MSG_STARTSIM message " << endl;
				simRunning = true;

				// reset the simulation time counter.
				currentSimTimems = 0;

				// We're setting our baseline for timeing.
				nextWakeupTime = startSimTime;

				 // Call the correct scheduling strategy
				if (strategy == TCBScheduler::RMS)
				{
					threadScheduled = rateMonotinicScheduler(currentSimTimems, runingTCBThread);
				}
				else if(strategy == TCBScheduler::EDF)
				{
					// Our first set of deadlines are equal to the configured thread deadline.
					for(unsigned int i = 0; i < TCBThreads.size(); ++i)
					{
						TCBThreads[i].setNextDeadline(TCBThreads[i].getDeadlinems());
					};

					threadScheduled = earliestDeadlineFirstScheduler(currentSimTimems, runingTCBThread);
				}
				else if(strategy == TCBScheduler::LST)
				{

				}

				cout << " thread number " << runingTCBThread->getTCBThreadID() << endl;

				// start the TCBThread Loop.
				runingTCBThread->resume();

				// set the next wake up time based on the computation time.
				// Yep it's set to 10 ms.
				updatetimeSpec (nextWakeupTime, simTimeIncrementms);
			}
			else if (message->messageType == MSG_TCBTHREADONE)
			{
				// This message is asynchronous so we dont' update the simulation
				// times.  If we blow past nextWakeupTime then mq_timedreceive will
				// immediately return.
				cout << __FUNCTION__  << " " << currentSimTimems << " MSG_TCBTHREADONE message thread " << message->threadNumber << endl;

				// this is how we will log stuff.
				trace_logf(_NTO_TRACE_USERFIRST, "%d %s %d", currentSimTimems, " MSG_TCBTHREADONE message thread ", message->threadNumber);

				int nextPeriod = 0;

				// set the thread up to run again.
				runingTCBThread->suspend();
				runingTCBThread->startNewComputePeriod();

				// Set update the thread to the next period.
				nextPeriod = runingTCBThread->getNextPeriod() + runingTCBThread->getPeriodms();
				runingTCBThread->setNextPeriod(nextPeriod);

				// the deadline is always after the period.
				runingTCBThread->setNextDeadline((nextPeriod + runingTCBThread->getDeadlinems()));

				// Since runingTCBThread isn't running any more set it to null.  This helps with
				// transitions out of the no threads are running state to run a thread state.
				runingTCBThread = NULL;

				// set this to false since a Thread is not running.
				threadScheduled = false;
			}
		}
		else if(errno == ETIMEDOUT)
		{
			// This code should only execute on an ETIMEDOUT error.

			if (simRunning)
			{
//				cout << currentSimTimems << endl;

				TCBThread* temp = 0;

				currentSimTimems += simTimeIncrementms;

				 // Call the correct scheduling strategy
				if (strategy == TCBScheduler::RMS)
				{
			 	 	 threadScheduled = rateMonotinicScheduler(currentSimTimems, temp);
				}
				else if(strategy == TCBScheduler::EDF)
				{
					threadScheduled = earliestDeadlineFirstScheduler(currentSimTimems, temp);
				}
				else if(strategy == TCBScheduler::LST)
				{

				}

				// If we found a thread to run and it's not the currently
				 // running thread then switch to it.
				 if (threadScheduled == true)
				 {
					// If we need to run a new thread suspend the
					// current thread and start up the new one
					if (temp != runingTCBThread)
					{
						// If the thread finished we set the running thread to null.  You don't
						// want it to crash by calling a null pointer.
						if (runingTCBThread != NULL)
						{
							runingTCBThread->suspend();
						}

						runingTCBThread = temp;
						runingTCBThread->resume();
						cout << " " << currentSimTimems << " change to thread number " << runingTCBThread->getTCBThreadID() << endl;

//						trace_logf(_NTO_TRACE_USERFIRST, "%d %s %d", currentSimTimems, " change to thread number ", runingTCBThread->getTCBThreadID());
					}
				 }

				// Update our wakeup time.
				updatetimeSpec (nextWakeupTime, simTimeIncrementms);
			}
			else
			{
				cout << __FUNCTION__ << " we got a timeout at " << nextWakeupTime.tv_sec << endl;
				nextWakeupTime.tv_sec += 1;
			}
		}
	}

    // Close the message queue before we exit the thread.
	if (toSchedmq != -1)
	{
		mq_close(toSchedmq);
	}

	if (fromSchedmq != -1)
	{
		mq_close(fromSchedmq);
	}

	cout << __FUNCTION__  << " done" << endl;
}

bool TCBScheduler::earliestDeadlineFirstScheduler(int currentSimTimems, TCBThread*& thread)
{
//	cout << __FUNCTION__  << " called " << endl;

	double priority = 5000;  // Just some really big number.
	unsigned int index = 0;

	// This handles the timeout period where nothing should run.
	bool threadScheduled = false;

	for(unsigned int i=0; i < TCBThreads.size(); ++i)
	{
		// if the period hasn't been satisfied
		if(TCBThreads[i].getNextPeriod () <= currentSimTimems)
		{
			// calculate the priority and set it.
			// for EDF the earliest deadline runs first.
			TCBThreads[i].setThreadPriority((double)(TCBThreads[i].getNextDeadline() - currentSimTimems));
		}
	}

	// find a runnable task
	for(unsigned int i=0; i < TCBThreads.size(); ++i)
	{
		// if the period hasn't been satisfied
		if(TCBThreads[i].getNextPeriod () <= currentSimTimems)
		{

//			cout << " Thread " << i << " " << TCBThreads[i].getThreadPriority() << endl;

			// The highest priority thread to run is the one with the earliest
			// deadline.
			if(TCBThreads[i].getThreadPriority() < priority)
			{
				priority = TCBThreads[i].getThreadPriority();
				index = i;

				threadScheduled = true;
			}
		}
	}

	// If a thread should run return a pointer to it otherwise there is nothing
	// to run so return null;
	if(threadScheduled)
	{
		// return the address of the scheduled task;
		thread = &TCBThreads[index];
	}

	return threadScheduled;

	cout << __FUNCTION__  << " done " << endl;
}

bool TCBScheduler::rateMonotinicScheduler(int currentSimTimems, TCBThread*& thread)
{
	double priority = 5000;  // Just some really big number.
	unsigned int index = 0;

	// This handles the timeout period where nothing should run.
	bool threadScheduled = false;

	for(unsigned int i=0; i < TCBThreads.size(); ++i)
	{
		// if the period hasn't been satisfied
		if(TCBThreads[i].getNextPeriod () <= currentSimTimems)
		{
			// calculate the priority and set it.
			// for RMS the shortest period runs first.
			TCBThreads[i].setThreadPriority((double)TCBThreads[i].getPeriodms());
		}
	}

	// find a runnable task
	for(unsigned int i=0; i < TCBThreads.size(); ++i)
	{
		// if the period hasn't been satisfied
		if(TCBThreads[i].getNextPeriod () <= currentSimTimems)
		{

//			cout << " Thread " << i << " " << TCBThreads[i].getThreadPriority() << endl;

			// find the highest priority thread to run.
			if(TCBThreads[i].getThreadPriority() < priority)
			{
				priority = TCBThreads[i].getThreadPriority();
				index = i;

				threadScheduled = true;
			}
		}
	}

	// If a thread should run return a pointer to it otherwise there is nothing
	// to run so return null;
	if(threadScheduled)
	{
		// return the address of the scheduled task;
		thread = &TCBThreads[index];
	}

	return threadScheduled;

//	cout << __FUNCTION__  << " done " << endl;
}

void TCBScheduler::run( )
{
	MyThread::StartInternalThread();
}

bool  TCBScheduler::schedulerIsInitialized ()
{
//	cout << __FUNCTION__  << " begin " << endl;

	char buffer [MAXMSGSIZE + 1];

	MsgStruct * message = reinterpret_cast<MsgStruct *> (buffer);

	// We're going to block until we get a message.  If we get
	// another message we just loop around.
	do {
		if(mq_receive(fromSchedmq, buffer, MAXMSGSIZE, NULL) < 0);
		{
			// we got a real error message.
			if(errno != EOK)
			{
				cout << __FUNCTION__  << " Error recieving a message "
							 	 << strerror( errno ) << endl;
			}
		}
	}
	while (message->messageType != MSG_SCHEDULARINITIALIZED);

//	cout << __FUNCTION__  << " end " << endl;

	return true;
}

void TCBScheduler::setSimTime(int newSimTimeSec)
{
	simTimeSec = newSimTimeSec;
}

void TCBScheduler::setSchedulingStrategy(SchedulingStrategy newStrategy)
{
	strategy = newStrategy;
}

void TCBScheduler::startSim ()
{
//	cout << __FUNCTION__  << " begin " << endl;

	// Get the starting time
	clock_gettime(CLOCK_REALTIME, &startSimTime);

	// Calculate the ending time
	endSimTime = startSimTime;
	endSimTime.tv_sec += simTimeSec;

	// Send a start message to the thread to kick off the simulation.
	MsgStruct startMessage;
	startMessage.messageType = MSG_STARTSIM;

	if (mq_send(toSchedmq, reinterpret_cast<char*>(&startMessage), sizeof(MsgStruct), 0) < 0)
	{
		cout << __FUNCTION__  << " Error sending startsim message "
					 << strerror( errno ) << endl;
	}

//	cout << __FUNCTION__  << " end " << endl;
}

void TCBScheduler::stop()
{
	running = false;
}

void TCBScheduler::updatetimeSpec (timespec & time, int valuems)
{
	long tempns = 0;
	const long nsPerSec = 1000000000;
	const int nsPerms = 1000000;

//	cout << __FUNCTION__  << " time.tv_sec " << time.tv_sec << " time.tv_nsec " << time.tv_nsec << endl;
//	cout << __FUNCTION__  << " valuems " << valuems << endl;

	tempns = time.tv_nsec + (valuems * nsPerms);
//	cout << __FUNCTION__  << " tempns " << tempns << " tempns " << tempns << endl;

	if (tempns < nsPerSec)
	{
		time.tv_nsec = tempns;
	}
	else
	{
		time.tv_sec +=1;
		tempns -= nsPerSec;
		time.tv_nsec = tempns;
	}

//	cout << __FUNCTION__  << " time.tv_sec " << time.tv_sec << " time.tv_nsec " << time.tv_nsec << endl << endl;
}
