# `va_*` in `printf` (variadic arguments)

This note expands on the [`printf` function section](https://operating-system-in-1000-lines.vercel.app/en/05-hello-world#printf-function), explaining the `va_*` functions.


## Why `va_*` exists

A function like:

```c
void printf(const char *fmt, ...);
```

takes a variable number of arguments (`...`). C does not provide:

* how many extra args there are
* where they are stored (registers vs stack)
* their types

So `printf` must:

1. parse `fmt` (`%d`, `%x`, `%s`, ...)
2. fetch the next argument for each specifier

`va_*` is the standard mechanism for step (2).

## What each macro does

* `va_list`: an opaque “cursor” over the extra arguments.
* `va_start(ap, last_named)`: initialize the cursor. `last_named` is `fmt` in `printf`.
* `va_arg(ap, T)`: read the next argument as type `T`, and advance the cursor.
* `va_end(ap)`: finish using the cursor (required even if it compiles to nothing).

Example:

```c
va_list ap;
va_start(ap, fmt);

int x = va_arg(ap, int);            // for %d
const char *s = va_arg(ap, char *); // for %s

va_end(ap);
```

## Why your code uses `__builtin_va_*`

In a kernel/freestanding environment you often do not have `<stdarg.h>`.
So the book maps:

```c
#define va_list  __builtin_va_list
#define va_start __builtin_va_start
#define va_arg   __builtin_va_arg
#define va_end   __builtin_va_end
```

These are compiler built-ins (GCC/Clang). The compiler knows the ABI (including RISC-V calling convention) and generates the correct code to fetch args from registers/stack.

## One important gotcha: promotions

Arguments passed through `...` are promoted:

* `char` / `short` → `int`
* `float` → `double`

So you must read them with the promoted type in `va_arg`.

If you paste your current `printf`, I can point out exactly which `va_arg` type should match each format specifier.
