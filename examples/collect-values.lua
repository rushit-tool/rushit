--
-- Demonstration of value collection from client/server threads.
--

local sockfd = collect(-1)
-- `sockfd` now refers to a special table ("{ -1 }") w/ metadata set.
-- Should be treated as an opaque object.

local function log_fd(fd)
  io.write(tostring(fd) .. " ")
  io.flush()
end

local function on_socket(fd)
  log_fd(fd)
  sockfd = fd
end

client_socket(on_socket)
server_socket(on_socket)

run()

-- Client/server threads have stopped. `sockfd` now refers to a table
-- that contains values copied from all threads, i.e.:
--
--   { <sockfd from thread #1>, <sockfd from thread #2>, ... }
--

io.write("\n")
for _, fd in pairs(sockfd) do
  log_fd(fd)
end
io.write("\n")
