/*
 * Shell.cc
 *
 *  Created on: Feb 16, 2015
 *      Author: clc1774
 */

#include "TCBScheduler.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/netmgr.h>

using namespace std;

#define MY_PULSE_CODE   _PULSE_CODE_MINAVAIL

typedef union {
	struct _pulse pulse;
} my_message_t;

vector<TaskParam> threadConfigs;
TCBScheduler::SchedulingStrategy selectedStrategy = TCBScheduler::UNDEFINED;
bool keepRunning = true;
pthread_mutex_t timimgMutex;

// Text helper functions
void usage(int i);
string getStringFromAlg(TCBScheduler::SchedulingStrategy selectedStrategy);

// Validation helper functions
bool validateInput(string strCommand, string strInput);
bool validateTaskInput(string strInput);
bool validateAlgInput(string strInput);
bool isInstruction(string strCommand);
bool schedulabilityTest(int compute, int period, int deadline);

// Execution functions
void fireInstruction(string strCommand);
void cleanup();
void stageExecutionWith(string strCommand, string strInput,
		TCBScheduler::SchedulingStrategy &selectedStrategy,
		vector<TaskParam> &threadConfigs);

// I/O helper functions
string promptForNewCommand();
string promptForCommandInput(string strCommand);

// Timing helper functions
int detectIterations();
bool calculateComputationTime(int channelID, int &iterationsPerSecond);
void* measureTime(void* arg);
void setStateHard();

int main(int argc, char *argv[]) {

	cout << "Welcome to the Group 2 Scheduling Project!\n";
	cout << "Type 'usage' for program instructions. \n";

	// After the user gets usage, begin the input loop
	// This loop creates two child loops which will prompt the user for a
	// StrCommand and a StrInput respectively. There exist three Instruction base-
	// cases for commands:
	//  -(strCommand: "exit") = the program exits
	//  -(strCommand: "usage") = the program prints usage instructions
	//  -(strCommand: "run") = the program runs the staged strCommand with strInput
	// The other two commands are:
	//  -(strCommand: 1) Command to enter a task. Will require strInput
	//  -(strCommand: 2) Command to enter an algorithm selection. Will require strInput.
	//

	for (;;) {
		cout << "Staged Algorithm: " << getStringFromAlg(selectedStrategy)
				<< "\n";
		cout << "Staged Tasks: ";
		int size = threadConfigs.size();
		if (size == 0) {
			cout << "UNDEFINED.\n";
		} else {
			for (int i = 0; i < size; i++) {
				cout << "<" << threadConfigs[i].configComputeTimems << ","
						<< threadConfigs[i].configPeriodms << ","
						<< threadConfigs[i].configDeadlinems << "> ";
			}
			cout << "\n";
		}
		string strCommand;
		string strInput = "";

		// Ask the user for a command, return only when it is valid and meaningful
		// Defining this strCommand is its own while loop.
		//
		strCommand = promptForNewCommand(); //this makes a while loop of user prompting

		if (isInstruction(strCommand) == true) {
			//if the command is an instruction that does not need further parameters
			fireInstruction(strCommand);
		} else {
			//if the command is (1) or (2), ask for more input
			strInput = promptForCommandInput(strCommand); //this makes a while loop of user prompting
			stageExecutionWith(strCommand, strInput, selectedStrategy,
					threadConfigs);
		}
	}
	return 0;
}

//Set hardcoded values for debug purposes
void setStateHard() {
	threadConfigs.clear();
	TaskParam one = TaskParam(100, 300, 200);
	TaskParam two = TaskParam(200, 500, 400);
	TaskParam three = TaskParam(100, 1000, 900);
	threadConfigs.push_back(one);
	threadConfigs.push_back(two);
	threadConfigs.push_back(three);

	selectedStrategy = TCBScheduler::RMS;
}

bool schedulabilityTest(int compute, int period, int deadline) {
	//Test failure case where deadline is greater than period

	if (compute >= period) {
		//it's bad
		cout << "ERROR: Compute >= Period. Cannot be Scheduled. \n";
		return false;
	}
	if (deadline > period) {
		//it's bad
		cout << "ERROR: Deadline > Period. Cannot be Scheduled. \n";
		return false;
	}
	if (compute >= deadline) {
		//it's bad
		cout << "ERROR: Compute > Deadline. Cannot be Scheduled. \n";
		return false;
	}
	//If we pass all tests
	return true;
}

// Helper function takes strCommand and strInput and sets the execution state correctly
void stageExecutionWith(string strCommand, string strInput,
		TCBScheduler::SchedulingStrategy &selectedStrategy,
		vector<TaskParam> &threadConfigs) {

	//Deal with Tasks
	if (strCommand.compare("1") == 0) {

		istringstream iss(strInput);
		string token;
		int counter = 0;

		int tempCompute = 0;
		int tempPeriod = 0;
		int tempDeadline = 0;

		// Loop over the input string to read data to temp variables
		while (getline(iss, token, ',') && counter < 3) {
			// String conversion here, can be improved
			int tokenValue = atoi(token.c_str());

			//Hardcode the way to load a tdb
			if (counter == 0) {
				tempCompute = tokenValue;
			} else if (counter == 1) {
				tempPeriod = tokenValue;
			} else if (counter == 2) {
				tempDeadline = tokenValue;
			}
			counter++;
		}
		bool isSchedulable = schedulabilityTest(tempCompute, tempPeriod,
				tempDeadline);
		if (isSchedulable == true) {
			struct TaskParam tempTP;
			tempTP.configComputeTimems = tempCompute;
			tempTP.configPeriodms = tempPeriod;
			tempTP.configDeadlinems = tempDeadline;
			threadConfigs.push_back(tempTP);
			cout << "Added TaskParam<" << tempTP.configComputeTimems << ","
					<< tempTP.configPeriodms << "," << tempTP.configDeadlinems
					<< "> to staging.\n";
		}

		//Deal with Algorithm
	} else if (strCommand.compare("2") == 0) {
		int tokenValue = atoi(strInput.c_str());
		switch (tokenValue) {
		case 1:
			selectedStrategy = TCBScheduler::RMS;
			break;
		case 2:
			selectedStrategy = TCBScheduler::SCT;
			break;
		case 3:
			selectedStrategy = TCBScheduler::EDF;
			break;
		default:
			cerr << "ERROR: bad algorithm input has deceived validation.\n";
			cout << "Exiting the application.\n";
			exit(1);
			break;
		}
	} else {
		cerr << "ERROR: bad command input has deceived validation.\n";
		cout << "Exiting the application.\n";
		exit(1);
	}
}

// Helper function exists to address the obvious input errors, such as entering nulls.
bool validateCommand(string strCommand) {
	if (strCommand.size() <= 0) {
		cerr << "ERROR: Null arguments are unaccepted.\n";
		return false;
	}
	if (isInstruction(strCommand)) {
		return true;
	}

	// parse out int commands that aren't 1 or 2
	int value = atoi(strCommand.c_str());
	if (value > 2 || value <= 0) {
		cerr << "ERROR: Not a valid command.\n";
		return false;
	}
	return true;
}

// Helper function exists to loop to grab a string input from the user and validate it
// Note: this function checks the populating of the strCommand variable.

string promptForNewCommand() {
	bool commandIsValid = false;
	string strCommand;

	while (!commandIsValid) {
		cout << "Specify a command below:\n";
		getline(cin, strCommand);
		commandIsValid = validateCommand(strCommand);
	}
	return strCommand;
}

// Helper function takes a string command and loops to get valid input for it
// Note: this is to populate the strInput variable.

string promptForCommandInput(string strCommand) {
	bool inputIsValid = false;
	string strInput;

	while (!inputIsValid) {
		int intCommand = atoi(strCommand.c_str());
		switch (intCommand) {
		case 1: //Case 1: Creating a task
			usage(1);
			cout << "Specify and enter a task below:\n";
			getline(cin, strInput);
			if (validateInput(strCommand, strInput) == true) {
				return strInput;
			}
			continue;
		case 2: //Case 2: Selecting an algorithm
			usage(2);
			cout << "Specify and enter a scheduling algorithm below:\n";
			getline(cin, strInput);
			if (validateInput(strCommand, strInput) == true) {
				return strInput;
			}
			continue;
		default:
			//Exit case does not require input
			//Usage case does not require input
			//run case does not require input
			continue;
		}
	}
	return strInput;
}

bool isInstruction(string strCommand) {
	// Begin whitelist
	if (strCommand.compare("hard") == 0) {
		return true;
	} else if (strCommand.compare("exit") == 0) {
		return true;
	} else if (strCommand.compare("usage") == 0) {
		return true;
	} else if (strCommand.compare("run") == 0) {
		return true;
	}
	return false;
}

// Helper function reconciles strCommand with its strInput,
// only returning true if strCommand can run with its provided input.

bool validateInput(string strCommand, string strInput) {
	if (strInput.size() <= 0) {
		cerr << "ERROR: Null arguments are unaccepted.\n";
		return false;
	}
	if (strCommand.compare("1") == 0) {
		//Check strInput for <c,p,d>
		return validateTaskInput(strInput);
	} else if (strCommand.compare("2") == 0) {
		//Check strInput for "RMS", "EDF", "SCT"
		return validateAlgInput(strInput);
	}
	return false;
}

// Input: a string of the form "c,p,d", where c, p, and d are able to be converted to integers.
// Helper function reports if the input string is a well-formed task definition.

bool validateTaskInput(string strInput) {
	istringstream iss(strInput);
	string token;
	int counter = 0;

	// If this hits 3, the input is good.
	// Everytime that a parameter is good, this is incremented up.
	int correctnessCounter = 0;

	// Loop over the input string to detect and validate its data
	while (getline(iss, token, ',') && counter < 3) {
		// String conversion here, can be improved
		int tokenValue = atoi(strInput.c_str());

		if (tokenValue > 0) {
			correctnessCounter++;
		}
		// All three c,p,d are valid, return true
		if (correctnessCounter == 3) {
			//cout << "The entered task is valid.\n";
			return true;
		}
		counter++;
	}
	cout << "ERROR: invalid task.\n";

	return false;
}

// Helper function that makes sure the input is a valid algorithm selection 1,2, or 3
bool validateAlgInput(string strInput) {
	int tokenValue = atoi(strInput.c_str());
	if (tokenValue > 0 && tokenValue < 4) {
		//cout << "The entered algorithm is valid.\n";
		return true;
	}
	cout << "ERROR: invalid algorithm.\n";
	return false;
}

// Check to see if we are good to invoke the scheduler
bool validateRun() {
	if (selectedStrategy == TCBScheduler::UNDEFINED) {
		return false;
	}
	if (threadConfigs.size() < 0) {
		return false;
	}
	return true;
}

// Helper function takes an instruction and executes it.

void fireInstruction(string strCommand) {
	//Textual base cases
	if (strCommand.compare("hard") == 0) {
		//Hardcode threadConfigs
		//set iterations time
		//set strategy to RMS
		//set runtime to 5s
		setStateHard();

	} else if (strCommand.compare("exit") == 0) {
		cout << "Program is exiting.\n";
		exit(0);
	} else if (strCommand.compare("usage") == 0) {
		usage(0);
		return;
	} else if (strCommand.compare("run") == 0) {
		cout << "Attempting to run...\n";
		// Invoke the scheduler
		if (validateRun() == true) {
			cout << "How long do you want to run for? (seconds)\n";
			string runtimeInput;
			getline(cin, runtimeInput);
			int runTime = atoi(runtimeInput.c_str());
			if (runTime < 0) {
				cout
						<< "ERROR: Cannot run, runtime not set as positive integer.\n";
				exit(1);
			}

			// Invoke Scheduler
			cout << "Run state is valid...\n";
			int iterationsPerSecond = detectIterations();
			sleep(2);

			TCBScheduler scheduler(threadConfigs, runTime, selectedStrategy,
					iterationsPerSecond);
			scheduler.run();

			// We need this sleep in here to allow the scheduler time to set up the message queues.
			sleep(1);

			if (scheduler.schedulerIsInitialized()) {
				scheduler.setSimTime(5);
				cout << "Simulation is beginning...\n";
				scheduler.startSim();
			}
			sleep(50);
		} else {
			cout << "ERROR: Cannot run, parameters not set.\n";
		}
		return;
	}
}

// Helper function exists to provide stdout message about how to use the program.
void usage(int section) {
	switch (section) {
	case 0:
		cout << "Printing usage Below: \n";
		cout << "(1) Enter 1 to enter a task.\n";
		cout << "(2) Enter 2 to select an algorithm.\n";
		cout << "(3) Enter " << "'" << "run" << "'" << " to run the tasks of "
			"(1) and the algorithm of (2) together.\n";
		cout << "(4) Type " << "'" << "usage" << "'"
				<< " to print usage instructions.\n";
		cout << "(5) Type " << "'" << "exit" << "'" << " to exit.\n";

		break;
	case 1:
		cout << "Instructions for creating a task:\n";
		cout
				<< "(1) Input text of the format 'c,p,d' (in milliseconds) then enter to create a task.\n";
		cout << "A valid task is '1000,7000,7000' (without the quotes).\n";

		break;
	case 2:
		cout << "Instructions for selecting an algorithm for runtime:\n";
		cout << "(1) Enter 1 to stage RMS for runtime.\n";
		cout << "(2) Enter 2 to stage SCT for runtime.\n";
		cout << "(3) Enter 3 to stage EDF for runtime.\n";
		break;
	default:
		break;
	}
}

// Helper print function
string getStringFromAlg(TCBScheduler::SchedulingStrategy selectedStrategy) {
	switch (selectedStrategy) {
	case TCBScheduler::RMS:
		return "RMS";
	case TCBScheduler::SCT:
		return "SCT";
	case TCBScheduler::EDF:
		return "EDF";
	case TCBScheduler::UNDEFINED:
		return "UNDEFINED";
	default:
		return "ERROR";
	}
}

int detectIterations() {
	int channelID = 0;
	int iterations = 0;
	double tuningFactor = 0.75;
	// Set up a message channel for this process.
	// You'll use this for getting messages back
	// from the scheduler and for figuring out how may loop iterations in 1 second.
	channelID = ChannelCreate(0);

	calculateComputationTime(channelID, iterations);

	iterations *= tuningFactor;
	cout << "The tuned iterationsPerSecond by using factor (" << tuningFactor
			<< ") : " << iterations << endl;

	return iterations;
}

bool calculateComputationTime(int channelID, int &iterationsPerSecond) {
	bool timingMutexLocked = false;

	struct sigevent event;
	struct itimerspec itime;
	my_message_t msg;
	int rcvid;
	timer_t timer_id;
	pthread_t thread_tid;

	// gotta initialize my mutex before starting.
	pthread_mutex_init(&timimgMutex, NULL);

	// lock the damn thing so that the timing thread doesn't start until we're ready.
	pthread_mutex_lock(&timimgMutex);
	timingMutexLocked = true;

	// create a timing thread.
	int iterationsPerSecondCounter = 0;
	if (pthread_create(&thread_tid, NULL, &measureTime,
			&iterationsPerSecondCounter)) {
		cout << __FUNCTION__ << " Holy crap we couldn't create a thread!!"
				<< endl;
		return false;
	}

	event.sigev_notify = SIGEV_PULSE;
	event.sigev_coid = ConnectAttach(ND_LOCAL_NODE, 0, channelID,
			_NTO_SIDE_CHANNEL, 0);
	event.sigev_priority = getprio(0);
	event.sigev_code = MY_PULSE_CODE;
	timer_create(CLOCK_REALTIME, &event, &timer_id);

	itime.it_value.tv_sec = 1;
	// 500 million nsecs = .5 secs
	itime.it_value.tv_nsec = 000000000;
	itime.it_interval.tv_sec = 1;
	// 500 million nsecs = .5 secs
	itime.it_interval.tv_nsec = 000000000;
	timer_settime(timer_id, 0, &itime, NULL);

	while (keepRunning == true) {
		rcvid = MsgReceive(channelID, &msg, sizeof(msg), NULL);
		if (rcvid == 0) {
			if (msg.pulse.code == MY_PULSE_CODE) {
				//cout << __FUNCTION__ << "we got a pulse from our timer\n" << endl;
				if (timingMutexLocked) {
					//cout << __FUNCTION__ << "unlocking the mutex\n" << endl;
					pthread_mutex_unlock(&timimgMutex);
					timingMutexLocked = false;
				} else if (keepRunning == true) {
					// kill the thread by stopping it and then setting the
					// keep running flag to false
					pthread_mutex_lock(&timimgMutex);
					keepRunning = false;

					// Turns out our numbers were off by 200% we need to capture
					// the value of the counter at this point to ensure accuracy.
					iterationsPerSecond = iterationsPerSecondCounter;
					pthread_mutex_unlock(&timimgMutex);

					//cout << __FUNCTION__ << " iterationsPerSecond 1 " << iterationsPerSecond << endl;
				}
			} // else other pulses ...
		} // else other messages ...
	}

	// Please dont' touch this I don't quite know what it will do.
	//	    if (ConnectDetach(channelID) == -1) {
	//	        printf("Timer: Error in ConnectDetach\n");
	//	    }

	// kill the periodic timer.
	if (timer_delete(timer_id) == -1) {
		cout << "Timer: Error in timer_delete()" << endl;
	}

	//cout << __FUNCTION__ << " waiting for thread to join " << endl;

	if (pthread_join(thread_tid, NULL)) {
		cout << __FUNCTION__ << "Could not join thread. " " << endl";
	}

	//cout << __FUNCTION__ << " end " << endl;

	return true;
}

void* measureTime(void* arg) {
	//cout << __FUNCTION__ << " begin " << endl;

	int *iterationsPerSecondCounter = (int*) arg;
	while (keepRunning == true) {
		pthread_mutex_lock(&timimgMutex);
		//Dereference point to access value
		(*iterationsPerSecondCounter)++;
		pthread_mutex_unlock(&timimgMutex);
	}

	//cout << __FUNCTION__ << " end " << endl;

	return NULL;
}
