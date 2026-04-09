#include <iostream>
#include <vector>
#include <string.h>
#include <vortex.h>
#include "common.h"

#define RT_CHECK(_expr)                                         \
   do {                                                         \
     int _ret = _expr;                                          \
     if (0 == _ret) break;                                      \
     printf("Error: '%s' returned %d!\n", #_expr, (int)_ret);   \
     cleanup();                                                  \
     exit(-1);                                                   \
   } while (false)

vx_device_h device = nullptr;
vx_buffer_h dst_buffer = nullptr;
vx_buffer_h krnl_buffer = nullptr;
vx_buffer_h args_buffer = nullptr;

void cleanup() {
  if (device) {
    vx_mem_free(dst_buffer);
    vx_mem_free(krnl_buffer);
    vx_mem_free(args_buffer);
    vx_dev_close(device);
  }
}

int main(int argc, char **argv) {
  uint32_t count = 64;

  // open device
  RT_CHECK(vx_dev_open(&device));

  uint32_t buf_size = count * sizeof(uint32_t);

  // allocate destination buffer on device
  RT_CHECK(vx_mem_alloc(device, buf_size, VX_MEM_READ_WRITE, &dst_buffer));

  uint64_t dst_addr;
  RT_CHECK(vx_mem_address(dst_buffer, &dst_addr));
  printf("dst_addr=0x%lx, count=%u\n", dst_addr, count);

  // clear destination
  std::vector<uint32_t> h_dst(count, 0);
  RT_CHECK(vx_copy_to_dev(dst_buffer, h_dst.data(), 0, buf_size));

  // upload kernel
  RT_CHECK(vx_upload_kernel_file(device, "kernel.vxbin", &krnl_buffer));

  // setup kernel arguments
  kernel_arg_t kernel_arg = {};
  kernel_arg.count = count;
  kernel_arg.dst_addr = dst_addr;
  RT_CHECK(vx_upload_bytes(device, &kernel_arg, sizeof(kernel_arg), &args_buffer));

  // start execution
  printf("start execution\n");
  RT_CHECK(vx_start(device, krnl_buffer, args_buffer));
  RT_CHECK(vx_ready_wait(device, VX_MAX_TIMEOUT));
  printf("execution completed\n");

  // read back results
  RT_CHECK(vx_copy_from_dev(h_dst.data(), dst_buffer, 0, buf_size));

  // verify
  int errors = 0;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t expected = NONCE + i;
    if (h_dst[i] != expected) {
      printf("*** error: [%d] expected=0x%x, actual=0x%x\n", i, expected, h_dst[i]);
      if (++errors > 8) break;
    }
  }

  // dump perf counters
  vx_dump_perf(device, stdout);

  cleanup();

  if (errors) {
    printf("FAILED (%d errors)\n", errors);
    return 1;
  }
  printf("PASSED!\n");
  return 0;
}
