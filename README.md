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


## fd, err, timeout = wait.readable( fd [, sec [, ...]] )

wait until specified file descriptor `fd` can be readable within specified timeout seconds `sec`.

**Parameters**

- `fd:integer`: file descriptor.
- `sec:number`: timeout seconds. if `nil` or `<0`, wait forever.
- `...:integer`: additional file descriptors.

**Returns**

- `fd:integer`: file descriptor that is ready to read.
- `err:error`: error object.
- `timeout:boolean`: timed-out.
- `hup:boolean`: `true` if `POLLHUP` or `POLLRDHUP` is set.

**NOTE:**

it reports the first file descriptor ready in the argument order.  


## fd, err, timeout = wait.writable( fd [, sec [, ...]] )

wait until specified file descriptor `fd` can be writable within specified timeout seconds `sec`.

**Parameters**

- `fd:integer`: file descriptor.
- `sec:number`: timeout seconds. if `nil` or `<0`, wait forever.
- `...:integer`: additional file descriptors.

**Returns**

- `fd:integer`: file descriptor that is ready to write.
- `err:error`: error object.
- `timeout:boolean`: timed-out.
- `hup:boolean`: `true` if `POLLHUP` or `POLLRDHUP` is set.

**NOTE:**

same as `wait.readable`.
