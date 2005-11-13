//-
// Copyright (c) 2005 Michael Smith
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

// $Id$

//
// This file defines the API between the ArcMSR UserClient and
// a userland client.
//

#ifndef ARCMSRUSERCLIENTINTERFACE_H
#define ARCMSRUSERCLIENTINTERFACE_H

#define ARCMSR_DRIVER_VERSION	20050924	// datestamp YYYYMMDD

enum {
	// Obtain exclusive access to the controller
	kArcMSRUserClientOpen,			// -

	// Release exclusive access to the controller
	kArcMSRUserClientClose,			// -

	// Send text to the controller
	kArcMSRUserClientSend,			// StructureI

	// Receive text from the controller
	kArcMSRUserClientRecv,			// StructureI

	// Clear the write buffer
	kArcMSRUserClientClearWQBuffer,		// -

	// Clear the read buffer
	kArcMSRUserClientClearRQBuffer,		// -

	// enum range limit
	kArcMSRUserClientMethodCount
};


// User command structure
typedef struct
{
	vm_address_t	data_buffer;
	vm_offset_t	data_size;
	int		timeout;
	int		result;
#define	ARCMSR_USERCLIENT_RETURNCODE_OK		0x01
#define	ARCMSR_USERCLIENT_RETURNCODE_ERROR	0x06
} ArcMSRUserCommand;

#endif ARCMSRUSERCLIENTINTERFACE_H
