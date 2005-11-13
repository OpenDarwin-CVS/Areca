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

class ArcMSR : public IOSCSIParallelInterfaceController
{
	OSDeclareAbstractStructors(ArcMSR)
 
public:
	//
	// IOSCSIParallelInterfaceController protocol
	//
    
	// initialisation and teardown
	bool			InitializeController(void);
	void			TerminateController(void);
    
	// controller start/stop
	bool			StartController(void);
	void			StopController(void);
    
	// property queries
	SCSIInitiatorIdentifier	ReportInitiatorIdentifier(void);
	SCSILogicalUnitNumber	ReportHBAHighestLogicalUnitNumber(void);
	SCSIDeviceIdentifier	ReportHighestSupportedDeviceID(void);
	UInt32			ReportMaximumTaskCount(void)				{return(maxSRB);};
	UInt32			ReportHBASpecificTaskDataSize(void)			{return(4);};	// must be > 0
	UInt32			ReportHBASpecificDeviceDataSize(void)			{return(4);};
    
	// feature queries
	bool			DoesHBASupportSCSIParallelFeature(SCSIParallelFeature theFeature) {return(false);};
	bool			DoesHBAPerformAutoSense(void)				{return(true);};
	bool			DoesHBAPerformDeviceManagement(void)			{return(true);};

	// callbacks
	bool			InitializeTargetForID(SCSITargetIdentifier targetID)	{return(true);};
	void			HandleInterruptRequest(void);
	void			HandleTimeout(SCSIParallelTaskIdentifier parallelRequest);
    
	// SCSI task management functions
	SCSIServiceResponse	AbortTaskRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL, SCSITaggedTaskIdentifier theQ);
	SCSIServiceResponse	AbortTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL);	
	SCSIServiceResponse	ClearACARequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL);
	SCSIServiceResponse	ClearTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL);
	SCSIServiceResponse	LogicalUnitResetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL);
	SCSIServiceResponse	TargetResetRequest(SCSITargetIdentifier theT);
    
	// inbound SCSI requests
	SCSIServiceResponse	ProcessParallelTask(SCSIParallelTaskIdentifier parallelRequest);
    
	// power management
	bool			finalize(IOOptionBits options)
	    {PMstop(); return IOSCSIParallelInterfaceController::finalize(options);};
	
	UInt32			initialPowerStateForDomainStat(IOPMPowerFlags flags)
	    {return(1);};
	
	IOReturn		setPowerState(UInt32 powerStateOrdinal, IOService *device)
	    {return(kIOReturnSuccess);};
    
    
	// Userclient incalls
	COMMANDGATE_PROTO1(inboundMQBufferInsert, IOMemoryDescriptor *, dataBuf);
	COMMANDGATE_PROTO3(outboundMQBufferRemove, IOMemoryDescriptor *, dataBuf, int *, bytesRead, int, wait);
	COMMANDGATE_PROTO0(inboundMQBufferClear);
	COMMANDGATE_PROTO0(outboundMQBufferClear);

	// Userclient exclusive-access flag control
	COMMANDGATE_PROTO1(setClientActive, bool *, state);

	// Command tag management
	COMMANDGATE_PROTO1(getTag, int *, tag);
	COMMANDGATE_PROTO1(returnTag, int, tag);

	// Command stuff into controller
	COMMANDGATE_PROTO1(postSRB, uint32_t *, postValue);

	// memory cursor segment outputter
	static void	outputArcMSRSegment(IOMemoryCursor::PhysicalSegment segment, void *pvt, UInt32 outSegmentIndex);

private:
	// Adapter registers
	IOPCIDevice		*pciNub;
	IOMemoryMap		*registerMap;
	IOMemoryCursor		*memoryCursor;

	struct arcmsr_mu	*mu;

	// Adapter state flags
	int			adapterState;
#define ARCMSR_STATE_MSG0_POSTING	(1<<0)
	
	// client mutex
	bool			clientActive;

	// Command pool
	int			maxSRB;
	int			sizeSRB;
	IOBufferMemoryDescriptor *SRBPool;
	char			*SRBPtr;
	IOPhysicalAddress	SRBPhys;
	RingBuffer		freeSRB;

	struct arcmsr_srb	*getSRBPtr(int tag);
	IOPhysicalAddress	getSRBPhys(int tag);
	int			getSRBTag(IOPhysicalAddress physAddr);

	// Device map
	uint8_t			deviceMap[16];		// one bit per LUN, one byte per target
	uint8_t			deviceMapUpdate[16];	// updated device map
	IOTimerEventSource	*deviceScanTimer;	// regular scan for device changes
	void			deviceScanStub(void *, OSObject *who, IOTimerEventSource *es);

	// Asynchronous event handling
	ArcMSREventSource	*asyncEventSource;
	static void		asyncEventHandlerStub(OSObject *owner, ArcMSREventSource *es);
	void			asyncEventHandler(void);
	void			targetRescan(void);
	
	// Message queues
#define ARCMSR_MESSAGE_BUFFER		4096
	uint32_t		MQFlags;
#define ARCMSR_MQF_OVERFLOW		(1<<0)		// outbound ringbuffer full
#define ARCMSR_MQF_UNDERFLOW		(1<<1)		// inbound controller buffer empty
	RingBuffer		outboundMQBuffer;
	RingBuffer		inboundMQBuffer;
    
	void			inboundMQWrite(void);
	void			outboundMQRead(void);

	IOTimerEventSource	*outboundMQTimeout;
	void			outboundMQWakeup(void *, OSObject *who, IOTimerEventSource *junk);

	// register accessors
	volatile uint32_t	getOutboundMsgaddr1(void);
	volatile uint32_t	getOutboundIntmask(void);
	volatile uint32_t	getOutboundIntstatus(void);
	volatile uint32_t	getOutboundDoorbell(void);
	volatile uint32_t	getOutboundQueueport(void);
    
	volatile void		setInboundMsgaddr0(uint32_t val);
	volatile void		setInboundDoorbell(uint32_t val);
	volatile void		setOutboundDoorbell(uint32_t val);
	volatile void		setOutboundIntstatus(uint32_t val);
	volatile void		setOutboundIntmask(uint32_t val);
	volatile void		setInboundQueueport(uint32_t val);

	// controller interface (ArcMSRControllerIO module)
	bool			CTLinit(void);				// controller/IOP init
	bool			CTLwaitMsgintReady(void);
	bool			CTLstartBackgroundRebuild(void);	// start background rebuild
	bool			CTLstopBackgroundRebuild(void);		// stop background rebuild
	bool			CTLflushCache(void);			// flush the cache
	bool			CTLgetConfig(void);			// get controller configuration
	void			CTLrequestConfig(void);			// request controller config message
	void			CTLdisableInterrupts(void);
	void			CTLenableInterrupts(void);
	void			handleDoorbellInterrupt(void);
	void			handlePostQueueInterrupt(void);
	void			handleMessageInterrupt(void);

	void			showStatus(const char *status);

};

