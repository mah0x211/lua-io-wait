local unpack = unpack or table.unpack
local testcase = require('testcase')
local assert = require('assert')
local wait = require('io.wait')
local timer = require('testcase.timer')
local socketpair = require('testcase.socketpair')
local fork = require('testcase.fork')
-- nosigchld(true/false): install/restore a pure C no-op SIGCHLD handler
-- without SA_RESTART, so poll() returns EINTR when a child exits.
-- Using a closure with an upvalue keeps all state off static variables.
local nosigchld = require('testcase.nosigchld')

function testcase.test_fdsetsize()
    -- test that return maximum number of file descriptors to wait
    local fdsetsize = wait.fdsetsize()
    assert.greater(fdsetsize, 0)
end

function testcase.test_readable_einval()
    -- test that return EINVAL error if too many file descriptors are specified
    local r, _ = assert(socketpair())
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

function testcase.test_readable_timeout()
    -- test that return timeout
    local r = assert(socketpair(true))
    local t = timer.nanotime()
    local fd, err, timeout, hup = wait.readable(r:fd(), 0.5)
    t = timer.nanotime() - t
    assert.is_nil(fd)
    assert(not err, err)
    assert.is_true(timeout)
    assert.is_nil(hup)
    assert(t > 0.5 and t < 1)
end

function testcase.test_readable_eintr()
    -- test that return timeout=true even if eintr error is occurred
    local r = assert(socketpair(true))
    assert(nosigchld(true))
    local p = assert(fork())
    if p:is_child() then
        timer.sleep(0.05)
        os.exit(0)
    end
    local t = timer.nanotime()
    local fd, err, timeout, hup = wait.readable(r:fd(), 1)
    assert(nosigchld(false))
    t = timer.nanotime() - t
    assert.is_nil(fd)
    assert.match(err, 'EINTR')
    assert.is_true(timeout)
    assert.is_nil(hup)
    assert(t < 1)
end

function testcase.test_readable_no_error_on_eintr()
    -- test that return no error even if eintr error is occurred
    local r = assert(socketpair(true))
    local p = assert(fork())
    if p:is_child() then
        os.exit(0)
    end
    local t = timer.nanotime()
    local fd, err, timeout, hup = wait.readable(r:fd(), 0)
    t = timer.nanotime() - t
    assert.is_nil(fd)
    assert(not err, err)
    assert.is_true(timeout)
    assert.is_nil(hup)
    assert.less(t, 0.001)
end

function testcase.test_writable()
    -- test that wait until fd is writable
    local _, w = assert(socketpair(true))
    local t = timer.nanotime()
    local fd, err, timeout, hup = assert(wait.writable(w:fd(), 1))
    t = timer.nanotime() - t
    assert.equal(fd, w:fd())
    assert(not err, err)
    assert.is_nil(timeout)
    assert.is_nil(hup)
    assert.less(t, 1)
end

function testcase.test_writable_timeout()
    -- test that wait until timeout when write buffer is full
    local _, w = assert(socketpair(true))
    -- fill write buffer
    repeat
        local _, err, again = w:write('x')
        if err then
            error(tostring(err))
        end
    until again == true
    local t = timer.nanotime()
    local fd, err, timeout, hup = wait.writable(w:fd(), 0.5)
    t = timer.nanotime() - t
    assert.is_nil(fd)
    assert(not err, err)
    assert.is_true(timeout)
    assert.is_nil(hup)
    assert(t > 0.5 and t < 1)
end

function testcase.test_readable_with_data()
    -- test that wait until fd is readable
    local r, w = assert(socketpair(true))
    -- fill write buffer to make r readable
    repeat
        local _, err, again = w:write('x')
        if err then
            error(tostring(err))
        end
    until again == true
    local t = timer.nanotime()
    local fd, err, timeout, hup = assert(wait.readable(r:fd(), 1))
    t = timer.nanotime() - t
    assert.equal(fd, r:fd())
    assert(not err, err)
    assert.is_nil(timeout)
    assert.is_nil(hup)
    assert.less(t, 1)
end

function testcase.test_readable_hup()
    -- test that return immediately if peer is closed
    local r, w = assert(socketpair(true))
    w:close()
    local t = timer.nanotime()
    local fd, err, timeout, hup = wait.readable(r:fd(), 1)
    t = timer.nanotime() - t
    assert.equal(fd, r:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_true(hup)
    assert.less(t, 1)
end

function testcase.test_readable_ebadf()
    -- test that return EBADF error if specified fd is invalid
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

function testcase.test_readable_ebadf_closed_fd()
    -- test that return error if fd is already closed
    local r = assert(socketpair(true))
    local badfd = r:fd()
    r:close()
    local fd, err, timeout, hup = wait.readable(badfd, 1)
    assert.is_nil(fd)
    assert.re_match(err, string.format('EBADF.+ fd#1:%d', badfd))
    assert.is_nil(timeout)
    assert.is_nil(hup)
end

function testcase.test_readable_multiple_fds()
    -- test that multiple fd can be waited
    local r, w = assert(socketpair(true))
    local r2, w2 = assert(socketpair(true))

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

    -- test that return fd of second pair
    w2:write('x')
    fd, err, timeout, hup = wait.readable(r:fd(), 1, r2:fd())
    assert.equal(fd, r2:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_nil(hup)

    -- test that return fd of first pair
    w:write('x')
    fd, err, timeout, hup = wait.readable(r:fd(), 1, r2:fd())
    assert.equal(fd, r:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_nil(hup)

    -- test that does not report hup if first pair is not closed
    w2:close()
    fd, err, timeout, hup = wait.readable(r:fd(), 1, r2:fd())
    assert.equal(fd, r:fd())
    assert.is_nil(err)
    assert.is_nil(timeout)
    assert.is_nil(hup)

    -- test that report hup of second pair
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
