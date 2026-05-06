// Adapted from tests/regression/sgemm/main.cpp — runs in caller-managed device.
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <vortex.h>
#include "../sgemm/common.h"

#define FLOAT_ULP 6

namespace {

template <typename T>
T gen() { return static_cast<T>(rand()) / RAND_MAX; }

bool fp_close(float a, float b) {
    union fi { float f; int32_t i; };
    fi fa{a}, fb{b};
    return std::abs(fa.i - fb.i) <= FLOAT_ULP;
}

void matmul_cpu(TYPE *out, const TYPE *A, const TYPE *B, uint32_t n) {
    for (uint32_t r = 0; r < n; ++r)
        for (uint32_t c = 0; c < n; ++c) {
            TYPE s = 0;
            for (uint32_t e = 0; e < n; ++e) s += A[r*n+e]*B[e*n+c];
            out[r*n+c] = s;
        }
}

}

extern "C" int run_sgemm(vx_device_h device, int n, const char *kernel_file, double *kernel_ms) {
    std::srand(50);
    uint32_t size = (uint32_t)n;
    uint32_t sq = size * size;
    uint32_t bsz = sq * sizeof(TYPE);

    vx_buffer_h A=nullptr, B=nullptr, C=nullptr, krnl=nullptr, args=nullptr;
    auto _cleanup = [&]() {
        if (A) vx_mem_free(A);
        if (B) vx_mem_free(B);
        if (C) vx_mem_free(C);
        if (krnl) vx_mem_free(krnl);
        if (args) vx_mem_free(args);
    };
#define CK(e) do { int r=(e); if (r!=0) { _cleanup(); return -1; } } while(0)

    kernel_arg_t kar{};
    kar.grid_dim[0] = size; kar.grid_dim[1] = size; kar.size = size;

    CK(vx_mem_alloc(device, bsz, VX_MEM_READ, &A));  CK(vx_mem_address(A, &kar.A_addr));
    CK(vx_mem_alloc(device, bsz, VX_MEM_READ, &B));  CK(vx_mem_address(B, &kar.B_addr));
    CK(vx_mem_alloc(device, bsz, VX_MEM_WRITE, &C)); CK(vx_mem_address(C, &kar.C_addr));

    std::vector<TYPE> hA(sq), hB(sq), hC(sq);
    for (uint32_t i = 0; i < sq; ++i) { hA[i] = gen<TYPE>(); hB[i] = gen<TYPE>(); }

    CK(vx_copy_to_dev(A, hA.data(), 0, bsz));
    CK(vx_copy_to_dev(B, hB.data(), 0, bsz));
    CK(vx_upload_kernel_file(device, kernel_file, &krnl));
    CK(vx_upload_bytes(device, &kar, sizeof(kar), &args));

    auto t0 = std::chrono::high_resolution_clock::now();
    CK(vx_start(device, krnl, args));
    CK(vx_ready_wait(device, VX_MAX_TIMEOUT));
    auto t1 = std::chrono::high_resolution_clock::now();
    *kernel_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    CK(vx_copy_from_dev(hC.data(), C, 0, bsz));

    std::vector<TYPE> hRef(sq);
    matmul_cpu(hRef.data(), hA.data(), hB.data(), size);
    int errors = 0;
    for (uint32_t i = 0; i < sq; ++i) if (!fp_close(hC[i], hRef[i])) ++errors;

    _cleanup();
    return errors;
#undef CK
}
