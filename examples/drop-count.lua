--
-- Demonstration of collecting per socket packet drop counts
-- statistics reported by the network stack.
--

local stats = collect({
    -- Received packets
    recv_pkts = 0,
    -- Dropped packets
    drop_pkts = 0,
})

server_socket(
  function (sockfd)
    -- XXX: Make setsockopt() wrapper abort or error by default
    local ok, err = setsockopt(sockfd, SOL_SOCKET, SO_RXQ_OVFL, 1)
    assert(ok, tostring(err))
  end
)

server_recvmsg(
  function (sockfd, msg, flags)
    local n_recv, err = recvmsg(sockfd, msg, flags)
    assert(n_recv, tostring(err))

    stats.recv_pkts = stats.recv_pkts + 1

    local _, cmsg = msg:cmsg_firsthdr()
    if cmsg then
      if cmsg.cmsg_level == SOL_SOCKET and cmsg.cmsg_type == SO_RXQ_OVFL then
        local n_drop = tonumber(uint32_ptr(cmsg.cmsg_data)[0])

        -- Uncomment to see packet drops being reported
        -- if n_drop > drop_pkts then
        --   io.stderr:write('.')
        -- end

        stats.drop_pkts = n_drop
      end
    end

    return n_recv
  end
)

run()

print()
print(string.format("%10s %12s %12s", "", "Received", "Dropped"))
print(string.format("%10s %12s %12s", "", "(packets)", "(packets)"))

local total = {
  recv_pkts = 0,
  drop_pkts = 0,
}

for i = 1, #stats do
  print(string.format("%10s %12d %12d",
                      string.format("Thread-%02d:", i),
                      stats[i].recv_pkts, stats[i].drop_pkts))

  total.recv_pkts = total.recv_pkts + stats[i].recv_pkts
  total.drop_pkts = total.drop_pkts + stats[i].drop_pkts
end

print(string.format("%10s %12d %12d",
                    "Total:", total.recv_pkts, total.drop_pkts))
print()
