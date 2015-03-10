/*
 * VuKNOB
 * Copyright (C) 2014 by Anton Persson
 *
 * http://www.vuknob.com/
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <jngldrum/jthread.hh>
#include "async_operations.hh"
#include "readerwriterqueue/readerwriterqueue.h"

//#define __DO_SATAN_DEBUG
#include "satan_debug.hh"

#include <unistd.h>

/************************
 *
 * AsyncOperationsThread
 *
 ************************/

class AsyncOperationsThread : public jThread {	
private:
	struct AsyncEventData {
		AsyncOp *operation_pointer;
		std::function<void()> function;
	};

	enum AsyncEventType {
		aop_pointer, aop_function
	};
	
	class AsyncEvent : public jThread::Event {
	public:
		AsyncEventType type;
		AsyncEventData data;
		
		AsyncEvent() {}
		virtual ~AsyncEvent() {} // we must have a virtual destructor

		void set_operation(AsyncOp *op) {
			data.operation_pointer = op;
			type = aop_pointer;
		}
		
		void set_operation(std::function<void()> func) {
			data.function = func;
			type = aop_function;
		}

		virtual void dummy_event_function() {}
	};

	
	AsyncOperationsThread() : jThread("AsyncOperationsThread") {
	}

	static jThread::EventQueue eq;
	static AsyncOperationsThread *thrd;
	static moodycamel::ReaderWriterQueue<AsyncEvent *> *free_events;

public:
	
	void thread_body() {
		while(1) {
			AsyncEvent *ae = (AsyncEvent *)eq.wait_for_event();

			switch(ae->type) {

			case aop_pointer:
			{
				ae->data.operation_pointer->func(ae->data.operation_pointer);
				ae->data.operation_pointer->finished = -1;
			}
			break;

			case aop_function:
			{
				ae->data.function();
			}
			break;
			
			}
			
			// put back into the free event pool
			free_events->enqueue(ae);
		}
	}

	static void queue_event(AsyncOp *op) {
		AsyncEvent *ev = NULL;
		do {
			if(free_events->try_dequeue(ev)) {
				ev->set_operation(op);
				eq.push_event(ev);
			} else {
				ev = NULL;
				usleep(1000); // this will cause performance drops if the number of total pre-allocated AsyncEvents is too small
				SATAN_ERROR("AsyncOperation error, pre-allocated event not available, delay injected.\n");
			}
		} while(ev == NULL);
	}

	static void queue_event(std::function<void()> f) {
		AsyncEvent *ev = NULL;
		do {
			if(free_events->try_dequeue(ev)) {
				ev->set_operation(f);
				eq.push_event(ev);
			} else {
				ev = NULL;
				usleep(1000); // this will cause performance drops if the number of total pre-allocated AsyncEvents is too small
				SATAN_ERROR("AsyncOperation error, pre-allocated event not available, delay injected.\n");
			}
		} while(ev == NULL);
	}

	static void start_me_up() {
		free_events = new moodycamel::ReaderWriterQueue<AsyncEvent *>(100);

		// fill upp the queue
		AsyncEvent *ev = NULL;
		do {
			ev = new AsyncEvent();
			if(ev) {
				if(free_events->try_enqueue(ev) == false) {
					delete ev;
					ev = NULL;
				}
			}
		} while(ev != NULL);
		
		thrd->run();
	}
	
};

jThread::EventQueue AsyncOperationsThread::eq;
AsyncOperationsThread * AsyncOperationsThread::thrd = new AsyncOperationsThread();
moodycamel::ReaderWriterQueue<AsyncOperationsThread::AsyncEvent *> * AsyncOperationsThread::free_events = NULL;

/************************
 *
 * AsyncOperations - the interface
 *
 ************************/
static AsyncOperations *token_object = NULL;

// empty default constructor - this is used to prevent creation of illegal AsyncOperations objects
AsyncOperations::AsyncOperations() {}

AsyncOperations * AsyncOperations::start_async_operations_thread() {
	if(!token_object) {
		token_object = new AsyncOperations();
		AsyncOperationsThread::start_me_up();
		return token_object;
	}
	return NULL;
}

void AsyncOperations::run_async_operation(AsyncOp *op) {
	if(this == token_object) {
		AsyncOperationsThread::queue_event(op);
	} else
		throw YouDontHaveTheToken();
}

void AsyncOperations::run_async_function(std::function<void()> f) {
	if(this == token_object) {
		SATAN_DEBUG(" will queue AsyncOperation function event.\n");
		AsyncOperationsThread::queue_event(f);
	} else
		throw YouDontHaveTheToken();
}
