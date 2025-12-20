# Operand Constraints in Inline Assembly

This note expands on the [inline assembly section](https://operating-system-in-1000-lines.vercel.app/en/02-assembly#inline-assembly) of *Operating System in 1000 Lines*, focusing on **operand constraints** and how the compiler interprets them.

The main idea is simple:

> The compiler does not understand your assembly template. It only understands the constraints you attach to operands.

If the constraints do not match what the assembly actually does, the compiler will optimize under false assumptions and you will get broken code.

---

## Inline assembly syntax refresher

```c
asm volatile ("template"
              : outputs
              : inputs
              : clobbers);
```

* **template**: a string the compiler does not parse for meaning
* **outputs/inputs**: a contract about dataflow between C and the assembly
* **clobbers**: a contract about extra side effects (registers, memory)

---

## The three most important operand modes

Most of the time you will use one of these:

| Form      | Name              | What you are promising the compiler                              |
| --------- | ----------------- | ---------------------------------------------------------------- |
| `"r"(x)`  | Input-only        | Assembly reads `x`. Assembly does not write back to `x`.         |
| `"=r"(x)` | Output-only       | Assembly writes a new value into `x`. Old `x` is irrelevant.     |
| `"+r"(x)` | Read-modify-write | Assembly reads the old value of `x` and writes an updated value. |

### The compiler takes this contract literally

If you say `"r"(x)`, the compiler is allowed to assume `x` never changes because of the asm.
If you say `"=r"(x)`, the compiler is allowed to assume the old value of `x` is not needed at all.
If you say `"+r"(x)`, the compiler must treat `x` as both an input and an output.

---

## Example 1: Missing output

### Buggy code (lies about writing)

```c
int buggy(int x) {
  int before = x;
  asm volatile ("addi %0, %0, 1"
                :
                : "r"(x));       // says: x is input-only
  return before + x;
}
```

What is wrong?

* `"r"(x)` claims the assembly only reads `x`.
* But `addi %0, %0, 1` updates the register holding `%0`.
* The compiler still believes `x` is unchanged, so it may reuse a stale value or even precompute expressions involving `x`.

### Correct code

```c
int ok(int x) {
  int before = x;
  asm volatile ("addi %0, %0, 1"
                : "+r"(x));      // says: x is read and written
  return before + x;
}
```

Now the compiler must treat `x` as updated after the asm.

### What the optimizer does (conceptually)

When compiled with optimizations, a typical outcome looks like this:

**buggy() can return the wrong result**

```asm
buggy:
    slli  a1, a0, 1       # a1 = 2x (compiler precomputes before + x)
    addi  a0, a0, 1       # asm changes a0, but compiler does not care
    mv    a0, a1          # compiler overwrites the asm result
    ret
```

**ok() returns the correct result**

```asm
ok:
    mv    a1, a0          # before = x
    addi  a1, a1, 1       # x becomes x+1 and compiler tracks it
    add   a0, a0, a1      # x + (x+1)
    ret
```

The important lesson is not the exact registers. The lesson is that the compiler will freely reorder and reuse values based on your constraints.

---

## Example 2: Missing input

### Buggy code (lies about reading)

```c
__attribute__((noinline))
int helper(int v) {
  return v * 2;
}

int buggy2(int x) {
  int y = helper(x);
  asm volatile ("addi %0, %0, 1"
                : "=r"(x));      // says: x is output-only
  return x - y;
}
```

What is wrong?

* `"=r"(x)` claims the old value of `x` is irrelevant.
* The compiler is allowed to provide an uninitialized register for `%0`.
* But `addi %0, %0, 1` reads `%0` before writing it, so it can read garbage.

### Correct code

```c
int ok2(int x) {
  int y = helper(x);
  asm volatile ("addi %0, %0, 1"
                : "+r"(x));      // says: x must be preserved and updated
  return x - y;
}
```

With `"+r"(x)`, the compiler knows it must keep `x` alive across the call to `helper()`.

### What the optimizer does (conceptually)

**buggy2() can use garbage**

```asm
buggy2:
    call  helper          # clobbers caller-saved registers
    addi  a1, a1, 1       # asm reads a1, but it was never initialized
    sub   a0, a1, a0
    ret
```

**ok2() preserves x**

```asm
ok2:
    mv    s0, a0          # save x in a callee-saved register
    call  helper
    addi  s0, s0, 1       # x = x+1
    sub   a0, s0, a0
    ret
```

Again, the details vary. The principle is stable:

* With `"=r"`, the compiler may discard the old value.
* With `"+r"`, the compiler must preserve it.

---

## Alternative pattern: separate input and output operands

Sometimes your assembly reads one value and writes to a different destination. In that case, do not force read-modify-write. Use separate operands:

```c
asm volatile ("addi %0, %1, 1"
              : "=r"(out)
              : "r"(in));
```

* `%0` is written only
* `%1` is read only
* the compiler sees accurate dataflow

---

## How to sanity-check your constraints

A quick workflow:

```bash
clang --target=riscv32-unknown-elf -march=rv32imac -mabi=ilp32 -O2 -g \
  -c file.c -o file.o

llvm-objdump -d -S file.o
```

When reading the disassembly, look for this:

* Does the compiler preserve values you need preserved across function calls?
* Does it reuse values as if they were unchanged after your asm?
* Does it materialize inputs before the asm, or does it “skip” them because it thinks they are not needed?

If the compiler’s behavior surprises you, the first thing to re-check is the constraints.

---

## Summary and the golden rule

| Constraint | Compiler assumption        | Typical failure if wrong      |
| ---------- | -------------------------- | ----------------------------- |
| `"r"(x)`   | `x` is not modified by asm | compiler ignores your updates |
| `"=r"(x)`  | old `x` is not needed      | asm reads garbage             |
| `"+r"(x)`  | asm reads and writes `x`   | correct for read-modify-write |

**Golden rule:** constraints must match the actual reads and writes performed by the assembly template. The compiler trusts your contract completely.
