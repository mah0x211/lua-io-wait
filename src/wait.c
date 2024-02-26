/**
 *  Copyright (C) 2022 Masatoshi Fukunaga
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
// lua
#include <lua_errno.h>

#if !defined(POLLRDHUP)
# define POLLRDHUP 0
#endif

static inline void push_ebadf(lua_State *L, int argidx, int fd)
{
    char msg[512];
    snprintf(msg, sizeof(msg), "invalid fd#%d:%d", argidx, fd);
    errno = EBADF;
    lua_errno_new_with_message(L, errno, "poll", msg);
}

static inline long calculate_diff_millis(struct timespec *start,
                                         struct timespec *end)
{
    long seconds     = end->tv_sec - start->tv_sec;
    long nanoseconds = end->tv_nsec - start->tv_nsec;

    if (nanoseconds < 0) {
        seconds -= 1;
        nanoseconds += 1000000000;
    }
    return (seconds * 1000) + (nanoseconds / 1000000);
}

static inline int poll_with_timeout_lua(struct pollfd *fds, nfds_t nfds,
                                        lua_Integer msec)
{
    struct timespec t = {};
    int rc            = 0;

RETRY:
    // get current time
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        return -1;
    }
    // wait until usable or exceeded timeout
    errno = 0;
    rc    = poll(fds, nfds, msec);
    if (rc == -1) {
        // got error
        if (errno == EINTR) {
            struct timespec now = {};
            // calculate elapsed time in msec and subtract it from msec
            if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
                return -1;
            }
            msec -= calculate_diff_millis(&t, &now);
            if (msec > 0) {
                goto RETRY;
            }
            // timeout
            rc = 0;
        }
    }
    return rc;
}

static inline int poll_without_timeout_lua(struct pollfd *fds, nfds_t nfds,
                                           lua_Integer msec)
{
    int rc = 0;

RETRY:
    // wait until usable or exceeded timeout
    errno = 0;
    rc    = poll(fds, nfds, msec);
    if (rc == -1 && errno == EINTR) {
        goto RETRY;
    }
    return rc;
}

static inline int poll_lua(lua_State *L, struct pollfd *fds, nfds_t nfds,
                           lua_Integer msec)
{
    int rc = msec > 0 ? poll_with_timeout_lua(fds, nfds, msec) :
                        poll_without_timeout_lua(fds, nfds, msec);

    switch (rc) {
    case 0:
        // timeout
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushboolean(L, 1);
        return 3;

    case -1:
        // got error
        lua_pushnil(L);
        lua_errno_new(L, errno, "poll");
        return 2;

    default:
        // selected
        for (nfds_t i = 0; i < nfds; i++) {
            if (fds[i].revents) {
                // report only first fd
                if (fds[i].revents & POLLNVAL) {
                    lua_pushnil(L);
                    push_ebadf(L, i + 1, fds[i].fd);
                    return 2;
                }

                lua_pushinteger(L, fds[i].fd);
                if (fds[i].revents & (POLLHUP | POLLRDHUP)) {
                    lua_pushnil(L);
                    lua_pushnil(L);
                    lua_pushboolean(L, 1);
                    return 4;
                }
                return 1;
            }
        }
        return luaL_error(L, "BUG: poll() returns unexpected value");
    }
}

static inline int waitevent_lua(lua_State *L, short events)
{
    int narg                      = lua_gettop(L);
    lua_Number sec                = luaL_optnumber(L, 2, -1);
    lua_Integer msec              = (sec < 0) ? -1 : (sec * 1000);
    int nfds                      = 0;
    struct pollfd fds[FD_SETSIZE] = {};

    // check first fd
    if (!lua_isnoneornil(L, 1)) {
        int fd = lauxh_checkinteger(L, 1);
        if (fd < 0) {
            lua_pushnil(L);
            push_ebadf(L, 1, fd);
            return 2;
        }
        fds[nfds].events = events;
        fds[nfds].fd     = fd;
        nfds++;
    }

    // check additional fds
    for (int i = 3; i <= narg; i++) {
        if (nfds >= FD_SETSIZE) {
            // too many fds
            errno = EINVAL;
            lua_pushnil(L);
            lua_errno_new(L, errno, "poll");
            return 2;
        } else if (lua_isnoneornil(L, i)) {
            // ignore nil
            continue;
        }

        fds[nfds].fd     = lauxh_checkinteger(L, i);
        fds[nfds].events = events;
        if (fds[nfds].fd < 0) {
            lua_pushnil(L);
            push_ebadf(L, nfds + 1, fds[nfds].fd);
            return 2;
        }
        nfds++;
    }

    if (!nfds) {
        // do nothing if no file descriptors are specified
        return 0;
    }

    // wait until usable or exceeded timeout
    lua_settop(L, 0);
    return poll_lua(L, fds, nfds, msec);
}

static int writable_lua(lua_State *L)
{
    return waitevent_lua(L, POLLOUT | POLLNVAL | POLLHUP | POLLERR);
}

static int readable_lua(lua_State *L)
{
    return waitevent_lua(L, POLLIN | POLLPRI | POLLRDHUP);
}

static int fdsetsize_lua(lua_State *L)
{
    lua_pushinteger(L, FD_SETSIZE);
    return 1;
}

LUALIB_API int luaopen_io_wait(lua_State *L)
{
    lua_errno_loadlib(L);

    lua_createtable(L, 0, 2);
    lauxh_pushfn2tbl(L, "fdsetsize", fdsetsize_lua);
    lauxh_pushfn2tbl(L, "readable", readable_lua);
    lauxh_pushfn2tbl(L, "writable", writable_lua);

    return 1;
}
