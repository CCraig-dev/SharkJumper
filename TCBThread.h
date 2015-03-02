#ifndef _TCBTHREAD_H_
#define _TCBTHREAD_H_

#include <mqueue.h>
#include <Time.h>

#include "MyThread.h"

class TCBThread: public MyThread
{
public:

	/**
	 * Function: TCBThread
	 *
	 *	Constructor
	 *
	 * @param configComputeTimems computation time of the thread from the user.
	 * @param configPeriodms of the thread from the user.
	 * @param configDeadlinems deadline of the thread from the user.
	 * @param iterationsPerSecond value derived after 1 second of c++ looping.
	 * @param configThreadNumber The ID of the task thread.
	 *
	 */
	TCBThread(int configComputeTimems, int configPeriodms, int configDeadlinems,
			  int iterationsPerSecond, int configTCBThreadID);

	/**
	 * Function: ~TCBThread
	 *
	 * Destructor
	 *
	 */
    virtual ~TCBThread() {/* empty */}

	/**
	 * Function: getComputeTimems
	 *
	 *	Gets the configured compute time of the thread.
	 *
	 * @return returns the configured compute time of the thread.
	 *
	 */
    int getComputeTimems();

	/**
	 * Function: getDeadlinems
	 *
	 *	Gets the configured deadline of the thread.
	 *
	 * @return returns the configured deadline of the thread.
	 *
	 */
    int getDeadlinems();

	/**
	 * Function: getPeriodms
	 *
	 *	Gets the configured deadline of the thread.
	 *
	 * @return returns the configured period of the thread.
	 *
	 */
    int getPeriodms();

	/**
	 * Function: getRemainingComputeTimems
	 *
	 *	Gets the remainign compute time of a thread.
	 *
	 * @return returns the configured period of the thread.
	 *
	 */
    double getRemainingComputeTimems();

	/**
	 * Function: getTCBThreadID
	 *
	 *	Gets the TCBThreadID of the thread.
	 *
	 * @return returns the configured TCBThreadID.
	 *
	 */
    int getTCBThreadID();

	/**
	 * Function: getNextDeadline
	 *
	 *	Gets the calculated next period the thread
	 *	is supposed to run in.  The deadline is
	 *	calculated from the start of the simulation
	 *	which is time 0.
	 *
	 * @return returns the next deadline.
	 *
	 */
    int getNextDeadline ();

	/**
	 * Function: getNextPeriod
	 *
	 *	Gets the calculated next period the thread
	 *	is supposed to run in.  The period is
	 *	calculated from the start of the simulation
	 *	which is time 0.
	 *
	 * @return returns the next Period.
	 *
	 */
    int getNextPeriod ();

	/**
	 * Function: getThreadPriority
	 *
	 *	Gets the calculated priority of a thread.  This is dependent on the
	 *	scheduling algorithm.
	 *
	 * @return 	 * @return returns calculated thread priority.
	 *
	 */
    double getThreadPriority();

	/**
	 * Function: suspend
	 *
	 *	This function is called to resume the "work" done in the thread.
	 *
	 * @return none.
	 *
	 */
	void resume ();

	/**
	 * Function: run
	 *
	 *	Starts the thread running.
	 *
	 * @return none.
	 *
	 */
	void run();


	/**
	 * Function: setNextDeadline
	 *
	 *	Sets the calculated next deadline the thread
	 *	is supposed to run in.  The deadline is
	 *	calculated from the start of the simulation
	 *	which is time 0.
	 *
	 * @return none.
	 *
	 */
    void setNextDeadline (int newDeadline);

	/**
	 * Function: setNextPeriod
	 *
	 *	Sets the calculated next period the thread
	 *	is supposed to run in.  The period is
	 *	calculated from the start of the simulation
	 *	which is time 0.
	 *
	 * @return none.
	 *
	 */
    void setNextPeriod (int newPeriod);

	/**
	 * Function: setThreadPriority
	 *
	 *	Sets the calculated priority of a thread.  This is dependent on the
	 *	scheduling algorithm.
	 *
	 * @return none.
	 *
	 */
    void setThreadPriority (double newThreadPriority);

	/**
	 * Function: startNewComputePeriod
	 *
	 *	This function is called once the "work" is done. It resets the
	 *	the loop variable and any variables related to computation time.
	 *
	 * @return none.
	 *
	 */
	void startNewComputePeriod ();

	/**
	 * Function: suspend
	 *
	 *	This function is called to pause the "work" done in the thread.
	 *
	 * @return none.
	 *
	 */
	void suspend ();

	/**
	 * Function: stop
	 *
	 *	This function is called terminate the thread.
	 *
	 * @return none.
	 *
	 */
	void stop();


private:

	// Implementation of our code from mythreadclass.h
	virtual void InternalThreadEntry();


	// Holds the thread's compute time.
	int computeTimems;

	// Holds the thread's deadline.
	int deadlinems;

	// Holds the thread's period.
	int periodms;

	// Holds the thread's unique threadID.
	int TCBThreadID;

	// Holds the number of iterations of "work" that is done in a millisec.
	double interationsPerMilisec;

	// Holds the priority of the threads for the algorithms. Whether a low
	// value represents a high priority or vice versa is dependent on the
	// algorithm.
	double threadPriority;

	// This is a counter that deincrements while work is being done.
	int doWork;

	// Holds the amount of the period executed.  Value is between 0 and periodms;
	int periodExecutedms;

	// Holds the calculated next period the thread is supposed to run in.
	// The period is calculated from the start of the simulation which is time 0.
	int nextPeriodms;

	// Holds the calculated next deadline the thread is supposed to run in.
	// The deadline is calculated from the start of the simulation which is time 0.
	int nextDeadlinems;

	// This variable controls the running of the main thread loop.
	bool running;

	// Holds the number of iterations the "work" loop is supposed to run for.
	// It equivalent in time to computeTimems.
	int computeTimeIterations;

	// This is a reference to the incoming message queue in TCBscheduler.
	mqd_t toSchedmq;

	// This is used to start and stop the "work" the thread is supposed to do.
	pthread_mutex_t TCBMutex;
};
#endif
