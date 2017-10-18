--
-- Measure time between ACKs and display the delay distribution.
--
-- TODO:
-- * Extend to handle multiple flows per client. Requires a valid
--   socket descriptor in _close() hoo (currently broken) to
--   differentiate between histograms (one per flow).
-- * Extend to handle multiple clients (threads). Requires either a
--   synchronization mechanism between threads to serialize histogram
--   printing, or a data passing mechanism between worker and main
--   threads.
-- * Get rid of open coded histogram generation. Introduce helpers.
--

client_socket(
  function (sockfd)
    assert(
      setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING,
                 bit.bor(SOF_TIMESTAMPING_SOFTWARE,
                         SOF_TIMESTAMPING_TX_ACK,
                         SOF_TIMESTAMPING_OPT_TSONLY))
    )

    hist = {}
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
        local ts_us = (ts.sec * 1000 * 1000)
                    + (ts.nsec / 1000)

        if prev_ts_us then
          local ival_us = ts_us - prev_ts_us
          local upper_us = 1
          local i = 0

          while ival_us >= upper_us do
            upper_us = upper_us * 2
            i = i + 1
          end

          hist[i] = (hist[i] or 0) + 1
        end

        prev_ts_us = ts_us

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

local function bar(val, max, width)
  if max == 0 then return "" end

  local s = ""
  local i = 0
  while i < (width * val / max) do
    s = s .. "="
    i = i + 1
  end
  return s
end

client_close(
  function (sockfd)
    local max = 0
    for _, v in ipairs(hist) do
      if v > max then max = v end
    end

    print()
    print(string.format("%10s .. %-10s: %-10s |%-40s|",
                        ">=", "< [us]", "Count", "Distribution"))
    print()

    local lower_us = 0
    local upper_us = 1
    for _, v in ipairs(hist) do
      print(string.format("%10d -> %-10d: %-10d |%-40s|",
                          lower_us, upper_us, v, bar(v, max, 40)))
      lower_us = upper_us
      upper_us = upper_us * 2
    end

    print()
  end
)
