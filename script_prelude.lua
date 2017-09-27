local F = require("ffi")
local S = require("syscall")

F.cdef[[
struct addrinfo {
        int              ai_flags;
        int              ai_family;
        int              ai_socktype;
        int              ai_protocol;
        socklen_t        ai_addrlen;
        struct sockaddr *ai_addr;
        char            *ai_canonname;
        struct addrinfo *ai_next;
};
]]

--
-- Constants
--
local C = S.c

AF_INET = C.AF.INET
-- ...

SOCK_STREAM = C.SOCK.STREAM
-- ...

IPPROTO_TCP = C.IPPROTO.TCP
-- ...

MSG_PEEK = C.MSG.PEEK
-- ...

SOL_SOCKET = C.SOL.SOCKET
SOL_IP     = C.SOL.IP
SOL_IPV6   = C.SOL.IPV6
SOL_ICMPV6 = C.SOL.ICMPV6
SOL_RAW    = C.SOL.RAW
SOL_DECNET = C.SOL.DECNET
SOL_X25    = C.SOL.X25
SOL_PACKET = C.SOL.PACKET
SOL_ATM    = C.SOL.ATM
SOL_AAL    = C.SOL.AAL
SOL_IRDA   = C.SOL.IRDA

SO_DEBUG                         = C.SO.DEBUG
SO_REUSEADDR                     = C.SO.REUSEADDR
SO_TYPE                          = C.SO.TYPE
SO_ERROR                         = C.SO.ERROR
SO_DONTROUTE                     = C.SO.DONTROUTE
SO_BROADCAST                     = C.SO.BROADCAST
SO_SNDBUF                        = C.SO.SNDBUF
SO_RCVBUF                        = C.SO.RCVBUF
SO_KEEPALIVE                     = C.SO.KEEPALIVE
SO_OOBINLINE                     = C.SO.OOBINLINE
SO_NO_CHECK                      = C.SO.NO_CHECK
SO_PRIORITY                      = C.SO.PRIORITY
SO_LINGER                        = C.SO.LINGER
SO_BSDCOMPAT                     = C.SO.BSDCOMPAT
SO_REUSEPORT                     = C.SO.REUSEPORT
SO_PASSCRED                      = C.SO.PASSCRED
SO_PEERCRED                      = C.SO.PEERCRED
SO_RCVLOWAT                      = C.SO.RCVLOWAT
SO_SNDLOWAT                      = C.SO.SNDLOWAT
SO_RCVTIMEO                      = C.SO.RCVTIMEO
SO_SNDTIMEO                      = C.SO.SNDTIMEO
SO_SECURITY_AUTHENTICATION       = C.SO.SECURITY_AUTHENTICATION
SO_SECURITY_ENCRYPTION_TRANSPORT = C.SO.SECURITY_ENCRYPTION_TRANSPORT
SO_SECURITY_ENCRYPTION_NETWORK   = C.SO.SECURITY_ENCRYPTION_NETWORK
SO_BINDTODEVICE                  = C.SO.BINDTODEVICE
SO_ATTACH_FILTER                 = C.SO.ATTACH_FILTER
SO_DETACH_FILTER                 = C.SO.DETACH_FILTER
SO_PEERNAME                      = C.SO.PEERNAME
SO_TIMESTAMP                     = C.SO.TIMESTAMP
SO_ACCEPTCONN                    = C.SO.ACCEPTCONN
SO_PEERSEC                       = C.SO.PEERSEC
SO_SNDBUFFORCE                   = C.SO.SNDBUFFORCE
SO_RCVBUFFORCE                   = C.SO.RCVBUFFORCE
SO_PASSSEC                       = C.SO.PASSSEC
SO_TIMESTAMPNS                   = C.SO.TIMESTAMPNS
SO_MARK                          = C.SO.MARK
SO_TIMESTAMPING                  = C.SO.TIMESTAMPING
SO_PROTOCOL                      = C.SO.PROTOCOL
SO_DOMAIN                        = C.SO.DOMAIN
SO_RXQ_OVFL                      = C.SO.RXQ_OVFL
SO_WIFI_STATUS                   = C.SO.WIFI_STATUS
SO_PEEK_OFF                      = C.SO.PEEK_OFF
SO_NOFCS                         = C.SO.NOFCS
SO_LOCK_FILTER                   = C.SO.LOCK_FILTER
SO_SELECT_ERR_QUEUE              = C.SO.SELECT_ERR_QUEUE
SO_BUSY_POLL                     = C.SO.BUSY_POLL
SO_MAX_PACING_RATE               = C.SO.MAX_PACING_RATE
SO_BPF_EXTENSIONS                = C.SO.BPF_EXTENSIONS
SO_INCOMING_CPU                  = C.SO.INCOMING_CPU
SO_ATTACH_BPF                    = C.SO.ATTACH_BPF
SO_ATTACH_REUSEPORT_CBPF         = C.SO.ATTACH_REUSEPORT_CBPF
SO_ATTACH_REUSEPORT_EBPF         = C.SO.ATTACH_REUSEPORT_EBPF

SOF_TIMESTAMPING_TX_HARDWARE  = C.SOF.TIMESTAMPING_TX_HARDWARE
SOF_TIMESTAMPING_TX_SOFTWARE  = C.SOF.TIMESTAMPING_TX_SOFTWARE
SOF_TIMESTAMPING_RX_HARDWARE  = C.SOF.TIMESTAMPING_RX_HARDWARE
SOF_TIMESTAMPING_RX_SOFTWARE  = C.SOF.TIMESTAMPING_RX_SOFTWARE
SOF_TIMESTAMPING_SOFTWARE     = C.SOF.TIMESTAMPING_SOFTWARE
SOF_TIMESTAMPING_SYS_HARDWARE = C.SOF.TIMESTAMPING_SYS_HARDWARE
SOF_TIMESTAMPING_RAW_HARDWARE = C.SOF.TIMESTAMPING_RAW_HARDWARE
SOF_TIMESTAMPING_OPT_ID       = C.SOF.TIMESTAMPING_OPT_ID
SOF_TIMESTAMPING_TX_SCHED     = C.SOF.TIMESTAMPING_TX_SCHED
SOF_TIMESTAMPING_TX_ACK       = C.SOF.TIMESTAMPING_TX_ACK
SOF_TIMESTAMPING_OPT_CMSG     = C.SOF.TIMESTAMPING_OPT_CMSG
SOF_TIMESTAMPING_OPT_TSONLY   = C.SOF.TIMESTAMPING_OPT_TSONLY
SOF_TIMESTAMPING_OPT_STATS    = C.SOF.TIMESTAMPING_OPT_STATS
SOF_TIMESTAMPING_OPT_PKTINFO  = C.SOF.TIMESTAMPING_OPT_PKTINFO
SOF_TIMESTAMPING_OPT_TX_SWHW  = C.SOF.TIMESTAMPING_OPT_TX_SWHW

--
-- Types
--
local T = S.t

sockaddr_in = T.sockaddr_in

--
-- Functions
--
getsockopt = S.getsockopt
recvmsg = S.recvmsg
sendmsg = S.sendmsg
