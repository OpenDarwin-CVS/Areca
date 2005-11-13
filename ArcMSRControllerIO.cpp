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
////////////////////////////////////////////////////////////////////////////////
// Hardware interface protocol
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Adapter hardware initialisation
bool
self::CTLinit(void)
{
	uint32_t	odb;
	int		timeout;
    
	CTLdisableInterrupts();
    
	// Spin waiting for the FIRMWARE_OK bit
	// Time out after a second (XXX should this be longer?)
	debug(DEBUGF_ADAPTER, "waiting for firmware...");
	showStatus("waiting for firmware");
	timeout = 0;
	while ((getOutboundMsgaddr1() & ARCMSR_OUTBOUND_MESG1_FIRMWARE_OK) == 0) {
		if (timeout++ > 100) {
			error("timed out waiting for firmware");
			return(false);
		}
		IOSleep(10);
	}
	debug(DEBUGF_ADAPTER, "firmware OK");

	// empty inbound message buffer
	odb = getOutboundDoorbell();
	if (odb & ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK) {
		setOutboundDoorbell(odb);
		setInboundDoorbell(ARCMSR_INBOUND_DRIVER_DATA_READ_OK);
	}

	// get adapter configuration parameters
	if (!CTLgetConfig())
		return(false);

	debug(DEBUGF_ADAPTER, "init done");
	return(true);
}

////////////////////////////////////////////////////////////////////////////////
// Fetch the adapter configuration
//
bool
self::CTLgetConfig(void)
{
	struct arcmsr_adapter_config	*cfg;
	char	sbuf[41];
	OSString *string;

	debug(DEBUGF_ADAPTER, "requesting initial adapter config");
	showStatus("waiting for initial adapter configuration");

	setInboundMsgaddr0(ARCMSR_INBOUND_MESG0_GET_CONFIG);
	if (!CTLwaitMsgintReady())
		return(false);

	cfg = (struct arcmsr_adapter_config *)&mu->message_wbuffer;
	if (OSSwapLittleToHostInt32(cfg->signature) != ARCMSR_CONFIG_SIGNATURE) {
		error("adapter responded to config request with bad data");
		return(false);
	}

	maxSRB = OSSwapLittleToHostInt32(cfg->queue_depth);
	if (maxSRB > ARCMSR_MAX_OUTSTANDING_SRB) {
		debug(DEBUGF_ADAPTER, "clamping maximum SRB count (adapter offered %d, we are limited to %d)",
		      maxSRB, ARCMSR_MAX_OUTSTANDING_SRB);
		maxSRB = ARCMSR_MAX_OUTSTANDING_SRB;
	}
	sizeSRB = OSSwapLittleToHostInt32(cfg->request_size);

	// set some registry properties
	bcopy(cfg->vendor, sbuf, 40);
	sbuf[40] = 0;
	string = OSString::withCString(sbuf);
	SetHBAProperty(kIOPropertyVendorNameKey, string);
	string->release();
	
	bcopy(cfg->model, sbuf, 8);
	sbuf[8] = 0;
	string = OSString::withCString(sbuf);
	SetHBAProperty(kIOPropertyProductNameKey, string);
	string->release();
	
	bcopy(cfg->firmware_version, sbuf, 16);
	sbuf[16] = 0;
	string = OSString::withCString(sbuf);
	SetHBAProperty(kIOPropertyProductRevisionLevelKey, string);
	string->release();

	if (strcmp(sbuf, "V1.37") < 0)
		IOLog("ArcMSR: WARNING: please update adapter firmware to V1.37 or later\n");

	// Note that we don't get the device map here, we will pick
	// it up the first time that we poll and devices will
	// appear then.
	
	debug(DEBUGF_EVENT, "found vendor '%40s'  model '%8s'  firmware '%16s'",
	      cfg->vendor, cfg->model, cfg->firmware_version);

	return(true);
}

/////////////////////////////////////////////////////////////////////////////////
// Disable interrupts
void
self::CTLdisableInterrupts(void)
{
	setOutboundIntmask(ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE);
	debug(DEBUGF_ADAPTER, "disabling interrupts");
}

/////////////////////////////////////////////////////////////////////////////////
// Enable interrupts
void
self::CTLenableInterrupts(void)
{
	uint32_t	intmask;
    
	intmask = getOutboundIntmask();
	setOutboundIntmask(intmask & ~(ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE |
				       ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE |
				       ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE));
	debug(DEBUGF_ADAPTER, "enabling interrupts");
}

////////////////////////////////////////////////////////////////////////////////
// Secondary interrupt handler
//
// We are on the workloop.
//
void
self::HandleInterruptRequest(void)
{
	uint32_t	intstatus;
    
	// fetch interrupt status and acknowledge
	intstatus = getOutboundIntstatus();
	setOutboundIntstatus(intstatus);
	debug(DEBUGF_INTERRUPT, "interrupted with status 0x%x", intstatus);

	// MU doorbell interrupts
	if (intstatus & ARCMSR_MU_OUTBOUND_DOORBELL_INT)
		handleDoorbellInterrupt();
    
	// MU post queue interrupts
	if (intstatus & ARCMSR_MU_OUTBOUND_POSTQUEUE_INT)
		handlePostQueueInterrupt();

	// MU message interrupt
	if (intstatus & ARCMSR_MU_OUTBOUND_MESSAGE0_INT)
		handleMessageInterrupt();

}

////////////////////////////////////////////////////////////////////////////////
// Handle doorbell interrupts
//
void
self::handleDoorbellInterrupt(void)
{
	uint32_t	doorbell;

	// Data posted in the IOCTL text buffer for us
	doorbell = getOutboundDoorbell();
	setOutboundDoorbell(doorbell);		// acknowledge
    
	if (doorbell & ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK) {
		debug(DEBUGF_INTERRUPT, "adapter posted outbound message");
		outboundMQRead();
	}
    
	// Adapter ready for more data from the IOCTL interface
	if (doorbell & ARCMSR_OUTBOUND_IOP331_DATA_READ_OK) {
		debug(DEBUGF_INTERRUPT, "adapter consumed inbound message");
		inboundMQWrite();
	}
}

////////////////////////////////////////////////////////////////////////////////
// Handle a message interrupt
//
// The only message interrupt we expect is in response to a query for the
// current adapter config.  We want this in order to compare the drivemap
// so that we can detect newly-attached drives.
//
void
self::handleMessageInterrupt(void)
{
	struct arcmsr_adapter_config	*cfg;
	int		target;
	bool		updated;

	// is it the async config check?
	cfg = (struct arcmsr_adapter_config *)&mu->message_wbuffer;
	if (OSSwapLittleToHostInt32(cfg->signature) == ARCMSR_CONFIG_SIGNATURE) {

		// asynchronous config update
		updated = false;

		debug(DEBUGF_INTERRUPT, "adapter posted CONFIG message");
		
		// copy the new map, note if there are differences with the current map
		for (target = 0; target < ARCMSR_MAX_TARGETID; target++) {
			deviceMapUpdate[target] = cfg->device_map[target];
			if (deviceMapUpdate[target] != deviceMap[target])
				updated = true;
		}
		if (updated) {
			debug(DEBUGF_RESCAN, "adapter config message contains changed device map");
			// queue a target rescan
			asyncEventSource->addNotification(ARCMSR_ESFLAG_RESCAN);
		}
	} else {
		debug(DEBUGF_INTERRUPT, "adapter posted message with unrecognised signature 0x%08x",
		      OSSwapLittleToHostInt32(cfg->signature));
	}
}

////////////////////////////////////////////////////////////////////////////////
// Spin waiting for a message0 command to finish
//
bool
self::CTLwaitMsgintReady(void)
{
	int	retries;

	for (retries = 0; retries < 2000; retries++) {
                if (getOutboundIntstatus() & ARCMSR_MU_OUTBOUND_MESSAGE0_INT) {
                        setOutboundIntstatus(ARCMSR_MU_OUTBOUND_MESSAGE0_INT);
                        debug(DEBUGF_ADAPTER, "polled command wait succeeded");
                        return(true);
                }
                IOSleep(10);
        }
        debug(DEBUGF_ADAPTER, "polled command wait failed");
        return(false);
}

////////////////////////////////////////////////////////////////////////////////
// Start/stop background rebuild
//
// These are called by ::start/::stop with interrupt delivery disabled.
//
bool
self::CTLstartBackgroundRebuild(void)
{
	setInboundMsgaddr0(ARCMSR_INBOUND_MESG0_START_BGRB);
	showStatus("starting background rebuild");
	return(CTLwaitMsgintReady());
}

bool
self::CTLstopBackgroundRebuild(void)
{
	setInboundMsgaddr0(ARCMSR_INBOUND_MESG0_STOP_BGRB);
	showStatus("stopping background rebuild");
	return(CTLwaitMsgintReady());
}

////////////////////////////////////////////////////////////////////////////////
// Flush the adapter cache
//
bool
self::CTLflushCache(void)
{
	debug(DEBUGF_ADAPTER, "flushing adapter cache");
	setInboundMsgaddr0(ARCMSR_INBOUND_MESG0_FLUSH_CACHE);
	showStatus("flushing cache");
	return(CTLwaitMsgintReady());
}

////////////////////////////////////////////////////////////////////////////////
// Post an SRB to the adapter
//
COMMANDGATE_GLUE1(postSRB, uint32_t *);

void
self::postSRB(uint32_t *postValue)
{
	// make sure that everything written to the SRB has made it to memory
	OSSynchronizeIO();
	setInboundQueueport(*postValue);
	debug(DEBUGF_SRB, "posting SRB at 0x%08x", postValue);
}

////////////////////////////////////////////////////////////////////////////////
// SRB handling
//
struct arcmsr_srb *
self::getSRBPtr(int tag)
{
	if ((tag < 0) || (tag >= maxSRB))
		return(NULL);
	return((struct arcmsr_srb *)(SRBPtr + (tag * sizeSRB)));
};

IOPhysicalAddress
self::getSRBPhys(int tag)
{
	return(SRBPhys + (tag * sizeSRB));
};

int
self::getSRBTag(IOPhysicalAddress physAddr)
{
	if ((physAddr < SRBPhys) || (physAddr > (SRBPhys + (maxSRB * sizeSRB))))
		return(-1);
	return((physAddr - SRBPhys) / sizeSRB);
};

////////////////////////////////////////////////////////////////////////////////
// Register accessors
//
volatile uint32_t
self::getOutboundMsgaddr1(void)
{
	return OSSwapLittleToHostInt32(mu->outbound_msgaddr1);
};

volatile uint32_t
self::getOutboundIntmask(void)
{
	return OSSwapLittleToHostInt32(mu->outbound_intmask);
};

volatile uint32_t
self::getOutboundIntstatus(void)
{
	return OSSwapLittleToHostInt32(mu->outbound_intstatus);
};

volatile uint32_t
self::getOutboundDoorbell(void)
{
	return OSSwapLittleToHostInt32(mu->outbound_doorbell);
};

volatile uint32_t
self::getOutboundQueueport(void)
{
	return OSSwapLittleToHostInt32(mu->outbound_queueport);
};

    
volatile void
self::setInboundMsgaddr0(uint32_t val)
{
	mu->inbound_msgaddr0 = OSSwapHostToLittleInt32(val);
};

volatile void
self::setInboundDoorbell(uint32_t val)
{
	mu->inbound_doorbell = OSSwapHostToLittleInt32(val);
};

volatile void
self::setOutboundDoorbell(uint32_t val)
{
	mu->outbound_doorbell = OSSwapHostToLittleInt32(val);
};

volatile void
self::setOutboundIntstatus(uint32_t val)
{
	mu->outbound_intstatus = OSSwapHostToLittleInt32(val);
};

volatile void
self::setOutboundIntmask(uint32_t val)
{
	mu->outbound_intmask = OSSwapHostToLittleInt32(val);
};

volatile void
self::setInboundQueueport(uint32_t val)
{
	mu->inbound_queueport = OSSwapHostToLittleInt32(val);
};

	
