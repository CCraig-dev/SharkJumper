/*
 * Common.h
 *
 *  Created on: Feb 21, 2015
 *      Author: hxr5656
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <string>

#define MAXMSGSIZE 128

#define MSG_UNDEFINED 0
#define MSG_TCBTHRINITIALIZED 1
#define MSG_SCHEDULARINITIALIZED 2
#define MSG_STARTSIM 3
#define MSG_TCBTHREADONE 4
#define MSG_SIMCOMPLETE 5

// Used for time calculations
#define MILISECPERSEC 1000

struct MsgStruct
{
	int messageType;
	int threadNumber;

	MsgStruct()
	{
		messageType = 0;
		threadNumber = 0;
	}
};

#endif /* COMMON_H_ */
