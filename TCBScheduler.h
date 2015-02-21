/*
 * TCBScheduler.h
 *
 *  Created on: Feb 18, 2015
 *      Author: hxr5656
 */

#ifndef TCBSCHEDULER_H_
#define TCBSCHEDULER_H_

#include <vector>
#include <list>

#include "MyThread.h"
#include "TCBThread.h"

struct TaskParam
{
	double configComputeTimems;
	double configPeriodms;
	double configDeadlinems;

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

	enum SchedulingStrategy
	{
		RMS,
		EDF,
		LST
	};

	// Its a constructor Jim. - Spock.
	TCBScheduler(std::vector <TaskParam>& threadConfigs, long iterationsPerSecond);

    virtual ~TCBScheduler() {/* empty */}

	// function that is called to run the banks main loop
	void run();

	void startSim ();

	void stopSim ();

	void initialize ();

	void stop();

	void setSimTime(int newSimTimeSec) {simTimeSec = newSimTimeSec; }

	int getSimTime() {return simTimeSec; }

	void setSchedulingStrategy(SchedulingStrategy newStrategy) {strategy = newStrategy; }

	SchedulingStrategy getSchedulingStrategy() {return strategy; }


private:
    // Implementation of our code from mythreadclass.h
	virtual void InternalThreadEntry();

//	void rateMonotinicScheduler();
//  void leastSlackTime();
//  void EarliestDeadlineFirst();

	int simTimeSec;

	SchedulingStrategy strategy;

	std::vector <TCBThread> TCBThreads;

	std::list <TCBThread> * TCBThreadQueue;

	bool running;
};

#endif /* TCBSCHEDULER_H_ */
