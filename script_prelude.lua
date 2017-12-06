local F = require("ffi")

-- XXX: Temporary global until we have convenience aliases for all
--      symbols from ljsyscall.
S = require("syscall")

-- Mark a local variable for collection by assiging it a special wrapped value.
--
-- Collected locals will be assigned a table of values gathered from all worker
-- Lua states after run() has been called.
function collect(value)
  local collector = { value }
  local collector_mt = { collector = true }

  setmetatable(collector, collector_mt)
  register_collector__(collector)

  return collector;
end

-- XXX: Push to ljsyscall?
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
AF_INET6 = C.AF.INET6
-- ...

SOCK_STREAM = C.SOCK.STREAM
SOCK_DGRAM = C.SOCK.DGRAM
-- ...

IP_TOS                    = C.IP.TOS
IP_TTL                    = C.IP.TTL
IP_HDRINCL                = C.IP.HDRINCL
IP_OPTIONS                = C.IP.OPTIONS
IP_ROUTER_ALERT           = C.IP.ROUTER_ALERT
IP_RECVOPTS               = C.IP.RECVOPTS
IP_RETOPTS                = C.IP.RETOPTS
IP_PKTINFO                = C.IP.PKTINFO
IP_PKTOPTIONS             = C.IP.PKTOPTIONS
IP_MTU_DISCOVER           = C.IP.MTU_DISCOVER
IP_RECVERR                = C.IP.RECVERR
IP_RECVTTL                = C.IP.RECVTTL
IP_RECVTOS                = C.IP.RECVTOS
IP_MTU                    = C.IP.MTU
IP_FREEBIND               = C.IP.FREEBIND
IP_IPSEC_POLICY           = C.IP.IPSEC_POLICY
IP_XFRM_POLICY            = C.IP.XFRM_POLICY
IP_PASSSEC                = C.IP.PASSSEC
IP_TRANSPARENT            = C.IP.TRANSPARENT
IP_ORIGDSTADDR            = C.IP.ORIGDSTADDR
IP_MINTTL                 = C.IP.MINTTL
IP_NODEFRAG               = C.IP.NODEFRAG
IP_MULTICAST_IF           = C.IP.MULTICAST_IF
IP_MULTICAST_TTL          = C.IP.MULTICAST_TTL
IP_MULTICAST_LOOP         = C.IP.MULTICAST_LOOP
IP_ADD_MEMBERSHIP         = C.IP.ADD_MEMBERSHIP
IP_DROP_MEMBERSHIP        = C.IP.DROP_MEMBERSHIP
IP_UNBLOCK_SOURCE         = C.IP.UNBLOCK_SOURCE
IP_BLOCK_SOURCE           = C.IP.BLOCK_SOURCE
IP_ADD_SOURCE_MEMBERSHIP  = C.IP.ADD_SOURCE_MEMBERSHIP
IP_DROP_SOURCE_MEMBERSHIP = C.IP.DROP_SOURCE_MEMBERSHIP
IP_MSFILTER               = C.IP.MSFILTER
IP_MULTICAST_ALL          = C.IP.MULTICAST_ALL
IP_UNICAST_IF             = C.IP.UNICAST_IF

IPPROTO_TCP = C.IPPROTO.TCP
-- ...

-- TODO: Add new constants the replace IPV6_2292* legacy ones
IPV6_ADDRFORM        = C.IPV6.ADDRFORM
IPV6_2292PKTINFO     = C.IPV6["2292PKTINFO"]
IPV6_2292HOPOPTS     = C.IPV6["2292HOPOPTS"]
IPV6_2292DSTOPTS     = C.IPV6["2292DSTOPTS"]
IPV6_2292RTHDR       = C.IPV6["2292RTHDR"]
IPV6_2292PKTOPTIONS  = C.IPV6["2292PKTOPTIONS"]
IPV6_CHECKSUM        = C.IPV6.CHECKSUM
IPV6_2292HOPLIMIT    = C.IPV6["2292HOPLIMIT"]
IPV6_NEXTHOP         = C.IPV6.NEXTHOP
IPV6_AUTHHDR         = C.IPV6.AUTHHDR
IPV6_FLOWINFO        = C.IPV6.FLOWINFO
IPV6_UNICAST_HOPS    = C.IPV6.UNICAST_HOPS
IPV6_MULTICAST_IF    = C.IPV6.MULTICAST_IF
IPV6_MULTICAST_HOPS  = C.IPV6.MULTICAST_HOPS
IPV6_MULTICAST_LOOP  = C.IPV6.MULTICAST_LOOP
IPV6_ADD_MEMBERSHIP  = C.IPV6.ADD_MEMBERSHIP
IPV6_DROP_MEMBERSHIP = C.IPV6.DROP_MEMBERSHIP
IPV6_ROUTER_ALERT    = C.IPV6.ROUTER_ALERT
IPV6_MTU_DISCOVER    = C.IPV6.MTU_DISCOVER
IPV6_MTU             = C.IPV6.MTU
IPV6_RECVERR         = C.IPV6.RECVERR
IPV6_V6ONLY          = C.IPV6.V6ONLY
IPV6_JOIN_ANYCAST    = C.IPV6.JOIN_ANYCAST
IPV6_LEAVE_ANYCAST   = C.IPV6.LEAVE_ANYCAST

MSG_PEEK = C.MSG.PEEK
-- ...

SCM_RIGHTS                 = C.SCM.RIGHTS
SCM_CREDENTIALS            = C.SCM.CREDENTIALS
SCM_TSTAMP_SND             = C.SCM.TSTAMP_SND
SCM_TSTAMP_SCHED           = C.SCM.TSTAMP_SCHED
SCM_TSTAMP_ACK             = C.SCM.TSTAMP_ACK
SCM_TIMESTAMPING_OPT_STATS = C.SCM.TIMESTAMPING_OPT_STATS
SCM_TIMESTAMP              = C.SCM.TIMESTAMP
SCM_TIMESTAMPNS            = C.SCM.TIMESTAMPNS
SCM_TIMESTAMPING           = C.SCM.TIMESTAMPING

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
local T = S.types.t
local PT = S.types.pt

in_addr = T.in_addr
in6_addr = T.in6_addr

sockaddr_in = T.sockaddr_in
sockaddr_storage = T.sockaddr_storage

scm_timestamping_ptr = PT.scm_timestamping

--
-- Functions
--
getsockopt = S.getsockopt
recvfrom = S.recvfrom
recvmsg = S.recvmsg
sendmsg = S.sendmsg
sendto = S.sendto
setsockopt = S.setsockopt
