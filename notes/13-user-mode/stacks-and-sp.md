# Stack Pointer and Stack Management in User Mode

This note explains how the stack pointer (`sp`) is managed across kernel and user mode, addressing questions about the [User Mode](https://operating-system-in-1000-lines.vercel.app/en/13-user-mode) chapter.

---

## The Three Stacks

This OS uses three different stacks:

| Stack | Defined In | Size | Purpose |
|-------|------------|------|---------|
| Global kernel stack | `kernel.ld` | 128KB | Boot-time kernel execution |
| Per-process kernel stack | `struct process.stack` | 8KB | Syscall/trap handling for each process |
| User stack | `user.ld` (in user image) | 64KB | User-mode execution |

### Why is user stack not in `struct process`?

The user stack lives in the **user's virtual address space**, not the kernel's. It's embedded in the user binary via the linker script and mapped through the page table. The kernel stack must be in `struct process` because the kernel needs direct access to it during traps.

---

## How `sp` Changes During Process Execution

```
Boot
  │  sp = global kernel stack (kernel.ld)
  ▼
yield()
  │  satp = process->page_table   ← Page table activated
  │  sscratch = process kernel stack top
  ▼
switch_context()
  │  sp = per-process kernel stack (proc->stack)
  ▼
user_entry() → sret
  │  sp = still per-process kernel stack
  ▼
user start()
  │  sp = user stack (user.ld)    ← First user instruction sets this
  ▼
syscall (ecall)
  │  csrrw sp, sscratch, sp       ← Atomic swap
  │  sp = per-process kernel stack
  │  sscratch = user stack (saved)
  ▼
handle_trap()
  │  sp = per-process kernel stack
  ▼
sret
  │  sp = user stack (restored)
```

The key instruction is `csrrw sp, sscratch, sp` in `kernel_entry`, which atomically swaps `sp` and `sscratch`. This is how the kernel switches from user stack to kernel stack during trap entry.

---

## `ret` vs `sret`

| Aspect | `ret` | `sret` |
|--------|-------|--------|
| Type | Pseudo-instruction (`jalr x0, ra, 0`) | Privileged instruction |
| Jumps to | Address in `ra` register | Address in `sepc` CSR |
| Privilege change | No | Yes (S-mode → U-mode) |
| Use case | Normal function return | Return from trap / enter user mode |

`sret` is the **only** way to transition from S-mode to U-mode. This is a security feature—privilege can only be dropped via controlled return instructions (`sret`, `mret`), not by arbitrary writes.

---

## Why a Single `sp` Register?

You might wonder: why not have separate registers for kernel stack and user stack?

**RISC philosophy**: The CPU doesn't understand "kernel stack" vs "user stack"—these are OS abstractions. Hardware stays simple; software handles complexity.

**Uniformity**: All stack operations (`push`, `pop`, `call`, `ret`) use the same `sp`. Multiple stack registers would require new instructions and complex hardware logic.

**The `sscratch` compromise**: RISC-V provides one extra register (`sscratch`) specifically for the trap-entry stack swap. This minimal addition solves the problem without bloating the register file.

**OS flexibility**: Different OSes organize stacks differently. A single `sp` lets each OS implement its own policy.

---

## Summary

- The kernel maintains separate stacks for boot, per-process kernel work, and user execution
- `sp` transitions between stacks via `switch_context` (kernel↔kernel) and `csrrw` (kernel↔user)
- `sret` is the only way to drop from S-mode to U-mode
- A single `sp` register keeps hardware simple; `sscratch` provides the minimal assist needed for trap entry
