client_socket(
  function (sockfd, ai)
    local val, err = getsockopt(sockfd, SOL_SOCKET, SO_TYPE)
    assert(val == SOCK_STREAM, 'Expected ' .. SOCK_STREAM .. ', got ' .. tostring(val))
  end
)
client_close(
  function (sockfd, ai)
    local val, err = getsockopt(sockfd, SOL_SOCKET, SO_TYPE)
    -- XXX: Broken, see run_client() -> script_slave_close_hook()
    -- assert(val == SOCK_STREAM, 'Expected ' .. SOCK_STREAM .. ', got ' .. tostring(val))
  end
)

server_socket(
  function (sockfd, ai)
    local val, err = getsockopt(sockfd, SOL_SOCKET, SO_TYPE)
    print('socket: getsockopt:', tostring(val))
    assert(val == SOCK_STREAM, 'Expected ' .. SOCK_STREAM .. ', got ' .. tostring(val))
  end
)
server_close(
  function (sockfd, ai)
    local val, err = getsockopt(sockfd, SOL_SOCKET, SO_TYPE)
    assert(val == SOCK_STREAM, 'Expected ' .. SOCK_STREAM .. ', got ' .. tostring(val))
  end
)
