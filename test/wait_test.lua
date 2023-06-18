local assert = require('assert')
local errno = require('errno')
local wait = require('io.wait')
local gettime = require('clock').gettime
local pipe = require('os.pipe')

local r, w, perr = pipe(true)
assert(r, perr)

-- test that return timeout
do
    local t = gettime()
    local ok, err, timeout = wait.readable(r:fd(), 1000)
    t = gettime() - t
    assert.is_false(ok)
    assert(not err, err)
    assert.is_true(timeout)
    assert(t > 1.0 and t < 1.5)
end

-- test that wait until fd is writable
do
    local t = gettime()
    local ok, err, timeout = assert(wait.writable(w:fd(), 1000))
    t = gettime() - t
    assert.is_true(ok)
    assert(not err, err)
    assert.is_nil(timeout)
    assert.less(t, 1.0)
end

-- fill data
repeat
    local _, err, again = w:write('x')
    if err then
        error(err)
    end
until again == true

-- test that wait untile timeout
do
    local t = gettime()
    local ok, err, timeout = wait.writable(w:fd(), 1000)
    t = gettime() - t
    assert.is_false(ok)
    assert(not err, err)
    assert.is_true(timeout)
    assert(t > 1.0 and t < 1.5)
end

-- test that wait until fd is readable
do
    local t = gettime()
    local ok, err, timeout = assert(wait.readable(r:fd(), 1000))
    t = gettime() - t
    assert.is_true(ok)
    assert(not err, err)
    assert.is_nil(timeout)
    assert.less(t, 1.0)
end

-- consume
repeat
    local _, err, again = r:read()
    assert.is_nil(err)
until again

-- test that return true imemdiatery if peer is closed
do
    assert.is_true(w:close())
    local t = gettime()
    local ok, err, timeout = wait.readable(r:fd(), 1000)
    t = gettime() - t
    assert.is_true(ok)
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.less(t, 0.1)
end

-- test that return error if fd is closed
do
    local fd = r:fd()
    r:close()
    local ok, err, timeout = wait.readable(fd, 1000)
    assert.is_false(ok)
    assert.equal(err.type, errno.EBADF)
    assert.is_nil(timeout)
end
