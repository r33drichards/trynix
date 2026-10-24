/* SPDX-License-Identifier: LGPL-2.0-or-later */
/*
 * Guest C-stack switching shims for the WASIX coroutine backend.
 *
 * WASIX execution contexts give each context its own *native* (engine) call
 * stack, but the guest C stack lives in linear memory and is addressed by the
 * module-level mutable __stack_pointer global, which is shared by every
 * context in the store. Without switching it, N concurrently suspended
 * coroutines consume one stack region until it runs off the end.
 *
 * These shims are frameless and touch no C locals: in Wasm, return addresses
 * live in the engine call stack rather than in linear memory, so replacing
 * __stack_pointer around a host call is safe provided the shim itself never
 * references a frame slot after the switch.
 */
	.globaltype	__stack_pointer, i32
	.text
	.functype	wasix_context_switch (i64) -> (i32)

	.globl	wasix_sp_get
	.type	wasix_sp_get,@function
wasix_sp_get:
	.functype	wasix_sp_get () -> (i32)
	global.get	__stack_pointer
	end_function

	/* int wasix_sp_ctx_switch(void **save_slot, void *new_sp,
	 *                         unsigned long long target_context);
	 * Saves the caller's stack pointer through save_slot, installs new_sp,
	 * performs the WASIX context switch, and on resume restores the pointer
	 * saved in save_slot before returning the syscall result. */
	.globl	wasix_sp_ctx_switch
	.type	wasix_sp_ctx_switch,@function
wasix_sp_ctx_switch:
	.functype	wasix_sp_ctx_switch (i32, i32, i64) -> (i32)
	local.get	0
	global.get	__stack_pointer
	i32.store	0
	local.get	1
	global.set	__stack_pointer
	local.get	2
	call	wasix_context_switch
	local.get	0
	i32.load	0
	global.set	__stack_pointer
	end_function
