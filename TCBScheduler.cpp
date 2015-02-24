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

using namespace std;

const int millisecPerSec = 1000;
const int nsPerSec = 1000000000;
const int nsPerms = 1000000;


TCBScheduler::TCBScheduler(std::vector <TaskParam>& threadConfigs, int iterationsPerSecond)
: toSchedmq(0),
  running (true),
  simRunning(false),
  simTimeSec(0),
  strategy(UNDEFINED)
{
	// Add the threads to the scheduler
	for(unsigned int i=0; i < threadConfigs.size(); ++i)
	{
		TCBThreads.push_back(TCBThread(threadConfigs[i].configComputeTimems,
				                      threadConfigs[i].configPeriodms,
				                      threadConfigs[i].configDeadlinems,
				                      iterationsPerSecond, i));
	}

	running = true;
}

void TCBScheduler::initializeSim()
{
	cout << __FUNCTION__  << " started " << endl;

	timespec vectorTemp;
	bool found = false;

	for(unsigned int i=0; i < TCBThreads.size(); ++i)
	{
		// Set the next period to the start sim time cause all threads need
		// to run now.
		vectorTemp = startSimTime;
		TCBThreads[i].setNextPeriod(vectorTemp);

		// Set the next deadline for each thread.
		updatetimeSpec (vectorTemp, TCBThreads[i].getDeadlinems());
		TCBThreads[i].setNextDeadline(vectorTemp);

		if (startTCBThreadQueue.empty())
		{
//			cout << __FUNCTION__  << " list is empty " << TCBThreads[i].getConfigThreadNumber() << endl;
			// if the list is empty just push it on.
			startTCBThreadQueue.push_back (&TCBThreads[i]);
		}
		else
		{
	//		cout << __FUNCTION__  << " list has stuff in it " << endl;
			found = false;

			for ( std::list<TCBThread*>::iterator iter = startTCBThreadQueue.begin();
				  iter != startTCBThreadQueue.end() && found == false; ++iter)
			{
	//			cout << __FUNCTION__  << " TCBThreads[i].getComputeTime() " << TCBThreads[i].getComputeTime() << endl;
	//			cout << __FUNCTION__  << " (*iter)->getComputeTime() " << (*iter)->getComputeTime() << endl;
				if (TCBThreads[i].getPeriodms() < (*iter)->getPeriodms())
				{
	//				cout << __FUNCTION__  << " list has stuff in it " << TCBThreads[i].getConfigThreadNumber() << endl;
				   // Put it in front of the current TCBThread in the list.
					startTCBThreadQueue.insert (iter, &TCBThreads[i]);
				   found = true;
				}
			}

			// The deadline is greater than any of the items in the list.
			if (found == false)
			{
	//			cout << __FUNCTION__  << " adding it to the back " << TCBThreads[i].getConfigThreadNumber() << endl;

				startTCBThreadQueue.push_back (&TCBThreads[i]);
			}
		}
	}

	// sanity check for RMS.
	 for (std::list<TCBThread*>::iterator iter=startTCBThreadQueue.begin(); iter != startTCBThreadQueue.end(); ++iter)
	 {
	    cout << ' ' << (*iter)->getConfigThreadNumber();
	 }

	 std::cout << '\n';
	 cout << __FUNCTION__  << " end " << endl;
}

// this is where we do all the work.
void  TCBScheduler::InternalThreadEntry()
{
 cout << __FUNCTION__  << " started " << endl;

    // set the name of the thread for tracing
 	std::string name = "TCBScheduler";
	pthread_setname_np(_thread, name.c_str());

//	cout << __FUNCTION__  << " priority " << getprio( 0 ) << endl;

	std::string msgQueueName = "TCBSchedulerMsgQueue";

	// Create a message queue
	char buffer[MAXMSGSIZE + 1] = {0};
	if ((toSchedmq = mq_open(msgQueueName.c_str(), O_CREAT|O_RDWR)) == -1)
	{
		cout << __FUNCTION__  << " Message queue was not created "
			 << strerror( errno ) << endl;
	}

//	cout << __FUNCTION__  << " TCBThreads.size() " << TCBThreads.size() << endl;
	//  your threads start them.
	for(unsigned int i = 0; i < TCBThreads.size(); ++i)
	{
		TCBThreads[i].run( );
	};


	// Set the priority 1 level higher than main.
	pthread_setschedprio(pthread_self(), 11);

	timespec nextWakeupTime;
	timespec temp;
	timespec tempNext;
	TCBThread* runingTCBThread = 0;

	// Have the timer periodically wake up just so we know it's alive.
	clock_gettime(CLOCK_REALTIME, &nextWakeupTime);
	startSimTime.tv_sec += 1;

	ssize_t msgSize = 0;

	while (running)
	{
		if((msgSize = mq_timedreceive( toSchedmq, buffer, MAXMSGSIZE, NULL,  &nextWakeupTime )) > 0 )
		{
//			cout << __FUNCTION__  << " We got a message " << endl;
			// The only messages we get are message structs so cast the buffer to
			// a message struct.
			MsgStruct * message = reinterpret_cast<MsgStruct *> (buffer);

			if (message->messageType == MSG_TCBTHRINITIALIZED)
			{
			    // Once a thread has initialized we lock the semaphore
				// finishe initializing it.
				cout << __FUNCTION__  << " We got a MSG_TCBTHRINITIALIZED message from thread " << message->threadNumber << endl;

				TCBThreads[message->threadNumber].suspend();
				TCBThreads[message->threadNumber].startNewComputePeriod ();
			}
			else if (message->messageType == MSG_STARTSIM)
			{
				cout << __FUNCTION__  << " We got a MSG_STARTSIM message " << endl;

				simRunning = true;

				initializeSim();

				// We're setting our baseline for timeing.
				nextWakeupTime = startSimTime;

				// doing the RMS algorithm first.
				// pop the first element off the TCBThreadQueue
				runingTCBThread = startTCBThreadQueue.front();
				startTCBThreadQueue.pop_front();

				//cout << " TCBThreadQueue.size() " << TCBThreadQueue.size() << endl;
				cout << " thread number " << runingTCBThread->getConfigThreadNumber() << endl;

				// Since the first thread in the queue is the highest priority
				// just run it.

				// set the next wake up time based on the computation time.
				updatetimeSpec (nextWakeupTime, runingTCBThread->getComputeTimems());

				// start the TCBThread Loop.
				runingTCBThread->resume();
			}
			else if (message->messageType == MSG_TCBTHREADONE)
			{
				cout << __FUNCTION__  << " We got a MSG_TCBTHREADONE message thread " << message->threadNumber << endl;

				// set the thread up to run again.
				runingTCBThread->suspend();
				runingTCBThread->startNewComputePeriod();

				// Update the deadline and the period.
				temp = runingTCBThread->getNextPeriod();
				updatetimeSpec (temp, runingTCBThread->getPeriodms());
				runingTCBThread->setNextPeriod(temp);

				// the deadline is x ms after the period.
				updatetimeSpec (temp, runingTCBThread->getDeadlinems());
				runingTCBThread->setNextDeadline(temp);

				// Insert it in the proper position in the TCBThreadQueue based on the
				// agorithm.
				rateMonotinicScheduler(runingTCBThread);

				// Always check to see if another thread is ready to run.
				// were using the nextwakeup time to get a 1 to 1 comparison.
				tempNext = TCBThreadQueue.front()->getNextPeriod();

//				cout << __FUNCTION__  << " tempNext.tv_sec " << tempNext.tv_sec << " tempNext.tv_nsec " << tempNext.tv_nsec << endl;
//				cout << __FUNCTION__  << " nextWakeupTime.tv_sec " << nextWakeupTime.tv_sec << " nextWakeupTime.tv_nsec " << nextWakeupTime.tv_nsec << endl;
				if(tempNext.tv_sec == nextWakeupTime.tv_sec && tempNext.tv_nsec == nextWakeupTime.tv_nsec)
				{
					cout << __FUNCTION__  << " the next task is ready to run " << endl;
					// next task is ready to run
					runingTCBThread = TCBThreadQueue.front();
					TCBThreadQueue.pop_front();

					//runingTCBThread->setcomputationInterruped(false);

				}
				else if (!startTCBThreadQueue.empty())
				{
					cout << __FUNCTION__  << " grabbing a task from the start queue " << endl;

					// Nothing was ready to run and we have something in that can run
					// in the start queue.
					runingTCBThread = startTCBThreadQueue.front();
					startTCBThreadQueue.pop_front();
				}


				cout << " thread number " << runingTCBThread->getConfigThreadNumber() << endl;

				// If the thread that needs to be run has a higher priority
				// than the next thread in the queue set our timer for its
				// the computation time.
				if( runingTCBThread->getComputeTimems () <= TCBThreadQueue.front()->getComputeTimems())
				{
					updatetimeSpec (nextWakeupTime, runingTCBThread->getComputeTimems());
				}
				else
				{
					// current thread priority is lower so
					// find out when the thread that will run will end.
					temp = nextWakeupTime;
					updatetimeSpec(temp, runingTCBThread->getComputeTimems ());

					// the compution time for the thread will end before a
					// the next thread is scheduled to run then wakeup time
					// is the computation time.
					if((temp.tv_sec < tempNext.tv_sec) ||
					   (temp.tv_sec == tempNext.tv_sec && temp.tv_nsec < tempNext.tv_nsec))
					{
						nextWakeupTime = temp;
					}
					else
					{
						// calculate how long you will run for.
						if (tempNext.tv_sec == nextWakeupTime.tv_sec)
						{
							runingTCBThread->setComputeTimeExecuted((tempNext.tv_nsec - nextWakeupTime.tv_sec)/nsPerms);
						}
						else
						{
							runingTCBThread->setComputeTimeExecuted((1 * nsPerSec + tempNext.tv_nsec - nextWakeupTime.tv_sec)/nsPerms);
						}

						nextWakeupTime = TCBThreadQueue.front()->getNextPeriod ();
					}
				}

				// start the TCBThread Loop.
				runingTCBThread->resume();
			}
		}
		else if (errno == ETIMEDOUT)
		{


			if (simRunning)
			{
				cout << " we got a sim timeout " << endl;

				// if we've run out of time just suspend the thread and
				// reschedule it.

				cout << " calling suspend on " << runingTCBThread->getConfigThreadNumber() << endl;
				runingTCBThread->suspend();
				//runingTCBThread->setcomputationInterruped(true);

				rateMonotinicSchedulerIncompleteTask(runingTCBThread);

				// get the next thread
				runingTCBThread = TCBThreadQueue.front();
			    TCBThreadQueue.pop_front();

				cout << " thread number " << runingTCBThread->getConfigThreadNumber() << endl;

				// If the thread that needs to be run has a shorter or equal computation
			    // time than the next thread in the queue set our timer for
				// the computation time.
				if( runingTCBThread->getComputeTimems () <= TCBThreadQueue.front()->getComputeTimems())
				{
//					cout << __FUNCTION__  << "MSG_TCBTHREADONE choice 1" << endl;
					updatetimeSpec (nextWakeupTime, runingTCBThread->getComputeTimems());
				}
				else
				{
//					cout << __FUNCTION__  << "MSG_TCBTHREADONE choice 2" << endl;
					nextWakeupTime = TCBThreadQueue.front()->getNextPeriod ();
				}

				// start the TCBThread Loop.
				runingTCBThread->resume();
			}
			else
			{
				//clock_gettime(CLOCK_REALTIME, &tm);
				cout << __FUNCTION__ << " we got a timeout at " << nextWakeupTime.tv_sec << endl;
				nextWakeupTime.tv_sec += 1;
			}
		}
	}

	if (toSchedmq != -1)
	{
		mq_close(toSchedmq);
	}

	cout << __FUNCTION__  << " done" << endl;
}

// Inserts the task in the correct position in the TCBThreadQueue based on the algorithm.
void TCBScheduler::rateMonotinicScheduler(TCBThread* runingTCBThread)
{

//	cout << __FUNCTION__  << " called" << endl;

	timespec timeTemp = runingTCBThread->getNextPeriod();

//	cout << __FUNCTION__  << " timeTemp sec " << timeTemp.tv_sec << " tv_ns " << timeTemp.tv_sec << endl;

	// Since everything is ready at the start of the simulation, emplace them
	// shortest computation time first order.
	if (TCBThreadQueue.empty())
	{
//			cout << __FUNCTION__  << " list is empty " << TCBThreads[i].getConfigThreadNumber() << endl;
		// if the list is empty just push it on.
		TCBThreadQueue.push_back (runingTCBThread);
	}
	else
	{
//			cout << __FUNCTION__  << " list has stuff in it " << endl;
		bool found = false;
		timespec listTemp;

		for ( std::list<TCBThread*>::iterator iter = TCBThreadQueue.begin();
			  iter != TCBThreadQueue.end() && found == false; ++iter)
		{

			listTemp = (*iter)-> getNextPeriod();
//			cout << __FUNCTION__  << " listTemp sec " << listTemp.tv_sec << " tv_ns " << listTemp.tv_sec << endl;

//				cout << __FUNCTION__  << " TCBThreads[i].getComputeTime() " << TCBThreads[i].getComputeTime() << endl;
//				cout << __FUNCTION__  << " (*iter)->getNextPeriod() " << (*iter)->getNextPeriod() << endl;
			if (timeTemp.tv_sec < listTemp.tv_sec)
			{
//					cout << __FUNCTION__  << " list has stuff in it " << TCBThreads[i].getConfigThreadNumber() << endl;
			   // Put it in front of the current TCBThread in the list.
			   TCBThreadQueue.insert (iter, runingTCBThread);
			   found = true;
			}
			else if (timeTemp.tv_sec == listTemp.tv_sec &&
					 timeTemp.tv_nsec < listTemp.tv_nsec)
			{
				// Put it in front of the current TCBThread in the list.
			    TCBThreadQueue.insert (iter, runingTCBThread);
			    found = true;
			}
			else if(timeTemp.tv_sec == listTemp.tv_sec &&
					timeTemp.tv_nsec == listTemp.tv_nsec)
			{
				// If two threads are scheduled to run at the same time then insert
				// the thread with the lower period first.
				if (runingTCBThread->getPeriodms() < (*iter)->getPeriodms())
				{
				//	cout << __FUNCTION__  << " list has stuff in it " << TCBThreads[i].getConfigThreadNumber() << endl;
					// Put it in front of the current TCBThread in the list.
					TCBThreadQueue.insert (iter, runingTCBThread);
					found = true;
				}
			}
		}

		// The period or nextPeriod is greater than any of the items in the list.
		if (found == false)
		{
//			cout << __FUNCTION__  << " adding it to the back " << TCBThreads[i].getConfigThreadNumber() << endl;

			TCBThreadQueue.push_back (runingTCBThread);
		}
	}

//	cout << __FUNCTION__  << " TCBThreadQueue.size() " << TCBThreadQueue.size() << endl;
		// sanity check for RMS.
	for (std::list<TCBThread*>::iterator iter=TCBThreadQueue.begin(); iter != TCBThreadQueue.end(); ++iter)
	{
		    cout << ' ' << (*iter)->getConfigThreadNumber();
	}

	cout << endl;

//	cout << __FUNCTION__  << " done " << endl;
}

void TCBScheduler::rateMonotinicSchedulerIncompleteTask(TCBThread* runingTCBThread)
{
//	cout << __FUNCTION__  << " called " << endl;

	if (TCBThreadQueue.empty())
	{
//			cout << __FUNCTION__  << " list is empty " << TCBThreads[i].getConfigThreadNumber() << endl;
		// if the list is empty just push it on.
		TCBThreadQueue.push_back (runingTCBThread);
	}
	else
	{
		// calculate the end of the compute time.
		timespec temp = runingTCBThread->getNextPeriod();
		updatetimeSpec(temp, runingTCBThread->getComputeTimeExecuted());

		timespec listTemp;

		bool found = false;

		for ( std::list<TCBThread*>::iterator iter = TCBThreadQueue.begin();
			  iter != TCBThreadQueue.end() && found == false; ++iter)
		{
//			cout << __FUNCTION__  << " TCBThreads[i].getComputeTime() " << TCBThreads[i].getComputeTime() << endl;
//			cout << __FUNCTION__  << " (*iter)->getComputeTime() " << (*iter)->getComputeTime() << endl;
			if (runingTCBThread->getPeriodms() <= (*iter)->getPeriodms())
			{
				// The interrupted thread has a higher priority
				TCBThreadQueue.insert (iter, runingTCBThread);
			   found = true;
			}
			else
			{
				// our priority is lower
				listTemp = (*iter)->getNextPeriod();

				if((temp.tv_sec < listTemp.tv_sec) ||
				   (temp.tv_sec == listTemp.tv_sec && temp.tv_nsec == listTemp.tv_nsec))
				{
					// we have room to run!!!
					TCBThreadQueue.insert (iter, runingTCBThread);
				   found = true;
				}
				else
				{
					updatetimeSpec(temp, (*iter)->getComputeTimems());
				}
			}
		}

		// The deadline is greater than any of the items in the list.
		if (found == false)
		{
//			cout << __FUNCTION__  << " adding it to the back " << TCBThreads[i].getConfigThreadNumber() << endl;

			TCBThreadQueue.push_back (runingTCBThread);
		}
	}

	// sanity check for RMS.
    for (std::list<TCBThread*>::iterator iter=TCBThreadQueue.begin(); iter != TCBThreadQueue.end(); ++iter)
    {
       cout << ' ' << (*iter)->getConfigThreadNumber();
    }

    cout << endl;

//    cout << __FUNCTION__  << " done " << endl;
}

void TCBScheduler::run( )
{
	MyThread::StartInternalThread();
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
	const long nsPerms = 1000000;

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

