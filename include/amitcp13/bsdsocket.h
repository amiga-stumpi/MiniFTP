#ifndef AMITCP13_BSDSOCKET_H
#define AMITCP13_BSDSOCKET_H

#include <exec/types.h>

#include "amitcp13/socket_limits.h"

#define AMITCP13_AF_INET      2
#define AMITCP13_PF_INET      AMITCP13_AF_INET

#define AMITCP13_SOCK_STREAM  1
#define AMITCP13_SOCK_DGRAM   2
#define AMITCP13_SOCK_RAW     3

#define AMITCP13_IPPROTO_IP   0
#define AMITCP13_IPPROTO_ICMP 1
#define AMITCP13_IPPROTO_TCP  6
#define AMITCP13_IPPROTO_UDP  17

#define AMITCP13_INADDR_ANY   0x00000000UL

#define AMITCP13_EPERM        1
#define AMITCP13_ENOENT       2
#define AMITCP13_EINTR        4
#define AMITCP13_EIO          5
#define AMITCP13_EBADF        9
#define AMITCP13_EAGAIN       11
#define AMITCP13_EWOULDBLOCK  AMITCP13_EAGAIN
#define AMITCP13_ENOMEM       12
#define AMITCP13_EACCES       13
#define AMITCP13_EFAULT       14
#define AMITCP13_EINVAL       22
#define AMITCP13_ENFILE       23
#define AMITCP13_EMFILE       24
#define AMITCP13_ENOTSOCK     38
#define AMITCP13_EDESTADDRREQ 39
#define AMITCP13_EMSGSIZE     40
#define AMITCP13_EPROTOTYPE   41
#define AMITCP13_ENOPROTOOPT  42
#define AMITCP13_EPROTONOSUPPORT 43
#define AMITCP13_ESOCKTNOSUPPORT 44
#define AMITCP13_EOPNOTSUPP   45
#define AMITCP13_EAFNOSUPPORT 47
#define AMITCP13_EADDRINUSE   48
#define AMITCP13_EADDRNOTAVAIL 49
#define AMITCP13_ENETDOWN     50
#define AMITCP13_ENETUNREACH  51
#define AMITCP13_EINPROGRESS  36
#define AMITCP13_EALREADY     37
#define AMITCP13_ECONNABORTED 53
#define AMITCP13_ECONNRESET   54
#define AMITCP13_ENOBUFS      55
#define AMITCP13_EISCONN      56
#define AMITCP13_ENOTCONN     57
#define AMITCP13_ESHUTDOWN    58
#define AMITCP13_ETIMEDOUT    60
#define AMITCP13_ECONNREFUSED 61

#define AMITCP13_SOL_SOCKET   0xffff
#define AMITCP13_SO_SNDBUF    0x1001
#define AMITCP13_SO_RCVBUF    0x1002
#define AMITCP13_SO_ERROR     0x1007

#define AMITCP13_F_GETFL      3
#define AMITCP13_F_SETFL      4
#define AMITCP13_O_NONBLOCK   0x0004
#define AMITCP13_FIONBIO      0x8004667eUL

#define AMITCP13_MSG_DONTWAIT 0x0040

#define AMITCP13_SHUT_RD      0
#define AMITCP13_SHUT_WR      1
#define AMITCP13_SHUT_RDWR    2

#define AMITCP13_BSD_FD_SETSIZE AMITCP13_BSDSOCKET_LIBRARY_FD_SETSIZE

#ifndef HOST_NOT_FOUND
#define HOST_NOT_FOUND 1
#endif
#ifndef TRY_AGAIN
#define TRY_AGAIN      2
#endif
#ifndef NO_RECOVERY
#define NO_RECOVERY    3
#endif
#ifndef NO_DATA
#define NO_DATA        4
#endif

#ifndef IPPROTO_IP
#define IPPROTO_IP   AMITCP13_IPPROTO_IP
#endif
#ifndef IPPROTO_ICMP
#define IPPROTO_ICMP AMITCP13_IPPROTO_ICMP
#endif
#ifndef IPPROTO_TCP
#define IPPROTO_TCP  AMITCP13_IPPROTO_TCP
#endif
#ifndef IPPROTO_UDP
#define IPPROTO_UDP  AMITCP13_IPPROTO_UDP
#endif
#ifndef SOCK_RAW
#define SOCK_RAW AMITCP13_SOCK_RAW
#endif

#define AMITCP13_SBTB_CODE 1
#define AMITCP13_SBTS_CODE 0x3fff
#define AMITCP13_SBTF_SET  0x0001UL
#define AMITCP13_SBTF_REF  0x8000UL
#define AMITCP13_TAG_USER  0x80000000UL
#define AMITCP13_SBTM_CODE(tag) \
    (((UWORD)(tag) >> AMITCP13_SBTB_CODE) & AMITCP13_SBTS_CODE)
#define AMITCP13_SBTM_GETREF(code) \
    (AMITCP13_TAG_USER | AMITCP13_SBTF_REF | (((code) & AMITCP13_SBTS_CODE) << AMITCP13_SBTB_CODE))
#define AMITCP13_SBTM_GETVAL(code) \
    (AMITCP13_TAG_USER | (((code) & AMITCP13_SBTS_CODE) << AMITCP13_SBTB_CODE))
#define AMITCP13_SBTM_SETREF(code) \
    (AMITCP13_TAG_USER | AMITCP13_SBTF_REF | (((code) & AMITCP13_SBTS_CODE) << AMITCP13_SBTB_CODE) | AMITCP13_SBTF_SET)
#define AMITCP13_SBTM_SETVAL(code) \
    (AMITCP13_TAG_USER | (((code) & AMITCP13_SBTS_CODE) << AMITCP13_SBTB_CODE) | AMITCP13_SBTF_SET)

#define AMITCP13_SBTC_ERRNO         6
#define AMITCP13_SBTC_HERRNO        7
#define AMITCP13_SBTC_DTABLESIZE    8
#define AMITCP13_SBTC_ERRNOBYTEPTR 21
#define AMITCP13_SBTC_ERRNOWORDPTR 22
#define AMITCP13_SBTC_ERRNOLONGPTR 24
#define AMITCP13_SBTC_HERRNOLONGPTR 25

#define AMITCP13_BSDSOCKET_LVO_SOCKET              -30
#define AMITCP13_BSDSOCKET_LVO_BIND                -36
#define AMITCP13_BSDSOCKET_LVO_LISTEN              -42
#define AMITCP13_BSDSOCKET_LVO_ACCEPT              -48
#define AMITCP13_BSDSOCKET_LVO_CONNECT             -54
#define AMITCP13_BSDSOCKET_LVO_SENDTO              -60
#define AMITCP13_BSDSOCKET_LVO_SEND                -66
#define AMITCP13_BSDSOCKET_LVO_RECVFROM            -72
#define AMITCP13_BSDSOCKET_LVO_RECV                -78
#define AMITCP13_BSDSOCKET_LVO_SHUTDOWN            -84
#define AMITCP13_BSDSOCKET_LVO_SETSOCKOPT          -90
#define AMITCP13_BSDSOCKET_LVO_GETSOCKOPT          -96
#define AMITCP13_BSDSOCKET_LVO_GETSOCKNAME        -102
#define AMITCP13_BSDSOCKET_LVO_GETPEERNAME        -108
#define AMITCP13_BSDSOCKET_LVO_IOCTL              -114
#define AMITCP13_BSDSOCKET_LVO_CLOSESOCKET        -120
#define AMITCP13_BSDSOCKET_LVO_WAITSELECT         -126
#define AMITCP13_BSDSOCKET_LVO_SETSOCKETSIGNALS   -132
#define AMITCP13_BSDSOCKET_LVO_GETDTABLESIZE      -138
#define AMITCP13_BSDSOCKET_LVO_OBTAINSOCKET       -144
#define AMITCP13_BSDSOCKET_LVO_RELEASESOCKET      -150
#define AMITCP13_BSDSOCKET_LVO_RELEASECOPY        -156
#define AMITCP13_BSDSOCKET_LVO_ERRNO              -162
#define AMITCP13_BSDSOCKET_LVO_SETERRNOPTR        -168
#define AMITCP13_BSDSOCKET_LVO_INET_NTOA          -174
#define AMITCP13_BSDSOCKET_LVO_INET_ADDR          -180
#define AMITCP13_BSDSOCKET_LVO_INET_LNAOF         -186
#define AMITCP13_BSDSOCKET_LVO_INET_NETOF         -192
#define AMITCP13_BSDSOCKET_LVO_INET_MAKEADDR      -198
#define AMITCP13_BSDSOCKET_LVO_INET_NETWORK       -204
#define AMITCP13_BSDSOCKET_LVO_GETHOSTBYNAME      -210
#define AMITCP13_BSDSOCKET_LVO_GETHOSTBYADDR      -216
#define AMITCP13_BSDSOCKET_LVO_GETNETBYNAME       -222
#define AMITCP13_BSDSOCKET_LVO_GETNETBYADDR       -228
#define AMITCP13_BSDSOCKET_LVO_GETSERVBYNAME      -234
#define AMITCP13_BSDSOCKET_LVO_GETSERVBYPORT      -240
#define AMITCP13_BSDSOCKET_LVO_GETPROTOBYNAME     -246
#define AMITCP13_BSDSOCKET_LVO_GETPROTOBYNUMBER   -252
#define AMITCP13_BSDSOCKET_LVO_SYSLOGA            -258
#define AMITCP13_BSDSOCKET_LVO_DUP2SOCKET         -264
#define AMITCP13_BSDSOCKET_LVO_SENDMSG            -270
#define AMITCP13_BSDSOCKET_LVO_RECVMSG            -276
#define AMITCP13_BSDSOCKET_LVO_GETHOSTNAME        -282
#define AMITCP13_BSDSOCKET_LVO_GETHOSTID          -288
#define AMITCP13_BSDSOCKET_LVO_SOCKETBASETAGLIST  -294

struct Amitcp13BsdFdSet
{
    ULONG bits;
};

struct Amitcp13BsdTimeVal
{
    LONG tv_sec;
    LONG tv_usec;
};

struct hostent
{
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

struct protoent
{
    char *p_name;
    char **p_aliases;
    int p_proto;
};

struct servent
{
    char *s_name;
    char **s_aliases;
    int s_port;
    char *s_proto;
};

struct Amitcp13BsdTagItem
{
    ULONG ti_Tag;
    ULONG ti_Data;
};

#define AMITCP13_BSD_FD_ZERO(setp) do { (setp)->bits = 0; } while (0)
#define AMITCP13_BSD_FD_SET(fd,setp) do { if ((fd) >= 0 && (fd) < 32) (setp)->bits |= (1UL << (fd)); } while (0)
#define AMITCP13_BSD_FD_CLR(fd,setp) do { if ((fd) >= 0 && (fd) < 32) (setp)->bits &= ~(1UL << (fd)); } while (0)
#define AMITCP13_BSD_FD_ISSET(fd,setp) ((fd) >= 0 && (fd) < 32 && ((setp)->bits & (1UL << (fd))))

struct Amitcp13BsdInAddr
{
    ULONG s_addr;
};

struct Amitcp13BsdSockAddr
{
    UBYTE sa_len;
    UBYTE sa_family;
    char sa_data[14];
};

struct Amitcp13BsdSockAddrIn
{
    UBYTE sin_len;
    UBYTE sin_family;
    UWORD sin_port;
    struct Amitcp13BsdInAddr sin_addr;
    char sin_zero[8];
};

void amitcp13_bsd_init(void);
void amitcp13_bsd_shutdown(void);

int amitcp13_bsd_socket(int domain, int type, int protocol);
int amitcp13_bsd_close(int fd);
int amitcp13_bsd_bind(int fd, const struct Amitcp13BsdSockAddr *addr, int addrlen);
int amitcp13_bsd_connect(int fd, const struct Amitcp13BsdSockAddr *addr, int addrlen);
int amitcp13_bsd_send(int fd, const void *buf, int len, int flags);
int amitcp13_bsd_recv(int fd, void *buf, int len, int flags);
int amitcp13_bsd_sendto(int fd, const void *buf, int len, int flags,
                        const struct Amitcp13BsdSockAddr *to, int tolen);
int amitcp13_bsd_recvfrom(int fd, void *buf, int len, int flags,
                          struct Amitcp13BsdSockAddr *from, int *fromlen);
int amitcp13_bsd_listen(int fd, int backlog);
int amitcp13_bsd_accept(int fd, struct Amitcp13BsdSockAddr *addr, int *addrlen);
int amitcp13_bsd_shutdown_socket(int fd, int how);
int amitcp13_bsd_select(int nfds,
                        struct Amitcp13BsdFdSet *readfds,
                        struct Amitcp13BsdFdSet *writefds,
                        struct Amitcp13BsdFdSet *exceptfds,
                        const struct Amitcp13BsdTimeVal *timeout);
int amitcp13_bsd_wait_select(int nfds,
                             struct Amitcp13BsdFdSet *readfds,
                             struct Amitcp13BsdFdSet *writefds,
                             struct Amitcp13BsdFdSet *exceptfds,
                             const struct Amitcp13BsdTimeVal *timeout,
                             ULONG *signals);
int amitcp13_bsd_getsockopt(int fd, int level, int optname, void *optval, int *optlen);
int amitcp13_bsd_fcntl(int fd, int cmd, int arg);
int amitcp13_bsd_ioctl(int fd, ULONG request, void *argp);

int amitcp13_bsd_errno(void);
void amitcp13_bsd_set_errno_ptr(int *errno_ptr, int size);

#endif
