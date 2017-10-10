local t_start = os.time()

run()

local t_diff = os.time() - t_start
io.stderr:write('Script ran for ' .. t_diff .. ' seconds.\n')
