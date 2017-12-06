--
-- Measure time between ACKs and display the delay distribution.
--
-- TODO:
-- * Get rid of open coded histogram generation. Introduce helpers.
--

-- Per-thread base-2 logarithmic histogram of interval lengths in
-- microseconds (us) between consecutive TCP ACKs. Keyed by the upper
-- bound (exclusive) of the bucket. That is:
--
--   hist[2^0] = # of measured intervals between [0, 1) us
--   hist[2^1] = # of measured intervals between [1, 2) us
--   ...
--   hist[2^N] = # of measured intervals between [2^N, 2^N+1) us
--
local hist = collect({})

-- Timestamp in microseconds of last TCP ACK. Keyed by socket FD.
local sock_last_ts = {}

client_socket(
  function (sockfd)
    local ok, err = setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING,
                               bit.bor(SOF_TIMESTAMPING_SOFTWARE,
                                       SOF_TIMESTAMPING_TX_ACK,
                                       SOF_TIMESTAMPING_OPT_TSONLY))
    assert(ok, tostring(err))
  end
)

client_recverr(
  function (sockfd, msg, flags)
    local n, err = recvmsg(sockfd, msg, flags)
    assert(n, tostring(err))

    for _, cmsg in msg:cmsgs() do
      if cmsg.cmsg_level == SOL_SOCKET and
         cmsg.cmsg_type == SCM_TIMESTAMPING then

        -- Anciallary message carries a pointer to an scm_timestamping
        -- structure. We are interested in the fist timestemp in it.
        --
        -- struct scm_timestamping {
        --         struct timespec ts[3];
        -- };
        --
        local tss = scm_timestamping(cmsg.cmsg_data)
        local tv = tss.ts[0]
        local ts = (tv.sec * 1000 * 1000)
                 + (tv.nsec / 1000)

        local last_ts = sock_last_ts[sockfd]

        if last_ts ~= nil then
          local ival = ts - last_ts
          local upper = 1 -- 2^0

          while ival >= upper do
            upper = upper * 2
          end

          hist[upper] = (hist[upper] or 0) + 1
        end

        sock_last_ts[sockfd] = ts

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

run();

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

local function print_hist(h)
  local max = 0

  for _, v in pairs(h) do
    if v > max then
      max = v
    end
  end

  print('\n',
        string.format("%10s .. %-10s: %-10s |%-40s|",
                      ">=", "< [us]", "Count", "Distribution"),
        '\n')

  local lower_us = 0
  local upper_us = 1
  while lower_us < table.maxn(h) do
    local count = (h[upper_us] or 0)
    print(string.format("%10d -> %-10d: %-10d |%-40s|",
                        lower_us, upper_us, count, bar(count, max, 40)))
    lower_us = upper_us
    upper_us = upper_us * 2
  end

  print()
end

-- Print per-thread histograms
for i, h in ipairs(hist) do
  print("Thread", i)
  print_hist(h)
end

-- Print aggregate (sum of all) histogram
hist_sum = {}
for _, h in ipairs(hist) do
  for k, v in pairs(h) do
    hist_sum[k] = (hist_sum[k] or 0) + v
  end
end

print("All threads (summed)")
print_hist(hist_sum)
