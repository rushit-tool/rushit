function socket_hook()
  local S = require('syscall')
  local val, err = S.getsockopt(-1, S.c.SOL.SOCKET, S.c.SO.TYPE)
  assert(not val and err.BADF)
end

client_socket(socket_hook)
server_socket(socket_hook)
