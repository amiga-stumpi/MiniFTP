#ifndef AMITCP13_SOCKET_LIMITS_H
#define AMITCP13_SOCKET_LIMITS_H

/*
 * Fixed socket pool sizing for AmigaOS 1.3 builds.
 * Keep these conservative and static; dynamic allocation is intentionally
 * deferred to avoid fragmentation and harder leak debugging on real hardware.
 */
#ifndef AMITCP13_MAX_FDS
#define AMITCP13_MAX_FDS 16
#endif

#ifndef AMITCP13_MAX_TCP_SOCKETS
#define AMITCP13_MAX_TCP_SOCKETS 12
#endif

#ifndef AMITCP13_MAX_UDP_SOCKETS
#define AMITCP13_MAX_UDP_SOCKETS 8
#endif

#ifndef AMITCP13_MAX_RAW_SOCKETS
#define AMITCP13_MAX_RAW_SOCKETS 4
#endif

#ifndef AMITCP13_MAX_SELECT_FDS
#define AMITCP13_MAX_SELECT_FDS AMITCP13_MAX_FDS
#endif

#ifndef AMITCP13_BSDSOCKET_LIBRARY_FD_SETSIZE
#define AMITCP13_BSDSOCKET_LIBRARY_FD_SETSIZE 32
#endif

#ifndef AMITCP13_SOCKET_CLEANUP_DEBUG
#define AMITCP13_SOCKET_CLEANUP_DEBUG 0
#endif

#ifndef AMITCP13_SOCKET_POOL_DEBUG
#define AMITCP13_SOCKET_POOL_DEBUG 0
#endif

#if AMITCP13_SOCKET_CLEANUP_DEBUG
#undef AMITCP13_SOCKET_POOL_DEBUG
#define AMITCP13_SOCKET_POOL_DEBUG 1
#endif

#endif
