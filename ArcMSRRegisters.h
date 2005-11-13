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
//
//
// Copyright (c) 2004-2006 ARECA Co. Ltd.
//	  Erich Chen, Taipei Taiwan All rights reserved.
//
// Redistribution and use in source and binary forms,with or without
// modification,are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice,this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice,this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. The name of the author may not be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES,INCLUDING,BUT NOT LIMITED TO,THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,INDIRECT,
// INCIDENTAL,SPECIAL,EXEMPLARY,OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT
// NOT LIMITED TO,PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA,OR PROFITS; OR BUSINESS INTERRUPTION)HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY,WHETHER IN CONTRACT,STRICT LIABILITY,OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE,EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

// $Id$

//
// Derived (with permission) from the Areca/FreeBSD 'arcmsr' driver.
//

#ifndef ARCMSRREGISTERS_H
#define ARCMSRREGISTERS_H

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// HARDWARE INTERFACE							      //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Controller-related constants
#define	ARCMSR_SCSI_INITIATOR_ID			16
#define	ARCMSR_MAX_TARGETID				16
#define	ARCMSR_MAX_TARGETLUN				8
#define ARCMSR_MAX_SG_ENTRIES				38

/******************************************************************************************************
 **		    Messaging Unit (MU) of the Intel R 80331 I/O processor (80331)
 **  ==================================================================================================
 **  The Messaging Unit (MU) transfers data between the PCI system and the 80331 
 **  notifies the respective system when new data arrives.
 **  The PCI window for messaging transactions is always the first 4 Kbytes of the inbound translation.
 **  window defined by: 
 **		       1.Inbound ATU Base Address Register 0 (IABAR0) 
 **		       2.Inbound ATU Limit Register 0 (IALR0)
 **  All of the Messaging Unit errors are reported in the same manner as ATU errors. 
 **  Error conditions and status can be found in :
 **						  1.ATUSR 
 **						  2.ATUISR
 **====================================================================================================
 **	Mechanism	 Quantity		Assert PCI Interrupt Signals	  Generate I/O Processor Interrupt
 **----------------------------------------------------------------------------------------------------
 **  Message Registers	    2 Inbound			Optional			      Optional
 **			    2 Outbound		      
 **----------------------------------------------------------------------------------------------------
 **  Doorbell Registers	    1 Inbound			Optional			      Optional
 **			    1 Outbound	
 **----------------------------------------------------------------------------------------------------
 **  Circular Queues	    4 Circular Queues		Under certain conditions	      Under certain conditions
 **----------------------------------------------------------------------------------------------------
 **  Index Registers	 1004 32-bit Memory Locations	No				      Optional
 **====================================================================================================
 **	PCI Memory Map: First 4 Kbytes of the ATU Inbound PCI Address Space
 **====================================================================================================
 **  0000H	     Reserved
 **  0004H	     Reserved
 **  0008H	     Reserved
 **  000CH	     Reserved
 **------------------------------------------------------------------------
 **  0010H	     Inbound Message Register 0		     ]
 **  0014H	     Inbound Message Register 1		     ]
 **  0018H	     Outbound Message Register 0	     ]
 **  001CH	     Outbound Message Register 1	     ]	 4 Message Registers
 **------------------------------------------------------------------------
 **  0020H	     Inbound Doorbell Register		     ]
 **  0024H	     Inbound Interrupt Status Register	     ]
 **  0028H	     Inbound Interrupt Mask Register	     ]
 **  002CH	     Outbound Doorbell Register		     ]
 **  0030H	     Outbound Interrupt Status Register	     ]
 **  0034H	     Outbound Interrupt Mask Register	     ]	 2 Doorbell Registers and 4 Interrupt Registers
 **------------------------------------------------------------------------
 **  0038H	     Reserved
 **  003CH	     Reserved
 **------------------------------------------------------------------------
 **  0040H	     Inbound Queue Port			     ]
 **  0044H	     Outbound Queue Port		     ]	 2 Queue Ports
 **------------------------------------------------------------------------
 **  0048H	     Reserved
 **  004CH	     Reserved
 **------------------------------------------------------------------------
 **  0050H						     ]
 **    :						     ]
 **    :      Intel Xscale Microarchitecture Local Memory    ]
 **    :						     ]
 **  0FFCH						     ]	 1004 Index Registers
 *******************************************************************************
 ************************************************************************************************
 **			       ARECA FIRMWARE SPEC
 ************************************************************************************************
 **	Usage of IOP331 adapter
 **	(All In/Out is in IOP331's view)
 **	1. Message 0 --> InitThread message and return code
 **
 **	2. Doorbell is used for RS-232 emulation
 **		inDoorBell :	bit0 -- data in ready		 (DRIVER DATA WRITE OK)
 **				bit1 -- data out has been read	 (DRIVER DATA READ OK)
 **		outDooeBell:	bit0 -- data out ready		 (IOP331 DATA WRITE OK)
 **				bit1 -- data in has been read	 (IOP331 DATA READ OK)
 **
 **	3. Index Memory Usage
 **		offset 0xf00 : for RS232 out (request buffer)
 **		offset 0xe00 : for RS232 in  (scratch buffer)
 **		offset 0xa00 : for inbound message code message_wbuffer (driver to IOP331)
 **		offset 0x800 : for outbound message code  message_rbuffer (IOP331 to driver)
 **
 **	4. RS-232 emulation
 **		Currently 128 byte buffer is used
 **			  1st ULONG : Data length (1--124)
 **			Byte 4--127 : Max 124 bytes of data
 **
 **	5. PostQ
 **	All SCSI Command must be sent through postQ:
 **		(inbound queue port)	Request frame must be 32 bytes aligned 
 **		bit27--bit31 => flag for post srb 
 **			bit31 : 0 : 256 bytes frame
 **				1 : 512 bytes frame
 **			bit30 : 0 : normal request
 **				1 : BIOS request
 **			bit29 : reserved
 **			bit28 : reserved
 **			bit27 : reserved
 **		bit0--bit26 => physical address (bit27--bit31) of post arcmsr_cdb  
 **
 **		(outbount queue port)	Request reply			       
 **		bit27--bit31 => flag for reply
 **			bit31 : must be 0 (for this type of reply)
 **			bit30 : reserved for BIOS handshake
 **			bit29 : reserved
 **			bit28 : 0 : no error, ignore AdapStatus/DevStatus/SenseData
 **				1 : Error, error code in AdapStatus/DevStatus/SenseData
 **			bit27 : reserved
 **			bit0--bit26 => physical address (bit27--bit31) of reply arcmsr_cdb
 **
 **	6. BIOS request
 **		All BIOS request is the same with request from PostQ
 **		Except :
 **			Request frame is sent from configuration space
 **				offset: 0x78 : Request Frame (bit30 == 1)
 **				offset: 0x18 : writeonly to generate IRQ to IOP331
 **			Completion of request:
 **				(bit30 == 0, bit28==err flag)
 **
 **	7. Definition of SGL entry (structure)
 **
 **	8. Message1 Out - Diag Status Code (????)
 **
 **	9. Message0 message code :
 **			0x00 : NOP
 **			0x01 : Get Config ->offset 0xa00 :for outbound message code  message_rbuffer (IOP331 to driver)
 **				Signature	0x87974060(4)
 **				Request len	0x00000200(4)
 **				# of queue	0x00000100(4)
 **				SDRAM Size	0x00000100(4)-->256 MB
 **				IDE Channels	0x00000008(4)
 **				vendor		40 bytes char
 **				model		 8 bytes char
 **				FirmVer		16 bytes char
 **				Device Map	16 Bytes
 **				FirmwareVersion DWORD <== Added for checking of new firmware capability
 **			0x02 : Set Config ->offset 0xa00 : for inbound message code message_wbuffer (driver to IOP331)
 **				Signature	0x87974063(4)
 **				UPPER32 of Request Frame  (4)-->Driver Only
 **			0x03 : Reset (Abort all queued Command)
 **			0x04 : Stop Background Activity
 **			0x05 : Flush Cache
 **			0x06 : Start Background Activity (re-start if background is halted)
 **			0x07 : Check If Host Command Pending (Novell May Need This Function)
 **			0x08 : Set controller time ->offset 0xa00 : for inbound message code message_wbuffer (driver to IOP331)
 **				byte 0 : 0xaa <-- signature
 **				byte 1 : 0x55 <-- signature
 **				byte 2 : year (04)
 **				byte 3 : month (1..12)
 **				byte 4 : date (1..31)
 **				byte 5 : hour (0..23)
 **				byte 6 : minute (0..59)
 **				byte 7 : second (0..59)
 ************************************************************************************************/

////////////////////////////////////////////////////////////////////////////////
// adapter configuration structure
//
struct arcmsr_adapter_config {
	uint32_t	signature;
#define ARCMSR_CONFIG_SIGNATURE	0x87974060
	uint32_t	request_size;
	uint32_t	queue_depth;
	uint32_t	installed_memory;
	uint32_t	channels;
	char		vendor[40];
	char		model[8];
	char		firmware_version[16];
	uint8_t		device_map[16];
	uint32_t	firmware_version2;
};

struct arcmsr_adapter_setconfig {
	uint32_t	signature;
#define ARCMSR_SETCONFIG_SIGNATURE 0x87974063
	uint32_t	rqphys_high;
};

////////////////////////////////////////////////////////////////////////////////
// in/outbound message buffer
//
struct ioctl_qbuffer {
	uint32_t	length;
	char		data[124];
};

////////////////////////////////////////////////////////////////////////////////
// Layout of the IOP message unit in memory
//
struct arcmsr_mu {
 	uint32_t				reserved0[4];		/*0000 000F*/
	uint32_t				inbound_msgaddr0;	/*0010 0013*/
#define	ARCMSR_INBOUND_MESG0_NOP			0x00000000
#define	ARCMSR_INBOUND_MESG0_GET_CONFIG			0x00000001	// read configuration
#define	ARCMSR_INBOUND_MESG0_SET_CONFIG			0x00000002	// write configuration
#define	ARCMSR_INBOUND_MESG0_ABORT_CMD			0x00000003	// abort all commands
#define	ARCMSR_INBOUND_MESG0_STOP_BGRB			0x00000004	// stop background rebuild
#define	ARCMSR_INBOUND_MESG0_FLUSH_CACHE		0x00000005	// flush controller cache
#define	ARCMSR_INBOUND_MESG0_START_BGRB			0x00000006	// start background rebuild
#define	ARCMSR_INBOUND_MESG0_CHK331PENDING		0x00000007	// test for command running
#define	ARCMSR_INBOUND_MESG0_SYNC_TIMER			0x00000008	// set adapter time of day

	uint32_t				inbound_msgaddr1;	/*0014 0017*/
	// unused
    
	uint32_t				outbound_msgaddr0;	/*0018 001B*/
	// unused
    
	uint32_t				outbound_msgaddr1;	/*001C 001F*/
#define ARCMSR_OUTBOUND_MESG1_FIRMWARE_OK		0x80000000	// firmware started OK
    
	uint32_t				inbound_doorbell;	/*0020 0023*/
#define ARCMSR_INBOUND_DRIVER_DATA_WRITE_OK		0x00000001	// driver wrote ioctl_wbuffer
#define ARCMSR_INBOUND_DRIVER_DATA_READ_OK		0x00000002	// driver read ioctl_rbuffer
    
	uint32_t				inbound_intstatus;	/*0024 0027*/
#define	    ARCMSR_MU_INBOUND_INDEX_INT				0x40
#define	    ARCMSR_MU_INBOUND_QUEUEFULL_INT			0x20
#define	    ARCMSR_MU_INBOUND_POSTQUEUE_INT			0x10	  
#define	    ARCMSR_MU_INBOUND_ERROR_DOORBELL_INT		0x08
#define	    ARCMSR_MU_INBOUND_DOORBELL_INT			0x04
#define	    ARCMSR_MU_INBOUND_MESSAGE1_INT			0x02
#define	    ARCMSR_MU_INBOUND_MESSAGE0_INT			0x01
    
	uint32_t				inbound_intmask;	/*0028 002B*/
#define	    ARCMSR_MU_INBOUND_INDEX_INTMASKENABLE		0x40
#define	    ARCMSR_MU_INBOUND_QUEUEFULL_INTMASKENABLE		0x20
#define	    ARCMSR_MU_INBOUND_POSTQUEUE_INTMASKENABLE		0x10	     
#define	    ARCMSR_MU_INBOUND_DOORBELL_ERROR_INTMASKENABLE	0x08
#define	    ARCMSR_MU_INBOUND_DOORBELL_INTMASKENABLE		0x04
#define	    ARCMSR_MU_INBOUND_MESSAGE1_INTMASKENABLE		0x02
#define	    ARCMSR_MU_INBOUND_MESSAGE0_INTMASKENABLE		0x01
    
	uint32_t				outbound_doorbell;	/*002C 002F*/
#define ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK		0x00000001	// adapter wrote ioctl_rbuffer
#define ARCMSR_OUTBOUND_IOP331_DATA_READ_OK		0x00000002	// adapter read ioctl_wbuffer
    
	uint32_t				outbound_intstatus;	/*0030 0033*/
#define	    ARCMSR_MU_OUTBOUND_PCI_INT				0x10
#define	    ARCMSR_MU_OUTBOUND_POSTQUEUE_INT			0x08 
#define	    ARCMSR_MU_OUTBOUND_DOORBELL_INT			0x04 
#define	    ARCMSR_MU_OUTBOUND_MESSAGE1_INT			0x02 
#define	    ARCMSR_MU_OUTBOUND_MESSAGE0_INT			0x01 
    
	uint32_t				outbound_intmask;	/*0034 0037*/
#define	    ARCMSR_MU_OUTBOUND_PCI_INTMASKENABLE		0x10
#define	    ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE		0x08 
#define	    ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE		0x04 
#define	    ARCMSR_MU_OUTBOUND_MESSAGE1_INTMASKENABLE		0x02 
#define	    ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE		0x01 
#define	    ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE		0x1F 
    
	uint32_t				reserved1[2];		/*0038 003F*/
	uint32_t				inbound_queueport;	/*0040 0043*/
#define ARCMSR_SRBPOST_FLAG_SGL_BSIZE			0x80000000
#define ARCMSR_SRBPOST_FLAG_IAM_BIOS			0x40000000
	uint32_t				outbound_queueport;	/*0044 0047*/
#define ARCMSR_SRBREPLY_FLAG_IAM_BIOS			0x40000000
#define ARCMSR_SRBREPLY_FLAG_ERROR			0x10000000
#define ARCMSR_SRBREPLY_FLAG_MASK			0xf8000000
	uint32_t				reserved2[2];		/*0048 004F*/
	uint32_t				reserved3[492];		/*0050 07FF ...local_buffer	492*/
	uint32_t				message_rbuffer[128];	/*0800 09FF			128*/
	uint32_t				message_wbuffer[256];	/*0a00 0DFF			256*/
	struct ioctl_qbuffer			ioctl_wbuffer;		/*0E00 0E7F			 32*/
	uint32_t				reserved4[32];		/*0E80 0EFF			 32*/
	struct ioctl_qbuffer			ioctl_rbuffer;		/*0F00 0F7F			 32*/
	uint32_t				reserved5[32];		/*0F80 0FFF			 32*/
} __attribute__((packed));


////////////////////////////////////////////////////////////////////////////////
// Scatter/gather list format
//
#define ARCMSR_SG_PHYS64		0x01000000	    // 64-bit physical address
#define ARCMSR_SG_FLAGMASK		0xff000000

struct sgentry32 {
	uint32_t	length;
	uint32_t	address;
};

struct sgentry64 {
	uint32_t	length;
	uint32_t	address;
};

////////////////////////////////////////////////////////////////////////////////
// Command descriptor as submitted to the adapter
//
struct arcmsr_srb {
	uint8_t		bus;
	uint8_t		target;
	uint8_t		lun;
	uint8_t		function;
    
	uint8_t		cdb_length;
	uint8_t		sg_count;
	uint8_t		flags;
#define ARCMSR_SRB_FLAG_SGL_BSIZE	0x01	// bit 0: 0(256) / 1(512) bytes
#define ARCMSR_SRB_FLAG_BIOS		0x02	// bit 1: 0(from driver) / 1(from BIOS)
#define ARCMSR_SRB_FLAG_WRITE		0x04	// bit 2: 0(Data in) / 1(Data out)
#define ARCMSR_SRB_FLAG_SIMPLEQ		0x00	// bit 4/3 ,00 : simple Q,01 : head of Q,10 : ordered Q
#define ARCMSR_SRB_FLAG_HEADQ		0x08
#define ARCMSR_SRB_FLAG_ORDEREDQ	0x10
	uint8_t		reserved0;
    
	uint32_t	context;		// caller context handle (not used)
	uint32_t	reserved1;		// was DataLength, not used
    
	uint8_t		cdb[16];
    
	uint8_t		device_status;		// SCSI command status
#define ARCMSR_DEV_CHECK_CONDITION	0x02	// our old friend
#define ARCMSR_DEV_SELECT_TIMEOUT	0xF0	// vendor-specific additional codes
#define ARCMSR_DEV_ABORTED		0xF1
#define ARCMSR_DEV_INIT_FAIL		0xF2
	
	uint8_t		sense_data[15];
    
	union {
		sgentry32	sg32entry[ARCMSR_MAX_SG_ENTRIES];
		sgentry64	sg64entry[ARCMSR_MAX_SG_ENTRIES];
	} sg;
};

#endif /* ARCMSRREGISTERS.H */
