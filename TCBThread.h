#ifndef _TCBTHREAD_H_
#define _TCBTHREAD_H_

#include <Time.h>
#include "MyThread.h"

class TCBThread: public MyThread
{
public:
	// Its a constructor Jim. - Spock.
	TCBThread(double configComputeTimems, double configPeriodms, double configDeadlinems,
			  long iterationsPerSecond, int configTaskNumber);

    virtual ~TCBThread() {/* empty */}

    double getComputeTime() { return computeTimems; }
    double getdeadline() { return deadlinems; }
    double getperiodms() { return periodms; }

    timespec getNextPeriod () {return nextPeriod; }

    timespec getNextDeadline () {return nextDeadline; }

    void SetNextPeriod (timespec & newPeriod) {nextPeriod = newPeriod;}
    void SetNextDeadline (timespec & newDeadline) {nextPeriod = newDeadline;}

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

	double computeTimems;
	double deadlinems;
	double periodms;

	int TCBThreadNumber;

	double computeTimeExecutedms;
	double periodExecutedms;
	long doWork;

	timespec nextPeriod;
	timespec nextDeadline;

	long computeTimeIterations;

	bool running;

	// Protects the customerQueue;
	pthread_mutex_t TCBMutex;
};
#endif
