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

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
// lua
#include <lua_errno.h>

static inline int poll_lua(lua_State *L, short event, short event_opts)
{
    int fd               = lauxh_checkinteger(L, 1);
    lua_Integer msec     = luaL_optinteger(L, 2, -1);
    struct pollfd fds[1] = {
        {
         .events  = event | event_opts,
         .revents = 0,
         .fd      = fd,
         }
    };

    lua_settop(L, 0);
    // wait until usable or exceeded timeout
    errno = 0;
    switch (poll(fds, 1, msec)) {
    case 0:
        // timeout
        lua_pushboolean(L, 0);
        lua_pushnil(L);
        lua_pushboolean(L, 1);
        return 3;

    case -1:
        // got error
        lua_pushboolean(L, 0);
        lua_errno_new(L, errno, "poll");
        return 2;

    default:
        // selected
        if (fds[0].revents & event) {
            lua_pushboolean(L, 1);
            return 1;
        }
        // got error
        lua_pushboolean(L, 0);
        if (errno) {
            lua_errno_new(L, errno, "poll");
            return 2;
        } else if (fds[0].revents & POLLNVAL) {
            errno = EBADF;
            lua_errno_new(L, errno, "poll");
            return 2;
        }
        return 1;
    }
}

static int writable_lua(lua_State *L)
{
    return poll_lua(L, POLLOUT, POLLNVAL | POLLHUP | POLLERR);
}

static int readable_lua(lua_State *L)
{
#if !defined(POLLRDHUP)
# define POLLRDHUP 0
#endif
    return poll_lua(L, POLLIN, POLLPRI | POLLRDHUP);
}

LUALIB_API int luaopen_io_wait(lua_State *L)
{
    lua_errno_loadlib(L);

    lua_createtable(L, 0, 2);
    lauxh_pushfn2tbl(L, "readable", readable_lua);
    lauxh_pushfn2tbl(L, "writable", writable_lua);

    return 1;
}
