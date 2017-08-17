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


--
-- Types
--
local T = S.t

sockaddr_in = T.sockaddr_in
