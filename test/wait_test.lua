local assert = require('assert')
local errno = require('errno')
local wait = require('io.wait')
local gettime = require('clock').gettime
local pipe = require('pipe')

local r, w, perr = pipe(true)
assert(r, perr)

-- test that return timeout
local t = gettime()
local ok, err, timeout = wait.readable(r:fd(), 1000)
t = gettime() - t
assert.is_false(ok)
assert(not err, err)
assert.is_true(timeout)
assert(t > 1.0 and t < 1.5)

-- test that wait until fd is writable
t = gettime()
ok, err, timeout = assert(wait.writable(w:fd(), 1000))
t = gettime() - t
assert.is_true(ok)
assert(not err, err)
assert.is_nil(timeout)
assert.less(t, 1.0)

-- fill data
repeat
    local _, again
    _, err, again = w:write('x')
    if err then
        error(err)
    end
until again == true

-- test that wait untile timeout
t = gettime()
ok, err, timeout = wait.writable(w:fd(), 1000)
t = gettime() - t
assert.is_false(ok)
assert(not err, err)
assert.is_true(timeout)
assert(t > 1.0 and t < 1.5)

-- test that wait until fd is readable
t = gettime()
ok, err, timeout = assert(wait.readable(r:fd(), 1000))
t = gettime() - t
assert.is_true(ok)
assert(not err, err)
assert.is_nil(timeout)
assert.less(t, 1.0)

-- test that return error if fd is closed
local fd = r:fd()
r:close()
ok, err, timeout = wait.readable(fd, 1000)
assert.is_false(ok)
assert.equal(err.type, errno.EBADF)
assert.is_nil(timeout)

