#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "segy.h"
#include <assert.h>

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif
#define NINT(x) ((int)((x)>0.0?(x)+0.5:(x)-0.5))

int readWindowData(char *filename, float *xrcv, float *xsrc, float *zsrc, int *xnx, int Nfoc, int nx, int ntfft, int *maxval, int hw, int verbose)
{
	FILE *fp;
	segy hdr;
	size_t nread;
	int fldr_shot, sx_shot, itrace, one_shot, isyn, i, j;
	int end_of_file, nt, gx0, gx1;
	int nx1, jmax, imax, tstart, tend;
	float xmax, tmax, lmax;
	float scl, scel, *trace, dxrcv, *shot;

	/* Reading first header  */

	if (filename == NULL) fp = stdin;
	else fp = fopen( filename, "r" );
	if ( fp == NULL ) {
		fprintf(stderr,"input file %s has an error\n", filename);
		perror("error in opening file: ");
		fflush(stderr);
		return -1;
	}

	fseek(fp, 0, SEEK_SET);
	nread = fread( &hdr, 1, TRCBYTES, fp );
	assert(nread == TRCBYTES);
	if (hdr.scalco < 0) scl = 1.0/fabs((float)hdr.scalco);
	else if (hdr.scalco == 0) scl = 1.0;
	else scl = hdr.scalco;
	if (hdr.scalel < 0) scel = 1.0/fabs((float)hdr.scalel);
	else if (hdr.scalel == 0) scel = 1.0;
	else scel = hdr.scalel;
	fseek(fp, 0, SEEK_SET);

	nt     = hdr.ns;
	trace  = (float *)calloc(ntfft,sizeof(float));
	shot   = (float *)calloc(nx*ntfft,sizeof(float));

	end_of_file = 0;
	one_shot    = 1;
	isyn        = 0;

	/* Read shots in file */

	while (!end_of_file) {

		/* start reading data (shot records) */
		itrace     = 0;
		nread = fread( &hdr, 1, TRCBYTES, fp );
		if (nread != TRCBYTES) { /* no more data in file */
			break;
		}

		sx_shot    = hdr.sx;
		fldr_shot  = hdr.fldr;
        gx0        = hdr.gx;
		xsrc[isyn] = sx_shot*scl;
		zsrc[isyn] = hdr.selev*scel;
		xnx[isyn]  = 0;
		while (one_shot) {
			xrcv[isyn*nx+itrace] = hdr.gx*scl;
			nread = fread( trace, sizeof(float), nt, fp );
			assert (nread == hdr.ns);

            memcpy( &shot[itrace*ntfft], trace, nt*sizeof(float));

            gx1 = hdr.gx;
			itrace++;

			/* read next hdr of next trace */
			nread = fread( &hdr, 1, TRCBYTES, fp );
			if (nread != TRCBYTES) { 
				one_shot = 0;
				end_of_file = 1;
				break;
			}
			if ((sx_shot != hdr.sx) || (fldr_shot != hdr.fldr) ) break;
		}
		if (verbose>3) {
			fprintf(stderr,"finished reading shot %d (%d) with %d traces\n",sx_shot,isyn,itrace);
			//disp_fileinfo(filename, nt, xnx[isyn], hdr.f1, xrcv[isyn*nxm], d1, d2, &hdr);
		}

		/* look for maximum in shot record to define mute window */
        /* find consistent (one event) maximum related to maximum value */
		nx1 = itrace;
		xnx[isyn]=nx1;
        /* find global maximum 
		xmax=0.0;
		for (i = 0; i < nx1; i++) {
            tmax=0.0;
            jmax = 0;
            for (j = 0; j < nt; j++) {
                lmax = fabs(shot[i*ntfft+j]);
                if (lmax > tmax) {
                    jmax = j;
                    tmax = lmax;
                    if (lmax > xmax) {
                        imax = i;
                        xmax=lmax;
                    }
                }
            }
            maxval[isyn*nx+i] = jmax;
		}
		*/

        /* alternative find maximum at source position */
        dxrcv = (gx1 - gx0)*scl/(float)(nx1-1);
        imax = NINT(((sx_shot-gx0)*scl)/dxrcv);
        tmax=0.0;
        jmax = 0;
        for (j = 0; j < nt; j++) {
            lmax = fabs(shot[imax*ntfft+j]);
            if (lmax > tmax) {
                jmax = j;
                tmax = lmax;
                   if (lmax > xmax) {
                       xmax=lmax;
                   }
            }
        }
        maxval[isyn*nx+imax] = jmax;
        if (verbose >= 2) vmess("Mute max at src-trace %d is sample %d", imax, maxval[imax]);

        /* search forward in trace direction from maximum in file */
        for (i = imax+1; i < nx1; i++) {
            tstart = MAX(0, (maxval[isyn*nx+i-1]-hw));
            tend   = MIN(nt-1, (maxval[isyn*nx+i-1]+hw));
            jmax=tstart;
            tmax=0.0;
            for(j = tstart; j <= tend; j++) {
                lmax = fabs(shot[i*ntfft+j]);
                if (lmax > tmax) {
                    jmax = j;
                    tmax = lmax;
                }
            }
            maxval[isyn*nx+i] = jmax;
        }
        /* search backward in trace direction from maximum in file */
        for (i = imax-1; i >=0; i--) {
            tstart = MAX(0, (maxval[isyn*nx+i+1]-hw));
            tend   = MIN(nt-1, (maxval[isyn*nx+i+1]+hw));
            jmax=tstart;
            tmax=0.0;
            for(j = tstart; j <= tend; j++) {
                lmax = fabs(shot[i*ntfft+j]);
                if (lmax > tmax) {
                    jmax = j;
                    tmax = lmax;
                }
            }
            maxval[isyn*nx+i] = jmax;
        }

		if (itrace != 0) { /* end of shot record, but not end-of-file */
			fseek( fp, -TRCBYTES, SEEK_CUR );
			isyn++;
            memset(shot,0,nx*ntfft*sizeof(float));
		}
		else {
			end_of_file = 1;
		}

	}

	free(trace);
	free(shot);

	return 0;
}