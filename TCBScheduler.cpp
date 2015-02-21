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
: mq(0),
  running (true),
  simTimeSec(0),
  strategy(RMS)
{
	// Add the threads to the scheduler
	for(unsigned int i=0; i < threadConfigs.size(); ++i)
	{
		TCBThreads.push_back(TCBThread(threadConfigs[i].configComputeTimems,
				                      threadConfigs[i].configPeriodms,
				                      threadConfigs[i].configDeadlinems,
				                      iterationsPerSecond, i));
	}

	// Set the threads to a suspended state.
	for(unsigned int i=0; i < TCBThreads.size(); ++i)
	{
		TCBThreads[i].suspend ();
	}

	running = true;
}

void TCBScheduler::initializeSim()
{
	timespec temp;

	for(unsigned int i=0; i < TCBThreads.size(); ++i)
	{
		// Set the next deadline for each thread.
		temp = startSimTime;
		updatetimeSpec (temp, TCBThreads[i].getdeadline());
		TCBThreads[i].SetNextDeadline(temp);

		// Set the next period for each thread.
		temp = startSimTime;
		updatetimeSpec (temp, TCBThreads[i].getperiodms());
		TCBThreads[i].SetNextPeriod(temp);
	}
}

// this is where we do all the work.
void  TCBScheduler::InternalThreadEntry()
{
 cout << __FUNCTION__  << " started " << endl;

    // set the name of the thread for tracing
 	std::string name = "TCBScheduler";
	pthread_setname_np(_thread, name.c_str());

	cout << __FUNCTION__  << " priority " << getprio( 0 ) << endl;

	// Create a message queue
	char buffer[MAXMSGSIZE + 1] = {0};
	if ((mq = mq_open(msgQueueName.c_str(), O_CREAT|O_RDWR)) == -1)
	{
		cout << __FUNCTION__  << " Message queue was not created "
			 << strerror( errno ) << endl;
	}

	cout << __FUNCTION__  << " TCBThreads.size() " << TCBThreads.size() << endl;
	// start up your threads
	for(unsigned int i = 0; i < TCBThreads.size(); ++i)
	{
		TCBThreads[i].run( );
	};

	// Set the priority 1 level higher than main.
	pthread_setschedprio(pthread_self(), 11);

	timespec nextWakeupTime;

	// Have the timer periodically wake up just so we know it's alive.
	clock_gettime(CLOCK_REALTIME, &nextWakeupTime);
	startSimTime.tv_sec += 1;

//	TODO: Replace Start Sime time with something generic.

	while (running)
	{
		if(mq_timedreceive( mq, buffer, MAXMSGSIZE, NULL,  &nextWakeupTime ) > 0 )
		{
			cout << __FUNCTION__  << " We got a message " << endl;
			// The only messages we get are message structs so cast the buffer to
			// a message struct.
			MsgStruct * message = reinterpret_cast<MsgStruct *> (buffer);

			if (message->messageType == MSG_STARTSIM)
			{
				cout << __FUNCTION__  << " We got a MSG_STARTSIM message " << endl;
			}
		}
		else if (errno == ETIMEDOUT)
		{
			//clock_gettime(CLOCK_REALTIME, &tm);
		    cout << "we got a timeout at " << nextWakeupTime.tv_sec << endl;
		    nextWakeupTime.tv_sec += 1;
		}
	}

	if (mq != -1)
	{
		mq_close(mq);
	}

	cout << __FUNCTION__  << " done" << endl;
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
	cout << __FUNCTION__  << " begin " << endl;

	// Get the starting time
	clock_gettime(CLOCK_REALTIME, &startSimTime);

	// Calculate the ending time
	endSimTime = startSimTime;
	endSimTime.tv_sec += simTimeSec;

	MsgStruct startMessage;
	startMessage.messageType = MSG_STARTSIM;

	if (mq_send(mq, reinterpret_cast<char*>(&startMessage), sizeof(MsgStruct), 0) < 0)
	{
		cout << __FUNCTION__  << " Error sending startsim message "
					 << strerror( errno ) << endl;
	}

	cout << __FUNCTION__  << " end " << endl;
}

void TCBScheduler::stop()
{

	running = false;
}

void TCBScheduler::updatetimeSpec (timespec & time, int valuems)
{
	long tempLong = 0;
	const long secsPerNs = 1000000000;

	tempLong = time.tv_nsec + (valuems * 1000);

	if (tempLong < secsPerNs)
	{
		time.tv_nsec = tempLong;
	}
	else
	{
		time.tv_sec +=1;
		tempLong -= secsPerNs;
		time.tv_nsec = tempLong;
	}

}

