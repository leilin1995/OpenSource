#include<stdlib.h>
#include<math.h>
#include<complex.h>
#include<fftw3.h>
#include"fdacrtmc.h"

int k1k2CircFilt(float *in, float *out, size_t n1, size_t n2, float d, float kl, float kh, fftPlansPar *fftPlans){
/* 2D-Wavenumber Circular Filter */
/* Dispersion is muted by applying a circular cosine-square tapered filter around the origin in
   the wavenumber domain. The taper is 1 till kl, after which it decays accoring to the cosine
   squared to 0 at kh.
   
   INPUTS:
      float       *in          Floating point input  array.
      float       *out         Floating point output array.
      size_t       n1          Size of fast dimension
      size_t       n2          Size of slow dimension
      float        d           Spatial step size for both dimensions
      float        kl          Inner radius, or start, of wavenumber filter
      float        kh          Outer radius, or end,   of wavenumber filter
      fftPlansPar *fftPlans    Structure containing FFTw plans

   OUTPUT:
      int                      0 for normal termination

   NOTE:
     1) Input pointer can be the same as output pointer.
*/
	/* Declarations */
	size_t i, i1, i2, n12, nk1p, nk1, nk2p, nk2, nk1k2;
	bool evenk1, evenk2;
	double dk, k, *k1, *k2, *IN;
	fftw_complex *KK, *OP;

	/* Compute Constants */
	n12=n1*n2;    //Total number of elements in arrays
	// Fast Dimension
	nk1p=n1/2;     //Number of positive fast dimension wavenumbers
	nk1 =nk1p+1;   //Number of          fast dimension wavenumbers
//	nk1n=n1-nk1;   //Number of negative fast dimension wavenumbers
	// Slow Dimension
	nk2p=n2/2;     //Number of positive slow dimension wavenumbers
	nk2 =nk2p+1;   //Number of          slow dimension wavenumbers
//	nk2n=n2-nk2;   //Number of negative slow dimension wavenumbers

	/* Check Even or Odd */
	if(n1%2){
		evenk1=false;
		nk1--;
	}else{
		// Note we mute the highest frequency component
		evenk1=true;
		nk1-=2;
	}
	if(n2%2){
		evenk2=false;
		nk2--;
	}else{
		// Note we mute the highest wavenumber component
		evenk2=true;
		nk2-=2;
	}
	nk1k2=nk1*n2;

	/* Allocate Arrays */
	k1 =(double*)fftw_malloc(nk1*sizeof(double));                 //Fast Dimension Wavenumber
	k2 =(double*)fftw_malloc(n2*sizeof(double));                  //Fast Dimension Wavenumber
	IN =(double*)fftw_malloc(n12*sizeof(double));                 //Double precision input  array
	KK =(fftw_complex*)fftw_malloc(nk1k2*sizeof(fftw_complex));   //Wavenumber domain array
	OP =(fftw_complex*)fftw_malloc(nk1k2*sizeof(fftw_complex));   //Wavenumber domain array

	/* Construct Wavenumber Arrays */
	// k1
	if(evenk1){
		dk=6.28318530717958647692528676655900576839433879875021164194989/(((double)n1)*d);
	}else{
		dk=6.28318530717958647692528676655900576839433879875021164194989/(((double)(n1-1))*d);
	}
	*k1=0; //First wavenumber is zero
	for(i1=1;i1<nk1;i1++)k1[i1]=i1*dk;
	// k1
	if(evenk2){
		dk=6.28318530717958647692528676655900576839433879875021164194989/(((double)n1)*d);
	}else{
		dk=6.28318530717958647692528676655900576839433879875021164194989/(((double)(n1-1))*d);
	}
	*k2=0; //First wavenumber is zero
	for(i2=1;i2<nk2p;i2++)k2[i2]=i2*dk;
	if(evenk2)i1=i2-1;else i1=i2;
	for(;i1>0;i1--)k2[++i2]=-k2[i1--];

	/* Compute Operator */
	dk=kh-kl;
	for(i2=0;i2<nk2;i2++){
		for(i1=0;i1<nk1;i1++){
			k=sqrtf(k1[i1]*k1[i1]+k2[i2]*k2[i2]);
			if(k<kl){
				OP[i2*nk1+i1]=1.;
			}else if(k>kh){
				OP[i2*nk1+i1]=0.;
			}else{
				OP[i2*nk1+i1]=cos(1.5707963267948966192313216916398*(k-kl)/dk);
				OP[i2*nk1+i1]*=OP[i2*nk1+i1];
			}
		}
	}
	if(evenk2)i=i2-1;else i=i2;
	for(;i>0;i--)OP[++i2*nk1+i1]=OP[i--*nk1+i1];

	/* Convert to Double */
	for(i=0;i<n12;i++) IN[i]=(double)in[i];

	/* Transform to the Wavenumber Domain */
	fftw_execute_dft_r2c(fftPlans->fft_2d_r2c_XZ,IN,KK);

	/* Apply Operator */
	for(i=0;i<nk1k2;i++)KK[i]*=OP[i];

	/* Transform to the Space Domain */
	fftw_execute_dft_c2r(fftPlans->ifft_2d_c2r_KxKz,KK,IN);

	/* Convert to Single */
	for(i=0;i<n12;i++) out[i]=(float)IN[i];

	/* Clean-Up & Return */
	free(k1);free(k2);
	free(IN);free(KK);
	free(OP);
	return(0);
}
