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
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "/home/arnab/SDAccel_Examples/libs/logger/logger.h"
#include "aes_app.h"
#include "aes_ecb.h"
#include "sys/time.h"
#include "/home/arnab/SDAccel_Examples/libs/simplebmp/simplebmp.h"

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

	if (argc < 1)
	{
		std::cout << "Usage: " << argv[0] << " <input bitmap>" << std::endl;
		return EXIT_FAILURE;
	}

#define ROUNDS 10
	//ROUNDS <= 10 valid

	//load "input.bmp"
	int err;
	struct bmp_t inputbmp;

	size_t inputbmpsize = inputbmp.height * inputbmp.width * 3;

	//AES ECB encryption in software
	//encrypted image setup
	struct bmp_t swencryptbmp;
	swencryptbmp.pixels = (uint32_t *)malloc(inputbmpsize);
	swencryptbmp.width = inputbmp.width;
	swencryptbmp.height = inputbmp.height;
	if (swencryptbmp.pixels == NULL)
	{
		printf(
			"Error : failed to allocate memory for sw encrypted swencrypted.bmp\n");
		return false;
	}

	//128 bit encryption key
	unsigned char key[] = "Xilinx SDAccel  ";

	//perform SW encryption
	//Xilinx
	aesecb_encrypt(key, ((unsigned char *)inputbmp.pixels),
				   ((unsigned char *)swencryptbmp.pixels), inputbmpsize, ROUNDS);
	//write "swencrypted.bmp"
	char swencryptbmpfile[] = "swencrypt.bmp";
	//writebmp(swencryptbmpfile, &swencryptbmp);

	//AES ECB decryption in hardware
	//decrypted image setup
	struct bmp_t hwdecryptbmp;
	hwdecryptbmp.pixels = (uint32_t *)malloc(inputbmpsize);
	hwdecryptbmp.width = inputbmp.width;
	hwdecryptbmp.height = inputbmp.height;
	if (hwdecryptbmp.pixels == NULL)
	{
		printf("Error : failed to allocate memory for sw encrypted hwdecrypted.bmp\n");
		return false;
	}

	//------------------------------------------------------------------
	// STEP 1 Discover and Initialize the platforms
	//------------------------------------------------------------------
	cl_int status;
	cl_platform_id platforms[100];
	cl_uint platforms_n = 0;
	//cl_uint num_events_in_wait=1;
	status = clGetPlatformIDs(100, platforms, &platforms_n);

	// printf("=== %d OpenCL platform(s) found: ===\n", platforms_n);
	for (int i = 0; i < platforms_n; i++)
	{
		char buffer[10240];
		printf("  -- %d --\n", i);
		status = clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, 10240, buffer, NULL);
		printf("  PROFILE = %s\n", buffer);
		status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, 10240, buffer, NULL);
		printf("  VERSION = %s\n", buffer);
		status = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 10240, buffer, NULL);
		printf("  NAME = %s\n", buffer);
		status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, 10240, buffer, NULL);
		printf("  VENDOR = %s\n", buffer);
		status = clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, 10240, buffer, NULL);
		printf("  EXTENSIONS = %s\n", buffer);
	}

	if (platforms_n == 0)
	{
		printf("No OpenCL platform found!\n\n");
		return 1;
	}

	//----------------------------------------------------------------
	// STEP 2 Discover and Initialize the devices
	//----------------------------------------------------------------
	cl_device_id devices[100];
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
		printf("  DEVICE_VERSION = %s\n", buffer);
		status = clGetDeviceInfo(devices[i], CL_DRIVER_VERSION, sizeof(buffer), buffer, NULL);
		printf("  DRIVER_VERSION = %s\n", buffer);
		status = clGetDeviceInfo(devices[i], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(buf_uint), &buf_uint, NULL);
		printf("  DEVICE_MAX_COMPUTE_UNITS = %u\n", (unsigned int)buf_uint);
		status = clGetDeviceInfo(devices[i], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(buf_uint), &buf_uint, NULL);
		printf("  DEVICE_MAX_CLOCK_FREQUENCY = %u\n", (unsigned int)buf_uint);
		status = clGetDeviceInfo(devices[i], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(buf_ulong), &buf_ulong, NULL);
		printf("  DEVICE_GLOBAL_MEM_SIZE = %llu\n", (unsigned long long)buf_ulong);
		printf("\n");
	}

	if (numDevices == 0)
	{
		printf("No OpenCL device found!\n\n");
		return 1;
	}

	//---------------------------------------------------------------
	// STEP 3 Creating Context
	//---------------------------------------------------------------
	cl_context context = NULL;
	context = clCreateContext(NULL, numDevices, devices, NULL, NULL, &status);

	//-------------------------------------------------------------------
	// STEP 4 Creating command queue
	//-------------------------------------------------------------------
	cl_command_queue cmdQueue;
	cmdQueue = clCreateCommandQueue(context, devices[0], 0, &status);

	double first_stamp = timestamp();

	cl_mem buffer_input;
	cl_mem roundkey_buffer;
	cl_mem buffer_output;
	unsigned int datasetsize = inputbmpsize;
	unsigned int blocks = datasetsize / 16;

	buffer_input = clCreateBuffer(context, CL_MEM_READ_WRITE, datasetsize, NULL, &status);
	roundkey_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, (ROUNDS + 1) * 16, NULL, &status);
	buffer_output = clCreateBuffer(context, CL_MEM_READ_WRITE, datasetsize, NULL, &status);

	//compute expanded roundkey
	unsigned char roundkey[(10 + 1) * 16];
	strcpy(((char *)roundkey), ((char *)key));
	KeyExpansion(roundkey);

	std::cout << "Writing input image to buffer...\n";

	//-----------------------------------------------------------------------------------------
	// STEP 5 Create and compile program
	//-----------------------------------------------------------------------------------------

	cl_program program = clCreateProgramWithSource(context, 1, (const char **)&inputbmpsize, NULL, &status);
	status = clBuildProgram(program, numDevices, devices, NULL, NULL, NULL);

	//---------------------------------------------------------------------
	//STEP 6 Create the kernel
	//------------------------------------------------------------------
	cl_kernel kernel = NULL;
	kernel = clCreateKernel(program, "krnl_aes_decrypt", &status);

	//std::cout << "Check..\n";
	//----------------------------------------------------------------------
	//STEP 7 Set the kernel arguements
	//----------------------------------------------------------------------
	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer_output);
	status |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &buffer_input);
	status |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &roundkey_buffer);
	status |= clSetKernelArg(kernel, 3, sizeof(unsigned int), &blocks);

	//---------------------------------------------------------
	//STEP 8 COnfiguring the work structure
	//---------------------------------------------------------
	size_t global[1];
	size_t local[1];
	global[0] = 1;
	local[0] = 1;

	//-------------------------------------------------
	//STEP 9 Enqueue the kernel for execution
	//---------------------------------------------------

	status = clEnqueueNDRangeKernel(cmdQueue, kernel, 1, NULL, global, local, 0, NULL, NULL);

	//call a second time to measure on-chip throughput

	status = clEnqueueNDRangeKernel(cmdQueue, kernel, 1, NULL, global, local, 0, NULL, NULL);

	//--------------------------------------------------
	//STEP 10 READ the kernel
	//----------------------------------------------------
	cl_uint num_events_in_wait = 1;
	clEnqueueReadBuffer(cmdQueue, buffer_output, CL_TRUE, 0, inputbmpsize, (void *)"hwdecrypt.bmp", num_events_in_wait, NULL, NULL);

	double second_stamp = timestamp();
	double time_elapsed = second_stamp - first_stamp;
	printf("\nTotal time elapsed is %fms \n", time_elapsed);

	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseCommandQueue(cmdQueue);
	clReleaseMemObject(buffer_input);
	clReleaseMemObject(buffer_output);
	clReleaseContext(context);
}
