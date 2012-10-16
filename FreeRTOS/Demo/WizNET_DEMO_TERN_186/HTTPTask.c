/*
    FreeRTOS V7.3.0 - Copyright (C) 2012 Real Time Engineers Ltd.

    FEATURES AND PORTS ARE ADDED TO FREERTOS ALL THE TIME.  PLEASE VISIT 
    http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS tutorial books are available in pdf and paperback.        *
     *    Complete, revised, and edited pdf reference manuals are also       *
     *    available.                                                         *
     *                                                                       *
     *    Purchasing FreeRTOS documentation will not only help you, by       *
     *    ensuring you get running as quickly as possible and with an        *
     *    in-depth knowledge of how to use FreeRTOS, it will also help       *
     *    the FreeRTOS project to continue with its mission of providing     *
     *    professional grade, cross platform, de facto standard solutions    *
     *    for microcontrollers - completely free of charge!                  *
     *                                                                       *
     *    >>> See http://www.FreeRTOS.org/Documentation for details. <<<     *
     *                                                                       *
     *    Thank you for using FreeRTOS, and thank you for your support!      *
     *                                                                       *
    ***************************************************************************


    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    >>>NOTE<<< The modification to the GPL is included to allow you to
    distribute a combined work that includes FreeRTOS without being obliged to
    provide the source code for proprietary components outside of the FreeRTOS
    kernel.  FreeRTOS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!
    
    ***************************************************************************
     *                                                                       *
     *    Having a problem?  Start by reading the FAQ "My application does   *
     *    not run, what could be wrong?"                                     *
     *                                                                       *
     *    http://www.FreeRTOS.org/FAQHelp.html                               *
     *                                                                       *
    ***************************************************************************

    
    http://www.FreeRTOS.org - Documentation, training, latest versions, license 
    and contact details.  
    
    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool.

    Real Time Engineers ltd license FreeRTOS to High Integrity Systems, who sell 
    the code with commercial support, indemnification, and middleware, under 
    the OpenRTOS brand: http://www.OpenRTOS.com.  High Integrity Systems also
    provide a safety engineered and independently SIL3 certified version under 
    the SafeRTOS brand: http://www.SafeRTOS.com.
*/

/* 
 * Very simple task that responds with a single WEB page to http requests.
 *
 * The WEB page displays task and system status.  A semaphore is used to 
 * wake the task when there is processing to perform as determined by the 
 * interrupts generated by the Ethernet interface.
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>

/* Tern includes. */
#include "utils\system_common.h"
#include "i2chip_hw.h"
#include "socket.h"

/* FreeRTOS.org includes. */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/* The standard http port on which we are going to listen. */
#define httpPORT 80

#define httpTX_WAIT 2

/* Network address configuration. */
const unsigned char ucMacAddress[] =			{ 12, 128, 12, 34, 56, 78 };
const unsigned char ucGatewayAddress[] =		{ 192, 168, 2, 1 };
const unsigned char ucIPAddress[] =				{ 172, 25, 218, 210 };
const unsigned char ucSubnetMask[] =			{ 255, 255, 255, 0 };

/* The number of sockets this task is going to handle. */
#define httpSOCKET_NUM                       3
unsigned char ucConnection[ httpSOCKET_NUM ];

/* The maximum data buffer size we can handle. */
#define httpSOCKET_BUFFER_SIZE	2048

/* Standard HTTP response. */
#define httpOUTPUT_OK	"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n"

/* Hard coded HTML components.  Other data is generated dynamically. */
#define HTML_OUTPUT_BEGIN "\
<HTML><head><meta http-equiv=\"Refresh\" content=\"1\;url=index.htm\"></head>\
<BODY bgcolor=\"#CCCCFF\"><font face=\"arial\"><H2>FreeRTOS.org<sup>tm</sup> + Tern E-Engine<sup>tm</sup></H2>\
<a href=\"http:\/\/www.FreeRTOS.org\">FreeRTOS.org Homepage</a><P>\
<HR>Task status table:\r\n\
<p><font face=\"courier\"><pre>Task          State  Priority  Stack	#<br>\
************************************************<br>"

#define HTML_OUTPUT_END   "\
</font></BODY></HTML>"

/*-----------------------------------------------------------*/

/*
 * Initialise the data structures used to hold the socket status.
 */
static void prvHTTPInit( void );

/*
 * Setup the Ethernet interface with the network addressing information.
 */
static void prvNetifInit( void );

/*
 * Generate the dynamic components of the served WEB page and transmit the 
 * entire page through the socket.
 */
static void prvTransmitHTTP( unsigned char socket );
/*-----------------------------------------------------------*/

/* This variable is simply incremented by the idle task hook so the number of
iterations the idle task has performed can be displayed as part of the served
page. */
unsigned long ulIdleLoops = 0UL;

/* Data buffer shared by sockets. */
unsigned char ucSocketBuffer[ httpSOCKET_BUFFER_SIZE ];

/* The semaphore used by the Ethernet ISR to signal that the task should wake
and process whatever caused the interrupt. */
xSemaphoreHandle xTCPSemaphore = NULL;

/*-----------------------------------------------------------*/
void vHTTPTask( void * pvParameters )
{
short i, sLen;
unsigned char ucState;

	( void ) pvParameters;

    /* Create the semaphore used to communicate between this task and the
    WIZnet ISR. */
    vSemaphoreCreateBinary( xTCPSemaphore );

	/* Make sure everything is setup before we start. */
	prvNetifInit();
	prvHTTPInit();

	for( ;; )
	{
		/* Wait until the ISR tells us there is something to do. */
    	xSemaphoreTake( xTCPSemaphore, portMAX_DELAY );

		/* Check each socket. */
		for( i = 0; i < httpSOCKET_NUM; i++ )
		{
			ucState = select( i, SEL_CONTROL );

			switch (ucState)
			{
				case SOCK_ESTABLISHED :  /* new connection established. */

					if( ( sLen = select( i, SEL_RECV ) ) > 0 )
					{
						if( sLen > httpSOCKET_BUFFER_SIZE ) 
						{
							sLen = httpSOCKET_BUFFER_SIZE;
						}

						disable();
						
						sLen = recv( i, ucSocketBuffer, sLen );    

						if( ucConnection[ i ] == 1 )
						{	
							/* This is our first time processing a HTTP
							 request on this connection. */
							prvTransmitHTTP( i );
							ucConnection[i] = 0;
						}
						enable();
					}
					break;

				case SOCK_CLOSE_WAIT :

					close(i);
					break;

				case SOCK_CLOSED :

					ucConnection[i] = 1;
					socket( i, SOCK_STREAM, 80, 0x00 );
					NBlisten( i ); /* reinitialize socket. */
					break;
			}
		}
	}
}
/*-----------------------------------------------------------*/

static void prvHTTPInit( void )
{
unsigned char ucIndex;

	/* There are 4 total sockets available; we will claim 3 for HTTP. */
	for(ucIndex = 0; ucIndex < httpSOCKET_NUM; ucIndex++)
	{
		socket( ucIndex, SOCK_STREAM, httpPORT, 0x00 );
		NBlisten( ucIndex );
		ucConnection[ ucIndex ] = 1;
	}
}
/*-----------------------------------------------------------*/

static void prvNetifInit( void )
{
	i2chip_init();
	initW3100A();

	setMACAddr( ( unsigned char * ) ucMacAddress );
	setgateway( ( unsigned char * ) ucGatewayAddress );
	setsubmask( ( unsigned char * ) ucSubnetMask );
	setIP( ( unsigned char * ) ucIPAddress );

	/* See definition of 'sysinit' in socket.c
	 - 8 KB transmit buffer, and 8 KB receive buffer available.  These buffers
	   are shared by all 4 channels.
	 - (0x55, 0x55) configures the send and receive buffers at 
		httpSOCKET_BUFFER_SIZE bytes for each of the 4 channels. */
	sysinit( 0x55, 0x55 );
}
/*-----------------------------------------------------------*/

static void prvTransmitHTTP(unsigned char socket)
{
extern short usCheckStatus;

	/* Send the http and html headers. */
	send( socket, ( unsigned char * ) httpOUTPUT_OK, strlen( httpOUTPUT_OK ) );
	send( socket, ( unsigned char * ) HTML_OUTPUT_BEGIN, strlen( HTML_OUTPUT_BEGIN ) );

	/* Generate then send the table showing the status of each task. */
	vTaskList( ucSocketBuffer );
 	send( socket, ( unsigned char * ) ucSocketBuffer, strlen( ucSocketBuffer ) );

	/* Send the number of times the idle task has looped. */
    sprintf( ucSocketBuffer, "</pre></font><p><br>The idle task has looped 0x%08lx times<br>", ulIdleLoops );
	send( socket, ( unsigned char * ) ucSocketBuffer, strlen( ucSocketBuffer ) );

	/* Send the tick count. */
    sprintf( ucSocketBuffer, "The tick count is 0x%08lx<br>", xTaskGetTickCount() );
	send( socket, ( unsigned char * ) ucSocketBuffer, strlen( ucSocketBuffer ) );

	/* Show a message indicating whether or not the check task has discovered 
	an error in any of the standard demo tasks. */
    if( usCheckStatus == 0 )
    {
	    sprintf( ucSocketBuffer, "No errors detected." );
		send( socket, ( unsigned char * ) ucSocketBuffer, strlen( ucSocketBuffer ) );
    }
    else
    {
	    sprintf( ucSocketBuffer, "<font color=\"red\">An error has been detected in at least one task %x.</font><p>", usCheckStatus );
		send( socket, ( unsigned char * ) ucSocketBuffer, strlen( ucSocketBuffer ) );
    }

	/* Finish the page off. */
	send( socket, (unsigned char*)HTML_OUTPUT_END, strlen(HTML_OUTPUT_END));

	/* Must make sure the data is gone before closing the socket. */
	while( !tx_empty( socket ) )
    {
    	vTaskDelay( httpTX_WAIT );
    }
	close(socket);
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	ulIdleLoops++;
}




















