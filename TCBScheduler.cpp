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
#include <mqueue.h>
#include <unistd.h>

using namespace std;

const int millisecPerSec = 1000;

TCBScheduler::TCBScheduler(std::vector <TaskParam>& threadConfigs, long iterationsPerSecond)
: simTimeSec(0),
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

// this is where we do all the work.
void  TCBScheduler::InternalThreadEntry()
{
 cout << __FUNCTION__  << " started" << endl;

    // set the name of the thread for tracing
 	std::string name = "TCBScheduler";
	pthread_setname_np(_thread, name.c_str());

	// Create a message queue.
	mqd_t mq = 0;
	ssize_t msgsize = 0;
	struct mq_attr attr;
	char buffer[MAXMSGSIZE + 1] = {0};
	struct   timespec tm;

	attr.mq_flags = 0;
	attr.mq_maxmsg = 10;
	attr.mq_msgsize = MAXMSGSIZE;
	attr.mq_curmsgs = 0;

	if ((mq = mq_open(msgQueueName.c_str(), O_CREAT)) == -1)
	{
		cout << __FUNCTION__  << "Message queue was not created "
			 << strerror( errno ) << endl;
	}


	// start up your tellers
	for(unsigned int i = 0; i < TCBThreads.size(); ++i)
	{
		TCBThreads[i].run( );
	};

	clock_gettime(CLOCK_REALTIME, &tm);
	tm.tv_sec += 10;

	while (running)
	{
		if(mq_timedreceive( mq, buffer, MAXMSGSIZE, NULL,  &tm ) > 0 )
		{
			cout << __FUNCTION__  << "We got a message" << endl;
		}
		else if (errno == ETIMEDOUT)
		{
			//clock_gettime(CLOCK_REALTIME, &tm);
		    cout << "we got a timeout at " << tm.tv_sec << endl;
			tm.tv_sec += 1;
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

void TCBScheduler::stop()
{

	running = false;
}

