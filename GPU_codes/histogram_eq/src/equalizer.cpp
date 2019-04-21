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
#include <iomanip>
#include <math.h>
#include <CL/cl.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <sys/time.h>
#include "equalizer.h"
#include <vector>

double timestamp()
{
    double ms = 0.0;
    timeval time;
    gettimeofday(&time, NULL);
    ms = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);
    return ms;
}

struct Mat
{
    int rows;
    int cols;
    int type;
};

CV_EXPORTS_W void cvtColor(Mat, Mat, int, int);
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <input_image>" << std::endl;
        return -1;
    }

    //Read the input image
    std::cout << "Reading input image..." << std::endl;
    const char *inputFilename = argv[1];

    Mat inputImageRaw;
    FILE *fp;
    fp = fopen(inputFilename, "w+");
    fread(NULL, sizeof(int), 1, fp);

    //Convert BGR Image into Gray Image
    cvtColor(inputImageRaw, inputImageRaw, CV_BGR2GRAY, 0);

    Mat inputImage;
    //inputImageRaw.convertTo(inputImage, CV_16U, 255);

    size_t image_size_bytes = sizeof(unsigned short) * IMAGE_WIDTH_PIXELS * IMAGE_HEIGHT_PIXELS;
    cl_int err;

    //----------------------------------------------------------------------------
    // STEP 1 : Discover and Initialize the platforms
    //----------------------------------------------------------------------------

    cl_int status;
    cl_platform_id platforms[100];
    cl_uint platforms_n = 0;

    //-----------------------------------------------------------------------------
    // STEP 2: Discover and initialize the device
    //-----------------------------------------------------------------------------

    cl_device_id devices[100];
    cl_uint numDevices = 0;

    //---------------------------------------------------------------------------
    // STEP 3 Creating Contetx
    //---------------------------------------------------------------------------
    cl_context context = NULL;
    context = clCreateContext(NULL, numDevices, devices, NULL, NULL, &status);
    double first_stamp = timestamp();
    cl_command_queue cmdQueue;
    cmdQueue = clCreateCommandQueue(context, devices[0], 0, &status);

    //Creating Context and Command Queue for selected Device

    unsigned short *eqimage;
    unsigned short *image;
    eqimage = (unsigned short *)malloc(image_size_bytes * 16);
    image = (unsigned short *)malloc(image_size_bytes * 16);

    cl_mem mem_image;
    cl_mem mem_eqimage;
    // These commands will allocate memory(Buffer) on the FPGA.

    mem_image = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(image_size_bytes), NULL, &status);
    mem_eqimage = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(image_size_bytes), NULL, &status);

    //Separate Read/write Buffer vector is needed to migrate data between host/device

    cl_mem inBufVec;
    cl_mem outBufVec;
    inBufVec = mem_image;
    outBufVec = mem_eqimage;

    //---------------------------------------------------------------------------
    //STEP 4 : Create and compile program
    //--------------------------------------------------------------------------

    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&inputImage, NULL, &status);
    status = clBuildProgram(program, numDevices, devices, NULL, NULL, NULL);

    /* Copy image to memory */

    //-------------------------------------------------------------------------------
    // STEP 5 : Create the kernel
    //-------------------------------------------------------------------------------

    cl_kernel kernel = NULL;
    kernel = clCreateKernel(program, "krnl_equalizer", &status);
    //set the kernel Arguments
    int narg = 0;

    //-------------------------------------------------------------------------------
    // STEP 6 Set the kernel Arguements
    //------------------------------------------------------------------------------
    status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &inBufVec);
    status |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &outBufVec);

    //Launch the Kernel

    //---------------------------------------------------------------------------------
    // STEP 7 Configuring the work structure
    //----------------------------------------------------------------------------------
    size_t globalWorkSize[1];
    globalWorkSize[0] = 1;

    uint64_t nstimestart, nstimeend;

    //---------------------------------------------------------------------------------
    //STEP 8 Enqueue the kernel for execution
    //--------------------------------------------------------------------------------
    cl_event event;
    status = clEnqueueNDRangeKernel(cmdQueue, kernel, 1, NULL, globalWorkSize, NULL, 0, NULL, NULL);

    /* Copy image to mat */
    Mat result_eq;
    result_eq.rows = inputImage.rows;
    result_eq.cols = inputImage.cols;
    result_eq.type = CV_16U;

    //-----------------------------------------------------------------------------------
    // STEP 9 READ the kernel
    //----------------------------------------------------------------------------------

    clEnqueueReadBuffer(cmdQueue, outBufVec, CL_TRUE, 0, sizeof(size_t), (void *)"out.bmp", 1, NULL, &event);

    double second_stamp = timestamp();
    double time_elapsed = second_stamp - first_stamp;
    printf("\nTotal time elspased is %fms \n", time_elapsed);

    std::string outFilename = std::string("out.bmp");

    std::cout << "Writing Image to " << outFilename << std::endl;
    // cv::imwrite(outFilename.c_str(), result_eq);
    fp = fopen("out.bmp", "w");
    fwrite(&result_eq, 1, sizeof(result_eq), fp);
    return 0;
}

void cvtColor(Mat inpuImage1, Mat inputImage, int dist, int src)
{
    Mat inpuImage;
    Mat inputImage1;
    int src1;

    inputImage = inputImage1;
    inputImage1 = inputImage;
    src1 = dist;
}
