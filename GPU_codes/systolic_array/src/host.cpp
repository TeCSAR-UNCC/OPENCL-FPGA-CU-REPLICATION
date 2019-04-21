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

    This is a matrix multiplication which showcases the "Systolic Array" based 
    algorithm design. Systolic array type of implementation is well suited for 
    FPGAs. It is a good coding practice to convert base algorithm into Systolic 
    Array implementation if it is feasible to do so.

*******************************************************************************/

#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <iostream>
#include "sys/time.h"
#include <CL/cl.h>

//Array Size to access
#define DATA_SIZE 24

//Maximum Array Size
#define MAX_SIZE 24

double timestamp()
{
  double ms = 0.0;
  timeval time;
  gettimeofday(&time, NULL);
  ms = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);
  return ms;
}

int main(int argc, char **argv)
{
  //Allocate Memory in Host Memory
  if (DATA_SIZE > MAX_SIZE)
  {
    std::cout << "Size is bigger than internal buffer size, please use a size smaller than " << MAX_SIZE << "!" << std::endl;
    return EXIT_FAILURE;
  }

  //--------------------------------------------------------------
  //TEST ARGUEMENTS
  //-------------------------------------------------------------

  cl_uint num_events_in_wait = 1;

  //----------------------------------------------------
  //STEP 1: Discover and Initialize the platforms
  //---------------------------------------------------

  cl_int status;
  cl_platform_id platforms[100];
  cl_uint platforms_n = 0;
  status = clGetPlatformIDs(100, platforms, &platforms_n);
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

  if (platforms_n == 0)
  {
    printf("No OpenCL platform found!\n\n");
    return 1;
  }

  //-------------------------------------------------------------
  //STEP 2: Discover and Initialize the devices
  //-------------------------------------------------------------
  cl_device_id devices[100];
  cl_uint numDevices = 0;
  // CL_CHECK(clGetDeviceIDs(NULL, CL_DEVICE_TYPE_ALL, 100, devices, &numDevices));
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

  if (numDevices == 0)
  {
    printf("No OpenCL device found!\n\n");
    return 1;
  }

  size_t matrix_size = DATA_SIZE * DATA_SIZE;
  size_t matrix_size_bytes = sizeof(int) * matrix_size;

  int *source_in1;
  int *source_in2;
  int *source_hw_results;
  int *source_sw_results;

  source_in1 = (int *)malloc(matrix_size * sizeof(size_t));
  source_in2 = (int *)malloc(matrix_size * sizeof(size_t));
  source_hw_results = (int *)malloc(matrix_size * sizeof(size_t));
  source_sw_results = (int *)malloc(matrix_size * sizeof(size_t));

  // Create the test data and Software Result
  for (size_t i = 0; i < matrix_size; i++)
  {
    source_in1[i] = i % 10;
    source_in2[i] = i % 10;
    source_sw_results[i] = 0;
    source_hw_results[i] = 0;
  }

  //---------------------------------------------------------------
  // STEP 3 Creating Context
  //---------------------------------------------------------------
  cl_context context = NULL;
  context = clCreateContext(NULL, numDevices, devices, NULL, NULL, &status);

  //---------------------------------------------------------------
  // STEP 4 Creating command queue
  //-------------------------------------------------------------------
  cl_command_queue cmdQueue;
  cmdQueue = clCreateCommandQueue(context, devices[0], 0, &status);
  double first_stamp = timestamp();

  cl_mem buffer_in1;
  cl_mem buffer_in2;
  cl_mem buffer_out;

  //Allocate Buffer in Global Memory
  buffer_in1 = clCreateBuffer(context, CL_MEM_READ_ONLY, matrix_size_bytes, NULL, &status);
  buffer_in2 = clCreateBuffer(context, CL_MEM_READ_ONLY, matrix_size_bytes, NULL, &status);
  buffer_out = clCreateBuffer(context, CL_MEM_WRITE_ONLY, matrix_size_bytes, NULL, &status);

  //-----------------------------------------------------------------------------------------
  // STEP 5 Create and compile program
  //-----------------------------------------------------------------------------------------

  cl_program program = clCreateProgramWithSource(context, 1, (const char **)&buffer_in1, NULL, &status);
  status = clBuildProgram(program, numDevices, devices, NULL, NULL, NULL);

  //---------------------------------------------------------------------
  //STEP 6 Create the kernel
  //------------------------------------------------------------------
  cl_kernel kernel = NULL;
  kernel = clCreateKernel(program, "mmult", &status);

  //----------------------------------------------------------------------
  //STEP 7 Set the kernel arguements
  //----------------------------------------------------------------------
  status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer_in1);
  status |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &buffer_in2);
  status |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &buffer_out);

  std::cout << "Arguements are set ....\n";

  //-----------------------------------------------------------------
  // STEP 8 Configuring the work structure
  //----------------------------------------------------------------

  // Define iteration space
  size_t globalSize[3] = {1, 1, 1};
  size_t localSize[3] = {1, 1, 1};

  std::cout << "work structure configured ....\n";

  // STEP 9 Enqueue The kernel for execution
  //--------------------------------------------------------------
  status = clEnqueueNDRangeKernel(cmdQueue, kernel, 1, NULL, globalSize, localSize, 0, NULL, NULL);
  std::cout << "kernel enqueued .....\n";
  // This function will execute the kernel on the FPGA

  //-------------------------------------------------------------------
  // STEP 10 READ the kernel
  //------------------------------------------------------------------
  clEnqueueReadBuffer(cmdQueue, buffer_out, CL_TRUE, 0, matrix_size_bytes, (void *)"output.bmp", num_events_in_wait, NULL, NULL);
  std::cout << "Reading the kernel ....\n";
  // Read back the image from the kernel
  std::cout << "Reading output image and writing to file...\n";

  double second_stamp = timestamp();
  double time_elapsed = second_stamp - first_stamp;
  printf("\nTotal time elspased is %fms \n", time_elapsed);

  //OPENCL HOST CODE AREA END

  // Compare the results of the Device to the simulation
  int match = 0;
  for (int i = 0; i < DATA_SIZE * DATA_SIZE; i++)
  {
    if (source_hw_results[i] != source_sw_results[i])
    {
      std::cout << "Error: Result mismatch" << std::endl;
      std::cout << "i = " << i << " CPU result = " << source_sw_results[i]
                << " Device result = " << source_hw_results[i] << std::endl;
      match = 1;
      break;
    }
  }

  std::cout << "TEST " << (match ? "FAILED" : "PASSED") << std::endl;
  return (match ? EXIT_FAILURE : EXIT_SUCCESS);
}
