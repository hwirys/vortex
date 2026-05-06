// Persistent runner: opens device once, runs N regression workloads sequentially,
// times each and reports per-workload kernel + wall-clock cost.
#include <iostream>
#include <chrono>
#include <cstdio>
#include <vortex.h>

extern "C" int run_sgemm(vx_device_h, int, const char*, double*);
extern "C" int run_vecadd(vx_device_h, int, const char*, double*);
extern "C" int run_dotproduct(vx_device_h, int, const char*, double*);
extern "C" int run_sgemv(vx_device_h, int, const char*, double*);
extern "C" int run_conv3(vx_device_h, int, const char*, double*);

struct Workload {
    const char *name;
    int (*fn)(vx_device_h, int, const char*, double*);
    int n;
    const char *kernel_file;
};

int main() {
    Workload tests[] = {
        {"sgemm-256",     run_sgemm,      256,   "/home/sicdl/vortex/build/tests/regression/sgemm/kernel.vxbin"},
        {"sgemm-128",     run_sgemm,      128,   "/home/sicdl/vortex/build/tests/regression/sgemm/kernel.vxbin"},
        {"vecadd-65536",  run_vecadd,     65536, "/home/sicdl/vortex/build/tests/regression/vecadd/kernel.vxbin"},
        {"vecadd-1M",     run_vecadd,     1048576, "/home/sicdl/vortex/build/tests/regression/vecadd/kernel.vxbin"},
        {"dotprod-65536", run_dotproduct, 65536, "/home/sicdl/vortex/build/tests/regression/dotproduct/kernel.vxbin"},
        {"sgemv-256",     run_sgemv,      256,   "/home/sicdl/vortex/build/tests/regression/sgemv/kernel.vxbin"},
        {"sgemv-1024",    run_sgemv,      1024,  "/home/sicdl/vortex/build/tests/regression/sgemv/kernel.vxbin"},
        {"conv3-64",      run_conv3,      64,    "/home/sicdl/vortex/build/tests/regression/conv3/kernel.vxbin"},
        {"conv3-256",     run_conv3,      256,   "/home/sicdl/vortex/build/tests/regression/conv3/kernel.vxbin"},
    };
    constexpr int N = sizeof(tests) / sizeof(tests[0]);

    auto wall_start = std::chrono::high_resolution_clock::now();
    vx_device_h device = nullptr;
    if (vx_dev_open(&device) != 0) {
        fprintf(stderr, "vx_dev_open failed\n");
        return 1;
    }
    auto init_end = std::chrono::high_resolution_clock::now();
    double init_ms = std::chrono::duration<double, std::milli>(init_end - wall_start).count();
    printf("\n=== persistent_runner: device init=%.0fms ===\n", init_ms);
    printf("%-18s %-10s %-12s %-10s\n", "workload", "wall_ms", "kernel_ms", "result");

    for (int i = 0; i < N; ++i) {
        const Workload &w = tests[i];
        double kernel_ms = 0;
        auto ws = std::chrono::high_resolution_clock::now();
        int errors = w.fn(device, w.n, w.kernel_file, &kernel_ms);
        auto we = std::chrono::high_resolution_clock::now();
        double wall_ms = std::chrono::duration<double, std::milli>(we - ws).count();
        const char *res = (errors == 0) ? "PASS" : (errors < 0) ? "ERR" : "FAIL";
        printf("%-18s %-10.1f %-12.2f %s%s\n",
               w.name, wall_ms, kernel_ms, res, (errors > 0 ? "" : ""));
    }

    auto wall_end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(wall_end - wall_start).count();
    printf("=== total wall-clock: %.0fms ===\n", total_ms);

    vx_dev_close(device);
    return 0;
}
