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

//
// Master include file
//
// The include dependancies for this project are terribly incestuous, so
// in the name of simplicity everything is included here, and then each
// source file just includes us once.
//

#ifndef ARCMSR_H
#define ARCMSR_H

#define DEBUG

// System headers

#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOCommand.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOService.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/scsi-parallel/IOSCSIParallelInterfaceController.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/pwr_mgt/RootDomain.h>

// tunable parameters

// ARCMSR_STATUS_INTERVAL
//
// The interval at which the driver will poll the adapter to check for changes in target status, in
// milliseconds.  This value is a tradeoff between the overhead introduced by polling, and the risk
// that a user may delete a volume and create a new one at the same address within the polling
// interval
//
#define ARCMSR_STATUS_INTERVAL		5000

// ARCMSR_MAX_OUTSTANDING_SRB
//
// The upper bound on the number of SRBs we permit outstanding.  At present this must not
// be increased above 256 due to our use of single byte tag values.
// Note that the adapter's supported configuration is checked to determine the actual
// upper bound.
//
#define ARCMSR_MAX_OUTSTANDING_SRB	256


// class forward decls
class ArcMSR;
class ArcMSRUserClient;
class ArcMSREventSource;

// Local headers, do not reorder
#include "ArcMSRUserClientInterface.h"
#include "ArcMSRUserClient.h"
#include "ArcMSRRegisters.h"
#include "ArcMSRUtils.h"
#include "ArcMSRController.h"

#endif /* ARCMSR_H */
