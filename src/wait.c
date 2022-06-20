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
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
// lua
#include <lua_errno.h>

static inline int select_lua(lua_State *L, int readable, int writable)
{
    int fd                 = lauxh_checkinteger(L, 1);
    lua_Integer msec       = luaL_optinteger(L, 2, 0);
    int except             = lauxh_optboolean(L, 3, 0);
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 0};
    fd_set rfds;
    fd_set wfds;
    fd_set efds;
    fd_set *rptr = NULL;
    fd_set *wptr = NULL;
    fd_set *eptr = NULL;

    lua_settop(L, 0);
    if (msec > 0) {
        timeout.tv_sec  = msec / 1000;
        timeout.tv_usec = msec % 1000 * 1000;
    }

    // select readable
    if (readable) {
        rptr = &rfds;
        FD_ZERO(rptr);
        FD_SET(fd, rptr);
    }
    // select writable
    if (writable) {
        wptr = &wfds;
        FD_ZERO(wptr);
        FD_SET(fd, wptr);
    }
    // select exception
    if (except) {
        eptr = &efds;
        FD_ZERO(eptr);
        FD_SET(fd, eptr);
    }

    // wait until usable or exceeded timeout
    switch (select(fd + 1, rptr, wptr, eptr, &timeout)) {
    case 0:
        // timeout
        lua_pushboolean(L, 0);
        lua_pushnil(L);
        lua_pushboolean(L, 1);
        return 3;

    case -1:
        // got error
        lua_pushboolean(L, 0);
        lua_errno_new(L, errno, "select");
        return 2;

    default:
        // selected
        lua_pushboolean(L, 1);
        return 1;
    }
}

static int writable_lua(lua_State *L)
{
    return select_lua(L, 0, 1);
}

static int readable_lua(lua_State *L)
{
    return select_lua(L, 1, 0);
}

LUALIB_API int luaopen_io_wait(lua_State *L)
{
    lua_errno_loadlib(L);

    lua_createtable(L, 0, 2);
    lauxh_pushfn2tbl(L, "readable", readable_lua);
    lauxh_pushfn2tbl(L, "writable", writable_lua);

    return 1;
}
