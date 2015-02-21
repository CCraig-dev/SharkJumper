#include "TCBThread.h"

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
}

// this is where we do all the work.
void  TCBThread::InternalThreadEntry()
{
 cout << __FUNCTION__  << " TCBThread " << TCBThreadNumber << " started" << endl;

    // set the name of the thread for tracing
    std::string name = "TCBThread " + TCBThreadNumber;
 	pthread_setname_np(_thread, name.c_str());

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
