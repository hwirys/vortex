#include <iostream>
#include <vector>
#include <vortex.h>
#include "common.h"

#define RT_CHECK(_expr) do { \
    int _ret = _expr; \
    if (_ret != 0) { \
      printf("Error: '%s' returned %d\n", #_expr, _ret); \
      cleanup(); \
      exit(-1); \
    } \
} while(0)

vx_device_h device = nullptr;
vx_buffer_h src_buffer = nullptr;
vx_buffer_h dst_buffer = nullptr;
vx_buffer_h krnl_buffer = nullptr;
vx_buffer_h args_buffer = nullptr;

void cleanup() {
  if (device) {
    vx_mem_free(src_buffer);
    vx_mem_free(dst_buffer);
    vx_mem_free(krnl_buffer);
    vx_mem_free(args_buffer);
    vx_dev_close(device);
  }
}

int main() {
  uint32_t count = 256;
  uint32_t buf_size = count * sizeof(int32_t);

  RT_CHECK(vx_dev_open(&device));

  RT_CHECK(vx_mem_alloc(device, buf_size, VX_MEM_READ_WRITE, &src_buffer));
  RT_CHECK(vx_mem_alloc(device, buf_size, VX_MEM_READ_WRITE, &dst_buffer));

  uint64_t src_addr, dst_addr;
  RT_CHECK(vx_mem_address(src_buffer, &src_addr));
  RT_CHECK(vx_mem_address(dst_buffer, &dst_addr));

  // prepare source data
  std::vector<int32_t> h_src(count);
  for (uint32_t i = 0; i < count; ++i) h_src[i] = i + 1;
  RT_CHECK(vx_copy_to_dev(src_buffer, h_src.data(), 0, buf_size));

  // upload kernel
  RT_CHECK(vx_upload_kernel_file(device, "kernel.vxbin", &krnl_buffer));

  // upload args
  kernel_arg_t arg = {};
  arg.count = count;
  arg.src_addr = src_addr;
  arg.dst_addr = dst_addr;
  RT_CHECK(vx_upload_bytes(device, &arg, sizeof(arg), &args_buffer));

  // execute
  printf("start execution\n");
  RT_CHECK(vx_start(device, krnl_buffer, args_buffer));
  RT_CHECK(vx_ready_wait(device, VX_MAX_TIMEOUT));
  printf("execution completed\n");

  // verify
  std::vector<int32_t> h_dst(count);
  RT_CHECK(vx_copy_from_dev(h_dst.data(), dst_buffer, 0, buf_size));

  int errors = 0;
  for (uint32_t i = 0; i < count; ++i) {
    int32_t expected = h_src[i] * 2;
    if (h_dst[i] != expected) {
      printf("error [%d]: expected=%d, got=%d\n", i, expected, h_dst[i]);
      if (++errors > 8) break;
    }
  }

  vx_dump_perf(device, stdout);
  cleanup();

  printf("%s\n", errors ? "FAILED" : "PASSED");
  return errors ? 1 : 0;
}
