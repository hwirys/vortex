#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <vortex.h>
#include "../sgemv/common.h"

namespace {
void sgemv_cpu(float *y, const float *A, const float *x, uint32_t M, uint32_t N) {
    for (uint32_t i = 0; i < M; ++i) {
        float s = 0; for (uint32_t j = 0; j < N; ++j) s += A[i*N+j] * x[j];
        y[i] = s;
    }
}
}

extern "C" int run_sgemv(vx_device_h device, int n, const char *kernel_file, double *kernel_ms) {
    std::srand(50);
    uint32_t M = (uint32_t)n, N = (uint32_t)n;
    uint32_t A_size = M*N*sizeof(float), x_size = N*sizeof(float), y_size = M*sizeof(float);

    vx_buffer_h A=nullptr, x=nullptr, y=nullptr, krnl=nullptr, args=nullptr;
    auto _cleanup = [&]() {
        if (A) vx_mem_free(A); if (x) vx_mem_free(x); if (y) vx_mem_free(y);
        if (krnl) vx_mem_free(krnl); if (args) vx_mem_free(args);
    };
#define CK(e) do { int r=(e); if (r!=0) { _cleanup(); return -1; } } while(0)

    kernel_arg_t kar{};
    kar.grid_dim[0] = M; kar.grid_dim[1] = 1; kar.M = M; kar.N = N;

    CK(vx_mem_alloc(device, A_size, VX_MEM_READ, &A));   CK(vx_mem_address(A, &kar.A_addr));
    CK(vx_mem_alloc(device, x_size, VX_MEM_READ, &x));   CK(vx_mem_address(x, &kar.x_addr));
    CK(vx_mem_alloc(device, y_size, VX_MEM_WRITE, &y));  CK(vx_mem_address(y, &kar.y_addr));

    std::vector<float> hA(M*N), hx(N), hy(M);
    for (auto &v : hA) v = (float)std::rand() / RAND_MAX;
    for (auto &v : hx) v = (float)std::rand() / RAND_MAX;
    CK(vx_copy_to_dev(A, hA.data(), 0, A_size));
    CK(vx_copy_to_dev(x, hx.data(), 0, x_size));
    CK(vx_upload_kernel_file(device, kernel_file, &krnl));
    CK(vx_upload_bytes(device, &kar, sizeof(kar), &args));

    auto t0 = std::chrono::high_resolution_clock::now();
    CK(vx_start(device, krnl, args));
    CK(vx_ready_wait(device, VX_MAX_TIMEOUT));
    auto t1 = std::chrono::high_resolution_clock::now();
    *kernel_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    CK(vx_copy_from_dev(hy.data(), y, 0, y_size));
    std::vector<float> hRef(M);
    sgemv_cpu(hRef.data(), hA.data(), hx.data(), M, N);
    int errors = 0;
    for (uint32_t i = 0; i < M; ++i) if (std::fabs(hy[i] - hRef[i]) > 1e-3f) ++errors;
    _cleanup();
    return errors;
#undef CK
}
