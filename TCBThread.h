#ifndef _TCBTHREAD_H_
#define _TCBTHREAD_H_

#include <mqueue.h>
#include <Time.h>

#include "MyThread.h"

class TCBThread: public MyThread
{
public:
	// Its a constructor Jim. - Spock.
	TCBThread(int configComputeTimems, int configPeriodms, int configDeadlinems,
			  int iterationsPerSecond, int configThreadNumber);

    virtual ~TCBThread() {/* empty */}

    int getComputeTimems();
    int getDeadlinems() { return deadlinems; }
    int getPeriodms() { return periodms; }

    int getConfigThreadNumber() { return TCBThreadNumber; }

    timespec getNextPeriod () { return nextPeriod; }

    timespec getNextDeadline () {return nextDeadline; }

    void setNextPeriod (timespec & newPeriod);
    void setNextDeadline (timespec & newDeadline);

	// function that is called to run the banks main loop
	void run();

	void startNewComputePeriod ();

	void suspend ();

	void resume ();

	void stop();

	// debug functions this will die in the release version.
	long getdoWork() { return doWork; }

private:
    // Implementation of our code from mythreadclass.h
	virtual void InternalThreadEntry();

	int computeTimems;
	int deadlinems;
	int periodms;

	int TCBThreadNumber;

	int computeTimeExecutedms;
	int periodExecutedms;
	int doWork;

	timespec nextPeriod;
	timespec nextDeadline;

	int computeTimeIterations;

	bool running;

	mqd_t toSchedmq;

	// Protects the customerQueue;
	pthread_mutex_t TCBMutex;
};
#endif
