/*
	FreeRTOS.org V5.0.4 - Copyright (C) 2003-2008 Richard Barry.

	This file is part of the FreeRTOS.org distribution.

	FreeRTOS.org is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	FreeRTOS.org is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FreeRTOS.org; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	A special exception to the GPL can be applied should you wish to distribute
	a combined work that includes FreeRTOS.org, without being obliged to provide
	the source code for any proprietary components.  See the licensing section 
	of http://www.FreeRTOS.org for full details of how and when the exception
	can be applied.

    ***************************************************************************
    ***************************************************************************
    *                                                                         *
    * SAVE TIME AND MONEY!  We can port FreeRTOS.org to your own hardware,    *
    * and even write all or part of your application on your behalf.          *
    * See http://www.OpenRTOS.com for details of the services we provide to   *
    * expedite your project.                                                  *
    *                                                                         *
    ***************************************************************************
    ***************************************************************************

	Please ensure to read the configuration and relevant port sections of the
	online documentation.

	http://www.FreeRTOS.org - Documentation, latest information, license and 
	contact details.

	http://www.SafeRTOS.com - A version that is certified for use in safety 
	critical systems.

	http://www.OpenRTOS.com - Commercial support, development, porting, 
	licensing and training services.
*/

#include "FreeRTOSConfig.h"

portSAVE_CONTEXT macro
                /* Save the remaining registers. */
		push	r4
		push	r5
		push	r6
		push	r7
		push	r8
		push	r9
		push	r10
		push	r11
		push	r12
		push	r13
		push	r14
		push	r15
		mov.w	&_usCriticalNesting, r14
		push	r14
		mov.w	&_pxCurrentTCB, r12
		mov.w	r1, @r12
		endm
/*-----------------------------------------------------------*/
		
portRESTORE_CONTEXT macro
		mov.w	&_pxCurrentTCB, r12
		mov.w	@r12, r1
		pop		r15
		mov.w	r15, &_usCriticalNesting
		pop		r15
		pop		r14
		pop		r13
		pop		r12
		pop		r11
		pop		r10
		pop		r9
		pop		r8
		pop		r7
		pop		r6
		pop		r5
		pop		r4

		/* The last thing on the stack will be the status register.
                Ensure the power down bits are clear ready for the next
                time this power down register is popped from the stack. */
		bic.w   #0xf0,0(SP)

		reti
		endm
/*-----------------------------------------------------------*/


.CODE

/*
 * The RTOS tick ISR.
 *
 * If the cooperative scheduler is in use this simply increments the tick 
 * count.
 *
 * If the preemptive scheduler is in use a context switch can also occur.
 */
_vTickISR:
		portSAVE_CONTEXT
				
		call	#_vTaskIncrementTick

		#if configUSE_PREEMPTION == 1
			call	#_vTaskSwitchContext
		#endif
		
		portRESTORE_CONTEXT
/*-----------------------------------------------------------*/


/*
 * Manual context switch called by the portYIELD() macro.
 */                
_vPortYield::

		/* Mimic an interrupt by pushing the SR. */
		push	SR			

		/* Now the SR is stacked we can disable interrupts. */
		dint			
				
		/* Save the context of the current task. */
		portSAVE_CONTEXT			

		/* Switch to the highest priority task that is ready to run. */
		call	#_vTaskSwitchContext		

		/* Restore the context of the new task. */
		portRESTORE_CONTEXT
/*-----------------------------------------------------------*/


/*
 * Start off the scheduler by initialising the RTOS tick timer, then restoring
 * the context of the first task.
 */
_xPortStartScheduler::

		/* Setup the hardware to generate the tick.  Interrupts are disabled 
		when this function is called. */
		call	#_prvSetupTimerInterrupt

		/* Restore the context of the first task that is going to run. */
		portRESTORE_CONTEXT
/*-----------------------------------------------------------*/          
      		

		/* Place the tick ISR in the correct vector. */
		.VECTORS
		
		.KEEP
		
		ORG		TIMERA0_VECTOR
		DW		_vTickISR
		


		END
		
