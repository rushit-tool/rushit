--
-- Example based on:
-- https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/tools/testing/selftests/net/reuseport_bpf_cpu.c
-- https://github.com/cloudflare/cloudflare-blog/blob/master/2017-11-perfect-locality/setcbpf.stp
--

-- XXX_1: Will be gone. Need to extend ljsyscall. We are missing constants. Ignore for now.
local SKF_AD_OFF              = -0x1000
local SKF_AD_PROTOCOL         = 0
local SKF_AD_PKTTYPE          = 4
local SKF_AD_IFINDEX          = 8
local SKF_AD_NLATTR           = 12
local SKF_AD_NLATTR_NEST      = 16
local SKF_AD_MARK             = 20
local SKF_AD_QUEUE            = 24
local SKF_AD_HATYPE           = 28
local SKF_AD_RXHASH           = 32
local SKF_AD_CPU              = 36
local SKF_AD_ALU_XOR_X        = 40
local SKF_AD_VLAN_TAG         = 44
local SKF_AD_VLAN_TAG_PRESENT = 48
local SKF_AD_PAY_OFFSET       = 52
local SKF_AD_MAX              = 56
local SKF_NET_OFF             = -0x100000
local SKF_LL_OFF              = -0x200000

server_socket(
  function (sockfd)
    -- XXX_2: Will be gone. Need to extend script_prelude.h. Ignore for now.
    local ffi = require("ffi")
    local sys = require("syscall")

    local sizeof = ffi.sizeof
    local sock_filter = sys.types.t.sock_filter
    local sock_filters = sys.types.t.sock_filters
    local sock_fprog1 = sys.types.t.sock_fprog1

    -- XXX_3: Defined here because can't use cdata as upvalue (yet). Will be moved to top-level.
    local group_size = 16
    local code = {
      sock_filter("LD,W,ABS",  SKF_AD_OFF + SKF_AD_CPU), -- A = #cpu
      sock_filter("ALU,MOD,K", tonumber(group_size)),    -- A = A % group_size
      sock_filter("RET,A"),                              -- return A
    }
    local prog = sock_fprog1( {{ #code, sock_filters(#code, code) }}) -- Ugh, ugly.
    assert(prog)

    assert( setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, true) )
    assert( setsockopt(sockfd, SOL_SOCKET, SO_ATTACH_REUSEPORT_CBPF, prog, sizeof(prog)) )
  end
)
