#include <sys/types.h>
#include <sys/signal.h>
#include <sys/select.h>
#include <curses.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <setjmp.h>
#include <pthread.h>
#include <machine/endian.h>
#include <machine/byte_order.h>
#include <ApplicationServices/ApplicationServices.h>

#include "../ArcMSRUserClientInterface.h"

#define MAXTRANSIZE		1031

io_connect_t	connectionPort;

pthread_t reader_thread;
int sig_exit = 0;
int reader_should_quit = 0;

void
init(void)
{
	kern_return_t	kernResult;
	mach_port_t	masterPort;
	io_service_t	serviceObject;
	io_iterator_t	iterator;
	CFDictionaryRef	classToMatch;

	// Get the IOKit master port
	if (IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS)
		errx(1, "IOMasterPort failed");

	// Create a matching dictionary for the driver class
	if ((classToMatch = IOServiceMatching("ArcMSR")) == NULL)
		errx(1, "IOServiceMatching failed (no controller found?)");

	// Iterate over the matching instances, consumes dictionary
	if ((IOServiceGetMatchingServices(masterPort, classToMatch, &iterator)) != KERN_SUCCESS)
		errx(1, "IOServiceGetMatchingServices failed");
	serviceObject = IOIteratorNext(iterator);
	if (serviceObject == 0)
		errx(1, "Controller not found");

        // instantiate the UserClient and release the service object
	kernResult = IOServiceOpen(serviceObject, mach_task_self(), 0, &connectionPort);
	IOObjectRelease(serviceObject);
	if (kernResult != KERN_SUCCESS)
		errx(1, "IOServiceOpen failed");

	// open the controller
	kernResult = IOConnectMethodScalarIScalarO(connectionPort,
						   kArcMSRUserClientOpen,
						   0,
						   0);
	if (kernResult != KERN_SUCCESS)
		errx(1, "controller is already in use");

}

void
deinit(void)
{
	IOConnectMethodScalarIScalarO(connectionPort, kArcMSRUserClientClose, 0, 0);
	IOObjectRelease(connectionPort);
}

void
arc_send(const char *outBuf, int outLen)
{
	ArcMSRUserCommand	cmd;
	kern_return_t		kernResult;
	IOByteCount		outSize;
	const char		*ptr;
	int			resid;

	ptr = outBuf;
	resid = outLen;

	// build outbound data descriptor
	cmd.data_buffer = (vm_address_t)ptr;
	cmd.data_size = resid;
	if (cmd.data_size > MAXTRANSIZE)
		cmd.data_size = MAXTRANSIZE;
		
	outSize = sizeof(cmd);
	kernResult = IOConnectMethodStructureIStructureO(connectionPort,
	    kArcMSRUserClientSend,
	    sizeof(cmd),
	    &outSize,
	    &cmd,
	    &cmd);
}

int
arc_recv(char *inBuf, int inLen)
{
	ArcMSRUserCommand	cmd;
	kern_return_t		kernResult;
	IOByteCount		outSize;

	cmd.data_buffer = (vm_address_t)inBuf;
	cmd.data_size = inLen;
	cmd.timeout = 500;		// 1/2 second
	outSize = sizeof(cmd);
	kernResult = IOConnectMethodStructureIStructureO(connectionPort,
	    kArcMSRUserClientRecv,
	    sizeof(cmd),
	    &outSize,
	    &cmd,
	    &cmd);
	if (kernResult != KERN_SUCCESS)
		return(-1);
	return(cmd.data_size);
}


void
sigint(int sig)
{
	sig_exit = 1;
}

void
screen_exit(void)
{
	redrawwin(stdscr);
	refresh();
	echo();
	nocbreak();
	endwin();	
}

void *
reader(void *junk)
{
	char	buf[2048];
	int	got;
	
	while(!reader_should_quit) {
		/* look for card output */
		got = arc_recv(buf, sizeof(buf));

		/* if we got something, output it */
		if (got > 0)
			write(1, buf, got);
	}
	return(NULL);
}

void
reader_exit(void)
{
	reader_should_quit = 1;

	pthread_join(reader_thread, NULL);
}

int
main(int argc, char *argv[])
{
	char	buf[8];
	int	got, ret;
	fd_set	readfd;

	/* bring the card interface up */
	init();
	atexit(deinit);

	/* minimal screen setup */
	initscr();
	cbreak();
	noecho();
	clear();
	refresh();
	atexit(screen_exit);

	/* start the reader thread */
	if (pthread_create(&reader_thread, NULL, reader, NULL))
		errx(1, "failed to create reader thread");
	atexit(reader_exit);

	/* send a screen refresh command */
	buf[0] = 'x';
	arc_send(buf, 1);
	
	signal(SIGINT, sigint);

	for (;;) {
		/* wait for input */
		FD_ZERO(&readfd);
		FD_SET(0, &readfd);
		ret = select(1, &readfd, NULL, NULL, NULL);
		if (ret < 0)
			break;
		if (ret > 0) {
			if (FD_ISSET(0, &readfd)) {
				got = read(0, buf, sizeof(buf));
				if (got < 0)
					err(1, "read error");
				if (got == 0)
					exit(0);
				arc_send(buf, got);
			} else {
				errx(1, "bogus select response");
			}
		}
		if (sig_exit)
			break;
		
	}
	return(0);
}
