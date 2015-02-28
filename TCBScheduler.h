/*
 * TCBScheduler.h
 *
 *  Created on: Feb 18, 2015
 *      Author: hxr5656
 */

#ifndef TCBSCHEDULER_H_
#define TCBSCHEDULER_H_

#include <cstdlib>
#include <list>
#include <mqueue.h>
#include <vector>


#include "MyThread.h"
#include "TCBThread.h"


struct TaskParam
{
	int configComputeTimems;
	int configPeriodms;
	int configDeadlinems;

	// Initialize our variables to default values.
	TaskParam ()
	{
		configComputeTimems = 0;
		configPeriodms = 0;
		configDeadlinems = 0;
	}
};

class TCBScheduler: public MyThread
{
public:

	// These eunums are used to provide common values for
	// the scheduling strategies.
	enum SchedulingStrategy
	{
		RMS,
		EDF,
		LST,
		UNDEFINED
	};

	/**
	 * Function: TCBScheduler
	 *
	 *	Constructor
	 *
	 * @param threadConfigs A vector of threads the user wants us to simulate.
	 * @param iterationsPerSecond value derived after 1 second of c++ looping.
	 *
	 */
	TCBScheduler(std::vector <TaskParam>& threadConfigs, int iterationsPerSecond);

	/**
	 * Function: ~TCBScheduler
	 *
	 * Destructor
	 *
	 */
    virtual ~TCBScheduler() {/* empty */}

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
	 * Function: startSim
	 *
	 *	Starts the simulation running
	 *
	 * @return none.
	 *
	 */
	void startSim ();

	// not sure if we need this.
	void stopSim ();

	/**
	 * Function: stop
	 *
	 *	This function is called terminate the thread.
	 *
	 * @return none.
	 *
	 */
	void stop();

	/**
	 * Function: setSimTime
	 *
	 *	Sets the amount of time the simulation will run for.
	 *
	 * @param newSimTimeSec Amount of time the simulation will run for.
	 *
	 */
	void setSimTime(int newSimTimeSec);

	/**
	 * Function: getDeadlinems
	 *
	 *	Gets the amount of time the simulation will run for.
	 *
	 * @return the user set simulation time.
	 *
	 */
	int getSimTime();

	/**
	 * Function: setSchedulingStrategy
	 *
	 *	Gets the strategy what is used in the simulation.
	 *
	 * @param newStrategy Amount of time the simulation will run for.
	 *
	 */
	void setSchedulingStrategy(SchedulingStrategy newStrategy);

	/**
	 * Function: getSchedulingStrategy
	 *
	 *	gets the strategy what is used in the simulation.
	 *
	 * @return the currently set strategy
	 *
	 */
	SchedulingStrategy getSchedulingStrategy() {return strategy; }


private:

    // Implementation of our code from mythreadclass.h
	virtual void InternalThreadEntry();

	/**
	 * Function: updatetimeSpec
	 *
	 *	Updates the value of timespec my valuems.
	 *
	 * @param time Some time value
	 * @param valuems The number of miliseconds to be added onto the timespec
	 *
	 */
    void updatetimeSpec (timespec & time, int valuems);

	/**
	 * Function: updatetimeSpec
	 *
	 *	Contains the RMS scheduling algorithm.
	 *
	 * @param runingTCBThread the currently running thread
	 * @param currentSimTimems the sim time.
	 *
	 */
    bool rateMonotinicScheduler(int currentSimTimems, TCBThread*& thread);

//  void leastSlackTime();
//  void EarliestDeadlineFirst();

	// This is a reference to the incoming message queue in TCBscheduler.
	mqd_t toSchedmq;

	// This variable controls the running of the main thread loop.
	bool running;

	// This variable is used to control the behaviour of the scheduler when it
	// is running verses not running.
	bool simRunning;

	// Holds the length of the simulation.
	int simTimeSec;

	// Holds the scheduling strategy to be used in the simulation.
	SchedulingStrategy strategy;

	// Holds the start time of the simulation
	timespec startSimTime;

	// Holds the end time of the simulation
	timespec endSimTime;

	// Holds the threads that are used in the simulation
	std::vector <TCBThread> TCBThreads;

	// Not sure if we need this.
	std::list <TCBThread*> TCBThreadQueue;
	std::list <TCBThread*> startTCBThreadQueue;
};

#endif /* TCBSCHEDULER_H_ */
