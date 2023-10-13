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

static inline int poll_lua(lua_State *L, struct pollfd *fds, nfds_t nfds,
                           lua_Integer msec)
{
    // wait until usable or exceeded timeout
    errno = 0;
    switch (poll(fds, nfds, msec)) {
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
    int fd                        = lauxh_checkinteger(L, 1);
    lua_Number sec                = luaL_optnumber(L, 2, -1);
    lua_Integer msec              = (sec < 0) ? -1 : (sec * 1000);
    int nfds                      = 1;
    struct pollfd fds[FD_SETSIZE] = {
        {
         .events  = events,
         .revents = 0,
         .fd      = fd,
         }
    };

    // check first fd
    if (fd < 0) {
        lua_pushnil(L);
        push_ebadf(L, 1, fd);
        return 2;
    }
    // check additional fds
    for (int i = 3; i <= narg; i++) {
        if (nfds >= FD_SETSIZE) {
            // too many fds
            errno = EINVAL;
            lua_pushnil(L);
            lua_errno_new(L, errno, "poll");
            return 2;
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
    lua_settop(L, 0);

    // wait until usable or exceeded timeout
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
