--
-- Check the execution order of the top-level script and the hooks.
--
-- Ensure that:
-- 1. everything before the run() call gets executed before the hooks,
-- 2. the socket hooks get called before and after the packet hooks,
-- 3. packet hooks get called in the order appropriate for the run mode (client/server)
-- 4. everything after the run() call gets executed after the hooks.
--
-- This test depends on the sequence of fake-events hardcoded in the
-- dummy_test workload.
--

io.stderr:write('1\n')

client_socket(  function () io.stderr:write('2\n') end )
client_sendmsg( function () io.stderr:write('3\n'); return 0; end )
client_recvmsg( function () io.stderr:write('4\n'); return 0; end )
client_recverr( function () io.stderr:write('5\n'); return 0; end )
client_close(   function () io.stderr:write('6\n') end )

server_socket(  function () io.stderr:write('2\n') end )
server_recvmsg( function () io.stderr:write('3\n'); return 0; end )
server_sendmsg( function () io.stderr:write('4\n'); return 0; end )
server_recverr( function () io.stderr:write('5\n'); return 0; end )
server_close(   function () io.stderr:write('6\n') end )

run()

io.stderr:write('7\n')
