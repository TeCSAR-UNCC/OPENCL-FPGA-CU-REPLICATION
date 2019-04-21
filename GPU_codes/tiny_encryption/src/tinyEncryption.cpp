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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <iostream>
#include <vector>
#include "sys/time.h"
#include <CL/cl.h>
#include "/home/arnab/SDAccel_Examples/libs/bitmap/bitmap.h"
#include "/home/arnab/SDAccel_Examples/libs/bitmap/bitmap.cpp"
#include "/home/arnab/SDAccel_Examples/libs/oclHelper/oclHelper.h"

void checkErrorStatus(cl_int error, const char *message)
{
  if (error != CL_SUCCESS)
  {
    printf("%s\n", message);
    //printf("%s\n", oclErrorCode(error)) ;
    exit(1);
  }
}

double timestamp()
{
  double ms = 0.0;
  timeval time;
  gettimeofday(&time, NULL);
  ms = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);
  return ms;
}

int main(int argc, char *argv[])
{

  if (argc != 3)
  {
    printf("Usage: %s <input bitmap1> <input bitmap2> \n", argv[0]);
    return -1;
  }

  //--------------------------------------------------------------
  //TEST ARGUEMENTS
  //-------------------------------------------------------------

  cl_uint num_events_in_wait = 1;
  const cl_event *event_wait_list;

  // Set up OpenCL hardware and software constructs
  std::cout << "Setting up OpenCL hardware and software...\n";
  cl_int err = 0;

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

  // Read the bit map file into memory and allocate memory for the
  //  final image
  std::cout << "Reading input image...\n";
  const char *bitmapFilename = argv[1];
  const char *bitmapFilenameKeys = argv[2];
  const char *goldenFilename;
  int width = 128;  // Default size
  int height = 128; // Default size

  BitmapInterface image(bitmapFilename);
  BitmapInterface imageKeys(bitmapFilenameKeys);

  bool result = image.readBitmapFile();

  result = imageKeys.readBitmapFile();

  int *inputImage;
  int *outImage;

  inputImage = (int *)malloc(image.numPixels() * sizeof(int));
  outImage = (int *)malloc(image.numPixels() * sizeof(int));

  if ((sizeof(inputImage) == 0) || (sizeof(outImage) == 0))
  {
    fprintf(stderr, "Unable to allocate the host memory!\n");
    return 0;
  }

  width = image.getWidth();
  height = image.getHeight();
  int length = image.numPixels() / 2;

  // Copy image host buffer
  memcpy(inputImage, image.bitmap(), image.numPixels() * sizeof(int));

  //---------------------------------------------------------------
  // STEP 3 Creating Context
  //---------------------------------------------------------------
  cl_context context = NULL;
  context = clCreateContext(NULL, numDevices, devices, NULL, NULL, &status);

  // STEP 4 Creating command queue
  //-------------------------------------------------------------------
  cl_command_queue cmdQueue;
  cmdQueue = clCreateCommandQueue(context, devices[0], 0, &status);
  double first_stamp = timestamp();

  cl_mem buffer_input;
  cl_mem buffer_output;
  cl_mem keys;

  buffer_input = clCreateBuffer(context, CL_MEM_READ_ONLY, image.numPixels() * sizeof(int), NULL, &status);
  buffer_output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, image.numPixels() * sizeof(int), NULL, &status);
  keys = clCreateBuffer(context, CL_MEM_READ_ONLY, image.numPixels() * sizeof(int) * 2, NULL, &status);

  //-----------------------------------------------------------------------------------------
  // STEP 5 Create and compile program
  //-----------------------------------------------------------------------------------------

  cl_program program = clCreateProgramWithSource(context, 1, (const char **)&inputImage, NULL, &status);
  status = clBuildProgram(program, numDevices, devices, NULL, NULL, NULL);

  // Send the image to the hardware
  std::cout << "Writing input image to buffer...\n";

  err = clEnqueueWriteBuffer(cmdQueue, buffer_input, CL_TRUE, 0, image.numPixels() * sizeof(int), image.bitmap(), 0, NULL, NULL);

  checkErrorStatus(err, "Unable to enqueue write buffer");

  // set the keys
  std::cout << "Writing keys to buffer...\n";
  err = clEnqueueWriteBuffer(cmdQueue, keys, CL_TRUE, 0, image.numPixels() * 2 * sizeof(int), imageKeys.bitmap(), 0, NULL, NULL);

  checkErrorStatus(err, "Unable to enqueue write buffer");

  //-----------------------------------------------------------------------------------------------
  //    STEP 6 Create kernel
  //---------------------------------------------------------------------------------------
  cl_kernel kernel = NULL;
  kernel = clCreateKernel(program, "krnl_tinyEncryption", &status);

  //------------------------------------------------------------------
  // STEP 7 Setting the kernel Arguements
  //------------------------------------------------------------------
  status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer_input);
  status |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &buffer_output);
  status = clSetKernelArg(kernel, 2, sizeof(cl_mem), &keys);
  status = clSetKernelArg(kernel, 3, sizeof(int), &length);

  std::cout << "Arguements are set ....\n";

  //-----------------------------------------------------------------
  // STEP 8 Configuring the work structure
  //----------------------------------------------------------------

  // Define iteration space
  size_t globalSize[3] = {1, 1, 1};
  size_t localSize[3] = {1, 1, 1};

  std::cout << "work structure configured ....\n";

  // STEP 8 Enqueue The kernel for execution
  //--------------------------------------------------------------
  status = clEnqueueNDRangeKernel(cmdQueue, kernel, 1, NULL, globalSize, localSize, 0, NULL, NULL);
  std::cout << "kernel enqueued .....\n";
  // This function will execute the kernel on the FPGA

  //-------------------------------------------------------------------
  // STEP 10 READ the kernel
  //------------------------------------------------------------------
  clEnqueueReadBuffer(cmdQueue, buffer_output, CL_TRUE, 0, image.numPixels() * sizeof(int), (void *)"output.bmp", num_events_in_wait, NULL, NULL);
  std::cout << "Reading the kernel ....\n";
  // Read back the image from the kernel
  std::cout << "Reading output image and writing to file...\n";

  double second_stamp = timestamp();
  double time_elapsed = second_stamp - first_stamp;
  printf("\nTotal time elspased is %fms \n", time_elapsed);

  bool match = true;

  if (argc == 4)
  {
    goldenFilename = argv[3];
    //Read the golden bit map file into memory
    BitmapInterface goldenImage(goldenFilename);
    result = goldenImage.readBitmapFile();
    if (!result)
    {
      std::cout << "ERROR:Unable to Read Golden Bitmap File " << goldenFilename << std::endl;
      return EXIT_FAILURE;
    }
    //Compare Golden Image with Output image
    if (image.getHeight() != goldenImage.getHeight() || image.getWidth() != goldenImage.getWidth())
    {
      match = false;
    }
    else
    {
      int *goldImgPtr = goldenImage.bitmap();
      for (unsigned int i = 0; i < image.numPixels(); i++)
      {
        if (outImage[i] != goldImgPtr[i])
        {
          match = false;
          printf("Pixel %d Mismatch Output %x and Expected %x \n", i, outImage[i], goldImgPtr[i]);
          break;
        }
      }
    }
  }

  // Write the final image to disk
  image.writeBitmapFile(outImage);
  printf("Writing RAW Image \n");

  std::cout << (match ? "TEST PASSED" : "TEST FAILED") << std::endl;
  return (match ? EXIT_SUCCESS : EXIT_FAILURE);
}
