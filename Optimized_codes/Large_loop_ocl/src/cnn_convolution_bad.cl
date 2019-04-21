#include "defns.h"
__kernel __attribute__ ((reqd_work_group_size(1, 1, 1)))
void cnn(
        const __global int *image,      // Read-Only Image 
        const __global int *weights,    // Read-Only Weight Matrix
        __global int *out,              // Output Filters/Images
        int size,                       // Size of Output Data
        int i_chan,                     // Input Channels
        int o_chan                      // Output Channels
    )
{
    int global_id      = get_global_id(0);         // Picks Work Item global ID
    int global_threads = get_global_size(0);       // Picks Total Threads
    
    // Local Buffer to Hold Input Image
    int img_lcl[IChan][ISize * ISize] __attribute__((xcl_array_partition(complete,1)));

    // Local Buffer to Hold Output Filters/Images
    int out_lcl[OSize * OSize];

    // Local Buffer to Hold Weight Matrix;
    int wgt_lcl[WInChan][WSize * WSize] __attribute__((xcl_array_partition(complete,1)));

    /////////////image///////////////////////////////////////////
    
    // Copy image data from DDR to local buffer per work group (Burst Mode)
    __attribute__((xcl_pipeline_loop))
    imgcopy:for(int itr = 0, i = 0, j = 0; itr < i_chan * ISize * ISize; itr++, j++){
        if(j == ISize * ISize) {j = 0; i++;}
            img_lcl[i][j] = image[itr];
    }

    // Calculate each work_item's start and end point (Output Filters/Images)
    int thread_work_start = global_id * o_chan / global_threads;
    int thread_work_end   = (global_id + 1) * o_chan / global_threads;
    
    outthread:for(int output = thread_work_start; output < thread_work_end; output++)
     {
            
        // Burst read weight data from global to local buffer
    //////////////////weight////////////////////////////////
    // Calculate each work_item's weight matrix location 
    int stride2 = output * WInChan * WSize * WSize;
    
    // Each work_item copies its weight data from DDR to local buffers
    __attribute__((xcl_pipeline_loop))
    readWt: 
    // To avoid automatic loop unrolling
    #pragma nounroll
    for(int itr = 0, i = 0, j = 0; itr < WInChan * WSize * WSize; itr++,j++) {
        if(j == WSize * WSize) {j = 0; i++;}
        wgt_lcl[i][j] = weights[stride2 + itr];
    }

        outYaxis:for(int y = 0; y < OSize; y++)
         {
            outXaxis:for(int x = 0; x < OSize; x++)
             {
               // Perform convolution for the current 'pixel'
               short acc[IChan][WSize][WSize] 
                __attribute__((xcl_array_partition(complete,1)));

                 // Holds Image Padding Boundary Check Variables
                    int xVal_base = x * Stride - Padding;
                    int yVal = y * Stride - Padding;

                     // Runs over filter window    
                    convYaxis: for(int i = 0; i < WSize; i++,yVal++)
                    {
                     __attribute__((xcl_pipeline_loop))
                         // Runs over filter window
                     convXaxis: for(int j = 0, xVal = xVal_base ; j < WSize; j++, xVal++)
                     {
                        // Runs over each of the input channels
                        convInchan: for(int input = 0; input < IChan; input++)
                         {
                
                // Convolution operation
                if(yVal >= 0 && yVal < ISize && xVal >= 0 && xVal < ISize)
                 {
                    acc[input][i][j] =  (short) img_lcl[input][yVal * ISize + xVal] *
                                        (short) wgt_lcl[input][i * WSize + j];
                }
                else {
                    acc[input][i][j] = 0;
                }
            }
        }           
    }
     short sum = 0;
    accJ: for(int j = 0; j < WSize;j++)
        __attribute__((xcl_pipeline_loop))
        accK: for(int k = 0; k < WSize; k++)
            accI: for(int i = 0; i < i_chan; i++)
                sum += acc[i][j][k];

    // Update output pixel
    out_lcl[y * OSize + x] = sum;

     }
        
    }   // Burst write output
        
            int stride = output * OSize * OSize;
    
    // Work_item updates output filter/image in DDR
    __attribute__((xcl_pipeline_loop))
    writeOut: for(int itr = 0; itr < OSize * OSize; itr++) {
        out[stride + itr] = out_lcl[itr];
    }
    }

    return;
}
