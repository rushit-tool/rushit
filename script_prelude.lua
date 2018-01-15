local F = require("ffi")

-- XXX: Temporary global until we have convenience aliases for all
--      symbols from ljsyscall.
S = require("syscall")

-- Mark a local variable for collection by assigning it a special wrapped value.
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
-- Types
--
local T = S.types.t
local PT = S.types.pt

in_addr = T.in_addr
in6_addr = T.in6_addr

sockaddr_in = T.sockaddr_in
sockaddr_storage = T.sockaddr_storage

uint32_ptr = PT.uint32
scm_timestamping_ptr = PT.scm_timestamping

-- Returns a list of (name, function) pairs, sorted by function name, with all
-- system calls (functions) wrapped by ljsyscall.
local function get_sys_funcs(sys)
  local funcs = {}

  for name, func in pairs(sys) do
    if type(func) == "function" then
      table.insert(funcs, { name = name, value = func })
    end
  end

  table.sort(funcs, function (f1, f2) return f1.name < f2.name end)
  return funcs
end

local function fatal(msg)
  io.stderr:write(msg .. "\n")
  os.exit(1)
end

-- ljsyscall deviates from system naming for some constants. Massage the name so
-- it matches the one used in system headers.
local function build_consts_group_prefix(group_name)
  -- Map of ljsyscall-specific prefixes to system standard prefixes
  local system_prefix = {
    AT_FDCWD_         = 'AT_',
    B_                = 'B',
    BPF_CMD_          = 'BPF_',
    BPF_MAP_          = 'BPF_MAP_TYPE_',
    BPF_PROG_         = 'BPF_PROG_TYPE_',
    CC_               = '',
    CFLAG_            = '',
    E_                = 'E',
    EM_CYGNUS_        = 'EM_',
    EPOLL_            = 'EPOLL',
    EPOLLCREATE_      = 'EPOLL_',
    FCNTL_LOCK_       = 'F_',
    IFLAG_            = '',
    IFLA_VF_INFO_     = 'IFLA_VF_',
    IFLA_VF_PORT_     = 'IFLA_VF_',
    IFREQ_            = 'IFF_',
    IN_INIT_          = 'IN_',
    LFLAG_            = '',
    LOCKF_            = 'F_',
    MODE_             = 'S_I',
    MSYNC_            = 'MS_',
    OFLAG_            = '',
    OPIPE_            = 'O_',
    PERF_READ_FORMAT_ = 'PERF_FORMAT_',
    POLL_             = 'POLL',
    PR_MCE_KILL_OPT_  = 'PR_MCE_KILL_',
    SIG_              = 'SIG',
    SIGACT_           = 'SIG_',
    SIGBUS_           = 'BUS_',
    SIGCLD_           = 'CLD_',
    SIGFPE_           = 'FPE_',
    SIGILL_           = 'ILL_',
    SIGPM_            = 'SIG_',
    SIGPOLL_          = 'POLL_',
    SIGSEGV_          = 'SEGV_',
    SIGTRAP_          = 'TRAP_',
    STD_              = 'STD',
    S_I_              = 'S_I',
    TCSA_             = 'TCSA',
    TCFLOW_           = '',
    TCFLUSH_          = '',
    TUN_              = 'IFF_',
    UMOUNT_           = 'MNT_',
    W_                = 'W',
  }

  local p = group_name .. '_'

  if system_prefix[p] then
    p = system_prefix[p]
  end

  return p
end

-- ljsyscall organizes constants into a two-level tree. 1st level are either
-- branch nodes that group constants that share a common prefix, or leaf nodes
-- for constants that don't belong to any group. 2nd level are only leaf nodes
-- that belong to one of the groups.
local function walk_consts_group(group_node, group_prefix)
  -- Tables in ljsyscall we need to skip over
  local skip = {
    errornames = true,
    IOCTL = true, -- TODO: Has different structure then all other groups
  }

  local out = {}

  for name, value in pairs(group_node) do
    if not skip[name] then
      if type(value) == "table" then

        -- Recursively process subgroups and merge results
        local p = build_consts_group_prefix(name)
        local t = walk_consts_group(value, p)
        for _, v in ipairs(t) do
          table.insert(out, v)
        end

      elseif
        type(value) == "number" or -- usual case
        type(value) == "string" or -- for strings from linux/if_bridge.h
        type(value) == "cdata"     -- for RLIM_INFINITY
      then

        -- Process an actual a constant
        name = group_prefix .. name
        table.insert(out, { name=name, value=value })

      else
        fatal("Unhandled constant: name=" .. tostring(k) .. " value=" .. tostring(v) .. " type=" .. type(v))
      end
    end
  end

  return out
end

-- Returns a list of (name, value) all functions wrapped by ljsyscall.
-- Constants names are sanitized to match kernel headers.
local function get_sys_consts(sys)
  local consts = walk_consts_group(sys.c, "")
  table.sort(consts, function (c1, c2) return c1.name < c2.name end)
  return consts
end

-- Pull in all (name, value) pairs into a global namespace
local function import(syms)
  for _, s in pairs(syms) do
    _G[s.name] = s.value
  end
end

local sys_funcs = get_sys_funcs(S)
local sys_consts = get_sys_consts(S)

import(sys_funcs)
import(sys_consts)

local function list_syms(syms)
  for _, s in ipairs(syms) do
    print(s.name)
  end
end

function list_sys_funcs()
  list_syms(sys_funcs)
end

function list_sys_consts()
  list_syms(sys_consts)
end
