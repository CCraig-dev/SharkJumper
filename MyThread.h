/*
 * MyThread.h
 *
 *  Created on: Feb 16, 2015
 *      Author: hxr5656
 */

// this code is from http://stackoverflow.com/questions/1151582/pthread-function-from-a-class
// I modified it by making some of the functions protected to better encapsulate the thread
// functionality HR.

#ifndef _MYTHREAD_H_
#define _MYTHREAD_H_

#include <pthread.h>
#include <iostream.h>

class MyThread
{
public:
   MyThread() {/* empty */}
   virtual ~MyThread() {/* empty */}

   /** Will not return until the internal thread has exited. */
   void WaitForInternalThreadToExit()
   {
      (void) pthread_join(_thread, NULL);
   }

protected:
   /** Returns true if the thread was successfully started, false if there was an error starting the thread */
   bool StartInternalThread()
   {
      return (pthread_create(&_thread, NULL, InternalThreadEntryFunc, this) == 0);
   }

   /** Implement this method in your subclass with the code you want your thread to run. */
   virtual void InternalThreadEntry() = 0;

protected:
   static void * InternalThreadEntryFunc(void * This) {((MyThread *)This)->InternalThreadEntry(); return NULL;}

   pthread_t _thread;
};

#endif

