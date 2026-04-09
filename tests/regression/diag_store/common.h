#ifndef _COMMON_H_
#define _COMMON_H_

#define NONCE 0xCAFEBABE

typedef struct {
  uint32_t count;
  uint64_t dst_addr;
} kernel_arg_t;

#endif
