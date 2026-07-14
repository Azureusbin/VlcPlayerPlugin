#pragma once

THIRD_PARTY_INCLUDES_START


#ifdef _WIN32
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef POLLIN
#define POLLIN 0x0001
#endif
#ifndef POLLOUT
#define POLLOUT 0x0004
#endif
#ifndef POLLERR
#define POLLERR 0x0008
#endif
#include "Windows/HideWindowsPlatformTypes.h"
#endif // _WIN32

// Replace poll with WSAPoll using Winsock2
static inline int poll(struct pollfd *fds, unsigned int nfds, int timeout)
{
	return WSAPoll(fds, nfds, timeout);
}

#include "vlc.h"
#include "libvlc_events.h"
#include "plugins/vlc_common.h"
#include "plugins/vlc_fourcc.h"

THIRD_PARTY_INCLUDES_END

DECLARE_LOG_CATEGORY_EXTERN(LogVlcMedia, Log, All);