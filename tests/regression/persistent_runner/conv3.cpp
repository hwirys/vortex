#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <vortex.h>
#include "../conv3/common.h"

namespace {
template <typename T> T gen() { return static_cast<T>(rand()) / RAND_MAX; }
bool fp_close(float a, float b) {
    union fi { float f; int32_t i; }; fi fa{a}, fb{b};
    return std::abs(fa.i - fb.i) <= 6;
}
void conv_cpu(TYPE *O, TYPE *I, TYPE *W, int32_t w, int32_t h) {
    int pw = w + 2;
    for (int32_t y = 0; y < h; ++y) for (int32_t x = 0; x < w; ++x) {
        int py = y + 1, px = x + 1;
        TYPE s = 0;
        for (int32_t ky = -1; ky <= 1; ++ky) for (int32_t kx = -1; kx <= 1; ++kx)
            s += I[(py+ky)*pw + (px+kx)] * W[(ky+1)*3 + (kx+1)];
        O[y*w + x] = s;
    }
}
}

extern "C" int run_conv3(vx_device_h device, int n, const char *kernel_file, double *kernel_ms) {
    std::srand(50);
    int size = n;
    uint32_t op = size*size, ip = (size+2)*(size+2), wp = 9;
    size_t in = ip*sizeof(TYPE), wn = wp*sizeof(TYPE), on = op*sizeof(TYPE);

    vx_buffer_h I=nullptr, W=nullptr, O=nullptr, krnl=nullptr, args=nullptr;
    auto _cleanup = [&]() {
        if (I) vx_mem_free(I); if (W) vx_mem_free(W); if (O) vx_mem_free(O);
        if (krnl) vx_mem_free(krnl); if (args) vx_mem_free(args);
    };
#define CK(e) do { int r=(e); if (r!=0) { _cleanup(); return -1; } } while(0)

    kernel_arg_t kar{};
    kar.grid_dim[0] = size; kar.grid_dim[1] = size;
    kar.width = size; kar.use_lmem = false;

    CK(vx_mem_alloc(device, in, VX_MEM_READ, &I));   CK(vx_mem_address(I, &kar.I_addr));
    CK(vx_mem_alloc(device, wn, VX_MEM_READ, &W));   CK(vx_mem_address(W, &kar.W_addr));
    CK(vx_mem_alloc(device, on, VX_MEM_WRITE, &O));  CK(vx_mem_address(O, &kar.O_addr));

    std::vector<TYPE> hI(ip, 0), hW(wp), hO(op);
    for (int32_t y = 0; y < size; ++y) for (int32_t x = 0; x < size; ++x)
        hI[(y+1)*(size+2) + (x+1)] = gen<TYPE>();
    for (uint32_t i = 0; i < wp; ++i) hW[i] = gen<TYPE>();

    CK(vx_copy_to_dev(I, hI.data(), 0, in));
    CK(vx_copy_to_dev(W, hW.data(), 0, wn));
    CK(vx_upload_kernel_file(device, kernel_file, &krnl));
    CK(vx_upload_bytes(device, &kar, sizeof(kar), &args));

    auto t0 = std::chrono::high_resolution_clock::now();
    CK(vx_start(device, krnl, args));
    CK(vx_ready_wait(device, VX_MAX_TIMEOUT));
    auto t1 = std::chrono::high_resolution_clock::now();
    *kernel_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    CK(vx_copy_from_dev(hO.data(), O, 0, on));
    std::vector<TYPE> hRef(op);
    conv_cpu(hRef.data(), hI.data(), hW.data(), size, size);
    int errors = 0;
    for (uint32_t i = 0; i < op; ++i) if (!fp_close(hO[i], hRef[i])) ++errors;
    _cleanup();
    return errors;
#undef CK
}
