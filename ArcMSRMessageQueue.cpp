//-
//
// @APPLE_LICENSE_HEADER_START@
// 
// Copyright (c) 2005 Apple Computer, Inc.  All Rights Reserved.
// 
// This file contains Original Code and/or Modifications of Original Code
// as defined in and that are subject to the Apple Public Source License
// Version 2.0 (the 'License'). You may not use this file except in
// compliance with the License. Please obtain a copy of the License at
// http://www.opensource.apple.com/apsl/ and read it before using this
// file.
// 
// The Original Code and all software distributed under the License are
// distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
// EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
// INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
// Please see the License for the specific language governing rights and
// limitations under the License.
// 
// @APPLE_LICENSE_HEADER_END@

// $Id$

#include "ArcMSR.h"

#define self ArcMSR

////////////////////////////////////////////////////////////////////////////////
// Adapter message queue handling
//
// There are two separate queues
//
// inbound:  driver -> adapter
// outbound: driver <- adapter
//
// We avoid waking up the writer until the inbound ringbuffer is empty to 
// avoid excessive wakeups.  For reads, this isn't possible as we don't know
// how much data is anticipated.
//
// XXX Note that the read handler does not block if there is nothing to read;
// this seems odd.  The interrupt handler supports waking a sleeper if we 
// decide to change this (we should).
//
COMMANDGATE_GLUE1(inboundMQBufferInsert, IOMemoryDescriptor *);

void
self::inboundMQBufferInsert(IOMemoryDescriptor *dataBuf)
{
	// spin trying to enqueue the data on the inbound queue
	for (;;) {
		if (inboundMQBuffer.insert(dataBuf)) {
			// insert successful, kick the adapter if it's quiescent
			if (MQFlags & ARCMSR_MQF_UNDERFLOW) {
				debug(DEBUGF_MESSAGES, "kicking adapter with inbound message");
				MQFlags &= ~ARCMSR_MQF_UNDERFLOW;
				inboundMQWrite();
			} else {
				debug(DEBUGF_MESSAGES, "queued inbound message");
			}
			break;
		}    
		// No room in the ringbuffer, sleep waiting for space
		debug(DEBUGF_MESSAGES, "waiting for inbound queue space");
		GetCommandGate()->commandSleep(&inboundMQBuffer);
	}
}

void
self::inboundMQWrite(void)
{
	uint32_t    length;

	// Check to see if we have more to send
	length = inboundMQBuffer.remove(mu->ioctl_wbuffer.data, sizeof(mu->ioctl_wbuffer.data));
	if (length > 0) {
		mu->ioctl_wbuffer.length = OSSwapHostToLittleInt32(length);
		setInboundDoorbell(ARCMSR_INBOUND_DRIVER_DATA_WRITE_OK);
		debug(DEBUGF_MESSAGES, "posted %d message bytes to adapter", length);
	} else {
		// nothing left to write, flag underflow and wake blocked sender
		MQFlags |= ARCMSR_MQF_UNDERFLOW;
		GetCommandGate()->commandWakeup(&inboundMQBuffer, false /* wake everyone */);
		debug(DEBUGF_MESSAGES, "inbound buffer empty, waking writers");
	}
}

COMMANDGATE_GLUE3(outboundMQBufferRemove, IOMemoryDescriptor *, int *, int);

void
self::outboundMQWakeup(void */*refcon*/, OSObject *owner, __unused IOTimerEventSource *junk)
{
	ArcMSR	*ap;

	if ((ap = OSDynamicCast(ArcMSR, owner)) == NULL) {
		error("timeout not signalled by ArcMSR");
		return;
	}

	debug(DEBUGF_MESSAGES, "timed out waiting for outbound message data");
	ap->GetCommandGate()->commandWakeup(&outboundMQBuffer, false);
}

void
self::outboundMQBufferRemove(IOMemoryDescriptor *dataBuf, int *bytesRead, int timeout)
{
	int	    total_length, length, slept;

	slept = 0;
restart:
	
	// Grab as much data as we can from the ringbuffer
	total_length = outboundMQBuffer.remove(dataBuf, 0);
	debug(DEBUGF_MESSAGES, "fetched %d bytes from outbound message buffer", total_length);
    
	// Do we have room for more?
	if (total_length < dataBuf->getLength()) {

		// Check to see if there is overflow data in the ioctl_rbuffer
		if (MQFlags & ARCMSR_MQF_OVERFLOW) {
			// This is guaranteed to fit in the ringbuffer now, since we just emptied it
			outboundMQRead();
			MQFlags &= ~ARCMSR_MQF_OVERFLOW;
	    
			// pull data out into the requester's buffer
			length = outboundMQBuffer.remove(dataBuf, total_length);
			total_length += length;
			debug(DEBUGF_MESSAGES, "  and %d additional bytes from adapter buffer", length);
		}
	}
	// If the caller is willing to block waiting for data, oblige them, but only onceb
	if ((total_length == 0) && !slept && (timeout > 0)) {
		slept = 1;
		debug(DEBUGF_MESSAGES, "waiting %dms for data from adapter", timeout);
		outboundMQTimeout->setTimeoutMS(timeout);
		GetCommandGate()->commandSleep(&outboundMQBuffer);
		goto restart;
	}
		
	*bytesRead = total_length;
}

void
self::outboundMQRead(void)
{
	uint32_t    length;

	length = OSSwapLittleToHostInt32(mu->ioctl_rbuffer.length);

	// Try to enqueue locally
	if (outboundMQBuffer.insert(mu->ioctl_rbuffer.data, length)) {
		// all good, acknowledge data read and wake reader
		setInboundDoorbell(ARCMSR_INBOUND_DRIVER_DATA_READ_OK);
		GetCommandGate()->commandWakeup(&outboundMQBuffer, false /* wake everyone */);
		debug(DEBUGF_MESSAGES, "got %d bytes of outbound message data from the adapter", length);
	} else {
		// no space, flag overflow and don't ack to the adapter
		MQFlags |= ARCMSR_MQF_OVERFLOW;
		debug(DEBUGF_MESSAGES, "no room for %d bytes, stalling in adapter", length);
	}
}

COMMANDGATE_GLUE0(inboundMQBufferClear);

void
self::inboundMQBufferClear(void)
{
	inboundMQBuffer.clear();
}

COMMANDGATE_GLUE0(outboundMQBufferClear);

void
self::outboundMQBufferClear(void)
{
	outboundMQBuffer.clear();
    
	// If there's overflow data we need to kick the adapter to start sending again
	if (MQFlags & ARCMSR_MQF_OVERFLOW) {
		MQFlags &= ~ARCMSR_MQF_OVERFLOW;
		setInboundDoorbell(ARCMSR_INBOUND_DRIVER_DATA_READ_OK);
	}
}

