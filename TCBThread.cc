#include "TCBThread.h"
#include "Common.h"

#include <errno.h>
#include <iostream.h>
#include <unistd.h>

// For trace events
#include <sys/neutrino.h>
#include <sys/trace.h>

// DEBUG CODE
#include <limits>

using namespace std;

TCBThread::TCBThread (int configComputeTimems, int configPeriodms,
		              int configDeadlinems, int iterationsPerSecond, int configTCBThreadID)
:computeTimems(configComputeTimems),
 deadlinems(configDeadlinems),
 periodms(configPeriodms),
 TCBThreadID(configTCBThreadID),
 threadPriority(0),
 computeTimeExecutedms(0),
 doWork(0),
 periodExecutedms(0),
 nextPeriodms(0),
 nextDeadlinems(0),
 running(true)
{
	// calculate the number of iterations we're supposed to run for in our work loop
	computeTimeIterations = iterationsPerSecond / MILISECPERSEC * configComputeTimems;

	// Not sure if we need this.
	computationInterruped = false;

	toSchedmq = 0;
}

int TCBThread::getComputeTimeExecuted()
{
	return computeTimeExecutedms;
}

int TCBThread::getComputeTimems()
{
	return computeTimems;
}

int TCBThread::getDeadlinems()
{
	return deadlinems;
}

bool TCBThread::getcomputationInterruped()
{
	return computationInterruped;
}

int TCBThread::getNextDeadline ()
{
	return nextDeadlinems;
}

int TCBThread::getNextPeriod ()
{
	return nextPeriodms;
}

int TCBThread::getPeriodms()
{
	return periodms;
}

int TCBThread::getTCBThreadID()
{
	return TCBThreadID;
}

double TCBThread::getThreadPriority()
{
	return threadPriority;
}

// this is where we do all the work.
void  TCBThread::InternalThreadEntry()
{
// cout << __FUNCTION__  << " TCBThread " << TCBThreadID << " started" << endl;

	// We only want to tell the scheduler we're intialized once.
	bool SendThreadIntializedMessage = false;

    // set the name of the thread for tracing
    std::string name = "TCBThread " + TCBThreadID;
 	pthread_setname_np(_thread, name.c_str());

    // Initialize my mutex used to start and stop the work loop before starting.
	pthread_mutex_init(&TCBMutex, NULL);

	// Open up the message queue to the TCBScheduler
 	std::string msgQueueName = "TCBSchedulerMsgQueue";
	if ((toSchedmq = mq_open(msgQueueName.c_str(), O_WRONLY)) == -1)
	{
		cout << __FUNCTION__  << " Message queue was not created "
			 << strerror( errno ) << endl;
	}

	// Set do work to -1 so that our work loop is not engaged while we're
	// starting up.
	doWork = -1;

	// Main thread loop
	while (running)
	{
		// this is the work loop. It is controlled by the suspend and resume functions.
		while (doWork > 0)
		{
			pthread_mutex_lock( &TCBMutex );

			-- doWork;

			pthread_mutex_unlock( &TCBMutex );
		}

		// Send one message to the scheduler saying we're done.
		if (doWork == 0)
		{
			MsgStruct doneMessage;
			doneMessage.messageType = MSG_TCBTHREADONE;
			doneMessage.threadNumber = TCBThreadID;

//			cout << __FUNCTION__  << "TCBThread " << TCBThreadID << " sending MSG_TCBTHREADONE message " << endl;

			if(mq_send(toSchedmq, reinterpret_cast<char*>(&doneMessage), sizeof(MsgStruct), 0) < 0)
			{
				cout << __FUNCTION__  << " Error sending MSG_TCBTHREADONE message "
							 << strerror( errno ) << endl;
			}

			-- doWork;
		}

		// We're intitialized let TCBscheduler know
		if (SendThreadIntializedMessage == false)
		{
			MsgStruct threadInitMessage;
			threadInitMessage.messageType = MSG_TCBTHRINITIALIZED;
			threadInitMessage.threadNumber = TCBThreadID;

//		    cout << __FUNCTION__  << "TCBThread " << TCBThreadID << " sending MSG_TCBTHRINITIALIZED message " << endl;

			if(mq_send(toSchedmq, reinterpret_cast<char*>(&threadInitMessage), sizeof(MsgStruct), 0) < 0)
			{
				cout << __FUNCTION__  << " Error sending MSG_TCBTHRINITIALIZED message "
					 << strerror( errno ) << endl;
			}

			SendThreadIntializedMessage =  true;
		}
	}

	cout << __FUNCTION__  << "TCBThread " << TCBThreadID << " done" << endl;
}

void TCBThread::resume ()
{
	pthread_mutex_unlock(&TCBMutex);
}

void TCBThread::run( )
{
	MyThread::StartInternalThread();
}

void TCBThread::setComputeTimeExecuted(int newComputeTimeExecutedms)
{
	computeTimeExecutedms = newComputeTimeExecutedms;
}

void TCBThread::setcomputationInterruped(bool newComputationInterrupted)
{
	computationInterruped = newComputationInterrupted;
}

void TCBThread::setNextDeadline (int newDeadline)
{
	nextDeadlinems = newDeadline;
}

void TCBThread::setNextPeriod (int newPeriod)
{
	nextPeriodms = newPeriod;
}

void TCBThread::setThreadPriority (double newThreadPriority)
{
	threadPriority = newThreadPriority;
}

void TCBThread::startNewComputePeriod ()
{
	computeTimeExecutedms = 0;
	periodExecutedms = 0;

	doWork = computeTimeIterations;

//	cout << __FUNCTION__  << " doWork " << doWork << endl;
}

void TCBThread::stop()
{
	pthread_mutex_lock(&TCBMutex);

	running = false;

	pthread_mutex_unlock(&TCBMutex);
}

void TCBThread::suspend ()
{
	pthread_mutex_lock(&TCBMutex);
}
