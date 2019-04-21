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
#include <CL/cl.h>
#include <vector>
#include "/home/arnab/SDAccel_Examples-2017.1/libs/bitmap/bitmap.h"
#include "sys/time.h"

#define X_SIZE 512
#define Y_SIZE 512

double timestamp(){
double ms=0.0;
timeval time;
gettimeofday(&time,NULL);
ms=(time.tv_sec*1000.0)+(time.tv_usec/1000.0);
return ms;


}
int main(int argc, char** argv)
{
	int i,j;
	//char buffer[10240];
	if (argc != 2)
	{
		printf("Usage: %s <image> \n", argv[0]) ;
		return -1 ;
	}
   
	FILE *input_file;               
	FILE *output_file;

	size_t vector_size_bytes = sizeof(unsigned short) * Y_SIZE*X_SIZE;
	unsigned short *input_image;
	unsigned short *output_image;
	input_image=(unsigned short *)calloc(262144,sizeof(unsigned short));
	cl_int err;
	output_image=(unsigned short *)calloc(262144,sizeof(unsigned short));
		
	//----------------------------------------------------
	//STEP 1: Discover and Initialize the platforms
	//---------------------------------------------------

	cl_int status;
	cl_platform_id platforms[100];
	cl_uint platforms_n = 0;
	status=clGetPlatformIDs(100, platforms, &platforms_n);
	printf("=== %d OpenCL platform(s) found: ===\n", platforms_n);
	for (int i=0; i<platforms_n; i++)
	{
		char buffer[10240];
		printf("  -- %d --\n", i);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, 10240, buffer, NULL);
		//printf("  PROFILE = %s\n", buffer);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, 10240, buffer, NULL);
		printf("  VERSION = %s\n", buffer);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 10240, buffer, NULL);
		printf("  NAME = %s\n", buffer);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, 10240, buffer, NULL);
		printf("  VENDOR = %s\n", buffer);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, 10240, buffer, NULL);
		//printf("  EXTENSIONS = %s\n", buffer);
	}

	if (platforms_n == 0)
	{	
		printf("No OpenCL platform found!\n\n");
		//return 1;
	}

	//-------------------------------------------------------------
	//STEP 2: Discover and Initialize the devices
	//-------------------------------------------------------------
	cl_device_id devices[100];
	cl_uint numDevices = 0;
	// CL_CHECK(clGetDeviceIDs(NULL, CL_DEVICE_TYPE_ALL, 100, devices, &numDevices));
	status=clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 100, devices, &numDevices);

	printf("=== %d OpenCL device(s) found on platform:\n", platforms_n);
	for (int i=0; i<numDevices; i++)
	{
		char buffer[10240];
		cl_uint buf_uint;
		cl_ulong buf_ulong;
		printf("  -- %d --\n", i);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(buffer), buffer, NULL);
		printf("  DEVICE_NAME = %s\n", buffer);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_VENDOR, sizeof(buffer), buffer, NULL);
		printf("  DEVICE_VENDOR = %s\n", buffer);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_VERSION, sizeof(buffer), buffer, NULL);
		//printf("  DEVICE_VERSION = %s\n", buffer);
		status=clGetDeviceInfo(devices[i], CL_DRIVER_VERSION, sizeof(buffer), buffer, NULL);
		//printf("  DRIVER_VERSION = %s\n", buffer);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(buf_uint), &buf_uint, NULL);
		printf("  DEVICE_MAX_COMPUTE_UNITS = %u\n", (unsigned int)buf_uint);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(buf_uint), &buf_uint, NULL);
		//printf("  DEVICE_MAX_CLOCK_FREQUENCY = %u\n", (unsigned int)buf_uint);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(buf_ulong), &buf_ulong, NULL);
		//printf("  DEVICE_GLOBAL_MEM_SIZE = %llu\n", (unsigned long long)buf_ulong);
		printf("\n");
	}

	if (numDevices == 0)
	{	printf("No OpenCL device found!\n\n");
		//return 1;
	}


	// Read the bit map file into memory and allocate memory for the final image
	std::cout << "Reading input image...\n";
	
	// Load the input image
	const char *imageFilename = argv[1];
	input_file = fopen(imageFilename, "rb");     // Opens the image file given in argv[1]
	

	if (!input_file)
	{
		printf("Error: Unable to open input image file %s!\n",
		imageFilename);
		return 1;
	 }	
	
	printf("\n");
	printf("   Reading RAW Image\n");
	size_t items_read = fread(input_image, vector_size_bytes,1,input_file);    
	printf("   Bytes read = %d\n\n", (int)(items_read* sizeof input_image));
	
	
	//-------------------------------------------------------
	//STEP 3 Creating Context
	//-------------------------------------------------------

	cl_context context=NULL;
	context=clCreateContext(NULL,numDevices,devices,NULL,NULL,&status);
	

	//-------------------------------------------------------
	//STEP 4 Creating command queue
	//-------------------------------------------------------
	cl_command_queue cmdQueue;
	cmdQueue=clCreateCommandQueue(context,devices[0],0,&status);
	
double first_stamp=timestamp();


	// Allocate Buffers in Global Memory
    // Buffers are allocated using CL_MEM_USE_HOST_PTR for efficient memory and
    // Device-to-host communication

	cl_mem inBufVec;
	cl_mem outBufVec;

	inBufVec=clCreateBuffer(context,CL_MEM_READ_ONLY,sizeof(int),NULL,&status);
	outBufVec=clCreateBuffer(context,CL_MEM_WRITE_ONLY,sizeof(int),NULL,&status);

	
	//--------------------------------------------------------------
	//STEP 5 Create and compile  program
	//--------------------------------------------------------------

	cl_program program=clCreateProgramWithSource(context,1,(const char**)&input_file,NULL,&status);
	status=clBuildProgram(program,numDevices,devices,NULL,NULL,NULL);


	
	//---------------------------------------------------------------------
	//STEP 6 Create the kernel
	//------------------------------------------------------------------
	cl_kernel kernel=NULL;
	kernel=clCreateKernel(program,"affine_kernel",&status);

	//----------------------------------------------------------------------
	//STEP 7 Set the kernel arguements
	//----------------------------------------------------------------------
	status=clSetKernelArg(kernel,0,sizeof(cl_mem),&inBufVec);
	status|=clSetKernelArg(kernel,1,sizeof(cl_mem),&outBufVec);

	//---------------------------------------------------------
	//STEP 8 Configuring the work structure
	//---------------------------------------
	size_t globalWorkSize[1];
	globalWorkSize[0]=1;

	//-------------------------------------------------
	//STEP 9 Enqueue the kernel for execution
	//---------------------------------------------------

	status=clEnqueueNDRangeKernel(cmdQueue,kernel,1,NULL,globalWorkSize,NULL,0,NULL,NULL);

	//--------------------------------------------------
	//STEP 10 Read back the image from the kernel
	//----------------------------------------------------
	clEnqueueReadBuffer(cmdQueue,outBufVec,CL_TRUE,0,sizeof(size_t),(void*)"transformed_image_1.raw",0,NULL,NULL);


	std::cout << "Reading output image and writing to file...\n";
	output_file = fopen("transformed_image_1.raw", "wb");
	if (!output_file)
	{
		printf("Error: Unable to open output image file!\n");
		return 1;
	}
	double second_stamp=timestamp();
	double time_elapsed=second_stamp-first_stamp;
	printf("\nTotal time elspased is %f ms\n",time_elapsed);
	

	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseCommandQueue(cmdQueue);
	clReleaseMemObject(inBufVec);
	clReleaseMemObject(outBufVec);
	clReleaseContext(context);
	
	printf("   Writing RAW Image\n");
	size_t items_written = fwrite(output_image, vector_size_bytes, 1, output_file);
	printf("   Bytes written = %d\n\n", (int)(items_written * sizeof output_image));

	return 0 ;
}


