#include <vx_intrinsics.h>
#include "common.h"

int main() {
	kernel_arg_t* __UNIFORM__ arg = (kernel_arg_t*)csr_read(VX_CSR_MSCRATCH);
	uint32_t count    = arg->count;
	uint32_t* dst_ptr = (uint32_t*)arg->dst_addr;

	uint32_t offset = vx_core_id() * count;

	for (uint32_t i = 0; i < count; ++i) {
		dst_ptr[offset + i] = NONCE + i;
	}

	return 0;
}
