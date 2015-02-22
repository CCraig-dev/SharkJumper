#include "TCBThread.h"
//#include "Common.h"

#include <errno.h>
#include <iostream.h>
#include <unistd.h>

using namespace std;

const int millisecPerSec = 1000;

TCBThread::TCBThread (int configComputeTimems, int configPeriodms,
		              int configDeadlinems, long iterationsPerSecond, int configThreadNumber)
:computeTimems(configComputeTimems),
 deadlinems(configDeadlinems),
 periodms(configPeriodms),
 TCBThreadNumber(configThreadNumber),
 computeTimeExecutedms(0),
 periodExecutedms(0),
 doWork(0)
{

	computeTimeIterations = iterationsPerSecond / millisecPerSec * configComputeTimems;

	memset(&nextPeriod, 0, sizeof(nextPeriod));
	memset(&nextDeadline, 0, sizeof(nextDeadline));

	running = true;

    // gotta initialize my mutex before starting.
	pthread_mutex_init(&TCBMutex, NULL);

	toSchedmq = 0;
}

// this is where we do all the work.
void  TCBThread::InternalThreadEntry()
{
 cout << __FUNCTION__  << " TCBThread " << TCBThreadNumber << " started" << endl;

    // set the name of the thread for tracing
    std::string name = "TCBThread " + TCBThreadNumber;
 	pthread_setname_np(_thread, name.c_str());

 	std::string msgQueueName = "TCBSchedulerMsgQueue";

	if ((toSchedmq = mq_open(msgQueueName.c_str(), O_RDWR)) == -1)
	{
		cout << __FUNCTION__  << " Message queue was not created "
			 << strerror( errno ) << endl;
	}

    doWork = computeTimeIterations;

	while (running)
	{
		while (doWork > 0)
		{
			pthread_mutex_lock( &TCBMutex );

			-- doWork;

			pthread_mutex_unlock( &TCBMutex );
		}
	}

	cout << __FUNCTION__  << "TCBThread " << TCBThreadNumber << " done" << endl;
}

void TCBThread::run( )
{
	MyThread::StartInternalThread();
}

void TCBThread::setNextDeadline (timespec & newDeadline)
{
	nextPeriod = newDeadline;
}

void TCBThread::setNextPeriod (timespec & newPeriod)
{
	nextPeriod = newPeriod;
}

void TCBThread::startNewComputePeriod ()
{
	computeTimeExecutedms = 0;
	periodExecutedms = 0;

	pthread_mutex_lock(&TCBMutex);

	doWork = computeTimeIterations;

	pthread_mutex_unlock(&TCBMutex);
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

void TCBThread::resume ()
{
	pthread_mutex_unlock(&TCBMutex);
}
