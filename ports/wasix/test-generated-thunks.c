/* Focused ABI test of the actual experimental generated macro header.
 * Minimal stand-ins only for QEMU's CPU state and TCI struct Int128.
 * This does not execute tcg_qemu_tb_exec or validate a guest boot.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <setjmp.h>
#include <ffi.h>
#define HOST_BIG_ENDIAN 0
#define GLUE_RAW(a,b) a##b
#define glue(a,b) GLUE_RAW(a,b)
#define G_NORETURN __attribute__((noreturn))
#define NEED_CPU_H 1
#define TARGET_LONG_BITS 64
typedef uint64_t target_ulong;
typedef struct CPUArchState { uint64_t marker; } CPUArchState;
typedef struct Int128 { uint64_t lo; int64_t hi; } Int128;
#include "exec/helper-head.h"
#include "exec/helper-wasix.h"
_Static_assert(sizeof(void *) == 4, "Pinned wasm32 target only");
_Static_assert(sizeof(Int128) == 16, "TCI Int128 layout");
_Static_assert(offsetof(Int128, hi) == 8, "TCI Int128 layout");
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
#define NOINLINE __attribute__((noinline))
typedef void (*Thunk)(void *, void **);
static unsigned tests, void_calls;
static ffi_type *wide_parts[] = { &ffi_type_uint64, &ffi_type_uint64, NULL };
static ffi_type wide_type = { .size = 0, .alignment = 0, .type = FFI_TYPE_STRUCT, .elements = wide_parts };
static CPUArchState cpu = { UINT64_C(0x8877665544332211) };

static NOINLINE int32_t helper_signed32(int32_t a) { CHECK(a < 0); return a + 1; }
static NOINLINE int64_t helper_signed64(int64_t a) { CHECK(a < 0); return a + 1; }
static NOINLINE Int128 helper_zero(void) { return (Int128){ UINT64_C(0xfedcba9876543210), -1234567890123LL }; }
static NOINLINE void helper_void0(void) { ++void_calls; }
static NOINLINE Int128 helper_wide(Int128 a, int32_t b, Int128 c) {
    CHECK(b == -17);
    return (Int128){ a.lo ^ c.lo ^ (uint32_t)b, a.hi + c.hi + b };
}
static NOINLINE Int128 helper_seven(Int128 a, int32_t b, uint64_t c, const void *p,
                                   CPUArchState *e, int d, Int128 f) {
    CHECK(b == -123456 && d == -9 && p == &cpu && e == &cpu);
    CHECK(e->marker == UINT64_C(0x8877665544332211));
    return (Int128){ a.lo ^ c ^ f.lo ^ (uint32_t)b, a.hi + f.hi + d };
}
static NOINLINE uint32_t helper_four(uint32_t a, int32_t b, uint32_t c, int d) {
    CHECK(b == -7 && d == -23); return a ^ (uint32_t)b ^ c ^ (uint32_t)d;
}
static NOINLINE uint64_t helper_five(uint64_t a, uint32_t b, int64_t c, uint64_t d, int e) {
    CHECK(c == -121 && e == -2); return a ^ b ^ (uint64_t)c ^ d ^ (uint64_t)e;
}
static NOINLINE void *helper_six(void *a, const void *b, CPUArchState *e, uint32_t c, uint64_t d, int f) {
    CHECK(a == &cpu && b == &cpu && e == &cpu && c == 0xdeadbeefU && d == UINT64_C(0x123456789abcdef0) && f == -55);
    return a;
}
static NOINLINE float32 helper_bits32(float32 a, uint32_t b) { return a ^ b; }
static NOINLINE float64 helper_bits64(float64 a, uint64_t b) { return a ^ b; }
static NOINLINE target_ulong helper_target(target_ulong a) { return a ^ UINT64_C(0xa5a5f0f05a5a0f0f); }
static NOINLINE uint32_t helper_half(uint32_t a) { return a ^ 0xf00d; }
/* Extracted verbatim from current accel/tcg/tcg-runtime.c by run.sh. */
#include "memset-adapter.inc"
#define helper_memset wasix_helper_memset
static jmp_buf jump_env;
static volatile unsigned jump_count;
static volatile int jump_expected;
static NOINLINE G_NORETURN void helper_jump(int v) { longjmp(jump_env, v); }
static NOINLINE int32_t helper_jump_result(int v) { longjmp(jump_env, v); }

WASIX_THUNK_0(zero, i128)
WASIX_THUNK_0(void0, void)
WASIX_THUNK_1(signed32, s32, s32)
WASIX_THUNK_1(signed64, s64, s64)
WASIX_THUNK_1(target, tl, tl)
WASIX_THUNK_1(half, f16, f16)
WASIX_THUNK_2(bits32, f32, f32, i32)
WASIX_THUNK_2(bits64, f64, f64, i64)
WASIX_THUNK_3(wide, i128, i128, s32, i128)
WASIX_THUNK_3(memset, ptr, ptr, int, ptr)
WASIX_THUNK_4(four, i32, i32, s32, i32, int)
WASIX_THUNK_5(five, i64, i64, i32, s64, i64, int)
WASIX_THUNK_6(six, ptr, ptr, cptr, env, i32, i64, int)
WASIX_THUNK_7(seven, i128, i128, s32, i64, cptr, env, int, i128)
WASIX_THUNK_1(jump, noreturn, int)
WASIX_THUNK_1(jump_result, s32, int)

/* Same per-argument placement as tcg/tci.c: ceil(ffi arg size / 8).
 * Return storage is deliberately the same stack as argument zero.
 */
static NOINLINE void invoke(Thunk fn, ffi_type *ret, unsigned n, ffi_type **types,
                           void **values, const void *expected, size_t ret_size) {
    ffi_cif cif;
    uint64_t stack[32];
    void *slots[7];
    CHECK(n <= 7);
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, n, ret, types) == FFI_OK);
    memset(stack, 0xa5, sizeof stack);
    unsigned s = 0;
    for (unsigned i = 0; i < n; ++i) {
        slots[i] = &stack[s];
        memcpy(slots[i], values[i], cif.arg_types[i]->size);
        s += (cif.arg_types[i]->size + 7) / 8;
    }
    CHECK(s < 31);
    Thunk volatile call = fn;
    call(stack, slots);
    if (ret_size) CHECK(memcmp(stack, expected, ret_size) == 0);
    CHECK(stack[31] == UINT64_C(0xa5a5a5a5a5a5a5a5));
    ++tests;
}
#define RUN1(name, rt, at, x, result) do { ffi_type *t[] = {at}; void *v[]={&(x)}; invoke(wasix_thunk_##name,rt,1,t,v,&(result),sizeof(result)); } while(0)
static void test_abi(void) {
    Int128 z = { UINT64_C(0xfedcba9876543210), -1234567890123LL };
    invoke(wasix_thunk_zero, &wide_type, 0, NULL, NULL, &z, sizeof z);
    invoke(wasix_thunk_void0, &ffi_type_void, 0, NULL, NULL, NULL, 0);
    CHECK(void_calls == 1);
    int32_t s32=INT32_MIN+7, r32=s32+1;
    int64_t s64=INT64_MIN+7, r64=s64+1;
    RUN1(signed32,&ffi_type_sint32,&ffi_type_sint32,s32,r32);
    RUN1(signed64,&ffi_type_sint64,&ffi_type_sint64,s64,r64);
    target_ulong tl=UINT64_C(0x123456789abcdef0), rtl=tl ^ UINT64_C(0xa5a5f0f05a5a0f0f);
    RUN1(target,&ffi_type_uint64,&ffi_type_uint64,tl,rtl);
    uint32_t hf=0xc123, rhf=hf ^ 0xf00d;
    RUN1(half,&ffi_type_uint32,&ffi_type_uint32,hf,rhf);
    uint32_t f32=0xff812345, mask32=0x12345678, rf32=f32 ^ mask32;
    ffi_type *t2a[]={&ffi_type_uint32,&ffi_type_uint32}; void *v2a[]={&f32,&mask32};
    invoke(wasix_thunk_bits32,&ffi_type_uint32,2,t2a,v2a,&rf32,sizeof rf32);
    uint64_t f64=UINT64_C(0xfff0123456789abc), mask64=UINT64_C(0xabcdef1234567890), rf64=f64 ^ mask64;
    ffi_type *t2b[]={&ffi_type_uint64,&ffi_type_uint64}; void *v2b[]={&f64,&mask64};
    invoke(wasix_thunk_bits64,&ffi_type_uint64,2,t2b,v2b,&rf64,sizeof rf64);
    Int128 a={UINT64_C(0xfedcba9876543210),-9823791}, c={UINT64_C(0x87654321abcdef01),8912371};
    int32_t b=-17; Int128 rw={a.lo ^ c.lo ^ (uint32_t)b,a.hi+c.hi+b};
    ffi_type *t3[]={&wide_type,&ffi_type_sint32,&wide_type};void *v3[]={&a,&b,&c};
    invoke(wasix_thunk_wide,&wide_type,3,t3,v3,&rw,sizeof rw);
    uint32_t a4=0x80000001, c4=0xfedcba98;int32_t b4=-7;int d4=-23;
    uint32_t r4=a4 ^ (uint32_t)b4 ^ c4 ^ (uint32_t)d4;
    ffi_type *t4[]={&ffi_type_uint32,&ffi_type_sint32,&ffi_type_uint32,&ffi_type_sint32};void *v4[]={&a4,&b4,&c4,&d4};
    invoke(wasix_thunk_four,&ffi_type_uint32,4,t4,v4,&r4,sizeof r4);
    uint64_t a5=UINT64_C(0xf123456789abcdef),d5=UINT64_C(0x7123456789abcdef);uint32_t b5=0xfedcba98;int64_t c5=-121;int e5=-2;
    uint64_t r5=a5 ^ b5 ^ (uint64_t)c5 ^ d5 ^ (uint64_t)e5;
    ffi_type *t5[]={&ffi_type_uint64,&ffi_type_uint32,&ffi_type_sint64,&ffi_type_uint64,&ffi_type_sint32};void *v5[]={&a5,&b5,&c5,&d5,&e5};
    invoke(wasix_thunk_five,&ffi_type_uint64,5,t5,v5,&r5,sizeof r5);
    void *p=&cpu;const void *cp=&cpu;CPUArchState *env=&cpu;uint32_t a6=0xdeadbeef;uint64_t b6=UINT64_C(0x123456789abcdef0);int c6=-55;
    ffi_type *t6[]={&ffi_type_pointer,&ffi_type_pointer,&ffi_type_pointer,&ffi_type_uint32,&ffi_type_uint64,&ffi_type_sint32};void *v6[]={&p,&cp,&env,&a6,&b6,&c6};
    invoke(wasix_thunk_six,&ffi_type_pointer,6,t6,v6,&p,sizeof p);
    b=-123456;int d7=-9;Int128 r7={a.lo ^ b6 ^ c.lo ^ (uint32_t)b,a.hi+c.hi+d7};
    ffi_type *t7[]={&wide_type,&ffi_type_sint32,&ffi_type_uint64,&ffi_type_pointer,&ffi_type_pointer,&ffi_type_sint32,&wide_type};void *v7[]={&a,&b,&b6,&cp,&env,&d7,&c};
    invoke(wasix_thunk_seven,&wide_type,7,t7,v7,&r7,sizeof r7);
    unsigned char buf[35];memset(buf,0x67,sizeof buf);void *dest=buf+1;int fill=-2;void *len=(void *)(uintptr_t)33;
    ffi_type *tm[]={&ffi_type_pointer,&ffi_type_sint32,&ffi_type_pointer};void *vm[]={&dest,&fill,&len};
    invoke(wasix_thunk_memset,&ffi_type_pointer,3,tm,vm,&dest,sizeof dest);
    CHECK(buf[0]==0x67 && buf[34]==0x67);for(unsigned i=1;i<34;++i)CHECK(buf[i]==0xfe);
    len=0;invoke(wasix_thunk_memset,&ffi_type_pointer,3,tm,vm,&dest,sizeof dest);
    CHECK(buf[0]==0x67 && buf[34]==0x67);
    printf("generated macros ABI: PASS (%u cases; arities 0..7, signed, Int128, pointer, softfloat bits, overlapping slots, memset)\n",tests);
}
static void test_jumps(unsigned limit) {
    int value=setjmp(jump_env);
    if(value) CHECK(value==jump_expected);
    if(jump_count<limit) {
        uint64_t stack[2]={0};
        int arg=jump_count%3==0 ? 0 : (jump_count%3==1 ? -71 : 12345);
        jump_expected=arg ? arg : 1;
        memcpy(stack,&arg,sizeof arg);void *args[]={stack};
        Thunk volatile fn=jump_count%2 ? wasix_thunk_jump : wasix_thunk_jump_result;
        ++jump_count;fn(stack,args);CHECK(0);
    }
    printf("generated macros normal EH longjmp: PASS (%u jumps, zero/negative/positive values, noreturn/value thunks)\n",jump_count);
}
int main(int argc,char **argv) {
    unsigned count=argc>1 ? (unsigned)strtoul(argv[1],NULL,10) : 1000;
    test_abi();test_jumps(count);return 0;
}
