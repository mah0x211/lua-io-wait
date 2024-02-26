local unpack = unpack or table.unpack
local assert = require('assert')
local wait = require('io.wait')
local gettime = require('time.clock').gettime
local pipe = require('os.pipe')
local signal = require('signal')
local fork = require('fork')

local r, w, perr = pipe(true)
assert(r, perr)

-- test that return maximum number of file descriptors to wait
do
    local fdsetsize = wait.fdsetsize()
    assert.greater(fdsetsize, 0)
end

-- test that return EINVAL error if too many file descriptors are specified
do
    local fds = {}
    for i = 1, wait.fdsetsize() do
        fds[i] = r:fd()
    end
    local fd, err, timeout, hup = wait.readable(0, nil, unpack(fds))
    assert.is_nil(fd)
    assert.match(err, 'EINVAL')
    assert.is_nil(timeout)
    assert.is_nil(hup)
end

-- test that return timeout
do
    local t = gettime()
    local fd, err, timeout, hup = wait.readable(r:fd(), 0.5)
    t = gettime() - t
    assert.is_nil(fd)
    assert(not err, err)
    assert.is_true(timeout)
    assert.is_nil(hup)
    assert(t > 0.5 and t < 1)

    -- test that return timeout even if eintr error is occurred
    local p = assert(fork())
    if p:is_child() then
        os.exit(0)
    end

    t = gettime()
    fd, err, timeout, hup = wait.readable(r:fd(), 0.5)
    t = gettime() - t
    assert.is_nil(fd)
    assert(not err, err)
    assert.is_true(timeout)
    assert.is_nil(hup)
    assert(t > 0.5 and t < 1)
end

-- test that wait until fd is writable
do
    local t = gettime()
    local fd, err, timeout, hup = assert(wait.writable(w:fd(), 1))
    t = gettime() - t
    assert.equal(fd, w:fd())
    assert(not err, err)
    assert.is_nil(timeout)
    assert.is_nil(hup)
    assert.less(t, 1)
end

-- fill data
repeat
    local _, err, again = w:write('x')
    if err then
        error(err)
    end
until again == true

-- test that wait until timeout
do
    local t = gettime()
    local fd, err, timeout, hup = wait.writable(w:fd(), 0.5)
    t = gettime() - t
    assert.is_nil(fd)
    assert(not err, err)
    assert.is_true(timeout)
    assert.is_nil(hup)
    assert(t > 0.5 and t < 1)
end

-- test that wait until fd is readable
do
    local t = gettime()
    local fd, err, timeout, hup = assert(wait.readable(r:fd(), 1))
    t = gettime() - t
    assert.equal(fd, r:fd())
    assert(not err, err)
    assert.is_nil(timeout)
    assert.is_nil(hup)
    assert.less(t, 1)
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
    local fd, err, timeout, hup = wait.readable(r:fd(), 1)
    t = gettime() - t
    assert.equal(fd, r:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_true(hup)
    assert.less(t, 1)
end

-- test that return EBADF error if specified fd is invalid
do
    local fd, err, timeout, hup = wait.readable(-1, 1)
    assert.is_nil(fd)
    assert.re_match(err, 'EBADF.+ fd#1:-1')
    assert.is_nil(timeout)
    assert.is_nil(hup)

    -- test that return error if additional fd is invalid
    fd, err, timeout, hup = wait.readable(0, 1.1, -123)
    assert.is_nil(fd)
    assert.re_match(err, string.format('EBADF.+ fd#2:%d', -123))
    assert.is_nil(timeout)
    assert.is_nil(hup)
end

-- test that return error if fd is already closed
do
    local badfd = r:fd()
    r:close()
    local fd, err, timeout, hup = wait.readable(badfd, 1)
    assert.is_nil(fd)
    assert.re_match(err, string.format('EBADF.+ fd#1:%d', badfd))
    assert.is_nil(timeout)
    assert.is_nil(hup)
end

-- test that multiple fd can be waited
do
    r, w, perr = pipe(true)
    assert(r, perr)
    local r2, w2
    r2, w2, perr = pipe(true)
    assert(r, perr)

    -- test that return timeout
    local fd, err, timeout, hup = wait.readable(r:fd(), 0.5, r2:fd())
    assert.is_nil(fd)
    assert.is_nil(err)
    assert.is_true(timeout)
    assert.is_nil(hup)

    -- test that can specify nil as file descriptors
    fd, err, timeout, hup = wait.readable(nil, 0.5, r2:fd())
    assert.is_nil(fd)
    assert.is_nil(err)
    assert.is_true(timeout)
    assert.is_nil(hup)

    fd, err, timeout, hup = wait.readable(nil, 0.5, nil)
    assert.is_nil(fd)
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_nil(hup)

    -- test that return fd of second pipe
    w2:write('x')
    fd, err, timeout, hup = wait.readable(r:fd(), 1, r2:fd())
    assert.equal(fd, r2:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_nil(hup)

    -- test that return fd of first pipe
    w:write('x')
    fd, err, timeout, hup = wait.readable(r:fd(), 1, r2:fd())
    assert.equal(fd, r:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_nil(hup)

    -- test that does not report hup if first pipe is not closed
    w2:close()
    fd, err, timeout, hup = wait.readable(r:fd(), 1, r2:fd())
    assert.equal(fd, r:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_nil(hup)

    -- test that report hup of second pipe
    r:read()
    fd, err, timeout, hup = wait.readable(r:fd(), 1, r2:fd())
    assert.equal(fd, r2:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_true(hup)

    -- test that does not report EBADF if file descriptor is not specified at first
    w:write('x')
    local badfd = r2:fd()
    r2:close()
    fd, err, timeout, hup = wait.readable(r:fd(), 1, badfd)
    assert.equal(fd, r:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_nil(hup)

    -- test that EBADF is returned if file descriptor is invalid
    fd, err, timeout, hup = wait.readable(badfd, 1, r:fd())
    assert.is_nil(fd)
    assert.re_match(err, string.format('EBADF.+ fd#1:%d', badfd))
    assert.is_nil(timeout)
    assert.is_nil(hup)
end
