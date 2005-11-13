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

////////////////////////////////////////////////////////////////////////////////
// Target/LUN mapping
//
// Due to the fact that IOSCSIParallelInterfaceController handles device
// arrival/departure on a per-target basis, we have to flatten our
// target space (because LUNs can come and go in our universe).
//

#define TARGETLUN2SCSITARGET(_t, _l)	(((_t) * ARCMSR_MAX_TARGETLUN) | (_l))
#define SCSITARGET2TARGET(_st)		((_st) / ARCMSR_MAX_TARGETLUN)
#define SCSITARGET2LUN(_st)		((_st) % ARCMSR_MAX_TARGETLUN)

SCSIInitiatorIdentifier
self::ReportInitiatorIdentifier(void)
{
	return(TARGETLUN2SCSITARGET(ARCMSR_SCSI_INITIATOR_ID, 0));
}

SCSILogicalUnitNumber
self::ReportHBAHighestLogicalUnitNumber(void)
{
	return(0);
}

SCSIDeviceIdentifier
self::ReportHighestSupportedDeviceID(void)
{
	return(TARGETLUN2SCSITARGET(ARCMSR_MAX_TARGETID - 1, ARCMSR_MAX_TARGETLUN - 1));
}

#ifdef DEBUG
////////////////////////////////////////////////////////////////////////////////
// Check that the CDB and the S/G list are in sync
//
static void
check_cdb(struct arcmsr_srb *srb)
{
	int32_t		xferlen;
	int		i;

	// find length values in SCSI commands we recognise
	switch(srb->cdb[0]) {
	case 0x08:	/* read6 */
	case 0x0a:	/* write6 */
		xferlen = srb->cdb[4];
		break;
		
	case 0x28:	/* read10 */
	case 0x2a:	/* write10 */
		xferlen = OSSwapBigToHostInt16(*(uint16_t *)&srb->cdb[7]);
		break;

	default:
		return;
	}

	xferlen *= 0x200;	// convert blocks -> bytes

	for (i = 0; i < srb->sg_count; i++)
		xferlen -= OSSwapLittleToHostInt32(srb->sg.sg32entry[i].length);

	if (i != 0)
		error("S/G list and SCSI CDB disagree by %d bytes", xferlen);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Handle an inbound SCSI request
//
SCSIServiceResponse
self::ProcessParallelTask(SCSIParallelTaskIdentifier parallelRequest)
{
	struct arcmsr_srb	*srb;
	int			tag;
	uint32_t		physaddr;

	// Get a tag for the task
	getTagInvoke(&tag);

	if (tag == -1) {	// XXX should never happen
		error("inbound request overrun");
		return(kSCSIServiceResponse_FUNCTION_REJECTED);
	}

	// Build the SRB
	srb = getSRBPtr(tag);

	srb->bus = 0;
	srb->target = SCSITARGET2TARGET(GetTargetIdentifier(parallelRequest));
	srb->lun = SCSITARGET2LUN(GetTargetIdentifier(parallelRequest));
	srb->function = 1;

	srb->cdb_length = GetCommandDescriptorBlockSize(parallelRequest);
	srb->flags = GetDataTransferDirection(parallelRequest) & kSCSIDataTransfer_FromInitiatorToTarget ? ARCMSR_SRB_FLAG_WRITE : 0;
	GetCommandDescriptorBlock(parallelRequest, (SCSICommandDescriptorBlock *)srb->cdb);

	debug(DEBUGF_SCSI, "Command for %d,%d (host target %d)",
	      SCSITARGET2TARGET(GetTargetIdentifier(parallelRequest)),
	      SCSITARGET2LUN(GetTargetIdentifier(parallelRequest)),
	      GetTargetIdentifier(parallelRequest));
	debug_hexdump(DEBUGF_SCSI, srb->cdb, srb->cdb_length);

	srb->context = (uintptr_t)srb;
	
	// Construct the scatter/gather list
	memoryCursor->genPhysicalSegments(GetDataBuffer(parallelRequest),
					  GetDataBufferOffset(parallelRequest),
					  srb,
					  ARCMSR_MAX_SG_ENTRIES);

#ifdef DEBUG
	check_cdb(srb);
#endif
	
	SetTimeoutForTask(parallelRequest);

	// arrange to be able to find the task again later
	SetControllerTaskIdentifier(parallelRequest, (uintptr_t)srb);

	// dispatch to the card
	physaddr = (uint32_t)(getSRBPhys(tag) >> 5);
	if (srb->flags & ARCMSR_SRB_FLAG_SGL_BSIZE)
		physaddr |= ARCMSR_SRBPOST_FLAG_SGL_BSIZE;

	postSRBInvoke(&physaddr);
	
	return(kSCSIServiceResponse_Request_In_Process);
}

////////////////////////////////////////////////////////////////////////////////
// Format one S/G list entry
//
void
self::outputArcMSRSegment(IOMemoryCursor::PhysicalSegment segment, void *pvt, UInt32 outSegmentIndex)
{
	struct arcmsr_srb	*srb;

	srb = (struct arcmsr_srb *)pvt;

	// first time through, initialise the things we may change
	if (outSegmentIndex == 0)
		srb->sg_count = 0;

	srb->sg.sg32entry[outSegmentIndex].length = OSSwapHostToLittleInt32(segment.length & ~ARCMSR_SG_FLAGMASK);
	srb->sg.sg32entry[outSegmentIndex].address = OSSwapHostToLittleInt32(segment.location);
	srb->sg_count++;

	// Have we become a "large" SRB?
	if (((char *)&srb->sg.sg32entry[srb->sg_count] - (char *)srb) > 256)
		srb->flags |= ARCMSR_SRB_FLAG_SGL_BSIZE;
		
}

////////////////////////////////////////////////////////////////////////////////
// Handle post queue interrupts
//
void
self::handlePostQueueInterrupt(void)
{
	uint32_t			response;
	uint32_t			result;
	uint32_t			physaddr;
	SCSIParallelTaskIdentifier	parallelRequest;
	int				tag;
	struct arcmsr_srb		*srb;
	SCSITaskStatus			taskStatus;
	SCSIServiceResponse		serviceResponse;
	
	while ((response = getOutboundQueueport()) != 0xffffffff) {

		// Separate result and response key
		result = response & ARCMSR_SRBREPLY_FLAG_MASK;
		physaddr = response << 5;

		// Find the srb and task
		tag = getSRBTag(physaddr);
		if (tag < 0) {
			debug(DEBUGF_SRB, "got response 0x%x giving invalid tag", response);
			continue;
		}
		srb = getSRBPtr(tag);
		if (srb == NULL) {
			debug(DEBUGF_SRB, "got bad tag %d", tag);
			continue;
		}
		parallelRequest = FindTaskForControllerIdentifier(TARGETLUN2SCSITARGET(srb->target, srb->lun), (uintptr_t)srb);

		if (parallelRequest == NULL) {
			debug(DEBUGF_SRB, "got response 0x%x giving invalid task with identifier 0x%08x and target/lun %d/%d",
			      response, srb, srb->target, srb->lun);
			// return the tag to the freelist since the request must
			// have been timed out
			returnTagInvoke(tag);
			continue;
		}
		
		// handle the task response
		taskStatus = kSCSITaskStatus_GOOD;
		serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
		if (result & ARCMSR_SRBREPLY_FLAG_ERROR) {
			switch (srb->device_status) {
			case ARCMSR_DEV_CHECK_CONDITION:
				debug(DEBUGF_SCSI, "CHECK CONDITION");
				debug(DEBUGF_SCSI, "target %d  lun %d", srb->target, srb->lun);
				debug_hexdump(DEBUGF_SCSI, srb->cdb, 16);
				debug_hexdump(DEBUGF_SCSI, srb->sense_data, 15);
				taskStatus = (SCSITaskStatus)srb->device_status;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
				SetAutoSenseData(parallelRequest,
						 (SCSI_Sense_Data *)srb->sense_data,
						 sizeof(srb->sense_data));
				break;
				
			case ARCMSR_DEV_SELECT_TIMEOUT:
				debug(DEBUGF_SCSI, "SELECTION TIMEOUT");
				taskStatus = kSCSITaskStatus_DeviceNotPresent;
				serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
				break;
				
			case ARCMSR_DEV_ABORTED:
			case ARCMSR_DEV_INIT_FAIL:
				debug(DEBUGF_SCSI, "DEVICE FAILURE");
				taskStatus = kSCSITaskStatus_DeliveryFailure;
				serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
				break;
			default:
				debug(DEBUGF_SCSI, "SCSI ERROR %d", srb->device_status);
				// Assume we have SCSI status to pass back
				taskStatus = (SCSITaskStatus)srb->device_status;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
			}
		} else {
			// we must have transferred everything
			SetRealizedDataTransferCount(parallelRequest, GetRequestedDataTransferCount(parallelRequest));
		}
		
		CompleteParallelTask(parallelRequest, taskStatus, serviceResponse);

		// return the tag to the freelist
		returnTagInvoke(tag);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Handle the SCSI stack timing out a command on us
//
// There isn't much we can do here, unless we want to abort *all* of the
// commands on the adapter, pick out the one we've been asked for
// and then reissue the rest.
//
// It's important that we don't recycle tags/srb's that are stuck; the
// adapter may update the srb or return the tag at any time.  If one
// does pop out after the stack has timed the request out, the request
// lookup will fail and the tag will be recycled safely at that point.
//
// Note that this is not really an ideal situation, as the stack above us
// is likely to release the physical pages that are related to the I/O,
// and the adapter may subsequently DMA to/from them.  Ultimately,
// we need to be able to kill single I/Os, but the adapter does not
// give us a mechanism for that.
//
void
HandleTimeout(SCSIParallelTaskIdentifier parallelRequest)
{
	debug(DEBUGF_SCSI, "request timeout ignored");
}

////////////////////////////////////////////////////////////////////////////////
// SCSI task management
//
// These functions are required by the superclass, but the protocol is
// currently unused.  No need to implement these at this time.
//

SCSIServiceResponse
self::AbortTaskRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL, SCSITaggedTaskIdentifier theQ)
{
	debug(DEBUGF_SCSI, "task abort ignored");
	return(kSCSIServiceResponse_FUNCTION_REJECTED);
};
	
SCSIServiceResponse
self::AbortTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL)
{
	debug(DEBUGF_SCSI, "task set abort ignored");
	return(kSCSIServiceResponse_FUNCTION_REJECTED);
};
	
SCSIServiceResponse
self::ClearACARequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL)
{
	debug(DEBUGF_SCSI, "clear ACAR ignored");
	return(kSCSIServiceResponse_FUNCTION_REJECTED);
};
	
SCSIServiceResponse
self::ClearTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL)
{
	debug(DEBUGF_SCSI, "clear task set request ignored");
	return(kSCSIServiceResponse_FUNCTION_REJECTED);
};
	
SCSIServiceResponse
self::LogicalUnitResetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL)
{
	debug(DEBUGF_SCSI, "logical unit reset ignored");
	return(kSCSIServiceResponse_FUNCTION_REJECTED);
};
	
SCSIServiceResponse
self::TargetResetRequest(SCSITargetIdentifier theT)
{
	debug(DEBUGF_SCSI, "target reset ignored");
	return(kSCSIServiceResponse_FUNCTION_REJECTED);
};

////////////////////////////////////////////////////////////////////////////////
// Handle the case where we have detected that the adapter's list of units
// has changed out from under us.
//
// Note that there is a small risk that a unit is deleted and then recreated
// at the same address between scan intervals.  In this case, the system's
// old representation of the unit will not be discarded.  In practice the
// user should avoid this, and an old unit should be unmounted at the very
// least.
//
void
self::targetRescan(void)
{
	int	target, lun;
	uint8_t	newtarget, lunmask;

	for (target = 0; target < ARCMSR_MAX_TARGETID; target++) {

		// avoid racing with the code that checks the map
		newtarget = deviceMapUpdate[target];

		// anything changed in this group?
		if (newtarget != deviceMap[target]) {
			debug(DEBUGF_RESCAN, "target %d changed (%02x -> %02x)", target, deviceMap[target], newtarget);
			for (lun = 0; lun < ARCMSR_MAX_TARGETLUN; lun++) {
				lunmask = 1 << lun;

				// unit arrived
				if (!(deviceMap[target] & lunmask) && (newtarget & lunmask)) {
					debug(DEBUGF_EVENT, "device appeared at %d,%d", target, lun);
					CreateTargetForID(TARGETLUN2SCSITARGET(target, lun));
				}

				// unit departed
				if ((deviceMap[target] & lunmask) && !(newtarget & lunmask)) {
					debug(DEBUGF_EVENT, "device at %d,%d disappeared", target, lun);
					DestroyTargetForID(TARGETLUN2SCSITARGET(target, lun));
				}
			}

			// save the mask that we have just scanned against
			deviceMap[target] = newtarget;
		}
	}
}
