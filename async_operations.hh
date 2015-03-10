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

#ifndef __ASYNC_OPERATIONS
#define __ASYNC_OPERATIONS

#include "dynlib/dynlib.h"
#include <functional>

class AsyncOperations {
private:
	// private constructor prevents creation of illegal objects
	AsyncOperations();
public:
	class YouDontHaveTheToken {};
	
	/// start up the AsyncOperationsThread - this will return a valid pointer only once, subsequent calls will return NULL
	static AsyncOperations *start_async_operations_thread();

	// Async Operations - if not called through a valid object it will throw YouDontHaveTheToken
	void run_async_operation(AsyncOp *op);

	// Async Operations - if not called through a valid object it will throw YouDontHaveTheToken
	void run_async_function(std::function<void()> function);
};

#endif

