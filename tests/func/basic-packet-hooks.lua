client_sendmsg(
  function (sockfd, msg, flags)
    return sendmsg(sockfd, msg, flags)
  end
)
client_recvmsg(
  function (sockfd, msg, flags)
    return recvmsg(sockfd, msg, flags)
  end
)

server_sendmsg(
  function (sockfd, msg, flags)
    return sendmsg(sockfd, msg, flags)
  end
)
server_recvmsg(
  function (sockfd, msg, flags)
    return recvmsg(sockfd, msg, flags)
  end
)
