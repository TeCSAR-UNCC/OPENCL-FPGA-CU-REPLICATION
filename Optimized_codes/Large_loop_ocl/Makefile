COMMON_REPO := ../../../

# wide Memory Access Application
include ${COMMON_REPO}/utility/boards.mk
include ${COMMON_REPO}/libs/xcl2/xcl2.mk
include ${COMMON_REPO}/libs/opencl/opencl.mk
# cnn Host Application
host_SRCS=./src/host.cpp $(xcl2_SRCS)
host_HDRS=$(xcl2_HDRS)
host_CXXFLAGS=-g -O0 -I./src/ $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS)
host_LDFLAGS=$(opencl_LDFLAGS)
host_CXXFLAGS+=-Wall

EXES=host

# cnn Kernels
cnn_GOOD_SRCS=./src/cnn_convolution.cl
cnn_GOOD_CLFLAGS=-g -k cnn -I./src/
cnn_BAD_SRCS=./src/cnn_convolution_bad.cl
cnn_BAD_CLFLAGS=-g -k cnn -I./src/
cnn_BAD_LDCLFLAGS = --nk cnn:8
cnn_BAD_NDEVICES=xilinx:vcu1525:dynamic xilinx_vcu1525_dynamic_5_0
XOS=cnn_BAD

# cnn xclbins

cnn_GOOD_XOS=cnn_GOOD


cnn_BAD_XOS=cnn_BAD

XCLBINS=cnn_BAD

# check
check_EXE=host
check_XCLBINS=cnn_BAD
check_ARGS =  8
#cnn_GOOD  
check_NTARGETS=$(cnn_BAD_NDEVICES)

CHECKS=check

include $(COMMON_REPO)/utility/rules.mk
