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

/**********************************************************************************************************
 **				  RS-232 Interface for Areca Raid Controller
 **		      The low level command interface is exclusive with VT100 terminal
 **  --------------------------------------------------------------------
 **    1. Sequence of command execution
 **  --------------------------------------------------------------------
 **	  (A) Header : 3 bytes sequence (0x5E, 0x01, 0x61)
 **	  (B) Command block : variable length of data including length, command code, data and checksum byte
 **	  (C) Return data : variable length of data
 **  --------------------------------------------------------------------  
 **    2. Command block
 **  --------------------------------------------------------------------
 **	  (A) 1st byte : command block length (low byte)
 **	  (B) 2nd byte : command block length (high byte)
 **		  note ..command block length shouldn't > 2040 bytes, length excludes these two bytes
 **	  (C) 3rd byte : command code
 **	  (D) 4th and following bytes : variable length data bytes depends on command code
 **	  (E) last byte : checksum byte (sum of 1st byte until last data byte)
 **  --------------------------------------------------------------------  
 **    3. Command code and associated data
 **  --------------------------------------------------------------------
 **	  The following are command code defined in raid controller Command code 0x10--0x1? are used
 **	  for system level management, no password checking is needed and should be implemented in
 **	  separate well controlled utility and not for end user access.
 **
 **	  Command code 0x20--0x?? always check the password, password must be entered to enable
 **	  these command.
 **/
enum
{
	GUI_SET_SERIAL=0x10,
	GUI_SET_VENDOR,
	GUI_SET_MODEL,
	GUI_IDENTIFY,
	GUI_CHECK_PASSWORD,
	GUI_LOGOUT,
	GUI_HTTP,
	GUI_SET_ETHERNET_ADDR,
	GUI_SET_LOGO,
	GUI_POLL_EVENT,
	GUI_GET_EVENT,
	GUI_GET_HW_MONITOR,
    
	GUI_GET_INFO_R=0x20,
	GUI_GET_INFO_V,
	GUI_GET_INFO_P,
	GUI_GET_INFO_S,
	GUI_CLEAR_EVENT,
    
	GUI_MUTE_BEEPER=0x30,
	GUI_BEEPER_SETTING,
	GUI_SET_PASSWORD,
	GUI_HOST_INTERFACE_MODE,
	GUI_REBUILD_PRIORITY,
	GUI_MAX_ATA_MODE,
	GUI_RESET_CONTROLLER,
	GUI_COM_PORT_SETTING,
	GUI_NO_OPERATION,
	GUI_DHCP_IP,
    
	GUI_CREATE_PASS_THROUGH=0x40,
	GUI_MODIFY_PASS_THROUGH,
	GUI_DELETE_PASS_THROUGH,
	GUI_IDENTIFY_DEVICE,
    
	GUI_CREATE_RAIDSET=0x50,
	GUI_DELETE_RAIDSET,
	GUI_EXPAND_RAIDSET,
	GUI_ACTIVATE_RAIDSET,
	GUI_CREATE_HOT_SPARE,
	GUI_DELETE_HOT_SPARE,
    
	GUI_CREATE_VOLUME=0x60,
	GUI_MODIFY_VOLUME,
	GUI_DELETE_VOLUME,
	GUI_START_CHECK_VOLUME,
	GUI_STOP_CHECK_VOLUME
};

/**    
 **    Command description :
 **    
 **	  GUI_SET_SERIAL : Set the controller serial#
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x10
 **	      byte 3	      : password length (should be 0x0f)
 **	      byte 4-0x13     : should be "ArEcATecHnoLogY"
 **	      byte 0x14--0x23 : Serial number string (must be 16 bytes)
 **/

struct gui_cmd_set_serial {
	uint8_t	pw_len;
	uint8_t	password[15];
	uint8_t	serial[16];
};

/**
 **	GUI_SET_VENDOR : Set vendor string for the controller
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x11
 **	      byte 3	      : password length (should be 0x08)
 **	      byte 4-0x13     : should be "ArEcAvAr"
 **	      byte 0x14--0x3B : vendor string (must be 40 bytes)
 **/

struct gui_cmd_set_vendor {
	uint8_t	pw_len;
	uint8_t	password[15];
	uint8_t	vendor[40];
};

/**
 **	GUI_SET_MODEL : Set the model name of the controller
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x12
 **	      byte 3	      : password length (should be 0x08)
 **	      byte 4-0x13     : should be "ArEcAvAr"
 **	      byte 0x14--0x1B : model string (must be 8 bytes)
 **/

struct gui_cmd_set_vendor {
	uint8_t	pw_len;
	uint8_t	password[15];
	uint8_t	model[8];
};

/**
 **	GUI_IDENTIFY : Identify device
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x13
 **				return "Areca RAID Subsystem "
 **
 **	GUI_CHECK_PASSWORD : Verify password
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x14
 **	      byte 3	      : password length
 **	      byte 4-0x??     : user password to be checked
 **
 **	GUI_LOGOUT : Logout GUI (force password checking on next command)
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x15
 **
 **	GUI_HTTP : HTTP interface (reserved for Http proxy service)(0x16)
 **		Data is passed directly to HTTP server.
 **
 **	GUI_SET_ETHERNET_ADDR : Set the ethernet MAC address
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x17
 **	      byte 3	      : password length (should be 0x08)
 **	      byte 4-0x13     : should be "ArEcAvAr"
 **	      byte 0x14--0x19 : Ethernet MAC address (must be 6 bytes)
 **
 **	GUI_SET_LOGO : Set logo in HTTP
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x18
 **	      byte 3	      : Page# (0/1/2/3) (0xff --> clear OEM logo)
 **	      byte 4/5/6/7    : 0x55/0xaa/0xa5/0x5a
 **	      byte 8	      : TITLE.JPG data (each page must be 2000 bytes)
 **				note .... page0 1st 2 byte must be actual length of the JPG file
 **/

struct gui_cmd_set_logo {
	uint8_t	page;
	uint8_t signature[4];
	union {
		struct {
			uint16_t imagesize;
			uint8_t bytes[];
		} page0;
		bytes[];
	} data;
};
		
/**
 **	GUI_POLL_EVENT : Poll If Event Log Changed
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x19
 **
 **	GUI_GET_EVENT : Read Event
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x1a
 **	      byte 3	      : Event Page (0:1st page/1/2/3:last page)
 **
 **	GUI_GET_HW_MONITOR : Get HW monitor data
 **	      byte 0,1	      : length
 **	      byte 2		 : command code 0x1b
 **	      byte 3		 : # of FANs(example 2)
 **	      byte 4		 : # of Voltage sensor(example 3)
 **	      byte 5		 : # of temperature sensor(example 2)
 **	      byte 6		 : # of power
 **	      byte 7/8	      : Fan#0 (RPM)
 **	      byte 9/10	      : Fan#1
 **	      byte 11/12	 : Voltage#0 original value in *1000
 **	      byte 13/14	 : Voltage#0 value
 **	      byte 15/16	 : Voltage#1 org
 **	      byte 17/18	 : Voltage#1
 **	      byte 19/20	 : Voltage#2 org
 **	      byte 21/22	 : Voltage#2
 **	      byte 23	      : Temp#0
 **	      byte 24	      : Temp#1
 **	      byte 25	      : Power indicator (bit0 : power#0, bit1 : power#1)
 **	      byte 26	      : UPS indicator
 **
 **	GUI_GET_INFO_R : Get Raid Set Information
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x20
 **	      byte 3	      : raidset#
 **/
typedef struct sGUI_RAIDSET
{
	uint8_t grsRaidSetName[16];
	uint32_t grsCapacity;
	uint32_t grsCapacityX;
	uint32_t grsFailMask;
	uint8_t grsDevArray[32];
	uint8_t grsMemberDevices;
	uint8_t grsNewMemberDevices;
	uint8_t grsRaidState;
	uint8_t grsVolumes;
	uint8_t grsVolumeList[16];
	uint8_t grsRes1;
	uint8_t grsRes2;
	uint8_t grsRes3;
	uint8_t grsFreeSegments;
	uint32_t grsRawStripes[8];
	uint32_t grsRes4;
	uint32_t grsRes5; //	    Total to 128 bytes
	uint32_t grsRes6; //	    Total to 128 bytes
} sGUI_RAIDSET, *pGUI_RAIDSET;

/**
 **	GUI_GET_INFO_V : Get Volume Set Information
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x21
 **	      byte 3	      : volumeset#
 **/
typedef struct sGUI_VOLUMESET
{
	uint8_t gvsVolumeName[16]; //     16
	uint32_t gvsCapacity;
	uint32_t gvsCapacityX;
	uint32_t gvsFailMask;
	uint32_t gvsStripeSize;
	uint32_t gvsNewFailMask;
	uint32_t gvsNewStripeSize;
	uint32_t gvsVolumeStatus;
	uint32_t gvsProgress; //	32
	sSCSI_ATTR gvsScsi; 
	uint8_t gvsMemberDisks;
	uint8_t gvsRaidLevel; //	8
    
	uint8_t gvsNewMemberDisks;
	uint8_t gvsNewRaidLevel;
	uint8_t gvsRaidSetNumber;
	uint8_t gvsRes0; //	   4
	uint8_t gvsRes1[4]; //     64 bytes
} sGUI_VOLUMESET, *pGUI_VOLUMESET;

/**    
 **	GUI_GET_INFO_P : Get Physical Drive Information
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x22
 **	      byte 3	      : drive # (from 0 to max-channels - 1)
 **/
typedef struct sGUI_PHY_DRV
{
	uint8_t gpdModelName[40];
	uint8_t gpdSerialNumber[20];
	uint8_t gpdFirmRev[8];
	uint32_t gpdCapacity;
	uint32_t gpdCapacityX; //	 Reserved for expansion
	uint8_t gpdDeviceState;
	uint8_t gpdPioMode;
	uint8_t gpdCurrentUdmaMode;
	uint8_t gpdUdmaMode;
	uint8_t gpdDriveSelect;
	uint8_t gpdRaidNumber; //	 0xff if not belongs to a raid set
	sSCSI_ATTR gpdScsi;
	uint8_t gpdReserved[40]; //	   Total to 128 bytes
} sGUI_PHY_DRV, *pGUI_PHY_DRV;

/**    
 **	  GUI_GET_INFO_S : Get System Information
 **	    byte 0,1	    : length
 **	    byte 2	    : command code 0x23
 **/
typedef struct sCOM_ATTR
{
	uint8_t comBaudRate;
	uint8_t comDataBits;
	uint8_t comStopBits;
	uint8_t comParity;
	uint8_t comFlowControl;
} sCOM_ATTR, *pCOM_ATTR;
    
typedef struct sSYSTEM_INFO
{
	uint8_t gsiVendorName[40];
	uint8_t gsiSerialNumber[16];
	uint8_t gsiFirmVersion[16];
	uint8_t gsiBootVersion[16];
	uint8_t gsiMbVersion[16];
	uint8_t gsiModelName[8];
	uint8_t gsiLocalIp[4];
	uint8_t gsiCurrentIp[4];
	uint32_t gsiTimeTick;
	uint32_t gsiCpuSpeed;
	uint32_t gsiICache;
	uint32_t gsiDCache;
	uint32_t gsiScache;
	uint32_t gsiMemorySize;
	uint32_t gsiMemorySpeed;
	uint32_t gsiEvents;
	uint8_t gsiMacAddress[6];
	uint8_t gsiDhcp;
	uint8_t gsiBeeper;
	uint8_t gsiChannelUsage;
	uint8_t gsiMaxAtaMode;
	uint8_t gsiSdramEcc; //     1:if ECC enabled
	uint8_t gsiRebuildPriority;
	sCOM_ATTR gsiComA; //	5 bytes
	sCOM_ATTR gsiComB; //	5 bytes
	uint8_t gsiIdeChannels;
	uint8_t gsiScsiHostChannels;
	uint8_t gsiIdeHostChannels;
	uint8_t gsiMaxVolumeSet;
	uint8_t gsiMaxRaidSet;
	uint8_t gsiEtherPort; //	1:if ether net port supported
	uint8_t gsiRaid6Engine; //	  1:Raid6 engine supported
	uint8_t gsiRes[75];
} sSYSTEM_INFO, *pSYSTEM_INFO;

/**    
 **	GUI_CLEAR_EVENT : Clear System Event
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x24
 **    
 **	GUI_MUTE_BEEPER : Mute current beeper
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x30
 **    
 **	GUI_BEEPER_SETTING : Disable beeper
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x31
 **	      byte 3	      : 0->disable, 1->enable
 **    
 **	GUI_SET_PASSWORD : Change password
 **	      byte 0,1	      : length
 **	      byte 2		 : command code 0x32
 **	      byte 3		 : pass word length ( must <= 15 )
 **	      byte 4		 : password (must be alpha-numerical)
 **    
 **	  GUI_HOST_INTERFACE_MODE : Set host interface mode
 **	      byte 0,1	      : length
 **	      byte 2		 : command code 0x33
 **	      byte 3		 : 0->Independent, 1->cluster
 **    
 **	GUI_REBUILD_PRIORITY : Set rebuild priority
 **	      byte 0,1	      : length
 **	      byte 2		 : command code 0x34
 **	      byte 3		 : 0/1/2/3 (low->high)
 **    
 **	GUI_MAX_ATA_MODE : Set maximum ATA mode to be used
 **	      byte 0,1	      : length
 **	      byte 2		 : command code 0x35
 **	      byte 3		 : 0/1/2/3 (133/100/66/33)
 **    
 **	GUI_RESET_CONTROLLER : Reset Controller
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x36
 **			      *Response with VT100 screen (discard it)
 **    
 **	GUI_COM_PORT_SETTING : COM port setting
 **	      byte 0,1	      : length
 **	      byte 2		 : command code 0x37
 **	      byte 3		 : 0->COMA (term port), 1->COMB (debug port)
 **	      byte 4		 : 0/1/2/3/4/5/6/7 (1200/2400/4800/9600/19200/38400/57600/115200)
 **	      byte 5		 : data bit (0:7 bit, 1:8 bit : must be 8 bit)
 **	      byte 6		 : stop bit (0:1, 1:2 stop bits)
 **	      byte 7		 : parity (0:none, 1:off, 2:even)
 **	      byte 8		 : flow control (0:none, 1:xon/xoff, 2:hardware => must use none)
 **    
 **	GUI_NO_OPERATION : No operation
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x38
 **    
 **	GUI_DHCP_IP : Set DHCP option and local IP address
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x39
 **	      byte 3	      : 0:dhcp disabled, 1:dhcp enabled
 **	      byte 4/5/6/7    : IP address
 **    
 **	GUI_CREATE_PASS_THROUGH : Create pass through disk
 **	      byte 0,1	      : length
 **	      byte 2		 : command code 0x40
 **	      byte 3		 : device #
 **	      byte 4		 : scsi channel (0/1)
 **	      byte 5		 : scsi id (0-->15)
 **	      byte 6		 : scsi lun (0-->7)
 **	      byte 7		 : tagged queue (1 : enabled)
 **	      byte 8		 : cache mode (1 : enabled)
 **	      byte 9		 : max speed (0/1/2/3/4, async/20/40/80/160 for scsi)
 **					  (0/1/2/3/4, 33/66/100/133/150 for ide	 )
 **    
 **	GUI_MODIFY_PASS_THROUGH : Modify pass through disk
 **	      byte 0,1	      : length
 **	      byte 2		 : command code 0x41
 **	      byte 3		 : device #
 **	      byte 4		 : scsi channel (0/1)
 **	      byte 5		 : scsi id (0-->15)
 **	      byte 6		 : scsi lun (0-->7)
 **	      byte 7		 : tagged queue (1 : enabled)
 **	      byte 8		 : cache mode (1 : enabled)
 **	      byte 9		 : max speed (0/1/2/3/4, async/20/40/80/160 for scsi)
 **					  (0/1/2/3/4, 33/66/100/133/150 for ide	 )
 **    
 **	GUI_DELETE_PASS_THROUGH : Delete pass through disk
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x42
 **	      byte 3	      : device# to be deleted
 **    
 **	GUI_IDENTIFY_DEVICE : Identify Device
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x43
 **	      byte 3	      : Flash Method(0:flash selected, 1:flash not selected)
 **	      byte 4/5/6/7    : IDE device mask to be flashed
 **			     note .... no response data available
 **    
 **	  GUI_CREATE_RAIDSET : Create Raid Set
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x50
 **	      byte 3/4/5/6    : device mask
 **	      byte 7-22	      : raidset name (if byte 7 == 0:use default)
 **    
 **	GUI_DELETE_RAIDSET : Delete Raid Set
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x51
 **	      byte 3	      : raidset#
 **    
 **	  GUI_EXPAND_RAIDSET : Expand Raid Set 
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x52
 **	      byte 3	      : raidset#
 **	      byte 4/5/6/7    : device mask for expansion
 **	      byte 8/9/10     : (8:0 no change, 1 change, 0xff:terminate, 9:new raid level,10:new stripe size 0/1/2/3/4/5->4/8/16/32/64/128K )
 **	      byte 11/12/13   : repeat for each volume in the raidset ....
 **    
 **	GUI_ACTIVATE_RAIDSET : Activate incomplete raid set 
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x53
 **	      byte 3	      : raidset#
 **    
 **	GUI_CREATE_HOT_SPARE : Create hot spare disk 
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x54
 **	      byte 3/4/5/6    : device mask for hot spare creation
 **    
 **	  GUI_DELETE_HOT_SPARE : Delete hot spare disk 
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x55
 **	      byte 3/4/5/6    : device mask for hot spare deletion
 **    
 **	  GUI_CREATE_VOLUME : Create volume set 
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x60
 **	      byte 3	      : raidset#
 **	      byte 4-19	      : volume set name (if byte4 == 0, use default)
 **	      byte 20-27      : volume capacity (blocks)
 **	      byte 28	      : raid level
 **	      byte 29	      : stripe size (0/1/2/3/4/5->4/8/16/32/64/128K)
 **	      byte 30	      : channel
 **	      byte 31	      : ID
 **	      byte 32	      : LUN
 **	      byte 33	      : 1 enable tag
 **	      byte 34	      : 1 enable cache
 **	      byte 35	      : speed (0/1/2/3/4->async/20/40/80/160 for scsi)
 **				      (0/1/2/3/4->33/66/100/133/150 for IDE  )
 **	      byte 36	      : 1 to select quick init
 **    
 **	  GUI_MODIFY_VOLUME : Modify volume Set
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x61
 **	      byte 3	      : volumeset#
 **	      byte 4-19	      : new volume set name (if byte4 == 0, not change)
 **	      byte 20-27      : new volume capacity (reserved)
 **	      byte 28	      : new raid level
 **	      byte 29	      : new stripe size (0/1/2/3/4/5->4/8/16/32/64/128K)
 **	      byte 30	      : new channel
 **	      byte 31	      : new ID
 **	      byte 32	      : new LUN
 **	      byte 33	      : 1 enable tag
 **	      byte 34	      : 1 enable cache
 **	      byte 35	      : speed (0/1/2/3/4->async/20/40/80/160 for scsi)
 **				      (0/1/2/3/4->33/66/100/133/150 for IDE  )
 **    
 **	  GUI_DELETE_VOLUME : Delete volume set
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x62
 **	      byte 3	      : volumeset#
 **    
 **	  GUI_START_CHECK_VOLUME : Start volume consistency check
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x63
 **	      byte 3	      : volumeset#
 **    
 **	  GUI_STOP_CHECK_VOLUME : Stop volume consistency check
 **	      byte 0,1	      : length
 **	      byte 2	      : command code 0x64
 ** ---------------------------------------------------------------------   
 **    4. Returned data
 ** ---------------------------------------------------------------------   
 **	  (A) Header	      : 3 bytes sequence (0x5E, 0x01, 0x61)
 **	  (B) Length	      : 2 bytes (low byte 1st, excludes length and checksum byte)
 **	  (C) status or data  :
 **	     <1> If length == 1 ==> 1 byte status code
 **				      #define GUI_OK			0x41
 **				      #define GUI_RAIDSET_NOT_NORMAL	0x42
 **				      #define GUI_VOLUMESET_NOT_NORMAL	0x43
 **				      #define GUI_NO_RAIDSET		0x44
 **				      #define GUI_NO_VOLUMESET		0x45
 **				      #define GUI_NO_PHYSICAL_DRIVE	0x46
 **				      #define GUI_PARAMETER_ERROR	0x47
 **				      #define GUI_UNSUPPORTED_COMMAND	0x48
 **				      #define GUI_DISK_CONFIG_CHANGED	0x49
 **				      #define GUI_INVALID_PASSWORD	0x4a
 **				      #define GUI_NO_DISK_SPACE		0x4b
 **				      #define GUI_CHECKSUM_ERROR	0x4c
 **				      #define GUI_PASSWORD_REQUIRED	0x4d
 **	     <2> If length > 1 ==> data block returned from controller and the contents depends on the command code
 **	  (E) Checksum : checksum of length and status or data byte
 **************************************************************************
 */


typedef struct sGUI_COMMAND {
	uint8_t	sig[3];
	uint16_t length;
	uint8_t opcode;
	
} sGUI_COMMAND, *pGUI_COMMAND;

