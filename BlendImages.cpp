///////////////////////////////////////////////////////////////////////////
//
// NAME
//  BlendImages.cpp -- blend together a set of overlapping images
//
// DESCRIPTION
//  This routine takes a collection of images aligned more or less horizontally
//  and stitches together a mosaic.
//
//  The images can be blended together any way you like, but I would recommend
//  using a soft halfway blend of the kind Steve presented in the first lecture.
//
//  Once you have blended the images together, you should crop the resulting
//  mosaic at the halfway points of the first and last image.  You should also
//  take out any accumulated vertical drift using an affine warp.
//  Lucas-Kanade Taylor series expansion of the registration error.
//
// SEE ALSO
//  BlendImages.h       longer description of parameters
//
// Copyright ?Richard Szeliski, 2001.  See Copyright.h for more details
// (modified for CSE455 Winter 2003)
//
///////////////////////////////////////////////////////////////////////////

#include "ImageLib/ImageLib.h"
#include "BlendImages.h"
#include <float.h>
#include <math.h>
#include <iostream>
using namespace std;

#define MAX(x,y) (((x) < (y)) ? (y) : (x))
#define MIN(x,y) (((x) < (y)) ? (x) : (y))

/* Return the closest integer to x, rounding up */
static int iround(double x) {
    if (x < 0.0) {
        return (int) (x - 0.5);
    } else {
        return (int) (x + 0.5);
    }
}

void ImageBoundingBox(CImage &image, CTransform3x3 &M, 
    int &min_x, int &min_y, int &max_x, int &max_y)
{
    // This is a useful helper function that you might choose to implement
    // takes an image, and a transform, and computes the bounding box of the
    // transformed image.
	CShape sh = image.Shape();
    int width = sh.width;
    int height = sh.height;
	CVector3 p1, p2, p3, p4;
	p1[0] = 0; p1[1] = 0; p1[2] = 1;
	p2[0] = 0; p2[1] = height; p2[2] = 1;
	p3[0] = width; p3[1] = 0; p3[2] = 1;
	p4[0] = width; p4[1] = height; p4[2] = 1;
	p1 = M*p1; p2 = M*p2; p3 = M*p3; p4 = M*p4;
	min_x = iround(MIN(MIN(MIN(p1[0]/p1[2],p2[0]/p2[2]),p3[0]/p3[2]),p4[0]/p4[2]));
	max_x = iround(MAX(MAX(MAX(p1[0]/p1[2],p2[0]/p2[2]),p3[0]/p3[2]),p4[0]/p4[2]));
	min_y = iround(MIN(MIN(MIN(p1[1]/p1[2],p2[1]/p2[2]),p3[1]/p3[2]),p4[1]/p4[2]));
	max_y = iround(MAX(MAX(MAX(p1[1]/p1[2],p2[1]/p2[2]),p3[1]/p3[2]),p4[1]/p4[2]));
}


/******************* TO DO *********************
* AccumulateBlend:
*	INPUT:
*		img: a new image to be added to acc
*		acc: portion of the accumulated image where img is to be added
*       M: the transformation mapping the input image 'img' into the output panorama 'acc'
*		blendWidth: width of the blending function (horizontal hat function;
*	    try other blending functions for extra credit)
*	OUTPUT:
*		add a weighted copy of img to the subimage specified in acc
*		the first 3 band of acc records the weighted sum of pixel colors
*		the fourth band of acc records the sum of weight
*/
static void AccumulateBlend(CByteImage& img, CFloatImage& acc, CTransform3x3 M, float blendWidth)
{
    // BEGIN TODO
    // Fill in this routine
	// get shape of acc and img
	CShape sh = img.Shape();
    int width = sh.width;
    int height = sh.height;
	CShape shacc = acc.Shape();
    int widthacc = shacc.width;
    int heightacc = shacc.height;
	
	// get the bounding box of img in acc
	int min_x, min_y, max_x, max_y;
	ImageBoundingBox(img, M, min_x, min_y, max_x, max_y);

	CVector3 p;
	double newx, newy;

	// Exposure Compensation
	double lumaScale = 1.0;
	double lumaAcc = 0.0;
	double lumaImg = 0.0;
	int cnt = 0;

	for (int ii = min_x; ii < max_x; ii++)
		for (int jj = min_y; jj < max_y; jj++)
		{
			// flag: current pixel black or not
			bool flag = false;
			p[0] = ii; p[1] = jj; p[2] = 1;
			p = M.Inverse() * p;
			newx = p[0] / p[2];
			newy = p[1] / p[2];
			// If in the overlapping region
			if (newx >=0 && newx < width && newy >=0 && newy < height)
			{
				if (acc.Pixel(ii,jj,0) == 0 &&
					acc.Pixel(ii,jj,1) == 0 &&
					acc.Pixel(ii,jj,2) == 0)
					flag = true;
				if (img.PixelLerp(newx,newy,0) == 0 &&
					img.PixelLerp(newx,newy,1) == 0 &&
					img.PixelLerp(newx,newy,2) == 0)
					flag = true;
				if (!flag)
				{
					// Compute Y using RGB (RGB -> YUV)
					lumaAcc = 0.299 * acc.Pixel(ii,jj,0) +
							   0.587 * acc.Pixel(ii,jj,1) +
							   0.114 * acc.Pixel(ii,jj,2);
					lumaImg = 0.299 * img.PixelLerp(newx,newy,0) +
							   0.587 * img.PixelLerp(newx,newy,1) +
							   0.114 * img.PixelLerp(newx,newy,2);
					
					if (lumaImg != 0)
					{
						double scale = lumaAcc / lumaImg;
						if (scale > 0.5 && scale < 2)
						{
							lumaScale += lumaAcc / lumaImg;
							cnt++;
						}
					}
				}
			}
		}

	if (cnt != 0)
		lumaScale = lumaScale / (double)cnt;
	else lumaScale = 1.0;

	// add every pixel in img to acc, feather the region withing blendwidth to the bounding box,
	// pure black pixels (caused by warping) are not added
	double weight;
	
	for (int ii = min_x; ii < max_x; ii++)
		for (int jj = min_y; jj < max_y; jj++)
		{
			p[0] = ii; p[1] = jj; p[2] = 1;
			p = M.Inverse() * p;
			newx = p[0] / p[2];
			newy = p[1] / p[2];
			if ((newx >= 0) && (newx < width-1) && (newy >= 0) && (newy < height-1))
			{
				weight = 1.0;
				if ( (ii >= min_x) && (ii < (min_x+blendWidth)) )
					weight = (ii-min_x) / blendWidth;
				if ( (ii <= max_x) && (ii > (max_x-blendWidth)) )
					weight = (max_x-ii) / blendWidth;
				if (img.Pixel(iround(newx),iround(newy),0) == 0 &&
					img.Pixel(iround(newx),iround(newy),1) == 0 &&
					img.Pixel(iround(newx),iround(newy),2) == 0)
					weight = 0.0;

				double LerpR = img.PixelLerp(newx, newy, 0);
				double LerpG = img.PixelLerp(newx, newy, 1);
				double LerpB = img.PixelLerp(newx, newy, 2);
				
				double r = LerpR*lumaScale > 255.0 ? 255.0 : LerpR*lumaScale;
				double g = LerpG*lumaScale > 255.0 ? 255.0 : LerpG*lumaScale;
				double b = LerpB*lumaScale > 255.0 ? 255.0 : LerpB*lumaScale;
				acc.Pixel(ii,jj,0) += r * weight;
				acc.Pixel(ii,jj,1) += g * weight;
				acc.Pixel(ii,jj,2) += b * weight;
				acc.Pixel(ii,jj,3) += weight;
			}
		}
	
	printf("AccumulateBlend\n"); 

    // END TODO
}



/******************* TO DO 5 *********************
* NormalizeBlend:
*	INPUT:
*		acc: input image whose alpha channel (4th channel) contains
*		     normalizing weight values
*		img: where output image will be stored
*	OUTPUT:
*		normalize r,g,b values (first 3 channels) of acc and store it into img
*/
static void NormalizeBlend(CFloatImage& acc, CByteImage& img)
{
    // BEGIN TODO
    // fill in this routine..
	// divide the total weight for every pixel
	CShape shacc=acc.Shape();
    int widthacc=shacc.width;
    int heightacc=shacc.height;
	for (int ii=0;ii<widthacc;ii++)
	{
        for (int jj=0;jj<heightacc;jj++)
		{
			if (acc.Pixel(ii,jj,3)>0)
			{
				img.Pixel(ii,jj,0)=(int)(acc.Pixel(ii,jj,0)/acc.Pixel(ii,jj,3));
				img.Pixel(ii,jj,1)=(int)(acc.Pixel(ii,jj,1)/acc.Pixel(ii,jj,3));
				img.Pixel(ii,jj,2)=(int)(acc.Pixel(ii,jj,2)/acc.Pixel(ii,jj,3));
			}
			else
			{
				img.Pixel(ii,jj,0)=0;
				img.Pixel(ii,jj,1)=0;
				img.Pixel(ii,jj,2)=0;
			}
		}
	}

    // END TODO
}



/******************* TO DO 5 *********************
* BlendImages:
*	INPUT:
*		ipv: list of input images and their relative positions in the mosaic
*		blendWidth: width of the blending function
*	OUTPUT:
*		create & return final mosaic by blending all images
*		and correcting for any vertical drift
*/
CByteImage BlendImages(CImagePositionV& ipv, float blendWidth)
{
    // Assume all the images are of the same shape (for now)
    CByteImage& img0 = ipv[0].img;
    CShape sh        = img0.Shape();
    int width        = sh.width;
    int height       = sh.height;
    int nBands       = sh.nBands;
    // int dim[2]       = {width, height};

    int n = ipv.size();
    if (n == 0) return CByteImage(0,0,1);

    bool is360 = false;

    // Hack to detect if this is a 360 panorama
    if (ipv[0].imgName == ipv[n-1].imgName)
        is360 = true;

    // Compute the bounding box for the mosaic
    float min_x = FLT_MAX, min_y = FLT_MAX;
    float max_x = 0, max_y = 0;
    int i;
	int tmpmin_x,tmpmin_y,tmpmax_x,tmpmax_y;
    for (i = 0; i < n; i++)
    {
        CTransform3x3 &T = ipv[i].position;

        // BEGIN TODO
        // add some code here to update min_x, ..., max_y
		ImageBoundingBox(img0,T,tmpmin_x,tmpmin_y,tmpmax_x,tmpmax_y);
		min_x=MIN(tmpmin_x,min_x);
		min_y=MIN(tmpmin_y,min_y);
		max_x=MAX(tmpmax_x,max_x);
		max_y=MAX(tmpmax_y,max_y);
		// END TODO
    }

    // Create a floating point accumulation image
    CShape mShape((int)(ceil(max_x) - floor(min_x)), (int)(ceil(max_y) - floor(min_y)), nBands + 1);
    CFloatImage accumulator(mShape);
    accumulator.ClearPixels();
	
    double x_init, x_final;
    double y_init, y_final;

    // Add in all of the images
    for (i = 0; i < n; i++) 
	{
        // Compute the sub-image involved
        CTransform3x3 &M = ipv[i].position;
        CTransform3x3 M_t = CTransform3x3::Translation(-min_x, -min_y) * M;
        CByteImage& img = ipv[i].img;
		
		// Perform the accumulation
        AccumulateBlend(img, accumulator, M_t, blendWidth);
		
        if (i == 0) 
		{
            CVector3 p;
            p[0] = 0.5 * width;
            p[1] = 0.0;
            p[2] = 1.0;

            p = M_t * p;
            x_init = p[0];
            y_init = p[1];
        } else if (i == n - 1) 
		{
            CVector3 p;
            p[0] = 0.5 * width;
            p[1] = 0.0;
            p[2] = 1.0;

            p = M_t * p;
            x_final = p[0];
            y_final = p[1];
        }
    }

    // Normalize the results
    mShape = CShape((int)(ceil(max_x) - floor(min_x)), (int)(ceil(max_y) - floor(min_y)), nBands);

    CByteImage compImage(mShape);
    NormalizeBlend(accumulator, compImage);
    bool debug_comp = false;
    if (debug_comp)
        WriteFile(compImage, "tmp_comp.tga");

    // Allocate the final image shape
    int outputWidth = 0;
    if (is360) 
	{
        outputWidth = mShape.width - width;
    } else 
	{
        outputWidth = mShape.width;
    }

    CShape cShape(outputWidth, mShape.height, nBands);

    CByteImage croppedImage(cShape);

    // Compute the affine transformation
    CTransform3x3 A = CTransform3x3(); // identify transform to initialize

    // BEGIN TODO
    // fill in appropriate entries in A to trim the left edge and
    // to take out the vertical drift if this is a 360 panorama
    // (i.e. is360 is true)
	double k;
	if (is360)
	{
		if (x_init>x_final)
		{
			int tmp=x_init;
			x_init=x_final;
			x_final=tmp;
		}
		CTransform3x3 AA = CTransform3x3();;
		k=-(y_final-y_init)/(x_final-x_init);
		AA[0][0]=1;AA[0][1]=0;AA[0][2]=0;
		AA[1][0]=k;AA[1][1]=1;AA[1][2]=0;
		AA[2][0]=0;AA[2][1]=0;AA[2][2]=1;
		A = CTransform3x3::Translation(0, 0) * AA;
		AA[0][0]=1;AA[0][1]=0;AA[0][2]=0;
		AA[1][0]=0;AA[1][1]=(double)height/(double)mShape.height;AA[1][2]=0;
		//cout<<(double)height/(double)mShape.height<<endl;
		AA[2][0]=0;AA[2][1]=0;AA[2][2]=1;
		A = A * AA;
	}

    // END TODO

    // Warp and crop the composite
    WarpGlobal(compImage, croppedImage, A, eWarpInterpLinear);

    return croppedImage;
}

