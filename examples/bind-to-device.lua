--
-- Bind socket to an interface to receive packets only from this
-- particular interface.
--
-- TODO:
-- * Pass interface name on the command line.
-- * Remove assert()'s once syscall wrappers abort on error.
--

local ifname = "veth0"

server_socket(
  function (sockfd)
    local _, err = setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifname, #ifname)
    assert(not err, tostring(err))
  end
)
