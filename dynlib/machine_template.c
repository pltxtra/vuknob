/*
 * SATAN, Signal Applications To Any Network
 * Copyright (C) 2003 by Anton Persson & Johan Thim
 * Copyright (C) 2005 by Anton Persson
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 * Copyright (C) 2011 by Anton Persson
 *
 * About SATAN:
 *   Originally developed as a small subproject in
 *   a course about applied signal processing.
 * Original Developers:
 *   Anton Persson
 *   Johan Thim
 *
 * http://www.733kru.org/
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

#include "dynlib.h"
#include <stdlib.h>
#include <string.h>

typedef struct _MData {
} machineTemplate_t;

void *init(MachineTable *mt, const char *name) {
	/* Allocate and initiate instance data here */
	machineTemplate_t *data = (machineTemplate_t *)malloc(sizeof(machineTemplate_t));
	if(data == NULL) return NULL;

	// init data here
	/* return pointer to instance data */
	return (void *)data;
}
 
void *get_controller_ptr(MachineTable *mt, void *data,
			 const char *name,
			 const char *group) {
	return NULL;
}

void reset(MachineTable *mt, void *data) {
	
}

void execute(MachineTable *mt, void *data) {
}

void delete(void *data) {
	// clear and clean up everything here
	
	/* then free instance data here */
	free(data);
}
