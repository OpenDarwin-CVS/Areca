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

#define super IOSCSIParallelInterfaceController
#define self ArcMSR

static void	showBanner(ArcMSR *us);

#ifdef DEBUG
ArcMSR	*arcmsr_classptr;
#endif

OSDefineMetaClassAndStructors(self, super);

////////////////////////////////////////////////////////////////////////////////
// Initialisation
//
bool
self::InitializeController(void)
{
	const OSSymbol *userClient;
	int		i;

	set_debug_flags("error,event");
	
	showBanner(this);
	showStatus("initialising");
	debug(DEBUGF_MISC, "ArcMSR %p initialising...", this);

	//
	// Initialise instance variables
	//
	pciNub = NULL;
	memoryCursor = NULL;
	SRBPool = NULL;

	// client mutex
	clientActive = false;

	// zero the initial device maps
	bzero(deviceMap, sizeof(deviceMap));
	bzero(deviceMapUpdate, sizeof(deviceMapUpdate));
	
	//
	// PCI configuration
	//
	if ((pciNub = OSDynamicCast(IOPCIDevice, getProvider())) == NULL) {
		error("count not get PCI nub");
		goto fail;
	}
	pciNub->setBusMasterEnable(true);
	pciNub->setMemoryEnable(true);

	//
	// Map the IOP message unit
	//
	if ((registerMap = pciNub->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0)) == NULL) {
		error("could not map registers");
		goto fail;
	}
	mu = (struct arcmsr_mu *)registerMap->getVirtualAddress();
	debug(DEBUGF_PCI, "PCI config done");

	//
	// Initialize adapter
	//
	if (!CTLinit() || (maxSRB == 0)) {
		error("adapter init failed");
		goto fail;
	}
    
	//
	// Allocate SRB pool
	//
	if ((SRBPool = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn | kIOMemoryPhysicallyContiguous,
							     sizeSRB * maxSRB,
							     sizeSRB)) == NULL) {
		error("could not allocate memory for SRBs");
		goto fail;
	}
	SRBPool->prepare(kIODirectionOutIn);
	SRBPtr = (char *)SRBPool->getBytesNoCopy();
	SRBPhys = SRBPool->getPhysicalSegment(0, NULL);	// XXX if this block is above 4GB, we need to tell the adapter the high 32 bits


	freeSRB.init(maxSRB * sizeof(uint32_t));
	for (i = 0; i < maxSRB; i++) {
		if (!freeSRB.insert((uint32_t)i)) {
			error("error queueing free SRB tags");
			goto fail;
		}
	}
	debug(DEBUGF_SRB, "allocated %d SRBs at %d bytes per adapter advice", maxSRB, sizeSRB);

	//
	// Initialise message queue handling
	//
	inboundMQBuffer.init(ARCMSR_MESSAGE_BUFFER);
	outboundMQBuffer.init(ARCMSR_MESSAGE_BUFFER);    
	MQFlags |= ARCMSR_MQF_UNDERFLOW;		// no send data pending

	outboundMQTimeout = IOTimerEventSource::timerEventSource(this,
								 OSMemberFunctionCast(IOTimerEventSource::Action,
										      this,
										      &ArcMSR::outboundMQWakeup));
	if (GetWorkLoop()->addEventSource(outboundMQTimeout)) {
		error("could not add message queue timeout source to workloop");
		goto fail;
	}
	debug(DEBUGF_MESSAGES, "message queues initialised, %d bytes in/out buffer", ARCMSR_MESSAGE_BUFFER);

	//
	// Initialise the device scanner
	//
	deviceScanTimer = IOTimerEventSource::timerEventSource(this,
							       OSMemberFunctionCast(IOTimerEventSource::Action,
										    this,
										    &ArcMSR::deviceScanStub));
	if (GetWorkLoop()->addEventSource(deviceScanTimer)) {
		error("could not add device scan timer source to workloop");
		goto fail;
	}
	
	//
	// Create the async event handler
	//
	asyncEventSource = ArcMSREventSource::Create((OSObject *)this, &ArcMSR::asyncEventHandlerStub);
	if (GetWorkLoop()->addEventSource(asyncEventSource)) {
		error("could not add async event handler to workloop");
		goto fail;
	}
	debug(DEBUGF_RESCAN, "device rescan initialised, rescan timer %p  async event timer %p",
	      deviceScanTimer, asyncEventSource);
	
	//
	// Announce our requirements for S/G elements and sizes
	//
	setProperty(kIOMaximumSegmentCountReadKey, ARCMSR_MAX_SG_ENTRIES, 64);  // maximum segment count
	setProperty(kIOMaximumSegmentCountWriteKey, ARCMSR_MAX_SG_ENTRIES, 64);
	setProperty(kIOMaximumSegmentByteCountReadKey, 0x00FFFFFF, 64);	    // SGL length limited to 24 bits
	setProperty(kIOMaximumSegmentByteCountWriteKey, 0x00FFFFFF, 64);
	debug(DEBUGF_MISC, "properties advertised");

	//
	// Intialise our memory cursor
	//
	if ((memoryCursor = IOMemoryCursor::withSpecification(outputArcMSRSegment, 0x00FFFFFF)) == NULL) {
		error("could not allocate IOMemoryCursor");
		goto fail;
	}
	debug(DEBUGF_SRB, "memory cursor initialised");
	
	//
	// Initialize Power Management
	//
    
	// XXX
	debug(DEBUGF_POWER, "XXX power management not implemented");
    
	//
	// Create the UserClient
	//
	// set up our userClientClass name, used when user clients
	// request a connection to the driver
	userClient = OSSymbol::withCStringNoCopy("ArcMSRUserClient");
	if (userClient == NULL) {
		error("could not allocate userClient name");
		goto fail;
	}
	setProperty(gIOUserClientClassKey, (OSObject *)userClient);
	userClient->release();
	registerService();
	debug(DEBUGF_USERCLIENT, "userclient registered");

	// Done
	debug(DEBUGF_MISC, "Initialisation complete");
	showStatus("initialisation done");
	return(true);
    
fail:
	error("Initialisation failed");
	showStatus("Initialisation failed");
	return(false);
}

////////////////////////////////////////////////////////////////////////////////
// Tear down the adapter
//
void
self::TerminateController(void)
{
	SRBPool->complete();
	SRBPool->release();

	freeSRB.deinit();
	inboundMQBuffer.deinit();
	outboundMQBuffer.deinit();

	if (deviceScanTimer)
		deviceScanTimer->release();
	
	if (outboundMQTimeout)
		outboundMQTimeout->release();
	
	if (asyncEventSource)
		asyncEventSource->release();

	if (memoryCursor)
		memoryCursor->release();

	if (registerMap != NULL)
		registerMap->release();

	debug(DEBUGF_MISC, "terminated");
	showStatus("terminated");
}

//////////////////////////////////////////////////////////////////////////////
// Start/stop the adapter
//
bool
self::StartController(void)
{
	debug(DEBUGF_MISC, "start adapter");
	
	CTLenableInterrupts();
	CTLstartBackgroundRebuild();

	// kick off the device scan timer
	deviceScanTimer->enable();
	deviceScanTimer->setTimeoutMS(1);		// scan ASAP
	debug(DEBUGF_RESCAN, "rescan handler started");
    
	showStatus("started");
	return(true);
};

void
self::StopController(void)
{
	debug(DEBUGF_MISC, "stop adapter");

	CTLstopBackgroundRebuild();
	CTLflushCache();
	CTLdisableInterrupts();
	
	deviceScanTimer->disable();
	debug(DEBUGF_RESCAN, "rescan handler stopped");

	showStatus("stopped");
};


//////////////////////////////////////////////////////////////////////////////
// Command tag management
//
// This could be done many ways; the RingBuffer stuff was just handy.
//
COMMANDGATE_GLUE1(getTag, int *);

void
self::getTag(int *tagp)
{
	uint32_t	tag;
	
	if (freeSRB.remove(&tag) == 1) {
		*tagp = tag;
		debug(DEBUGF_SRB, "vending tag %d", tag);
	} else {
		*tagp = -1;
		debug(DEBUGF_SRB, "no free tags to vend");
	}
}

COMMANDGATE_GLUE1(returnTag, int);

void
self::returnTag(int tag)
{
	debug(DEBUGF_SRB, "freeing tag %d", tag);
	freeSRB.insert((uint32_t)tag);
}

//////////////////////////////////////////////////////////////////////////////
// Ensure only one userclient can be active at a time
//
COMMANDGATE_GLUE1(setClientActive, bool *);

void
self::setClientActive(bool *state)
{
	
	if (*state) {		// want to make new client active
		
		if (clientActive) {	// already a client active
			debug(DEBUGF_USERCLIENT, "client activation failed, already active");
			*state = false;		// failure
		} else {		// no client active
			debug(DEBUGF_USERCLIENT, "client activation succeeded");
			clientActive = true;	// set new client as active
			*state = true;		// success
		}
	} else {		// want to make client inactive
		
		if (!clientActive) {	// no client active
			debug(DEBUGF_USERCLIENT, "client deactivation failed, none active");
			*state = false;		// failure
		} else {		// client active
			debug(DEBUGF_USERCLIENT, "client deactivation succeeded");
			clientActive = false;	// turn off active flag
			*state = true;		// success
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
// Asynchronous event handling
//
void
self::asyncEventHandlerStub(OSObject *owner, ArcMSREventSource *es)
{
	ArcMSR	*adapter;


	adapter = (ArcMSR *)owner;
	debug(DEBUGF_MISC, "async event handler stub for adapter %p", adapter);
	adapter->asyncEventHandler();
}

void
self::asyncEventHandler()
{
	while (!asyncEventSource->handlerDone()) {
		if (asyncEventSource->targetRescan()) {
			debug(DEBUGF_RESCAN, "async handler fielding rescan response");
			targetRescan();
		}
	}		
}

////////////////////////////////////////////////////////////////////////////////
// Device scanner
//
void
self::deviceScanStub(void */*refcon*/, OSObject *owner, IOTimerEventSource *es)
{
	ArcMSR	*ap;

	if ((ap = OSDynamicCast(ArcMSR, owner)) == NULL) {
		error("device scan not signalled by ArcMSR");
		return;
	}
	// initiate a scan and queue another instance
	debug(DEBUGF_RESCAN, "periodic device rescan requesting current status");
	setInboundMsgaddr0(ARCMSR_INBOUND_MESG0_GET_CONFIG);
	debug(DEBUGF_RESCAN, "periodic device rescan setting new timeout");
	ap->deviceScanTimer->setTimeoutMS(ARCMSR_STATUS_INTERVAL);
	debug(DEBUGF_RESCAN, "periodic device rescan done");
}

//////////////////////////////////////////////////////////////////////////////
// Advertise the driver status in the registry
//
void
self::showStatus(const char *status)
{
	setProperty("adapter-status", status);

}

//////////////////////////////////////////////////////////////////////////////
// Print our banner into the system log, but only once.
//
static void
showBanner(ArcMSR *us)
{
	static int done = 0;

	if (!done) {
		IOLog("Areca SATA RAID Adapter family driver  Built %s\n", __DATE__);
		IOLog("  (c) 2005 Apple Computer, Areca Corporation, Michael Smith, All Rights Reserved\n");
		done = 1;
#ifdef DEBUG
		arcmsr_classptr = us;
#endif
	}
}
