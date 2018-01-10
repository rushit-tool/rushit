--
-- Test if setting a socket option works.
--

local function socket_hook(sockfd)
  local ok, err = setsockopt(sockfd, SOL_SOCKET, SO_PRIORITY, 0)
  assert(ok, tostring(err))
end

client_socket(socket_hook)
server_socket(socket_hook)
