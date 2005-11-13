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

#ifndef ARCMSRUTILS_H
#define ARCMSRUTILS_H

#include <IOKit/IOMemoryDescriptor.h>

// Debugging
#ifdef DEBUG
__private_extern__ uint32_t arcmsr_debug_mask;
__private_extern__ void	set_debug_flags(const char *flagstring);
__private_extern__ void hexdump(void *vp, int count);

#define DEBUGF_PCI		(1<<0)
#define DEBUGF_RESOURCE		(1<<1)
#define DEBUGF_INTERRUPT	(1<<2)
#define DEBUGF_SRB		(1<<3)
#define DEBUGF_SCSI		(1<<4)
#define DEBUGF_USERCLIENT	(1<<5)
#define DEBUGF_MESSAGES		(1<<6)
#define DEBUGF_RESCAN		(1<<7)
#define DEBUGF_MISC		(1<<8)
#define DEBUGF_ADAPTER		(1<<9)
#define DEBUGF_POWER		(1<10)
#define DEBUGF_EVENT		(1<<11)
#define DEBUGF_ERROR		(1<<12)

#define debug(fac, fmt, args...)					\
do {									\
	if (arcmsr_debug_mask & fac) {					\
		kprintf("%s: " fmt "\n", __FUNCTION__ , ##args);	\
	}								\
} while(0)

#define debug_hexdump(fac, ptr, len)					\
do {									\
	if (arcmsr_debug_mask & fac)					\
		hexdump(ptr, len);					\
} while(0)

#define error(fmt, args...)						\
do {									\
	if (arcmsr_debug_mask & DEBUGF_ERROR) {				\
		kprintf("%s: " fmt "\n", __FUNCTION__ , ##args);	\
	}								\
} while(0)

#define D(fmt, args...)	do {kprintf("%s: " fmt "\n", __FUNCTION__ , ##args); IOSleep(10); } while(0)

#else
# define set_debug_flags(str)
# define debug(fac, fmt, args...)
# define error(fmt, args...)	IOLog("%s: " fmt "\n", __FUNCTION__ , ##args)
#endif



// General-purpose ringbuffer
class RingBuffer {
public:
    void	init(int desiredSize);
    void	deinit(void);
    void	clear(void);
    int		avail(void);
    bool	insert(char *indata, int len);
    bool	insert(IOMemoryDescriptor *md);
    bool	insert(uint8_t one);
    bool	insert(uint16_t one);
    bool	insert(uint32_t one);
    int		remove(char *outdata, int len);
    int		remove(IOMemoryDescriptor *md, int offset);
    int		remove(uint8_t *one);
    int		remove(uint16_t *one);
    int		remove(uint32_t *one);
private:
    int		head, tail, size;
    char	*data;
};



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// CommandGate helpers                                                        //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Some goop to simplify running things via the commandGate
//
#define COMMANDGATE_GLUE0(_func)										\
static IOReturn													\
_func ## Action(OSObject *owner, void *arg0, __unused void *arg1, __unused void *arg2, __unused void *arg3)	\
{														\
	self  *sp = OSDynamicCast(self, owner);									\
	sp->_func ();												\
	return(kIOReturnSuccess);										\
}														\
void														\
self:: _func ## Invoke ()											\
{														\
	GetCommandGate()->runAction(_func ## Action);								\
}														\
struct hack

#define COMMANDGATE_PROTO0(_func)					\
	void	_func (void);						\
	void	_func ## Invoke(void)


#define COMMANDGATE_GLUE1(_func, cast0)										\
static IOReturn													\
_func ## Action(OSObject *owner, void *arg0, __unused void *arg1, __unused void *arg2, __unused void *arg3)	\
{														\
	self  *sp = OSDynamicCast(self, owner);									\
	sp->_func ((cast0)arg0);										\
	return(kIOReturnSuccess);										\
}														\
void														\
self:: _func ## Invoke (cast0 arg0)										\
{														\
	GetCommandGate()->runAction(_func ## Action, (void *)arg0);						\
}														\
struct hack

#define COMMANDGATE_PROTO1(_func, cast0, argname0)			\
	void	_func (cast0 argname0);					\
	void	_func ## Invoke(cast0 arg0)


#define COMMANDGATE_GLUE2(_func, cast0, cast1)									\
static IOReturn													\
_func ## Action(OSObject *owner, void *arg0, __unused void *arg1, __unused void *arg2, __unused void *arg3)	\
{														\
	self  *sp = OSDynamicCast(self, owner);									\
	sp->_func ((cast0)arg0, (cast1)arg1);									\
	return(kIOReturnSuccess);										\
}														\
void														\
self:: _func ## Invoke (cast0 arg0, cast1 arg1)									\
{														\
	GetCommandGate()->runAction(_func ## Action, (void *)arg0, (void *)arg1);				\
}														\
struct hack

#define COMMANDGATE_PROTO2(_func, cast0, argname0, cast1, argname1)	\
	void	_func (cast0 argname0, cast1 argname1);			\
	void	_func ## Invoke(cast0 arg0, cast1 arg1)

#define COMMANDGATE_GLUE3(_func, cast0, cast1, cast2)								\
static IOReturn													\
_func ## Action(OSObject *owner, void *arg0, __unused void *arg1, __unused void *arg2, __unused void *arg3)	\
{														\
	self  *sp = OSDynamicCast(self, owner);									\
	sp->_func ((cast0)arg0, (cast1)arg1, (cast2)arg2);							\
	return(kIOReturnSuccess);										\
}														\
void														\
self:: _func ## Invoke (cast0 arg0, cast1 arg1, cast2 arg2)							\
{														\
	GetCommandGate()->runAction(_func ## Action, (void *)arg0, (void *)arg1, (void *)arg2);			\
}														\
struct hack

#define COMMANDGATE_PROTO3(_func, cast0, argname0, cast1, argname1, cast2, argname2)	\
	void	_func (cast0 argname0, cast1 argname1, cast2 argname2);			\
	void	_func ## Invoke(cast0 arg0, cast1 arg1, cast2 arg2)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Custom EventSource							      //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define	ARCMSR_ESFLAG_RESCAN	(1<<0)

class ArcMSREventSource : public IOTimerEventSource
{
	OSDeclareDefaultStructors(ArcMSREventSource)
	
	typedef void	(*Action) (OSObject *owner, ArcMSREventSource *sender);
    
public:

	static ArcMSREventSource *Create(OSObject *owner, Action action);

	void		addNotification(uint32_t mask);
	bool		handlerDone(void);

	bool		targetRescan(void);
	
	static void	Timeout(void *es);

private:
	void		setTimeoutFunc(void);

	uint32_t	actionMask;
	bool		handlerRunning;
};

#endif /* ARCMSRUTILS_H */
