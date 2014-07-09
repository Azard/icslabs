/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following team struct 
 */
team_t team = {
    "5120379076",              /* Student ID */

    "熊伟伦",     	    /* Your Name */
    "330815461@qq.com",  /* First member email address */

    "",                   /* Second member full name (leave blank if none) */
    ""                    /* Second member email addr (leave blank if none) */
};

/***************
 * ROTATE KERNEL
 ***************/

/******************************************************
 * Your different versions of the rotate kernel go here
 ******************************************************/
/*
 
 
 *Add the description of your Rotate implementation here!!!
 *1. Optimize write use cache and 16 unrolling.
 *2. CPE 2.4 on testbed.
 
 
 */

/* 
 * naive_rotate - The naive baseline version of rotate 
 */
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
}

/* 
 * rotate - Your current working version of rotate
 * IMPORTANT: This is the version you will be graded on
 */
char rotate_descr[] = "rotate: Current working version";
void rotate(int dim, pixel *src, pixel *dst) 
{
	int i,j;
	int block = 16;
    int count = dim >> 4;
	int temp = dim << 4; //dim*block dim*16
	src += dim - 1;	
	for(i=0; i<count; ++i){
		for(j=0; j<dim; ++j){
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			*dst++ = *src;
			src += dim;
			src -= temp + 1;
			dst += dim - block;
		}
		src += temp + dim;
		dst -= dim * dim - block;
	}
}


/*********************************************************************
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_rotate_functions() 
{
    add_rotate_function(&naive_rotate, naive_rotate_descr);   
    add_rotate_function(&rotate, rotate_descr);   
    /* ... Register additional test functions here */
}


/***************
 * SMOOTH KERNEL
 **************/
/*


 *1. three pixel a set, reduce the times of load pixel from memory,
     reduce 9 times per pixel to 3 times per pixel.
 *2. CPE 3.7 on testbed.


 */
/***************************************************************
 * Various typedefs and helper functions for the smooth function
 * You may modify these any way you like.
 **************************************************************/

/* A struct used to compute averaged pixel value */
typedef struct {
    int red;
    int green;
    int blue;
    int num;
} pixel_sum;

/* Compute min and max of two integers, respectively */
static int min(int a, int b) { return (a < b ? a : b); }
static int max(int a, int b) { return (a > b ? a : b); }

/* 
 * initialize_pixel_sum - Initializes all fields of sum to 0 
 */
static void initialize_pixel_sum(pixel_sum *sum) 
{
    sum->red = sum->green = sum->blue = 0;
    sum->num = 0;
    return;
}

/* 
 * accumulate_sum - Accumulates field values of p in corresponding 
 * fields of sum 
 */
static void accumulate_sum(pixel_sum *sum, pixel p) 
{
    sum->red += (int) p.red;
    sum->green += (int) p.green;
    sum->blue += (int) p.blue;
    sum->num++;
    return;
}

/* 
 * assign_sum_to_pixel - Computes averaged pixel value in current_pixel 
 */
static void assign_sum_to_pixel(pixel *current_pixel, pixel_sum sum) 
{
    current_pixel->red = (unsigned short) (sum.red/sum.num);
    current_pixel->green = (unsigned short) (sum.green/sum.num);
    current_pixel->blue = (unsigned short) (sum.blue/sum.num);
    return;
}

/* 
 * avg - Returns averaged pixel value at (i,j) 
 */
static pixel avg(int dim, int i, int j, pixel *src) 
{
    int ii, jj;
    pixel_sum sum;
    pixel current_pixel;

    initialize_pixel_sum(&sum);
    for(ii = max(i-1, 0); ii <= min(i+1, dim-1); ii++) 
	for(jj = max(j-1, 0); jj <= min(j+1, dim-1); jj++) 
	    accumulate_sum(&sum, src[RIDX(ii, jj, dim)]);

    assign_sum_to_pixel(&current_pixel, sum);
    return current_pixel;
}

/******************************************************
 * Your different versions of the smooth kernel go here
 ******************************************************/

/*
 * naive_smooth - The naive baseline version of smooth 
 */
char naive_smooth_descr[] = "naive_smooth: Naive baseline implementation";
void naive_smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(i, j, dim)] = avg(dim, i, j, src);
}

/*
 * smooth - Your current working version of smooth. 
 * IMPORTANT: This is the version you will be graded on
 */
typedef struct {
    int red;
    int green;
    int blue;
} pixel_color;

char smooth_descr[] = "smooth: Current working version";
void smooth(int dim, pixel *src, pixel *dst) 
{
	pixel_color save[dim][dim];  
	// load data to stack
	int i,j;
	for (i = 0; i < dim; ++i) {
		for (j = 1; j < dim-1; ++j) {
			save[i][j].red = save[i][j].green = save[i][j].blue = 0;
			int temp = i*dim+j;
			save[i][j].red = src[temp-1].red + src[temp].red + src[temp+1].red;
			save[i][j].green = src[temp-1].green + src[temp].green + src[temp+1].green;
			save[i][j].blue = src[temp-1].blue + src[temp].blue + src[temp+1].blue;
		}
		int temp = i*dim;
		save[i][0].red = save[i][0].green = save[i][0].blue = 0;
		save[i][0].red = src[temp].red + src[temp+1].red;
		save[i][0].green = src[temp].green + src[temp+1].green;
		save[i][0].blue = src[temp].blue + src[temp+1].blue;
		temp += dim-2;
		save[i][dim-1].red = save[i][dim-1].green = save[i][dim-1].blue = 0;
		save[i][dim-1].red = src[temp].red + src[temp+1].red;
		save[i][dim-1].green = src[temp].green + src[temp+1].green;
		save[i][dim-1].blue = src[temp].blue + src[temp+1].blue;
	}

	// write to new picture
	for (i = 1; i < dim-1; ++i)
		for (j = 1; j < dim-1; ++j) {	
			dst[i*dim+j].red = (save[i-1][j].red + save[i][j].red + save[i+1][j].red)/9;
			dst[i*dim+j].green = (save[i-1][j].green + save[i][j].green + save[i+1][j].green)/9;
			dst[i*dim+j].blue = (save[i-1][j].blue + save[i][j].blue + save[i+1][j].blue)/9;
		}
	// top and bottom
	int temp = dim*dim-dim;
	for (j = 1; j < dim-1; ++j) {
		dst[j].red = (save[0][j].red + save[1][j].red)/6;
		dst[j].green = (save[0][j].green + save[1][j].green)/6;
		dst[j].blue = (save[0][j].blue + save[1][j].blue)/6;
		dst[temp+j].red = (save[dim-2][j].red + save[dim-1][j].red)/6;
		dst[temp+j].green = (save[dim-2][j].green + save[dim-1][j].green)/6;
		dst[temp+j].blue = (save[dim-2][j].blue + save[dim-1][j].blue)/6;
	}
	// left and right
	temp = dim-1;
	for (i = 1; i < dim-1; ++i) {
		dst[i*dim].red = (save[i-1][0].red + save[i][0].red + save[i+1][0].red)/6;
		dst[i*dim].green = (save[i-1][0].green + save[i][0].green + save[i+1][0].green)/6;
		dst[i*dim].blue = (save[i-1][0].blue + save[i][0].blue + save[i+1][0].blue)/6;
		dst[i*dim+temp].red = (save[i-1][temp].red + save[i][temp].red + save[i+1][temp].red)/6;
		dst[i*dim+temp].green = (save[i-1][temp].green + save[i][temp].green + save[i+1][temp].green)/6;
		dst[i*dim+temp].blue = (save[i-1][temp].blue + save[i][temp].blue + save[i+1][temp].blue)/6;
	}
	// four corner
	dst[0].red = (save[0][0].red + save[1][0].red)/4;
	dst[0].green = (save[0][0].green + save[1][0].green)/4;
	dst[0].blue = (save[0][0].blue + save[1][0].blue)/4;
	dst[dim-1].red = (save[0][dim-1].red + save[1][dim-1].red)/4;
	dst[dim-1].green = (save[0][dim-1].green + save[1][dim-1].green)/4;
	dst[dim-1].blue = (save[0][dim-1].blue + save[1][dim-1].blue)/4;
	dst[dim*dim-dim].red = (save[dim-2][0].red + save[dim-1][0].red)/4;
	dst[dim*dim-dim].green = (save[dim-2][0].green + save[dim-1][0].green)/4;
	dst[dim*dim-dim].blue = (save[dim-2][0].blue + save[dim-1][0].blue)/4;
	dst[dim*dim-1].red = (save[dim-2][dim-1].red + save[dim-1][dim-1].red)/4;
	dst[dim*dim-1].green = (save[dim-2][dim-1].green + save[dim-1][dim-1].green)/4;
	dst[dim*dim-1].blue = (save[dim-2][dim-1].blue + save[dim-1][dim-1].blue)/4;

}


/********************************************************************* 
 * register_smooth_functions - Register all of your different versions
 *     of the smooth kernel with the driver by calling the
 *     add_smooth_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_smooth_functions() {
    add_smooth_function(&naive_smooth, naive_smooth_descr);
    add_smooth_function(&smooth, smooth_descr);
    /* ... Register additional test functions here */
}
