# Why does the SBI tutorial pin `a0`–`a7`?

This note expands the [“Say hello to SBI” section](https://operating-system-in-1000-lines.vercel.app/en/05-hello-world#say-hello-to-sbi). Here we explain why the “pinning” part is necessary, even though it can look redundant at first.

## Start from the correct code

The tutorial uses this pattern:

```c
struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {
  register long a0 __asm__("a0") = arg0;
  register long a1 __asm__("a1") = arg1;
  register long a2 __asm__("a2") = arg2;
  register long a3 __asm__("a3") = arg3;
  register long a4 __asm__("a4") = arg4;
  register long a5 __asm__("a5") = arg5;
  register long a6 __asm__("a6") = fid;
  register long a7 __asm__("a7") = eid;

  __asm__ __volatile__("ecall"
                       : "=r"(a0), "=r"(a1)
                       : "r"(a0), "r"(a1), "r"(a2), "r"(a3),
                         "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                       : "memory");

  return (struct sbiret){ .error = a0, .value = a1 };
}
```

At a glance, you might ask:

* “Why create `a0`, `a1`, … variables?”
* “Why not just pass `arg0`, `arg1`, … directly to inline asm?”
* “Sometimes `sbi_call` compiles to just `ecall; ret`. So is pinning doing anything?”

Let’s answer those questions.

## What SBI requires at the moment of `ecall`

SBI is not a normal C function call. It is a convention about **which registers** contain what when the CPU executes `ecall`.

For the SBI calling convention used in this tutorial:

* Inputs must be in fixed registers:

  * `a0`–`a5`: arguments
  * `a6`: FID
  * `a7`: EID
* Outputs come back in fixed registers:

  * `a0`: error
  * `a1`: value

So SBI is “register ABI based”. If the values are not in the correct `a*` registers at `ecall`, the firmware reads the wrong request.

## Why the pinning lines exist

These lines:

```c
register long a7 __asm__("a7") = eid;
```

mean:

> This C variable must live in the hardware register `a7`.

So after those assignments, the compiler must arrange that:

* `eid` is actually placed in `a7`
* `fid` is actually placed in `a6`
* and so on for `a0`–`a5`

Pinning is how we translate the SBI register ABI into something the compiler will obey.

## “But my `sbi_call` compiles to just `ecall; ret`”

That can still be correct.

Why: on RISC-V, the C ABI passes the first 8 integer arguments in `a0`–`a7`. If `sbi_call(arg0, ..., eid)` is called normally, arguments often arrive already in `a0`–`a7`. In that situation, the compiler can emit:

```asm
sbi_call:
  ecall
  ret
```

This does not mean pinning is useless. It means the C ABI and the SBI ABI happen to match for this function signature, so the compiler can optimize away the moves.

Pinning still matters because the compiler may inline the call site, and you want correctness even when optimizations change where values live.

## The tempting rewrite that breaks SBI

A common “cleaner” attempt is:

```c
long out0, out1;
__asm__ __volatile__("ecall"
                     : "=r"(out0), "=r"(out1)
                     : "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3),
                       "r"(arg4), "r"(arg5), "r"(fid), "r"(eid)
                     : "memory");
return (struct sbiret){ .error = out0, .value = out1 };
```

The key problem is the `"r"` constraint.

`"r"` means: “put this input in some general-purpose register”.

It does not mean: “put it in `a0`”, “put it in `a7`”, etc.

So the compiler is free to choose any registers for those inputs, especially after inlining.

## Your observed “wrong” assembly results

When using that “cleaner” version, you observed output like this (call site is `putchar`):

### Case A: many arguments are zero

```asm
00000016 <sbi_call>:
      16: 00000073      ecall
      1a: 8082          ret

0000001c <putchar>:
      1c: 4581          li      a1, 0x0
      1e: 4605          li      a2, 0x1
      20: 00000073      ecall
      24: 8082          ret
```

What this tells you:

* The compiler decided to place `0` into `a1`.
* It decided to place `1` into `a2`.
* It did **not** place the EID into `a7` or the FID into `a6`.

From SBI’s perspective this is wrong: SBI expects `eid` in `a7`, but the compiler put `1` in `a2`.

Why did it do that?

* Because you only told it `"r"(eid)` (some register).
* And many other inputs were `0`, which the compiler can supply using the zero register `x0` without emitting instructions.
* So the minimal code at the call site might only materialize a couple values.

This can make the bug look subtle when most inputs are zeros.

### Case B: change inputs to non-zero, the scramble becomes obvious

You then changed the inputs to non-zero and observed:

```asm
00000016 <sbi_call>:
      16: 00000073      ecall
      1a: 8082          ret

0000001c <putchar>:
      1c: 4805          li      a6, 0x1
      1e: 4889          li      a7, 0x2
      20: 468d          li      a3, 0x3
      22: 4711          li      a4, 0x4
      24: 4795          li      a5, 0x5
      26: 4599          li      a1, 0x6
      28: 461d          li      a2, 0x7
      2a: 00000073      ecall
      2e: 8082          ret
```

Now you can clearly see what happened:

* The compiler assigned your input values to **whatever registers it wanted**:

  * It put `1` in `a6`, `2` in `a7`, `3` in `a3`, `6` in `a1`, `7` in `a2`, etc.
* The values are not in “argument order”, because `"r"` never required them to be.
* This is legal for the inline asm constraints you gave.
* But SBI is not satisfied unless the values are in the specific registers `a0`–`a7` as defined by SBI.

So `ecall` still happens, but it is not an SBI call with the intended meaning.

## Why `sbi_call` itself still shows `ecall; ret`

You might wonder: “Why didn’t `sbi_call` show any register moves then?”

Because the important scrambling often happens at the inlined call site (`putchar`).

If the compiler inlines `sbi_call`, it will prepare registers directly in the caller, then emit `ecall`. If your constraints do not demand specific registers, it will prepare them arbitrarily.

## Correct approach: tell the compiler the fixed-register ABI

Pinning is one straightforward way:

```c
register long a0 asm("a0") = arg0;
register long a1 asm("a1") = arg1;
register long a2 asm("a2") = arg2;
register long a3 asm("a3") = arg3;
register long a4 asm("a4") = arg4;
register long a5 asm("a5") = arg5;
register long a6 asm("a6") = fid;
register long a7 asm("a7") = eid;

asm volatile("ecall"
             : "+r"(a0), "+r"(a1)
             : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
             : "memory");
```

This encodes the real requirement: values must be in `a0`–`a7` at `ecall`.

## Takeaway

SBI is defined in terms of fixed registers at `ecall` time.
If you use only `"r"` operands, the compiler can legally assign inputs to arbitrary registers (and often uses `x0` for zero constants), producing assembly that looks “optimized” but breaks the SBI ABI.
