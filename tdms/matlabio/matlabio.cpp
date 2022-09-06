/*****************************************************************
 *  Application.:  matlab data structure manipulation
 *  Description.:  code for processing the matlab data structures
 ******************************************************************/
#include "mat_io.h"
#include "matlabio.h"


using namespace std;


/*Casts a 4-dimensional array such that it may be indexed according to the 
  usual array indexing scheme array[l,k,j,i].

  array is a point to a matlab 4 dimensional array
  nrows the number of rows in the array
  ncols the number of columns in the array
  nlayers the number of layers, each of dimension nrows*ncols 
  nblocks the number of blocks, each of dimension nrows*ncols*nlayers 

*/

double ****castMatlab4DArray(double *array, int nrows, int ncols, int nlayers, int nblocks){
  double ****p;
  int j,k,l;

  if( nlayers==0 )
	  nlayers++;
  
  if( nblocks==0 )
	  nblocks++;
  
  p = (double ****)malloc((unsigned) (nblocks*sizeof(double ***)));
  for(l=0; l<nblocks; l++)
    p[l] = (double ***)malloc((unsigned) (nlayers*sizeof(double **)));
  
  for(l=0; l<nblocks; l++)
    for(k =0; k<nlayers;k++)
      p[l][k] = (double **)malloc((unsigned) (ncols*sizeof(double *)));

  for(l=0; l<nblocks; l++)
    for(k =0; k<nlayers;k++)
      for(j =0; j<ncols;j++)
	p[l][k][j] = (array + l*nrows*ncols*nlayers + k*nrows*ncols + j*nrows);
  return p;

}

/*Frees the axilliary memory used by the castMatlab3DArray
 */
void freeCastMatlab4DArray(double ****castArray, int nlayers, int nblocks){
  for(int l =0; l<nblocks; l++)
    for(int k =0; k<nlayers;k++)
      free(castArray[l][k]);

  for(int l =0; l<nblocks; l++)
    free(castArray[l]);
  free(castArray);
}


/*Casts a 3-dimensional array such that it may be indexed according to the 
  usual array indexing scheme array[k,j,i].

  array is a point to a matlab 3 dimensional array
  nrows the number of rows in the array
  ncols the number of columns in the array
  nlayers the number of layers, each of dimension nrows*ncols 

*/

double ***castMatlab3DArray(double *array, int nrows, int ncols, int nlayers){
  double ***p;
  int j,k;

  if( nlayers==0 )
	  nlayers++;

  p = (double ***)malloc((unsigned) (nlayers*sizeof(double **)));
  for(k =0; k<nlayers;k++)
    p[k] = (double **)malloc((unsigned) (ncols*sizeof(double *)));
  
  for(k =0; k<nlayers;k++)
    for(j =0; j<ncols;j++)
      p[k][j] = (array + k*nrows*ncols+ j*nrows);
  return p;

}

/*Frees the axilliary memory used by the castMatlab3DArray
 */
void freeCastMatlab3DArray(double ***castArray, int nlayers){
  for(int k =0; k<nlayers;k++)
    free(castArray[k]);
  free(castArray);
}

/*Casts a 2-dimensional array such that it may be indexed according to the 
  usual array indexing scheme array[j,i].

  array is a point to a matlab 2 dimensional array
  nrows the number of rows in the array
  ncols the number of columns in the array

*/

double **castMatlab2DArray(double *array, int nrows, int ncols){
  double **p;
  int j;

  p = (double **)malloc((unsigned) (ncols*sizeof(double *)));
  for(j =0; j<ncols;j++)
      p[j] = (array + j*nrows);
  return p;

}

/*Frees the axilliary memory used by the castMatlab2DArray
 */
void freeCastMatlab2DArray(double **castArray){
  free(castArray);
}
/*Casts a 3-dimensional array such that it may be indexed according to the 
  usual array indexing scheme array[k,j,i].

  array is a point to a matlab 3 dimensional array
  nrows the number of rows in the array
  ncols the number of columns in the array
  nlayers the number of layers, each of dimension nrows*ncols 

*/

unsigned char ***castMatlab3DArrayUint8(unsigned char *array, int nrows, int ncols, int nlayers){
  unsigned char ***p;
  int j,k;
  
  if( nlayers==0 )
    nlayers++;
  
  p = (unsigned char ***)malloc((unsigned) (nlayers*sizeof(unsigned char **)));
  for(k =0; k<nlayers;k++)
    p[k] = (unsigned char **)malloc((unsigned) (ncols*sizeof(unsigned char *)));
  
  for(k =0; k<nlayers;k++)
    for(j =0; j<ncols;j++)
      p[k][j] = (array + k*nrows*ncols+ j*nrows);
  return p;

}

/*Frees the axilliary memory used by the castMatlab3DArray
 */
void freeCastMatlab3DArrayUint8(unsigned char ***castArray, int nlayers){
  for(int k =0; k<nlayers;k++)
    free(castArray[k]);
  free(castArray);
}

/*Casts a 3-dimensional array such that it may be indexed according to the 
  usual array indexing scheme array[k,j,i].

  array is a point to a matlab 3 dimensional array
  nrows the number of rows in the array
  ncols the number of columns in the array
  nlayers the number of layers, each of dimension nrows*ncols 

*/

int ***castMatlab3DArrayInt(int *array, int nrows, int ncols, int nlayers){
  int ***p;
  int j,k;
  
  if( nlayers==0 )
    nlayers++;

  p = (int ***)malloc((unsigned) (nlayers*sizeof(int **)));
  for(k =0; k<nlayers;k++)
    p[k] = (int **)malloc((unsigned) (ncols*sizeof(int *)));
  
  for(k =0; k<nlayers;k++)
    for(j =0; j<ncols;j++)
      p[k][j] = (array + k*nrows*ncols+ j*nrows);
  return p;

}

/*Frees the axilliary memory used by the castMatlab3DArray
 */
void freeCastMatlab3DArrayInt(int ***castArray, int nlayers){
  for(int k =0; k<nlayers;k++)
    free(castArray[k]);
  free(castArray);
}

/*Casts a 2-dimensional array such that it may be indexed according to the 
  usual array indexing scheme array[j,i].

  array is a point to a matlab 2 dimensional array
  nrows the number of rows in the array
  ncols the number of columns in the array

*/

int **castMatlab2DArrayInt(int *array, int nrows, int ncols){
  int **p;
  int j;

  p = (int **)malloc((unsigned) (ncols*sizeof(int *)));
  for(j =0; j<ncols;j++)
      p[j] = (array + j*nrows);
  return p;

}

/*Frees the axilliary memory used by the castMatlab2DArray
 */
void freeCastMatlab2DArrayInt(int **castArray){
  free(castArray);
}
