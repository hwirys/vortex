ROOT_DIR := $(realpath ../../..)
include $(ROOT_DIR)/config.mk

PROJECT := persistent_runner
SRC_DIR := $(VORTEX_HOME)/tests/regression/$(PROJECT)

# Reuse kernel.vxbin from per-test directories; this binary only needs the host driver.
SRCS := $(SRC_DIR)/runner.cpp $(SRC_DIR)/sgemm.cpp $(SRC_DIR)/vecadd.cpp \
        $(SRC_DIR)/dotproduct.cpp $(SRC_DIR)/sgemv.cpp $(SRC_DIR)/conv3.cpp

VORTEX_RT_PATH ?= $(ROOT_DIR)/runtime

CXXFLAGS += -std=c++17 -Wall -Wextra -Wfatal-errors
CXXFLAGS += -I$(VORTEX_HOME)/runtime/include -I$(VORTEX_HOME)/runtime/common -I$(ROOT_DIR)/hw -O2

LDFLAGS  += -L$(VORTEX_RT_PATH) -lvortex
LDFLAGS  += -Wl,-rpath,$(VORTEX_RT_PATH)

.PHONY: all clean run-xrt

all: $(PROJECT)

$(PROJECT): $(SRCS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

run-xrt: $(PROJECT)
	SCOPE_JSON_PATH=$(FPGA_BIN_DIR)/scope.json \
	  XRT_INI_PATH=$(VORTEX_RT_PATH)/xrt/xrt.ini \
	  EMCONFIG_PATH=$(FPGA_BIN_DIR) \
	  XRT_DEVICE_INDEX=$(XRT_DEVICE_INDEX) \
	  XRT_XCLBIN_PATH=$(FPGA_BIN_DIR)/vortex_afu.xclbin \
	  LD_LIBRARY_PATH=$(XILINX_XRT)/lib:$(VORTEX_RT_PATH):$(LD_LIBRARY_PATH) \
	  VORTEX_DRIVER=xrt \
	  ./$(PROJECT)

clean:
	rm -f $(PROJECT) *.o
