#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <iostream>

#include <CL/cl.h>

#include "bitmap.h"
#include "oclHelper.h"

void checkErrorStatus(cl_int error, const char* message)
{
  if (error != CL_SUCCESS)
  {
    printf("%s\n", message) ;
    printf("%s\n", oclErrorCode(error)) ;
    exit(1) ;
  }
}

int main(int argc, char* argv[])
{
  if (argc != 5)
  {
    printf("Usage: %s <input bitmap1> <input bitmap2> <xclbin> <wg> \n", argv[0]) ;
    return -1 ;
  }
  
  // Read the bit map file into memory and allocate memory for the
  //  final image
  std::cout << "Reading input image...\n";
  const char* bitmapFilename = argv[1] ;
  const char* bitmapFilenameKeys = argv[2] ;
  int work_group = atoi(argv[4]);
  BitmapInterface image(bitmapFilename);
  BitmapInterface imageKeys(bitmapFilenameKeys);

  bool result = image.readBitmapFile();
  if (!result)
  {
    return -1 ;
  }

  result = imageKeys.readBitmapFile() ;
  if (!result)
  {
    return -1 ;
  }

  int length=image.numPixels()/2;

  int* outImage = (int*)(malloc(image.numPixels() * sizeof(int))) ;

  if (outImage == NULL)
  {
    fprintf(stderr, "Unable to allocate host memory!\n") ;
    return 0 ;
  }

    int* keySchedule = (int*)(malloc(image.numPixels() *2* sizeof(int))) ;

  if (keySchedule == NULL)
  {
    fprintf(stderr, "Unable to allocate host memory!\n") ;
    return 0 ;
  }

  // Set up OpenCL hardware and software constructs
  std::cout << "Setting up OpenCL hardware and software...\n";
  cl_int err = 0 ;
  const char* xclbinFilename = argv[3] ;

  oclHardware hardware = getOclHardware(CL_DEVICE_TYPE_ACCELERATOR) ;
  oclSoftware software ;

  memset(&software, 0, sizeof(oclSoftware)) ;
  strcpy(software.mKernelName, "tinyEncryption") ;
  strcpy(software.mFileName, xclbinFilename) ;
  strcpy(software.mCompileOptions, "-g -Wall") ;

  getOclSoftware(software, hardware) ;

  software.mKernel = clCreateKernel(software.mProgram,
				    software.mKernelName,
				    &err) ;
  checkErrorStatus(err, "Unable to create kernel!") ;
  int buffer = image.numPixels()/work_group;
  printf("buffer:%d",buffer);
  // Initialize OpenCL buffers with pointers to allocated memory
  cl_mem imageToDevice ;
  cl_mem imageFromDevice ;
  cl_mem keys;

  imageToDevice = clCreateBuffer(hardware.mContext,
				 CL_MEM_READ_ONLY ,
				 image.numPixels() * sizeof(int),
				 NULL,
				 &err) ;
  checkErrorStatus(err, "Unable to create read buffer") ;

 keys = clCreateBuffer(hardware.mContext,
				   CL_MEM_READ_ONLY,
				   image.numPixels() * sizeof(int)*2,
				   NULL,
				   &err) ;
  checkErrorStatus(err, "Unable to create keys buffer") ;

  imageFromDevice = clCreateBuffer(hardware.mContext,
				   CL_MEM_READ_WRITE,
				   image.numPixels() * sizeof(int),
				   NULL,
				   &err) ;
  checkErrorStatus(err, "Unable to create write buffer") ;


  // Send the image to the hardware
  std::cout << "Writing input image to buffer...\n";
  err = clEnqueueWriteBuffer(hardware.mQueue,
			     imageToDevice,
			     CL_TRUE,
			     0,
			     image.numPixels() * sizeof(int),
			     image.bitmap(),
			     0,
			     NULL,
			     NULL) ;

  checkErrorStatus(err, "Unable to enqueue write buffer") ;



  // set the keys
  std::cout << "Writing keys to buffer...\n";
  err = clEnqueueWriteBuffer(hardware.mQueue,
			     keys,
			     CL_TRUE,
			     0,
			     image.numPixels() *2* sizeof(int),
			     imageKeys.bitmap(),
			     0,
			     NULL,
			     NULL) ;

  checkErrorStatus(err, "Unable to enqueue write buffer") ;

  // Pass the arguments to the kernel
  std::cout << "Setting arguments and enqueueing kernel...\n";
  err = clSetKernelArg(software.mKernel, 0, sizeof(cl_mem), &imageToDevice);
  if (err != CL_SUCCESS) 
  {
        printf("Error: Failed to execute kernel! %d\n", err);
        exit(1);
  }
  //std::cout << " kernel 1...\n";
  checkErrorStatus(err, "Unable to set argument 0") ;
  err = clSetKernelArg(software.mKernel, 1, sizeof(cl_mem), &imageFromDevice);
  if (err != CL_SUCCESS){
        printf("Error: Failed to execute kernel! %d\n", err);
        exit(1);
   }
  //   std::cout << " kernel 2...\n";
  checkErrorStatus(err, "Unable to set argument 1") ;
  err = clSetKernelArg(software.mKernel, 2, sizeof(cl_mem), &keys) ;
    if (err != CL_SUCCESS){
        printf("Error: Failed to execute kernel! %d\n", err);
        exit(1);
  }
  //  std::cout << " kernel 3...\n";
  //checkErrorStatus(err, "Unable to set argument 2") ;
  
  err = clSetKernelArg(software.mKernel, 3, sizeof(int), &length) ;
    std::cout << " kernel 4...\n";
  checkErrorStatus(err, "Unable to set argument 3") ;


  // Define iteration space 
  size_t globalSize[3] = { 1, 1, 1 } ;
  size_t localSize[3] = { 1, 1, 1} ;
  cl_event seq_complete ;
   std::cout << "Setting arguments and enqueueing kernel...\n";
  // Actually start the kernels on the hardware
  err = clEnqueueNDRangeKernel(hardware.mQueue,
			       software.mKernel,
			       1,
			       NULL,
			       globalSize,
			       localSize,
			       0,
			       NULL,
			       &seq_complete) ;
   if (err != CL_SUCCESS){
        printf("Error: Failed to execute kernel! %d\n", err);
        exit(1);
  }
  //checkErrorStatus(err, "Unable to enqueue NDRange") ;

  // Wait for kernel to finish
  clWaitForEvents(1, &seq_complete) ;

  // Read back the image from the kernel
  std::cout << "Reading output image and writing to file...\n";
  err = clEnqueueReadBuffer(hardware.mQueue,
			    imageFromDevice,
			    CL_TRUE,
			    0,
			    image.numPixels() * sizeof(int),
			    outImage,
			    0,
			    NULL,
			    &seq_complete) ;

  checkErrorStatus(err, "Unable to enqueue read buffer") ;
 std::cout << "DONE Setting arguments and enqueueing kernel...\n";
  clWaitForEvents(1, &seq_complete) ;

  // Write the final image to disk
  image.writeBitmapFile(outImage) ;
  free(outImage) ;

  release(software) ;
  release(hardware) ;
  return 0 ;
}