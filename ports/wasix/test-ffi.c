#include <ffi.h>
#include <stdint.h>
#include <stdio.h>
static uint64_t add(uint32_t a, uint64_t b){return a+b;}
int main(void){ffi_cif cif; ffi_type *t[]={&ffi_type_uint32,&ffi_type_uint64};uint32_t a=7;uint64_t b=0x100000000ULL, r=0;void *v[]={&a,&b};if(ffi_prep_cif(&cif,FFI_DEFAULT_ABI,2,&ffi_type_uint64,t)!=FFI_OK)return 1;ffi_call(&cif,FFI_FN(add),&r,v);printf("ffi result: %llu\n",(unsigned long long)r);return r!=0x100000007ULL;}
