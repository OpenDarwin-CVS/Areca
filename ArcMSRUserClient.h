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

#ifndef ARCMSRUSERCLIENT_H
#define ARCMSRUSERCLIENT_H

class ArcMSRUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(ArcMSRUserClient);
    
protected:
	static const IOExternalMethod sMethods[kArcMSRUserClientMethodCount];
	task_t			fTask;		// currently active user task
	ArcMSR			*fProvider;	// our provider
	void			*fSecurity;     // the user task's credentials

	// generic interface
	bool			initWithTask(task_t owningTask, void *security_id, UInt32 type);
	bool			start(IOService *provider);
	IOExternalMethod	*getTargetAndMethodForIndex(IOService **target, UInt32 index);
	IOReturn		open(void);
	IOReturn		close(void);
	IOReturn		clientClose(void);
	IOReturn		clientDied(void);
    
	// our work methods
	IOReturn		send(ArcMSRUserCommand *inCommand, ArcMSRUserCommand *outCommand, IOByteCount inCount, IOByteCount *outCount);
	IOReturn		recv(ArcMSRUserCommand *inCommand, ArcMSRUserCommand *outCommand, IOByteCount inCount, IOByteCount *outCount);
	IOReturn		clearWQBuffer(void);
	IOReturn		clearRQBuffer(void);
};

#endif /* ARCMSRUSERCLIENT_H */
