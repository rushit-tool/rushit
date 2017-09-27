client_socket(
  function (sockfd)
    assert(
      setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING,
                 bit.bor(SOF_TIMESTAMPING_SOFTWARE,
                         SOF_TIMESTAMPING_TX_ACK,
                         SOF_TIMESTAMPING_OPT_TSONLY))
    )
  end
)

client_recverr(
  function (sockfd, msg, flags)
    local n = recvmsg(sockfd, msg, flags)
    assert(n ~= -1)

    for _, cmsg in msg:cmsgs() do
      if cmsg.cmsg_level == SOL_SOCKET and
         cmsg.cmsg_type == SCM_TIMESTAMPING then
        local tss = scm_timestamping(cmsg.cmsg_data)
        local ts = tss.ts[0]
        local usec = (ts.sec * 1000 * 1000)
                   + (ts.nsec / 1000)

        if usec_prev then
          io.stderr:write(usec - usec_prev, "\n")
        end
        usec_prev = usec

      elseif (cmsg.cmsg_level == SOL_IP and
              cmsg.cmsg_type == IP_RECVERR) or
             (cmsg.cmsg_level == SOL_IPV6 and
              cmsg.cmsg_type == IPV6_RECVERR) then
        -- XXX: Check ee_errno & ee_origin
      end
    end

    return n
  end
)
