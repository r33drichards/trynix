#include <stdio.h>
#include <stdint.h>
#include <setjmp.h>
#include <ffi.h>
static jmp_buf env;
static void target(uint32_t value) { longjmp(env,value); }
int main(void) {
 int result=setjmp(env);
 if(!result) {
  ffi_cif cif;ffi_type *types[]={&ffi_type_uint32};uint32_t x=7;void *values[]={&x};
  if(ffi_prep_cif(&cif,FFI_DEFAULT_ABI,1,&ffi_type_void,types)!=FFI_OK)return 2;
  ffi_call(&cif,FFI_FN(target),NULL,values);
  return 3;
 }
 printf("ffi jump=%d\n",result);return result==7?0:1;
}
