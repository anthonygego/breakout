/**************************************************************
 * HTTPPrint.h
 * Provides callback headers and resolution for user's custom
 * HTTP Application.
 * 
 * This file is automatically generated by the MPFS Utility
 * ALL MODIFICATIONS WILL BE OVERWRITTEN BY THE MPFS GENERATOR
 **************************************************************/

#ifndef __HTTPPRINT_H
#define __HTTPPRINT_H

#include "TCPIPStack/TCPIP.h"

#if defined(STACK_USE_HTTP2_SERVER)

extern HTTP_STUB httpStubs[MAX_HTTP_CONNECTIONS];
extern BYTE curHTTPID;

void HTTPPrint(DWORD callbackID);
void HTTPPrint_state(void);
void HTTPPrint_lives(void);
void HTTPPrint_ball(void);
void HTTPPrint_bricks(void);
void HTTPPrint_score(void);
void HTTPPrint_size(void);
void HTTPPrint_speed(void);

void HTTPPrint(DWORD callbackID)
{
	switch(callbackID)
	{
        case 0x00000035:
			HTTPPrint_state();
			break;
        case 0x00000036:
			HTTPPrint_lives();
			break;
        case 0x00000037:
			HTTPPrint_ball();
			break;
        case 0x00000038:
			HTTPPrint_bricks();
			break;
        case 0x00000039:
			HTTPPrint_score();
			break;
        case 0x0000003a:
			HTTPPrint_size();
			break;
        case 0x0000003b:
			HTTPPrint_speed();
			break;
		default:
			// Output notification for undefined values
			TCPPutROMArray(sktHTTP, (ROM BYTE*)"!DEF", 4);
	}

	return;
}

void HTTPPrint_(void)
{
	TCPPut(sktHTTP, '~');
	return;
}

#endif

#endif
