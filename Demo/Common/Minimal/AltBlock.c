/*
	FreeRTOS.org V4.8.0 - Copyright (C) 2003-2008 Richard Barry.

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
	*																		  *
	* SAVE TIME AND MONEY!  Why not get us to quote to get FreeRTOS.org		  *
	* running on your hardware - or even write all or part of your application*
	* for you?  See http://www.OpenRTOS.com for details.					  *
	*																		  *
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

/*
 * This is a version of BlockTim.c that uses the light weight API.
 *
 * This file contains some test scenarios that ensure tasks do not exit queue
 * send or receive functions prematurely.  A description of the tests is
 * included within the code.
 */

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Demo includes. */
#include "AltBlock.h"

/* Task priorities. */
#define bktPRIMARY_PRIORITY			( 3 )
#define bktSECONDARY_PRIORITY		( 2 )

/* Task behaviour. */
#define bktQUEUE_LENGTH				( 5 )
#define bktSHORT_WAIT				( ( ( portTickType ) 20 ) / portTICK_RATE_MS )
#define bktPRIMARY_BLOCK_TIME		( 10 )
#define bktALLOWABLE_MARGIN			( 12 )
#define bktTIME_TO_BLOCK			( 175 )
#define bktDONT_BLOCK				( ( portTickType ) 0 )
#define bktRUN_INDICATOR			( ( unsigned portBASE_TYPE ) 0x55 )

/* The queue on which the tasks block. */
static xQueueHandle xTestQueue;

/* Handle to the secondary task is required by the primary task for calls
to vTaskSuspend/Resume(). */
static xTaskHandle xSecondary;

/* Used to ensure that tasks are still executing without error. */
static portBASE_TYPE xPrimaryCycles = 0, xSecondaryCycles = 0;
static portBASE_TYPE xErrorOccurred = pdFALSE;

/* Provides a simple mechanism for the primary task to know when the
secondary task has executed. */
static volatile unsigned portBASE_TYPE xRunIndicator;

/* The two test tasks.  Their behaviour is commented within the files. */
static void vPrimaryBlockTimeTestTask( void *pvParameters );
static void vSecondaryBlockTimeTestTask( void *pvParameters );

/*-----------------------------------------------------------*/

void vCreateAltBlockTimeTasks( void )
{
	/* Create the queue on which the two tasks block. */
    xTestQueue = xQueueCreate( bktQUEUE_LENGTH, sizeof( portBASE_TYPE ) );

	/* Create the two test tasks. */
	xTaskCreate( vPrimaryBlockTimeTestTask, ( signed portCHAR * )"FBTest1", configMINIMAL_STACK_SIZE, NULL, bktPRIMARY_PRIORITY, NULL );
	xTaskCreate( vSecondaryBlockTimeTestTask, ( signed portCHAR * )"FBTest2", configMINIMAL_STACK_SIZE, NULL, bktSECONDARY_PRIORITY, &xSecondary );
}
/*-----------------------------------------------------------*/

static void vPrimaryBlockTimeTestTask( void *pvParameters )
{
portBASE_TYPE xItem, xData;
portTickType xTimeWhenBlocking;
portTickType xTimeToBlock, xBlockedTime;

	#ifdef USE_STDIO
	void vPrintDisplayMessage( const portCHAR * const * ppcMessageToSend );
	
		const portCHAR * const pcTaskStartMsg = "Alt primary block time test started.\r\n";

		/* Queue a message for printing to say the task has started. */
		vPrintDisplayMessage( &pcTaskStartMsg );
	#endif

	( void ) pvParameters;

	for( ;; )
	{
		/*********************************************************************
        Test 1

        Simple block time wakeup test on queue receives. */
		for( xItem = 0; xItem < bktQUEUE_LENGTH; xItem++ )
		{
			/* The queue is empty. Attempt to read from the queue using a block
			time.  When we wake, ensure the delta in time is as expected. */
			xTimeToBlock = bktPRIMARY_BLOCK_TIME << xItem;

			/* A critical section is used to minimise the jitter in the time
			measurements. */
			portENTER_CRITICAL();
			{
				xTimeWhenBlocking = xTaskGetTickCount();
				
				/* We should unblock after xTimeToBlock having not received
				anything on the queue. */
				if( xQueueAltReceive( xTestQueue, &xData, xTimeToBlock ) != errQUEUE_EMPTY )
				{
					xErrorOccurred = pdTRUE;
				}

				/* How long were we blocked for? */
				xBlockedTime = xTaskGetTickCount() - xTimeWhenBlocking;
			}
			portEXIT_CRITICAL();

			if( xBlockedTime < xTimeToBlock )
			{
				/* Should not have blocked for less than we requested. */
				xErrorOccurred = pdTRUE;
			}

			if( xBlockedTime > ( xTimeToBlock + bktALLOWABLE_MARGIN ) )
			{
				/* Should not have blocked for longer than we requested,
				although we would not necessarily run as soon as we were
				unblocked so a margin is allowed. */
				xErrorOccurred = pdTRUE;
			}
		}


		#if configUSE_PREEMPTION == 0
			taskYIELD();
		#endif


		/*********************************************************************
        Test 2

        Simple block time wakeup test on queue sends.

		First fill the queue.  It should be empty so all sends should pass. */
		for( xItem = 0; xItem < bktQUEUE_LENGTH; xItem++ )
		{
			if( xQueueAltSendToBack( xTestQueue, &xItem, bktDONT_BLOCK ) != pdPASS )
			{
				xErrorOccurred = pdTRUE;
			}
		}

		for( xItem = 0; xItem < bktQUEUE_LENGTH; xItem++ )
		{
			/* The queue is full. Attempt to write to the queue using a block
			time.  When we wake, ensure the delta in time is as expected. */
			xTimeToBlock = bktPRIMARY_BLOCK_TIME << xItem;

			portENTER_CRITICAL();
			{
				xTimeWhenBlocking = xTaskGetTickCount();
				
				/* We should unblock after xTimeToBlock having not received
				anything on the queue. */
				if( xQueueAltSendToBack( xTestQueue, &xItem, xTimeToBlock ) != errQUEUE_FULL )
				{
					xErrorOccurred = pdTRUE;
				}

				/* How long were we blocked for? */
				xBlockedTime = xTaskGetTickCount() - xTimeWhenBlocking;
			}
			portEXIT_CRITICAL();

			if( xBlockedTime < xTimeToBlock )
			{
				/* Should not have blocked for less than we requested. */
				xErrorOccurred = pdTRUE;
			}

			if( xBlockedTime > ( xTimeToBlock + bktALLOWABLE_MARGIN ) )
			{
				/* Should not have blocked for longer than we requested,
				although we would not necessarily run as soon as we were
				unblocked so a margin is allowed. */
				xErrorOccurred = pdTRUE;
			}
		}

		#if configUSE_PREEMPTION == 0
			taskYIELD();
		#endif

		
		/*********************************************************************
        Test 3

		Wake the other task, it will block attempting to post to the queue.
		When we read from the queue the other task will wake, but before it
		can run we will post to the queue again.  When the other task runs it
		will find the queue still full, even though it was woken.  It should
		recognise that its block time has not expired and return to block for
		the remains of its block time.

		Wake the other task so it blocks attempting to post to the already
		full queue. */
		xRunIndicator = 0;
		vTaskResume( xSecondary );

		/* We need to wait a little to ensure the other task executes. */
		while( xRunIndicator != bktRUN_INDICATOR )
		{
			/* The other task has not yet executed. */
			vTaskDelay( bktSHORT_WAIT );
		}
		/* Make sure the other task is blocked on the queue. */
		vTaskDelay( bktSHORT_WAIT );
		xRunIndicator = 0;

		for( xItem = 0; xItem < bktQUEUE_LENGTH; xItem++ )
		{
			/* Now when we make space on the queue the other task should wake
			but not execute as this task has higher priority. */				
			if( xQueueAltReceive( xTestQueue, &xData, bktDONT_BLOCK ) != pdPASS )
			{
				xErrorOccurred = pdTRUE;
			}

			/* Now fill the queue again before the other task gets a chance to
			execute.  If the other task had executed we would find the queue
			full ourselves, and the other task have set xRunIndicator. */
			if( xQueueAltSendToBack( xTestQueue, &xItem, bktDONT_BLOCK ) != pdPASS )
			{
				xErrorOccurred = pdTRUE;
			}

			if( xRunIndicator == bktRUN_INDICATOR )
			{
				/* The other task should not have executed. */
				xErrorOccurred = pdTRUE;
			}

			/* Raise the priority of the other task so it executes and blocks
			on the queue again. */
			vTaskPrioritySet( xSecondary, bktPRIMARY_PRIORITY + 2 );

			/* The other task should now have re-blocked without exiting the
			queue function. */
			if( xRunIndicator == bktRUN_INDICATOR )
			{
				/* The other task should not have executed outside of the
				queue function. */
				xErrorOccurred = pdTRUE;
			}

			/* Set the priority back down. */
			vTaskPrioritySet( xSecondary, bktSECONDARY_PRIORITY );			
		}

		/* Let the other task timeout.  When it unblockes it will check that it
		unblocked at the correct time, then suspend itself. */
		while( xRunIndicator != bktRUN_INDICATOR )
		{
			vTaskDelay( bktSHORT_WAIT );
		}
		vTaskDelay( bktSHORT_WAIT );
		xRunIndicator = 0;

		#if configUSE_PREEMPTION == 0
			taskYIELD();
		#endif

		/*********************************************************************
        Test 4

		As per test 3 - but with the send and receive the other way around.
		The other task blocks attempting to read from the queue.

		Empty the queue.  We should find that it is full. */
		for( xItem = 0; xItem < bktQUEUE_LENGTH; xItem++ )
		{
			if( xQueueAltReceive( xTestQueue, &xData, bktDONT_BLOCK ) != pdPASS )
			{
				xErrorOccurred = pdTRUE;
			}
		}
		
		/* Wake the other task so it blocks attempting to read from  the
		already	empty queue. */
		vTaskResume( xSecondary );

		/* We need to wait a little to ensure the other task executes. */
		while( xRunIndicator != bktRUN_INDICATOR )
		{
			vTaskDelay( bktSHORT_WAIT );
		}
		vTaskDelay( bktSHORT_WAIT );
		xRunIndicator = 0;

		for( xItem = 0; xItem < bktQUEUE_LENGTH; xItem++ )
		{
			/* Now when we place an item on the queue the other task should
			wake but not execute as this task has higher priority. */				
			if( xQueueAltSendToBack( xTestQueue, &xItem, bktDONT_BLOCK ) != pdPASS )
			{
				xErrorOccurred = pdTRUE;
			}

			/* Now empty the queue again before the other task gets a chance to
			execute.  If the other task had executed we would find the queue
			empty ourselves, and the other task would be suspended. */
			if( xQueueAltReceive( xTestQueue, &xData, bktDONT_BLOCK ) != pdPASS )
			{
				xErrorOccurred = pdTRUE;
			}

			if( xRunIndicator == bktRUN_INDICATOR )
			{
				/* The other task should not have executed. */
				xErrorOccurred = pdTRUE;
			}

			/* Raise the priority of the other task so it executes and blocks
			on the queue again. */
			vTaskPrioritySet( xSecondary, bktPRIMARY_PRIORITY + 2 );

			/* The other task should now have re-blocked without exiting the
			queue function. */
			if( xRunIndicator == bktRUN_INDICATOR )
			{
				/* The other task should not have executed outside of the
				queue function. */
				xErrorOccurred = pdTRUE;
			}
			vTaskPrioritySet( xSecondary, bktSECONDARY_PRIORITY );			
		}

		/* Let the other task timeout.  When it unblockes it will check that it
		unblocked at the correct time, then suspend itself. */
		while( xRunIndicator != bktRUN_INDICATOR )
		{
			vTaskDelay( bktSHORT_WAIT );
		}
		vTaskDelay( bktSHORT_WAIT );

		xPrimaryCycles++;
	}
}
/*-----------------------------------------------------------*/

static void vSecondaryBlockTimeTestTask( void *pvParameters )
{
portTickType xTimeWhenBlocking, xBlockedTime;
portBASE_TYPE xData;

	#ifdef USE_STDIO
	void vPrintDisplayMessage( const portCHAR * const * ppcMessageToSend );
	
		const portCHAR * const pcTaskStartMsg = "Alt secondary block time test started.\r\n";

		/* Queue a message for printing to say the task has started. */
		vPrintDisplayMessage( &pcTaskStartMsg );
	#endif

	( void ) pvParameters;

	for( ;; )
	{
		/*********************************************************************
        Test 1 and 2

		This task does does not participate in these tests. */
		vTaskSuspend( NULL );

		/*********************************************************************
        Test 3

		The first thing we do is attempt to read from the queue.  It should be
		full so we block.  Note the time before we block so we can check the
		wake time is as per that expected. */
		portENTER_CRITICAL();
		{
			xTimeWhenBlocking = xTaskGetTickCount();
			
			/* We should unblock after bktTIME_TO_BLOCK having not received
			anything on the queue. */
			xData = 0;
			xRunIndicator = bktRUN_INDICATOR;
			if( xQueueAltSendToBack( xTestQueue, &xData, bktTIME_TO_BLOCK ) != errQUEUE_FULL )
			{
				xErrorOccurred = pdTRUE;
			}

			/* How long were we inside the send function? */
			xBlockedTime = xTaskGetTickCount() - xTimeWhenBlocking;
		}
		portEXIT_CRITICAL();

		/* We should not have blocked for less time than bktTIME_TO_BLOCK. */
		if( xBlockedTime < bktTIME_TO_BLOCK )
		{
			xErrorOccurred = pdTRUE;
		}

		/* We should of not blocked for much longer than bktALLOWABLE_MARGIN
		either.  A margin is permitted as we would not necessarily run as
		soon as we unblocked. */
		if( xBlockedTime > ( bktTIME_TO_BLOCK + bktALLOWABLE_MARGIN ) )
		{
			xErrorOccurred = pdTRUE;
		}

		/* Suspend ready for test 3. */
		xRunIndicator = bktRUN_INDICATOR;
		vTaskSuspend( NULL );

		/*********************************************************************
        Test 4

		As per test three, but with the send and receive reversed. */
		portENTER_CRITICAL();
		{
			xTimeWhenBlocking = xTaskGetTickCount();
			
			/* We should unblock after bktTIME_TO_BLOCK having not received
			anything on the queue. */
			xRunIndicator = bktRUN_INDICATOR;
			if( xQueueAltReceive( xTestQueue, &xData, bktTIME_TO_BLOCK ) != errQUEUE_EMPTY )
			{
				xErrorOccurred = pdTRUE;
			}

			xBlockedTime = xTaskGetTickCount() - xTimeWhenBlocking;
		}
		portEXIT_CRITICAL();

		/* We should not have blocked for less time than bktTIME_TO_BLOCK. */
		if( xBlockedTime < bktTIME_TO_BLOCK )
		{
			xErrorOccurred = pdTRUE;
		}

		/* We should of not blocked for much longer than bktALLOWABLE_MARGIN
		either.  A margin is permitted as we would not necessarily run as soon
		as we unblocked. */
		if( xBlockedTime > ( bktTIME_TO_BLOCK + bktALLOWABLE_MARGIN ) )
		{
			xErrorOccurred = pdTRUE;
		}

		xRunIndicator = bktRUN_INDICATOR;

		xSecondaryCycles++;
	}
}
/*-----------------------------------------------------------*/

portBASE_TYPE xAreAltBlockTimeTestTasksStillRunning( void )
{
static portBASE_TYPE xLastPrimaryCycleCount = 0, xLastSecondaryCycleCount = 0;
portBASE_TYPE xReturn = pdPASS;

	/* Have both tasks performed at least one cycle since this function was
	last called? */
	if( xPrimaryCycles == xLastPrimaryCycleCount )
	{
		xReturn = pdFAIL;
	}

	if( xSecondaryCycles == xLastSecondaryCycleCount )
	{
		xReturn = pdFAIL;
	}

	if( xErrorOccurred == pdTRUE )
	{
		xReturn = pdFAIL;
	}

	xLastSecondaryCycleCount = xSecondaryCycles;
	xLastPrimaryCycleCount = xPrimaryCycles;

	return xReturn;
}
