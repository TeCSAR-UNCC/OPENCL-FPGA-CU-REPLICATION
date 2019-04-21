/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

/*******************************************************************************
Description: 

    This is a CNN (Convolutional Neural Network) convolutional layer based
    example to showcase the effectiveness of using multiple compute units when
    the base algorithm consists of multiple nested loops with large loop count.    

*******************************************************************************/
#include <stdint.h>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <CL/cl.h>
#include "defns.h"
#include "sys/time.h"

#define WORK_GROUP 4
#define WORK_ITEM_PER_GROUP 1

#define DATA_SIZE OChan *OSize *OSize

double timestamp()
{
    double ms = 0.0;
    timeval time;
    gettimeofday(&time, NULL);
    ms = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);
    return ms;
}

uint64_t run_opencl_cnn(

    cl_device_id *devices,
    cl_command_queue cmdQueue,

    cl_context context,
    char *device_name,

    bool good,
    int size,

    int *weight,
    int *image,
    int *output,

    int i_chan,
    int o_chan)
{
    cl_uint numDevices = 0;
    cl_int status;

    cl_kernel binaryFile = NULL;
    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&binaryFile, NULL, &status);
    //cl_device_id* devices;
    if (good)
    {

        binaryFile = clCreateKernel(program, "cnn_GOOD", &status);
    }
    else
    {

        binaryFile = clCreateKernel(program, "cnn_BAD", &status);
    }

    context = clCreateContext(NULL, numDevices, devices, NULL, NULL, &status);

    double first_stamp = timestamp();
    cmdQueue = clCreateCommandQueue(context, *devices, 0, &status);

    status = clBuildProgram(program, numDevices, devices, NULL, NULL, NULL);
    cl_kernel kernel = NULL;
    kernel = clCreateKernel(program, "cnn", &status);

    std::cout << "Starting " << (good ? "GOOD" : "BAD") << " Kernel" << std::endl;

    size_t image_size_bytes = sizeof(int) * i_chan * ISize * ISize;
    size_t weight_size_bytes = sizeof(int) * o_chan * WInChan * WSize * WSize;
    size_t output_size_bytes = sizeof(int) * o_chan * OSize * OSize;

    cl_mem inBufVec;
    cl_mem outBufVec;
    cl_mem buffer_image;
    cl_mem buffer_weight;
    cl_mem buffer_output;

    buffer_image = clCreateBuffer(context, CL_MEM_READ_ONLY, image_size_bytes, NULL, &status);
    buffer_weight = clCreateBuffer(context, CL_MEM_WRITE_ONLY, weight_size_bytes, NULL, &status);
    buffer_output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, output_size_bytes, NULL, &status);
    // Allocate Buffer in Global Memory

    size_t globalWorkSize[1];
    globalWorkSize[0] = 1;
    status = clEnqueueNDRangeKernel(cmdQueue, kernel, 1, NULL, globalWorkSize, NULL, 0, NULL, NULL);

    //Set the Kernel Arguments
    int narg = 0;
    status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer_image);
    status |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &buffer_weight);
    status |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &buffer_output);
    status |= clSetKernelArg(kernel, 3, sizeof(cl_mem), &size);
    status |= clSetKernelArg(kernel, 4, sizeof(cl_mem), &i_chan);
    status |= clSetKernelArg(kernel, 5, sizeof(cl_mem), &o_chan);

    std::cout << "Begin " << (good ? "GOOD" : "BAD") << " Kernel" << std::endl;

    uint64_t duration = 0;

    cl_event *event;
    if (good)
    {
        int work_group = WORK_GROUP;
        int work_item_per_group = WORK_ITEM_PER_GROUP;

        //Set global & local grids
        const size_t *global_size = (const size_t *)work_group;
        const size_t *local_size = (const size_t *)work_item_per_group;
        status = clEnqueueNDRangeKernel(cmdQueue, kernel, 1, NULL, local_size, NULL, 0, NULL, event);
    }
    else
    {
        clEnqueueTask(cmdQueue, kernel, NULL, NULL, event);
    }
    clEnqueueReadBuffer(cmdQueue, outBufVec, CL_TRUE, 0, sizeof(size_t), NULL, 0, NULL, NULL);
    double second_stamp = timestamp();
    double time_elapsed = second_stamp - first_stamp;
    printf("\nTotal time elapsed is %fms \n", time_elapsed);

    std::cout << "Finished " << (good ? "GOOD" : "BAD") << " Kernel" << std::endl;
    return duration;
}
int main(int argc, char **argv)
{
    int i_chan = IChan;
    int o_chan = OChan;
    cl_device_id devices[100];
    int size = DATA_SIZE;
    char *device_name;
    cl_command_queue cmdQueue;
    const char *xcl_emu = getenv("XCL_EMULATION_MODE");
    if (xcl_emu && !strcmp(xcl_emu, "hw_emu"))
    {
        i_chan = 1;
        o_chan = 1;

        size = o_chan * OSize * OSize;

        printf("\nOriginal Dataset is Reduced for Faster Execution of Hardware Emulation Flow\n");
        printf("\t#Input_Channels (IChan)            = %d (Original : 96 )\n", i_chan);
        printf("\t#Weight_Output_Channels (WOutChan) = %d (Original : 256)\n\n", o_chan);
    }

    // Allocate Memory in Host (Image, Weights and Output)
    size_t image_size_bytes = sizeof(int) * i_chan * ISize * ISize;
    size_t weight_size_bytes = sizeof(int) * o_chan * WInChan * WSize * WSize;
    size_t output_size_bytes = sizeof(int) * o_chan * OSize * OSize;

    int *image;
    int *weight;
    int *source_good_hw_results;
    int *source_bad_hw_results;
    int *source_sw_results;

    image = (int *)malloc(image_size_bytes * sizeof(size_t));
    weight = (int *)malloc(weight_size_bytes * sizeof(size_t));
    source_good_hw_results = (int *)malloc(output_size_bytes * sizeof(size_t));
    source_bad_hw_results = (int *)malloc(output_size_bytes * sizeof(size_t));
    source_sw_results = (int *)malloc(output_size_bytes * sizeof(size_t));

    // Initialize Image, Weights & Output Host Buffers
    for (int i = 0; i < i_chan * ISize * ISize; i++)
        image[i] = i % 255;

    for (int i = 0; i < o_chan * WInChan * WSize * WSize; i++)
        weight[i] = i % 255;

    for (int i = 0; i < o_chan * OSize * OSize; i++)
        source_sw_results[i] = source_good_hw_results[i] = source_bad_hw_results[i] = 0;

    //------------------------------------------------------------------------------
    // STEP 1 Discover and Initialize the kernel
    //------------------------------------------------------------------------------
    cl_int status;
    cl_platform_id platforms[100];
    cl_uint platforms_n = 0;
    status = clGetPlatformIDs(100, platforms, &platforms_n);

    //OPENCL HOST CODE AREA START
    //Create Program and Kernels

    printf("=== %d OpenCL platform(s) found: ===\n", platforms_n);
    for (int i = 0; i < platforms_n; i++)
    {
        char buffer[10240];
        printf("  -- %d --\n", i);
        status = clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, 10240, buffer, NULL);
        //printf("  PROFILE = %s\n", buffer);
        status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, 10240, buffer, NULL);
        printf("  VERSION = %s\n", buffer);
        status = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 10240, buffer, NULL);
        printf("  NAME = %s\n", buffer);
        status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, 10240, buffer, NULL);
        printf("  VENDOR = %s\n", buffer);
        status = clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, 10240, buffer, NULL);
        //printf("  EXTENSIONS = %s\n", buffer);
    }
    //----------------------------------------------------------------------------------
    // STEP 2 Discover and Initialize the platform
    //-----------------------------------------------------------------------------------

    cl_uint numDevices = 0;
    status = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 100, devices, &numDevices);

    printf("=== %d OpenCL device(s) found on platform:\n", platforms_n);
    for (int i = 0; i < numDevices; i++)
    {
        char buffer[10240];
        cl_uint buf_uint;
        cl_ulong buf_ulong;
        printf("  -- %d --\n", i);
        status = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(buffer), buffer, NULL);
        printf("  DEVICE_NAME = %s\n", buffer);
        status = clGetDeviceInfo(devices[i], CL_DEVICE_VENDOR, sizeof(buffer), buffer, NULL);
        printf("  DEVICE_VENDOR = %s\n", buffer);
        status = clGetDeviceInfo(devices[i], CL_DEVICE_VERSION, sizeof(buffer), buffer, NULL);
        //printf("  DEVICE_VERSION = %s\n", buffer);
        status = clGetDeviceInfo(devices[i], CL_DRIVER_VERSION, sizeof(buffer), buffer, NULL);
        //printf("  DRIVER_VERSION = %s\n", buffer);
        status = clGetDeviceInfo(devices[i], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(buf_uint), &buf_uint, NULL);
        printf("  DEVICE_MAX_COMPUTE_UNITS = %u\n", (unsigned int)buf_uint);
        status = clGetDeviceInfo(devices[i], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(buf_uint), &buf_uint, NULL);
        //printf("  DEVICE_MAX_CLOCK_FREQUENCY = %u\n", (unsigned int)buf_uint);
        status = clGetDeviceInfo(devices[i], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(buf_ulong), &buf_ulong, NULL);
        //printf("  DEVICE_GLOBAL_MEM_SIZE = %llu\n", (unsigned long long)buf_ulong);
        printf("\n");
    }

    //-------------------------------------------------------------------------------------
    // STEP 3 Creating the context
    //-------------------------------------------------------------------------------------
    cl_context context = NULL;
    context = clCreateContext(NULL, numDevices, devices, NULL, NULL, &status);

    uint64_t bad_duration = run_opencl_cnn(devices, cmdQueue, context, device_name,
                                           false, size, weight, image, source_bad_hw_results, i_chan, o_chan);

    uint64_t good_duration = run_opencl_cnn(devices, cmdQueue, context, device_name,
                                            true, size, weight, image, source_good_hw_results, i_chan, o_chan);
    //OPENCL HOST CODE AREA END

    // Compare the results of the Device to the simulation
    bool match = true;
    for (int i = 0; i < size; i++)
    {
        /* if bad_duration is 0 then the kernel was unable to be produced for
         * the hardware thus there's no reason to check the results */
        if (bad_duration != 0)
        {
            if (source_bad_hw_results[i] != source_sw_results[i])
            {
                std::cout << "Error: Result mismatch in bad kernel" << std::endl;
                std::cout << "i = " << i << " CPU result = " << source_sw_results[i]
                          << " Device result = " << source_bad_hw_results[i] << std::endl;
                match = false;
                break;
            }
        }
        if (source_good_hw_results[i] != source_sw_results[i])
        {
            std::cout << "Error: Result mismatch in good kernel" << std::endl;
            std::cout << "i = " << i << " CPU result = " << source_sw_results[i]
                      << " Device result = " << source_good_hw_results[i] << std::endl;
            match = false;
            break;
        }
    }

    if (bad_duration != 0)
    {
        std::cout << "BAD duration = " << bad_duration << " ns" << std::endl;
    }

    std::cout << "TEST " << (match ? "PASSED" : "FAILED") << std::endl;
    return (match ? EXIT_SUCCESS : EXIT_FAILURE);
}
