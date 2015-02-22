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

TCBScheduler::TCBScheduler(std::vector <TaskParam>& threadConfigs, long iterationsPerSecond)
: toSchedmq(0),
  running (true),
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
		vectorTemp = startSimTime;
		updatetimeSpec (vectorTemp, TCBThreads[i].getDeadlinems());
		TCBThreads[i].setNextDeadline(vectorTemp);


		// RMS scheduling
		// Since everything is ready at the start of the simulation, emplace them
		// shortest computation time first order.
		if (TCBThreadQueue.empty())
		{
//			cout << __FUNCTION__  << " list is empty " << TCBThreads[i].getConfigThreadNumber() << endl;
			// if the list is empty just push it on.
			TCBThreadQueue.push_back (&TCBThreads[i]);
		}
		else
		{
//			cout << __FUNCTION__  << " list has stuff in it " << endl;
			found = false;

			for ( std::list<TCBThread*>::iterator iter = TCBThreadQueue.begin();
				  iter != TCBThreadQueue.end() && found == false; ++iter)
			{
//				cout << __FUNCTION__  << " TCBThreads[i].getComputeTime() " << TCBThreads[i].getComputeTime() << endl;
//				cout << __FUNCTION__  << " (*iter)->getComputeTime() " << (*iter)->getComputeTime() << endl;
				if (TCBThreads[i].getComputeTimems() < (*iter)->getComputeTimems())
				{
//					cout << __FUNCTION__  << " list has stuff in it " << TCBThreads[i].getConfigThreadNumber() << endl;
				   // Put it in front of the current TCBThread in the list.
				   TCBThreadQueue.insert (iter, &TCBThreads[i]);
				   found = true;
				}
			}

			// The deadline is greater than any of the items in the list.
			if (found == false)
			{
				cout << __FUNCTION__  << " adding it to the back " << TCBThreads[i].getConfigThreadNumber() << endl;

				TCBThreadQueue.push_back (&TCBThreads[i]);
			}
		}
	}

//	cout << __FUNCTION__  << " TCBThreadQueue.size() " << TCBThreadQueue.size() << endl;
	// sanity check for RMS.
//	 for (std::list<TCBThread*>::iterator iter=TCBThreadQueue.begin(); iter != TCBThreadQueue.end(); ++iter)
//	 {
//	    cout << ' ' << (*iter)->getConfigThreadNumber();
//	 }

//	 std::cout << '\n';

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

				initializeSim();

				// We're setting our baseline for timeing.
				nextWakeupTime = startSimTime;

				// doing the RMS algorithm first.
				// pop the first element off the TCBThreadQueue
				runingTCBThread = TCBThreadQueue.front();
				TCBThreadQueue.pop_front();

				// Since the first thread in the queue is the highest priority
				// just run it.

				// set the next wake up time based on the computation time.
				updatetimeSpec (nextWakeupTime, runingTCBThread->getComputeTimems());

				// start the TCBThread Loop.
				runingTCBThread->resume();
			}
			else if (message->messageType == MSG_TCBTHREADONE)
			{
				cout << __FUNCTION__  << " We got a MSG_TCBTHREADONE message message from thread " << message->threadNumber << endl;
				cout << __FUNCTION__  << " msgSize " << msgSize << endl;

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

				rateMonotinicScheduler(runingTCBThread);

				struct mq_attr attributes;
				mq_getattr(toSchedmq, &attributes);

				cout << __FUNCTION__  << " attributes.mq_curmsgs " << attributes.mq_curmsgs << endl;
			}

		}
		else if (errno == ETIMEDOUT)
		{
			//clock_gettime(CLOCK_REALTIME, &tm);
		    cout << "we got a timeout at " << nextWakeupTime.tv_sec << endl;
		    nextWakeupTime.tv_sec += 1;
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

	cout << __FUNCTION__  << " called" << endl;

	timespec timeTemp = runingTCBThread->getNextDeadline();

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
//				cout << __FUNCTION__  << " TCBThreads[i].getComputeTime() " << TCBThreads[i].getComputeTime() << endl;
//				cout << __FUNCTION__  << " (*iter)->getComputeTime() " << (*iter)->getComputeTime() << endl;
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
				// the higher priority thread first.
				if (runingTCBThread->getComputeTimems() < (*iter)->getComputeTimems())
				{
				//	cout << __FUNCTION__  << " list has stuff in it " << TCBThreads[i].getConfigThreadNumber() << endl;
					// Put it in front of the current TCBThread in the list.
					TCBThreadQueue.insert (iter, runingTCBThread);
					found = true;
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

	cout << __FUNCTION__  << " TCBThreadQueue.size() " << TCBThreadQueue.size() << endl;
		// sanity check for RMS.
	for (std::list<TCBThread*>::iterator iter=TCBThreadQueue.begin(); iter != TCBThreadQueue.end(); ++iter)
	{
		    cout << ' ' << (*iter)->getConfigThreadNumber();
	}

	cout << endl;

	cout << __FUNCTION__  << " done " << endl;
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

