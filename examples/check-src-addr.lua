--
-- Verify that we are receving packets only from a certian address.
--
-- TODO:
-- * Pass client address on the command line.
--

-- The source (client) address we expect
local SRC_ADDR = "127.0.0.1"

server_socket(
  function(sockfd, ai)
    if ai.ai_family ~= AF_INET then
      print("This script works only with IPv4! Please use -4 option.")
      os.exit(1)
    end
  end
)

server_recvmsg(
  function (sockfd, msg, flags)
    -- XXX: Accessing cdata objects from worker threads doesn't work
    -- yet. That's why we can't create in_addr in top level context.
    --
    local src_addr = in_addr(SRC_ADDR)

    -- server_recvmg() hook gets a struct msghdr as an argument, which
    -- we can pass directly to recvmsg(). If we want to call
    -- recvfrom() instead then we need to extract the buffer pointer
    -- and buffer length from struct msghdr.
    --
    -- For reference from recvmsg(2) man page:
    --
    -- struct msghdr {
    --     void         *msg_name;       /* optional address */
    --     socklen_t     msg_namelen;    /* size of address */
    --     struct iovec *msg_iov;        /* scatter/gather array */
    --     size_t        msg_iovlen;     /* # elements in msg_iov */
    --     void         *msg_control;    /* ancillary data, see below */
    --     size_t        msg_controllen; /* ancillary data buffer len */
    --     intmsg_flags;      /* flags on received message */
    -- };
    --
    -- struct iovec {         /* Scatter/gather array items */
    --     void  *iov_base;   /* Starting address */
    --     size_t iov_len;    /* Number of bytes to transfer */
    -- };
    --
    local iov = msg.msg_iov[0]
    local buf = iov.iov_base
    local len = iov.iov_len
    local sin = sockaddr_in()

    local n = recvfrom(sockfd, buf, len, flags, sin)
    assert(n)

    if sin.sin_addr.s_addr ~= src_addr.s_addr then
      print("Wrong source address! " ..
            "Expected " .. tostring(src_addr) ..
            ", but got " .. tostring(sin.sin_addr))
      os.exit(1)
    end

    return n
  end
)
