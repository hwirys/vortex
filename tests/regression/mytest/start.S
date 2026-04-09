#include <VX_config.h>
#include <VX_types.h>

.section .init, "ax"
.global _start
.type _start, @function
_start:
  call main
  call vx_perf_dump
  .insn r 0x0b, 0, 0, x0, x0, x0
.size _start, .-_start
