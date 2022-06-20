# lua-io-wait

[![test](https://github.com/mah0x211/lua-io-wait/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-io-wait/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/lua-io-wait/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/lua-io-wait)

wait until the file descriptor is "ready" for I/O operation.


## Installation

```bash
$ luarocks install io-wait
```

## Error Handling

the functions/methods are return the error object created by https://github.com/mah0x211/lua-errno module.


## ok, err, timeout = wait.readable( fd [, msec [, exception]] )

wait until specified file descriptor `fd` can be readable within specified timeout milliseconds `msec.

**Parameters**

- `fd:integer`: file descriptor.
- `msec:integer`: timeout milliseconds. (default `0`)
- `exception:boolean`: enable exception waiting. (default `false`)

**Returns**

- `ok:boolean`: if `true`, socket is ready to send.
- `err:error`: error object.
- `timeout:boolean`: timed-out.


## ok, err, timeout = wait.writable( fd [, msec [, exception]] )

wait until specified file descriptor `fd` can be writable within specified timeout milliseconds `msec.

**Parameters**

- `fd:integer`: file descriptor.
- `msec:integer`: timeout milliseconds. (default `0`)
- `exception:boolean`: enable exception waiting. (default `false`)

**Returns**

- `ok:boolean`: if `true`, socket is ready to send.
- `err:error`: error object.
- `timeout:boolean`: timed-out.
