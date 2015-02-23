#include "TCBThread.h"
#include "Common.h"

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
 doWork(-2)
{

	computeTimeIterations = iterationsPerSecond / millisecPerSec * configComputeTimems;

	memset(&nextPeriod, 0, sizeof(nextPeriod));
	memset(&nextDeadline, 0, sizeof(nextDeadline));

	running = true;

	toSchedmq = 0;
}

int TCBThread::getComputeTimems()
{
	return computeTimems;
}

// this is where we do all the work.
void  TCBThread::InternalThreadEntry()
{
// cout << __FUNCTION__  << " TCBThread " << TCBThreadNumber << " started" << endl;

    // set the name of the thread for tracing
    std::string name = "TCBThread " + TCBThreadNumber;
 	pthread_setname_np(_thread, name.c_str());

    // gotta initialize my mutex before starting.
	pthread_mutex_init(&TCBMutex, NULL);

 	std::string msgQueueName = "TCBSchedulerMsgQueue";

	if ((toSchedmq = mq_open(msgQueueName.c_str(), O_WRONLY)) == -1)
	{
		cout << __FUNCTION__  << " Message queue was not created "
			 << strerror( errno ) << endl;
	}

    //doWork = computeTimeIterations;

	while (running)
	{
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
			doneMessage.threadNumber = TCBThreadNumber;

//			cout << __FUNCTION__  << "TCBThread " << TCBThreadNumber << " sending MSG_TCBTHREADONE message " << endl;

			if(mq_send(toSchedmq, reinterpret_cast<char*>(&doneMessage), sizeof(MsgStruct), 0) < 0)
			{
				cout << __FUNCTION__  << " Error sending MSG_TCBTHREADONE message "
							 << strerror( errno ) << endl;
			}

			-- doWork;
		}

		if (doWork == -2)
		{
			MsgStruct threadInitMessage;
			threadInitMessage.messageType = MSG_TCBTHRINITIALIZED;
			threadInitMessage.threadNumber = TCBThreadNumber;

//					cout << __FUNCTION__  << "TCBThread " << TCBThreadNumber << " sending MSG_TCBTHRINITIALIZED message " << endl;

					if(mq_send(toSchedmq, reinterpret_cast<char*>(&threadInitMessage), sizeof(MsgStruct), 0) < 0)
					{
						cout << __FUNCTION__  << " Error sending MSG_TCBTHREADONE message "
									 << strerror( errno ) << endl;
					}

					-- doWork;
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
	nextDeadline = newDeadline;
}

void TCBThread::setNextPeriod (timespec & newPeriod)
{
	nextPeriod = newPeriod;
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

void TCBThread::resume ()
{
	pthread_mutex_unlock(&TCBMutex);
}
