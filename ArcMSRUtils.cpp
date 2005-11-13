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

uint32_t arcmsr_debug_mask = 0;

////////////////////////////////////////////////////////////////////////////////
// Debugging support
//

#ifdef DEBUG

static struct {
	char		*name;
	uint32_t	flag;
} debug_flagnames[] = {
	{"pci",		DEBUGF_PCI},
	{"resource",	DEBUGF_RESOURCE},
	{"interrupt",	DEBUGF_INTERRUPT},
	{"srb",		DEBUGF_SRB},
	{"scsi",	DEBUGF_SCSI},
	{"userclient",	DEBUGF_USERCLIENT},
	{"messages",	DEBUGF_MESSAGES},
	{"rescan",	DEBUGF_RESCAN},
	{"misc",	DEBUGF_MISC},
	{"adapter",	DEBUGF_ADAPTER},
	{"power",	DEBUGF_POWER},
	{"event",	DEBUGF_EVENT},
	{"all",		~(uint32_t)0},
	{NULL, 0}
};

__private_extern__ void
set_debug_flags(const char *flagstring)
{
	static bool	done = false;
	int		i, r;
	const char	*s, *p;

	if (!done) {
		done = true;
		s = flagstring;
		while (s != NULL) {
			p = strchr(s, ',');
			for (i = 0; debug_flagnames[i].name != NULL; i++) {
				if (p != NULL) {
					r = strncmp(debug_flagnames[i].name, s, p - s);
				} else {
					r = strcmp(debug_flagnames[i].name, s);
				}
				if (!r) {
					arcmsr_debug_mask |= debug_flagnames[i].flag;
					break;
				}
			}
			if (p != NULL) {
				s = p + 1;
			} else {
				s = NULL;
			}
		}
	}
	debug(DEBUGF_MISC, "set debug flags 0x%x for '%s'", arcmsr_debug_mask, flagstring);
}

	
__private_extern__ void
hexdump(void *vp, int count)
{
    UInt8       *p = (UInt8 *)vp;
    int         i, j;
    
    for (j = 0; j < count; j += 16) {
        kprintf("    %04x:", j);
        for (i = j; i < (j + 16); i++) {
	    if (i < count) {
		kprintf(" %02x", (uint)*(p + i));
	    } else {
		kprintf("   ");
	    }
	}
	kprintf(" - ");
        for (i = j; i < (j + 16); i++) {
	    if ((i < count) && (*(p + i) >= 32) && (*(p + i) < 127)) {
		kprintf("%c", (char)*(p + i));
	    } else {
		kprintf(".");
	    }
	}
        kprintf("\n");
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////
// General-purpose byte-holding ringbuffer
//

void
RingBuffer::init(int desiredSize) 
{ 
    size = desiredSize + 1;
    data = (char *)IOMalloc(size);
    head = 0; 
    tail = 0;
};

void
RingBuffer::deinit(void)
{
    if (data != NULL)
	IOFree(data, size);
};

void
RingBuffer::clear(void)
{
	head = 0;
	tail = 0;
}

int
RingBuffer::avail(void) {
    return(size - (((head + size) - tail) % size));
};

bool
RingBuffer::insert(char *indata, int len) 
{
    int	frag;
    
    if (len > avail()) 
	return(false);	    // space check
    frag = size - head;	    // fill head-to-end first
    if (frag > len)
	frag = len;
    memcpy(data + head, indata, frag);
    head += frag;
    if (head == size)	    // head never points to end
	head = 0;
    if (len > frag) {	    // more to copy?
	memcpy(data, indata + frag, len - frag);
	head = len - frag;
    }
//    debug(DEBUGF_MISC, "inserted %d  head %d tail %d", len, head, tail);
    return(true);
};

bool
RingBuffer::insert(IOMemoryDescriptor *md)
{
    int	frag, len = md->getLength();
    
    if (len > avail()) 
	return(false);	    // space check
    frag = size - head;	    // fill head-to-end first
    if (frag > len)
	frag = len;
    md->readBytes(0, data + head, frag);
    head += frag;
    if (head == size)	    // head never points to end
	head = 0;
    if (len > frag) {	    // more to copy?
	md->readBytes(frag, data, len - frag);
	head = len - frag;
    }
//    debug(DEBUGF_MISC, "inserted %d  head %d tail %d", len, head, tail);
    return(true);
};

bool
RingBuffer::insert(uint8_t one)
{
    return(insert((char *)&one, sizeof(one)));
}

bool
RingBuffer::insert(uint16_t one)
{
    return(insert((char *)&one, sizeof(one)));
}

bool
RingBuffer::insert(uint32_t one)
{
    return(insert((char *)&one, sizeof(one)));
}

int
RingBuffer::remove(char *outdata, int len) 
{
    int	frag, frag2;
    if (head >= tail) {	    // from tail to head
	frag = head - tail;
	if (frag > len)
	    frag = len;
	if (frag > 0)
	    memcpy(outdata, data + tail, frag);
	tail += frag;
//	debug(DEBUGF_MISC, "removed %d  head %d tail %d", frag, head, tail);
	return(frag);
    }
    // from tail to end
    frag = size - tail;
    if (frag > len)
	frag = len;
    memcpy(outdata, data + tail, frag);
    tail += frag;
    if (tail == size)
	tail = 0;
    // maybe wrap
    if (len > frag) {
	frag2 = head;
	if (frag2 > (len - frag))
	    frag2 = (len - frag);
	memcpy(outdata + frag, data, frag2);
	tail = frag2;
	frag += frag2;
    }
//    debug(DEBUGF_MISC, "removed %d  head %d tail %d", frag, head, tail);
    return(frag);
};

int
RingBuffer::remove(IOMemoryDescriptor *md, int offset) 
{
    int	frag, frag2, len = md->getLength() - offset;
    
    if (head >= tail) {	    // from tail to head
	frag = head - tail;
	if (frag > len)
	    frag = len;
	if (frag > 0)
	    md->writeBytes(offset, data + tail, frag);
	tail += frag;
//	debug(DEBUGF_MISC, "removed %d  head %d tail %d", frag, head, tail);
	return(frag);
    }
    // from tail to end
    frag = size - tail;
    if (frag > len)
	frag = len;
    md->writeBytes(offset, data + tail, frag);
    tail += frag;
    if (tail == size)
	tail = 0;
    // maybe wrap
    if (len > frag) {
	frag2 = head;
	if (frag2 > (len - frag))
	    frag2 = (len - frag);
	md->writeBytes(offset + frag, data, frag2);
	tail = frag2;
	frag += frag2;
    }
//    debug(DEBUGF_MISC, "removed %d  head %d tail %d", frag, head, tail);
    return(frag);
};

int
RingBuffer::remove(uint8_t *one)
{
    
    if (remove((char *)one, sizeof(*one)) == sizeof(*one))
	return(1);
    return(-1);
}

int
RingBuffer::remove(uint16_t *one)
{
    
    if (remove((char *)one, sizeof(*one)) == sizeof(*one))
	return(1);
    return(-1);
}

int
RingBuffer::remove(uint32_t *one)
{
    
    if (remove((char *)one, sizeof(*one)) == sizeof(*one))
	return(1);
    return(-1);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Custom EventSource							      //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifdef super
# undef super
#endif
#ifdef self
# undef self
#endif
#define super IOTimerEventSource
#define self ArcMSREventSource

OSDefineMetaClassAndStructors(self, super);

ArcMSREventSource *
self::Create(OSObject *owner, Action action)
{
	ArcMSREventSource	*es;

	es = new ArcMSREventSource;
	if (es != NULL) {
		if (es->init(owner, (IOTimerEventSource::Action)action)) {
			es->actionMask = 0;
			es->handlerRunning = false;
			debug(DEBUGF_MISC, "init async event source");
			return(es);
		}
		es->release();
	}
	return(NULL);
}

void
self::setTimeoutFunc(void)
{
	debug(DEBUGF_MISC, "setting our timeout");
	calloutEntry = (void *)thread_call_allocate((thread_call_func_t)Timeout,
						    (thread_call_param_t)this);
}

void
self::addNotification(uint32_t mask)
{
	closeGate();
	debug(DEBUGF_MISC, "async handler adding notification %x", mask);
	actionMask |= mask;

	// if the handler's not currently running, kick it off shortly
	if (!handlerRunning) {
		debug(DEBUGF_MISC, "async handler not running, kicking it off with flags %x", actionMask);
		handlerRunning = true;
		setTimeoutUS(1);
	}
	openGate();
}

void
self::Timeout(void *owner)
{
	self	*es = (self *)owner;

	debug(DEBUGF_MISC, "async handler %p invoked with actionMask %x", owner, es->actionMask);
	if ((es->enabled = true) && (es->action != NULL))
		es->action(es->owner, es);
}

bool
self::handlerDone(void)
{
	bool	done;

	closeGate();

	if (actionMask == 0) {
		debug(DEBUGF_MISC, "async handler is done");
		handlerRunning = false;
		done = true;
	} else {
		debug(DEBUGF_MISC, "async handler has work to do");
		done = false;
	}
	openGate();
	return(done);
}

bool
self::targetRescan(void)
{
	bool	result;

	closeGate();
	result = actionMask & ARCMSR_ESFLAG_RESCAN;
	if (result)
		actionMask &= ~ARCMSR_ESFLAG_RESCAN;
	openGate();

	debug(DEBUGF_RESCAN, "async handler checking for rescan - %s", result ? "true" : "false");
	return(result);
}
