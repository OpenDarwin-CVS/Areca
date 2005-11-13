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

#define super IOUserClient
#define self ArcMSRUserClient

OSDefineMetaClassAndStructors(self, super);

//////////////////////////////////////////////////////////////////////////////
// Method lookup
//
// NOTE:  This must stay in synch with the enum in ArcMSR_Userclient.h
IOExternalMethod *
self::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
	static const IOExternalMethod sMethods[kArcMSRUserClientMethodCount] = {
		{   // kArcMSRUserClientOpen
			NULL,                               // IOService
			(IOMethod) &self::open,
			kIOUCScalarIScalarO,
			0,                                  // no input
			0                                   // no output
		},
		{   // kArcMSRUserClientClose
			NULL,                               // IOService
			(IOMethod) &self::close,
			kIOUCScalarIScalarO,
			0,                                  // no input
			0                                   // no output
		},
		{   // kArcMSRUserClientSend
			NULL,                               // IOService
			(IOMethod) &self::send,
			kIOUCStructIStructO,
			sizeof(ArcMSRUserCommand),        // command in
			sizeof(ArcMSRUserCommand)         // command out
		},
		{   // kArcMSRUserClientRecv
			NULL,                               // IOService
			(IOMethod) &self::recv,
			kIOUCStructIStructO,
			sizeof(ArcMSRUserCommand),        // command in
			sizeof(ArcMSRUserCommand)         // command out
		},
		{   // kArcMSRUserClientClearWQBuffer
			NULL,                               // IOService
			(IOMethod) &self::clearWQBuffer,
			kIOUCScalarIScalarO,
			0,                                  // no input
			0                                   // no output
		},
		{   // kArcMSRUserClientClearRQBuffer
			NULL,                               // IOService
			(IOMethod) &self::clearRQBuffer,
			kIOUCScalarIScalarO,
			0,                                  // no input
			0                                   // no output
		},
	};
    
	// range check
	if (index < kArcMSRUserClientMethodCount) {
		*target = this;
		return((IOExternalMethod *)&sMethods[index]);
	}
    
	// invalid request
	error("request for invalid method %d", (int)index);
	return(NULL);
};

//////////////////////////////////////////////////////////////////////////////
// Generic interface
//
bool
self::initWithTask(task_t owningTask, void *security_id, UInt32 type)
{
	// only an administrator can invoke this interface
	if (clientHasPrivilege(security_id, kIOClientPrivilegeAdministrator) != kIOReturnSuccess) {
		debug(DEBUGF_USERCLIENT, "unprivileged connection refused");
		return(false);
	}
    
	if (!super::initWithTask(owningTask, security_id , type))
		return(false);
    
	if (!owningTask)
		return(false);
    
	fTask = owningTask;
	fProvider = NULL;
	fSecurity = security_id;

	debug(DEBUGF_USERCLIENT, "init done");
	return(true);
}

bool
self::start(IOService *provider)
{
	// generic start
	if (!super::start(provider))
		return(false);
    
	// Must be associated with the right driver
	if ((fProvider = OSDynamicCast(ArcMSR, provider)) == NULL)
		return(false);
    
	return(true);
}

IOReturn
self::open(void)
{
	bool openState;
	
	// provider terminated?
	if (isInactive())
		return(kIOReturnNotAttached);

	// ensure exclusive access
	openState = true;
	fProvider->setClientActiveInvoke(&openState);
	if (!openState)
		return(kIOReturnExclusiveAccess);

	// open our provider to maintain a reference
	fProvider->open(this);

	debug(DEBUGF_USERCLIENT, "opened");
    
	return(kIOReturnSuccess);
}

IOReturn
self::close(void)
{
	bool openState;
	
	// provider terminated?
	if (isInactive())
		return(kIOReturnNotAttached);

	// drop our lock on the provider
	openState = false;
	fProvider->setClientActiveInvoke(&openState);

	// close the provider and drop our reference
	if (fProvider->isOpen(this))
		fProvider->close(this);

	debug(DEBUGF_USERCLIENT, "closed");

	return(kIOReturnSuccess);
}

IOReturn
self::clientClose(void)
{
	// close parent if required
	close();
    
	if (fTask)
		fTask = NULL;
	fProvider = NULL;
	terminate();
    
	return(kIOReturnSuccess);
}

//////////////////////////////////////////////////////////////////////////////
// ArcMSR-specific interface
//
IOReturn
self::send(ArcMSRUserCommand *inCommand, ArcMSRUserCommand *outCommand, IOByteCount inCount, IOByteCount *outCount)
{
	IOMemoryDescriptor	*dataBuf;

	debug(DEBUGF_USERCLIENT, "%d bytes at %p", inCommand->data_size, inCommand->data_buffer);
    
	// provider terminated?
	if (isInactive())
		return(kIOReturnNotAttached);

	// size-check the transfer to guarantee that we can enqueue in a single pass
	if (inCommand->data_size < 1)
		return(kIOReturnBadArgument);
	if (inCommand->data_size > ARCMSR_MESSAGE_BUFFER)
		return(kIOReturnMessageTooLarge);
    
	// get an IOMemoryDescriptor for the data
	dataBuf = IOMemoryDescriptor::withAddress(inCommand->data_buffer, 
						  inCommand->data_size,
						  kIODirectionInOut,
						  fTask);
	if (!dataBuf)
		return(kIOReturnBadArgument);

	// enqueue and hand off to the adapter
	dataBuf->prepare();
	fProvider->inboundMQBufferInsertInvoke(dataBuf);
	dataBuf->complete();
	dataBuf->release();
    
	// currently we can't fail
	outCommand->data_size = inCommand->data_size;
	outCommand->result = ARCMSR_USERCLIENT_RETURNCODE_OK;
    
	return(kIOReturnSuccess);
}

IOReturn
self::recv(ArcMSRUserCommand *inCommand, ArcMSRUserCommand *outCommand, IOByteCount inCount, IOByteCount *outCount)
{
	IOMemoryDescriptor	*dataBuf;
	int			bytesRead;

	debug(DEBUGF_USERCLIENT, "%d bytes at %p", inCommand->data_size, inCommand->data_buffer);

	// provider terminated?
	if (isInactive())
		return(kIOReturnNotAttached);

	if (inCommand->data_size < 1)
		return(kIOReturnBadArgument);
	if (inCommand->data_size > ARCMSR_MESSAGE_BUFFER)
		inCommand->data_size = ARCMSR_MESSAGE_BUFFER;
    
	// get an IOMemoryDescriptor for the data
	dataBuf = IOMemoryDescriptor::withAddress(inCommand->data_buffer, 
						  inCommand->data_size,
						  kIODirectionInOut,
						  fTask);
	if (!dataBuf)
		return(kIOReturnBadArgument);
    
	// dequeue any data waiting from the adapter
	dataBuf->prepare();
	fProvider->outboundMQBufferRemoveInvoke(dataBuf, &bytesRead, inCommand->timeout);
	dataBuf->complete();
	dataBuf->release();
    
	outCommand->data_size = bytesRead;
	outCommand->result = ARCMSR_USERCLIENT_RETURNCODE_OK;

	return(kIOReturnSuccess);
}

IOReturn
self::clearWQBuffer(void)
{
	// provider terminated?
	if (isInactive())
		return(kIOReturnNotAttached);

	fProvider->outboundMQBufferClearInvoke();
    
	return(kIOReturnSuccess);
}

IOReturn
self::clearRQBuffer(void)
{
	// provider terminated?
	if (isInactive())
		return(kIOReturnNotAttached);

	fProvider->inboundMQBufferClearInvoke();

	return(kIOReturnSuccess);
}
