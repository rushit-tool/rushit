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

SO_TYPE = C.SO.TYPE
-- ...

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
