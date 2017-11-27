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

print('1')

client_socket(  function () print('2') end )
client_sendmsg( function () print('3'); return 0; end )
client_recvmsg( function () print('4'); return 0; end )
client_recverr( function () print('5'); return 0; end )
client_close(   function () print('6') end )

server_socket(  function () print('2') end )
server_recvmsg( function () print('3'); return 0; end )
server_sendmsg( function () print('4'); return 0; end )
server_recverr( function () print('5'); return 0; end )
server_close(   function () print('6') end )

run()

print('7')
