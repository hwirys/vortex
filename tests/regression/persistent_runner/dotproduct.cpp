#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <vortex.h>
#include "../dotproduct/common.h"

namespace {
template <typename T> T gen() { return static_cast<T>(rand()) / RAND_MAX; }
bool fp_close(float a, float b) {
    union fi { float f; int32_t i; }; fi fa{a}, fb{b};
    return std::abs(fa.i - fb.i) <= 6;
}
}

extern "C" int run_dotproduct(vx_device_h device, int n, const char *kernel_file, double *kernel_ms) {
    std::srand(50);
    uint32_t num_points = (uint32_t)n;
    uint32_t bsz = num_points * sizeof(TYPE);
    const uint32_t threadsPerBlock = 8;
    const uint32_t blocksPerGrid = (num_points + threadsPerBlock - 1) / threadsPerBlock;
    uint32_t dst_bsz = blocksPerGrid * sizeof(TYPE);

    vx_buffer_h src0=nullptr, src1=nullptr, dst=nullptr, krnl=nullptr, args=nullptr;
    auto _cleanup = [&]() {
        if (src0) vx_mem_free(src0);
        if (src1) vx_mem_free(src1);
        if (dst) vx_mem_free(dst);
        if (krnl) vx_mem_free(krnl);
        if (args) vx_mem_free(args);
    };
#define CK(e) do { int r=(e); if (r!=0) { _cleanup(); return -1; } } while(0)

    kernel_arg_t kar{};
    kar.num_points = num_points;
    kar.block_dim[0] = threadsPerBlock;
    kar.grid_dim[0] = blocksPerGrid;

    CK(vx_mem_alloc(device, bsz, VX_MEM_READ, &src0));     CK(vx_mem_address(src0, &kar.src0_addr));
    CK(vx_mem_alloc(device, bsz, VX_MEM_READ, &src1));     CK(vx_mem_address(src1, &kar.src1_addr));
    CK(vx_mem_alloc(device, dst_bsz, VX_MEM_WRITE, &dst)); CK(vx_mem_address(dst, &kar.dst_addr));

    std::vector<TYPE> h0(num_points), h1(num_points), hd(blocksPerGrid);
    for (uint32_t i = 0; i < num_points; ++i) { h0[i] = gen<TYPE>(); h1[i] = gen<TYPE>(); }
    CK(vx_copy_to_dev(src0, h0.data(), 0, bsz));
    CK(vx_copy_to_dev(src1, h1.data(), 0, bsz));
    CK(vx_upload_kernel_file(device, kernel_file, &krnl));
    CK(vx_upload_bytes(device, &kar, sizeof(kar), &args));

    auto t0 = std::chrono::high_resolution_clock::now();
    CK(vx_start(device, krnl, args));
    CK(vx_ready_wait(device, VX_MAX_TIMEOUT));
    auto t1 = std::chrono::high_resolution_clock::now();
    *kernel_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    CK(vx_copy_from_dev(hd.data(), dst, 0, dst_bsz));

    TYPE ref = 0, cur = 0;
    for (uint32_t i = 0; i < num_points; ++i) ref += h0[i] * h1[i];
    for (uint32_t i = 0; i < blocksPerGrid; ++i) cur += hd[i];

    int errors = fp_close(cur, ref) ? 0 : 1;
    _cleanup();
    return errors;
#undef CK
}
