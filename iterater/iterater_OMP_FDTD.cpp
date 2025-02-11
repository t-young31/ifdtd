/*****************************************************************
 *
 *  Project.....:  isotropic FDTD code
 *  Application.:  main FDTD algorithm
 *  Module......:  iterater.cpp
 *  Description.:  Contains the main FDTD loop as well as other functions
 *                 such as phasor extraction etc. Works in both pulsed 
 *                 and steady state mode.
 *  Compiler....:  g++
 *  Written by..:  Peter Munro, Imperial College London, 2002-2008
 *  Environment.:  Linux
 *  Modified....:  Numerous times
 *
 ******************************************************************/

/*iterater_OMP_reduced.cpp iterater_OMP.cpp are the same
 *iterater_OMP_full.cpp predates iterater_OMP.cpp and doesn't skip any steps in phasor extraction.
 *
 */

/*---------------------------------------------------------------*/
//                        INCLUDE section
/*---------------------------------------------------------------*/
#include <omp.h>
#include "matio.h"
#include <complex>
#include "iterater_OMP_PSTD.h"
#include <string.h>
#include "interpolate.h"
#include "numeric.h"
#include "mesh_base.h"
#include "numerical_derivative.h"
#include <time.h>

using namespace std;
#include "globals.h"
#include "matlabio.h"
#include "mesh_base.h"

/*---------------------------------------------------------------*/
//                        DEFINE section
/*---------------------------------------------------------------*/
//whether of not to time execution
#define TIME_EXEC 0
//time main loop
#define TIME_MAIN_LOOP 1
//threshold used to terminate the steady state iterations
#define TOL 1e-6
//parameter to control the with of the ramp when introducing the waveform in steady state mode
#define ramp_width 4.
//different run modes
#define rm_complete    0
#define rm_analyse     1
//different source modes
#define sm_steadystate 0
#define sm_pulsed      1
//dimensionallity of simulation
#define THREE 0
#define TE 1
#define TM 2

//Used for the 2D case when J_tot=0
inline int max ( int a, int b ) { return a > b ? a : b; }
inline int min ( int a, int b ) { return a < b ? a : b; }

/*This mex function will take in the following arguments
  
  fdtdgrid
  Cmaterial
  Dmaterial
  C
  D
  freespace
  disp_params
  delta
  interface
  Isource
  Jsource
  Ksource
  grid_labels
  omega_an
  to_l
  hwhm
  Dxl
  Dxu
  Dyl
  Dyu
  Dzl
  Dzu
  Nt
  dt
  start_tind
  sourcemode
  runmode
  exphasorsvolume
  exphasorssurface
  intphasorsurface
  phasorsurface
  phasorinc
  dimension
  conductive_aux
  dispersive_aux
  structure
  tdfield

  fdtdgrid - A structre with the following members, each of which is a 3 dimensional
  array:
	   
  fdtdgrid.Exy       (double)
  fdtdgrid.Exz
  fdtdgrid.Eyx 
  fdtdgrid.Eyz
  fdtdgrid.Ezx
  fdtdgrid.Ezy
  fdtdgrid.Hxy
  fdtdgrid.Hxz
  fdtdgrid.Hyx
  fdtdgrid.Hyz
  fdtdgrid.Hzx
  fdtdgrid.Hzy

  fdtdgrid.materials (uint8)

  Cmaterial - A structure with the following members, each of which is a 1 dimensional 
  array indexed by indices od scattering materials:
	    
  Cmaterial.Cax      (double)
  Cmaterial.Cay
  Cmaterial.Caz
  Cmaterial.Cbx
  Cmaterial.Cby
  Cmaterial.Cbz
  Cmaterial.Ccx
  Cmaterial.Ccy
  Cmaterial.Ccz

  Dmaterial - A structure with the following members, each of which is a 1 dimensional 
  array indexed by indices od scattering materials:
	    
  Dmaterial.Cax      (double)
  Dmaterial.Cay
  Dmaterial.Caz
  Dmaterial.Cbx
  Dmaterial.Cby
  Dmaterial.Cbz

  C - A structure with the same elements as Cmaterial whose elements relate to the background 
  region. Elements are not restricted to 1 dimension. 

  D - A structure with the same elements as Cmaterial whose elements relate to the background 
  region. Elements are not restricted to 1 dimension. 
	   
  freespace - A structure with the following members each of which is a double

  freespace.Cbx      (double)
  freespace.Cby
  freespace.Cbz
  freespace.Dbx
  freespace.Dby
  freespace.Dbz

  disp_params - A structure with the following members:

  disp_params.alpha      (double)
  disp_params.beta
  disp_params.gamma

  delta - A structure with the following elements representing Yee cell dimension

  delta.x (double)
  delta.y
  delta.z
  
  interface - A structure with the following members each of which is a double
         
  interface.I0 - [I0 apply]
  interface.I1 - [I1 apply]
  interface.J0 - [J0 apply]
  interface.J1 - [J1 apply]
  interface.K0 - [K0 apply]
  interface.K1 - [K1 apply]

  where, for example, I0 is the position of the interface plane between
  total and scattered formulations. apply is a boolean which indicates
  whether or not to apply the boundary condition
	    
  Isource - A 3d matrix of dimension 8x(J1-J0+1)x(K1-K0+1) which contains
  complex amplitude information for the interface equations at the I0 and I1 
  planes. Should be Isource(1,:,:) - Ey, Isource(2,:,:) - Ez, Isource(3,:,:) - Hy, 
  Isource(4,:,:) - Hz,Isource(5,:,:) - Ey, Isource(6,:,:) - Ez, Isource(7,:,:) - Hy, 
  Isource(8,:,:) - Hz

  Jsource - A 3d matrix of dimension 8x(I1-I0+1)x(K1-K0+1) which contains
  complex amplitude information for the interface equations at the J0 and J1 
  planes. Should be Jsource(1,:,:) - Ex, Jsource(2,:,:) - Ez, Jsource(3,:,:) - Hx, 
  Jsource(4,:,:) - Hz,Jsource(5,:,:) - Ex, Jsource(6,:,:) - Ez, Jsource(7,:,:) - Hx, 
  Jsource(8,:,:) - Hz

  Ksource - A 3d matrix of dimension 8x(I1-I0+1)x(J1-J0+1) which contains
  complex amplitude information for the interface equations at the K0 and K1 
  planes. Should be Ksource(1,:,:) - Ex, Ksource(2,:,:) - Ey, Ksource(3,:,:) - Hx, 
  Ksource(4,:,:) - Hy,Ksource(5,:,:) - Ex, Ksource(6,:,:) - Ey, Ksource(7,:,:) - Hx, 
  Ksource(8,:,:) - Hy

  grid_labels - A structure with 3 elements, represents the axis labels:
  x_grid_label - cartesian labels of Yee cell origins (x-coordinates)
  y_grid_label - cartesian labels of Yee cell origins (y-coordinates)
  z_grid_label - cartesian labels of Yee cell origins (z-coordinates)

  omega_an - analytic angular frequency source

  to_l - time delay of pulse

  hwhm - hwhm of pulse

  Dxl - thickness of upper pml in the x direction
  Dxu - thickness of upper pml in the x direction
  Dyl - thickness of upper pml in the y direction
  Dyu - thickness of upper pml in the y direction
  Dzl - thickness of upper pml in the z direction
  Dzu - thickness of upper pml in the z direction

  lower_boundary_update - boolean for applying the lower update equations

  Nt - the number of iterations

  start_tind - the starting value of the tind index which controls the current time
  step of each fdtd iteration. The default for this would be 0;

  sourcemode - integer of value 0 for s steady state simulation and value 1 for a pulse simulation

  runmode - integer of value 0 for "complete" and 1 for "analyse"

  exphasorsvolume - Extract phasors in the FDTD volume if set to true

  exphasorssurface - Extract phasors about a specified cuboid surface if set to true

  intphasorssurface - Interpolate surface phasors onto a common point if true

  phasorsurface - A list of indices defining the cuboid to extract the phasors at

  phasorinc - An integer vector of three elements describing the factor by which to reduce the 
  density of vertices in the enclosing observation mesh

  dimension - A string of value "3", "TE" or "TM"

  conductive_aux - auxilliary parameters required to model conductive multilayer

  dispersive_aux - auxilliary parameters required to model dispersive multilayer

  structure - 2 x (I_tot+1) integer array describing the grating stucture, if one is present

  f_ex_vec - 1xN or Nx1 vector of frequencies at which to perform the extraction of complex amplitudes

  tdfield - structure containing elements exi and eyi which have dimension (I1-I0+1)x(J1-J0+1)xNt

  fieldsample.i - indices along the x-direction of locations at which to sample the field
  fieldsample.j - indices along the y-direction of locations at which to sample the field
  fieldsample.k - indices along the z-direction of locations at which to sample the field
  fieldsample.n - vector of the moments of the field to sample

  campssample.vertices - N x 3 matrix of indices where the complex amplitudes will be sampled 
  campssample.components - numerical array of up to six elements which defines which field components
                           will be sampled, 1 means Ex, 2 Ey etc.

  
*/

/*void mexFunction(int nlhs, mxArray *plhs[], int nrhs,const mxArray *prhs[])

  The principle of structuring the program in this way is to be able to compile both a mex-file
  and an executable. This is a monolithic function which performs the entire FDTD simulation. It
  does farm out tasks to other functions however much of the core functionallity is found here. This
  is certainly not an example of good programming style however it reduces the amount of argument 
  passing.

*/

void mexfprintf(const char err[]){
  fprintf(stderr,(char *)err);
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs,const mxArray *prhs[])
{
  
  /*Local variables*/
  int useCD = 1;//determines whether FDTD or PSTD is used
  double time_0, time_1, time_ml_1, time_ml_0;
  double secs;
  complex<double> commonPhase;
  complex<double> *E_norm;
  complex<double> *H_norm;

  complex<double>  E_norm_an=0.;
  complex<double>  H_norm_an=0.;

  double fte = 0., fth = 0.;
  double commonAmplitude;
  double *x_grid_labels, *y_grid_labels, *z_grid_labels, *x_grid_labels_out, *y_grid_labels_out, *z_grid_labels_out;
  double ***Exy, ***Exz, ***Eyx, ***Eyz, ***Ezx, ***Ezy, ***Hxy, ***Hxz, ***Hyx, ***Hyz, ***Hzx, ***Hzy; 
  double ***Exy_nm1, ***Exz_nm1, ***Eyx_nm1, ***Eyz_nm1, ***Ezx_nm1, ***Ezy_nm1;
  double ***Jxy, ***Jxz, ***Jyx, ***Jyz, ***Jzx, ***Jzy, ***Jcxy, ***Jcxz, ***Jcyx, ***Jcyz, ***Jczx, ***Jczy;
  double ***Jxy_nm1, ***Jxz_nm1, ***Jyx_nm1, ***Jyz_nm1, ***Jzx_nm1, ***Jzy_nm1;
  double ***ExR, ***ExI, ***EyR, ***EyI, ***EzR, ***EzI,***HxR, ***HxI, ***HyR, ***HyI, ***HzR, ***HzI;
  double ***ExR2, ***ExI2, ***EyR2, ***EyI2, ***EzR2, ***EzI2;
  double **ex_tdf;
  mxArray *ex_tdf_array;

  double Ex_temp, Ey_temp, Ez_temp;
  
  double ***exi, ***eyi;
  int exi_present, eyi_present;
  double *I0, *I1, *J0, *J1, *K0, *K1;
  double ***IsourceI, ***JsourceI, ***KsourceI, ***IsourceR, ***JsourceR, ***KsourceR;
  double ***surface_EHr,***surface_EHi;
  double *alpha, *beta, *gamma;
  double *ml_alpha, *ml_beta, *ml_gamma, *ml_kappa_x, *ml_kappa_y, *ml_kappa_z, *ml_sigma_x, *ml_sigma_y, *ml_sigma_z;
  double *rho_x, *rho_y, *rho_z, rho;
  double alpha_l, beta_l, gamma_l;
  double kappa_l, sigma_l;
  double dx, dy, dz;
  double t0;
  double *Cmaterial_Cax,*Cmaterial_Cay,*Cmaterial_Caz, *Cmaterial_Cbx, *Cmaterial_Cby, *Cmaterial_Cbz, *Cmaterial_Ccx, *Cmaterial_Ccy, *Cmaterial_Ccz, *Dmaterial_Dax, *Dmaterial_Day, *Dmaterial_Daz, *Dmaterial_Dbx, *Dmaterial_Dby, *Dmaterial_Dbz;//non free space material parameters
  double *freespace_Cbx, *freespace_Cby __attribute__ ((unused)),*freespace_Cbz __attribute__ ((unused)),*freespace_Dbx __attribute__ ((unused)), *freespace_Dby __attribute__ ((unused)),*freespace_Dbz __attribute__ ((unused));//freespace variables 
  double Ca, Cb, Cc;//used by interpolation scheme 
  double *Cax, *Cay, *Caz,*Cbx, *Cby, *Cbz,*Ccx, *Ccy, *Ccz,*Dax, *Day, *Daz,*Dbx, *Dby, *Dbz;
  double *f_ex_vec;
  int N_f_ex_vec;
  //the C and D vars for free space and pml
  double Enp1, Jnp1;
  //these are used for boot strapping. There is currently no way of exporting this.
  double **iwave_lEx_Rbs, **iwave_lEy_Rbs, **iwave_lHx_Rbs, **iwave_lHy_Rbs,**iwave_lEx_Ibs, **iwave_lEy_Ibs, **iwave_lHx_Ibs, **iwave_lHy_Ibs;
  double *to_l, *hwhm, *omega_an, *dt;
  double maxfield = 0, tempfield;
  double *place_holder;
  double *array_ptr_dbl;
  int intmatprops=1;//means the material properties will be interpolated
  int intmethod;//method of interpolating surface field quantities

  double *fieldsample_i, *fieldsample_j, *fieldsample_k, *fieldsample_n;
  int N_fieldsample_i, N_fieldsample_j, N_fieldsample_k, N_fieldsample_n;
  
  double air_interface;
  int air_interface_present;
  //refractive index of the first layer of the multilayer, or of the bulk of homogeneous
  double refind;

  //PSTD storage
  double **ca_vec, **cb_vec, **cc_vec;
  fftw_complex **eh_vec, *dk_e_x, *dk_e_y, *dk_e_z, *dk_h_x, *dk_h_y, *dk_h_z;
  fftw_plan *pb_exy, *pf_exy, *pb_exz, *pf_exz, *pb_eyx, *pf_eyx, *pb_eyz, *pf_eyz, *pb_ezx, *pf_ezx,*pb_ezy, *pf_ezy, *pb_hxy, *pf_hxy, *pb_hxz, *pf_hxz, *pb_hyx, *pf_hyx, *pb_hyz, *pf_hyz, *pb_hzx, *pf_hzx,*pb_hzy, *pf_hzy;
  fftw_plan pex_t, pey_t;
  int N_e_x, N_e_y, N_e_z, N_h_x, N_h_y, N_h_z;
  int exdetintegral;
  int Ndetmodes;

  complex<double> ***Dx_tilde, ***Dy_tilde;
  double phaseTermE;
  complex<double> cphaseTermE;
  //these are 2d matrices and must be in row-major order, which diffes from Matlab
  fftw_complex *Ex_t, *Ey_t;
  complex<double> **Ex_t_cm, **Ey_t_cm;
  double lambda_an_t;
  double ***D_temp_re, ***D_temp_im;
  double **Pupil;
  int k_det_obs_global;
  double *fx_vec, *fy_vec;
  int Nfx_vec, Nfy_vec;
  double z_obs = 0.;

  //end PSTD storage
 
  unsigned char ***materials;
  unsigned char *array_ptr_uint8;

  //  int *lower_boundary_update;
  int *Dxl, *Dxu, *Dyl, *Dyu, *Dzl, *Dzu, *Nt;
  int i,j,k, material_nlayers, is_disp, is_cond, is_disp_ml = 0;
  int k_loc;
  int tind;
  int input_counter = 0;
  int is_multilayer = 0;
  int start_tind;
  int pind_il, pind_iu, pind_jl, pind_ju, pind_kl, pind_ku;
  int cuboid[6];
  int **vertices;
  int nvertices;
  int *components;
  int ncomponents;
  double ***camplitudesR, ***camplitudesI;
  mxArray *mx_camplitudes;
  
  int sourcemode = sm_steadystate;//0 - steadystate, 1 - pulsed
  int runmode = rm_complete;//0 - complete, 1 - analyse
  int exphasorsvolume, exphasorssurface, intphasorssurface;
  int phasorinc[3];
  int  dimension = 0;
  int num_fields = 0;
  int ndims;
  int **structure, is_structure = 0;
  int I_tot, J_tot, K_tot, K, max_IJK;
  int Nsteps = 0, dft_counter = 0;
  int **surface_vertices, n_surface_vertices;
  int poutfile = 0;
  int Np=0; //The phasor extraction algorithm will be executed every Np iterations.
  int Npe=0; //The number of terms in the algorithm to extract the phasors
  double dtp=0.; //The phasor extraction time step
  int Ni_tdf=0, Nk_tdf=0;
  int skip_tdf = 6;
  const mwSize *dimptr_out;

  mwSize *dims;dims = (mwSize *)malloc(3*sizeof(mwSize));
  mwSize *label_dims;label_dims = (mwSize *)malloc(2*sizeof(mwSize));
  
  mxArray *dummy_array[3];
  mxArray *element;
  mxArray *mx_surface_vertices, *mx_surface_facets, *mx_surface_amplitudes;
  mxArray *mx_fieldsample, *mx_fieldsample_x, *mx_fieldsample_y, *mx_fieldsample_z;
  double ****fieldsample,****fieldsample_x,****fieldsample_y,****fieldsample_z;
  
  
  mxArray *mx_Idx, *mx_Idy;
  double **Idx_re, **Idx_im, **Idy_re, **Idy_im;
  complex<double> **Idx, **Idy;
  complex<double> Idxt,Idyt,kprop;
  
  char message_buffer[50];
 
  char dimension_str[3];
  const char fdtdgrid_elements[][15] = {"Exy","Exz","Eyx","Eyz","Ezx","Ezy","Hxy","Hxz","Hyx","Hyz","Hzx","Hzy","materials"};
  const char Cmaterial_elements[][10] = {"Cax","Cay","Caz","Cbx","Cby","Cbz","Ccx","Ccy","Ccz"};
  const char Dmaterial_elements[][10] = {"Dax","Day","Daz","Dbx","Dby","Dbz"};
  const char C_elements[][10] = {"Cax","Cay","Caz","Cbx","Cby","Cbz"};
  const char C_elements_disp_ml[][10] = {"Cax","Cay","Caz","Cbx","Cby","Cbz","Ccx","Ccy","Ccz"};
  const char D_elements[][10] = {"Dax","Day","Daz","Dbx","Dby","Dbz"};
  const char freespace_elements[][10] = {"Cbx","Cby","Cbz","Dbx","Dby","Dbz"};
  const char disp_params_elements[][10] = {"alpha","beta","gamma"};
  const char conductive_aux_elements[][10] = {"rho_x","rho_y","rho_z"};
  const char dispersive_aux_elements[][10] = {"alpha","beta","gamma","kappa_x","kappa_y","kappa_z","sigma_x","sigma_y","sigma_z"};
  const char delta_elements[][10] = {"x","y","z"};
  const char interface_fields[][5] = {"I0","I1","J0","J1","K0","K1"};
  const char grid_labels_fields[][15] = {"x_grid_labels","y_grid_labels","z_grid_labels"};

  const char fieldsample_elements[][2] = {"i","j","k","n"};
  const char campssample_elements[][15] = {"vertices","components"};
  
  char *sourcemodestr;
  char *tdfdirstr;

  FILE *outfile;
  if(poutfile){
    outfile = fopen("out.1.2.txt","w");
  }
  
  //  FILE *eyfile;
  //  FILE *jyfile;

  //  eyfile = fopen("Eyz.txt","w");
  //  jyfile = fopen("Jyz.txt","w");
  if( nrhs != 49 ){
    fprintf(stderr,"%d\n",nrhs);
    mexfprintf("Expected 49 inputs.");
  }

  if (nlhs != 27) 
    mexfprintf("27 outputs required.");

  /*Get fdtdgrid*/
  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
   
    //check that all fields are present
    if(num_fields != 13){
      sprintf(message_buffer, "fdtdgrid should have 13 members, it only has %d",num_fields);
      mexfprintf(message_buffer);
    } 
    //now loop over the fields 
    
    for(int i=0;i<num_fields;i++){
      //element = mxGetField(prhs[input_counter], 0, fdtdgrid_elements[i]);
      element = mxGetField( (mxArray *)prhs[input_counter], 0, fdtdgrid_elements[i]);
      if(mxIsDouble(element))
	array_ptr_dbl = mxGetPr(element);
      else if(mxIsUint8(element)){
	array_ptr_uint8 = (unsigned char *)mxGetPr(element);
      }
      else{
	sprintf(message_buffer, "Incorrect data type in fdtdgrid.%s",fdtdgrid_elements[i]);
	mexfprintf(message_buffer);
      }

      ndims = mxGetNumberOfDimensions(element);
      dimptr_out = mxGetDimensions(element);

      if( (ndims != 2) && (ndims != 3) ){
    	sprintf(message_buffer, "field matrix %s should be 2- or 3-dimensional",fdtdgrid_elements[i]);
	mexfprintf(message_buffer);
      }
      //start
      if(!strcmp(fdtdgrid_elements[i],"Exy")){
	if(ndims==2)
	  Exy = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else{
	  //fprintf(stderr,"Dims Exy=%d %d %d\n",dimptr_out[0], dimptr_out[1], dimptr_out[2]);
	  Exy = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
	}
      }
      else if(!strcmp(fdtdgrid_elements[i],"Exz")){
	if(ndims==2)
	  Exz = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Exz = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Eyx")){
	if(ndims==2)
	  Eyx = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Eyx = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Eyz")){
	if(ndims==2)
	  Eyz = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Eyz = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Ezx")){
	if(ndims==2)
	  Ezx = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Ezx = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Ezy")){
	if(ndims==2)
	  Ezy = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Ezy = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Hxy")){
	if(ndims==2)
	  Hxy = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Hxy = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Hxz")){
	if(ndims==2)
	  Hxz = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Hxz = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Hyx")){
	if(ndims==2)
	  Hyx = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Hyx = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Hyz")){
	if(ndims==2)
	  Hyz = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Hyz = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Hzx")){
	if(ndims==2)
	  Hzx = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Hzx = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"Hzy")){
	if(ndims==2)
	  Hzy = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], 0);
	else
	  Hzy = castMatlab3DArray(array_ptr_dbl, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      }
      else if(!strcmp(fdtdgrid_elements[i],"materials")){
	
	if(ndims==2)
	  materials = castMatlab3DArrayUint8(array_ptr_uint8, dimptr_out[0], dimptr_out[1], 0);
	else
	  materials = castMatlab3DArrayUint8(array_ptr_uint8, dimptr_out[0], dimptr_out[1], dimptr_out[2]);
	//save this for later when freeing memory
	material_nlayers = dimptr_out[2];
	I_tot = dimptr_out[0]-1;//The _tot variables do NOT include the additional cell at the 
	J_tot = dimptr_out[1]-1;//edge of the grid which is only partially used
	if(ndims==2)
	  K_tot = 0;
	else
	  K_tot = dimptr_out[2]-1;
      }
      else{
	sprintf(message_buffer, "element fdtdgrid.%s not handled",fdtdgrid_elements[i]);
	mexfprintf(message_buffer);
      }
    
    }//end
    input_counter++;
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure",input_counter);
    mexfprintf(message_buffer);
  }
  /*Got fdtdgrid*/
  //fprintf(stderr,"Got fdtdgrid\n");
  /*Get Cmaterials */
  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    //check that all fields are present
    if(num_fields != 9){
      sprintf(message_buffer, "Cmaterials should have 9 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
    
    for(int i=0;i<9;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, Cmaterial_elements[i]);
      ndims = mxGetNumberOfDimensions(element);
      if( ndims == 2 ){
	dimptr_out = mxGetDimensions(element);
	if(dimptr_out[0] != 1){ 
	  sprintf(message_buffer, "Incorrect dimension on Cmaterial.%s",Cmaterial_elements[i]);
	  mexfprintf(message_buffer);
	}
	if(!strcmp(Cmaterial_elements[i],"Cax")){
	  Cmaterial_Cax = mxGetPr(element);
	}
	else if(!strcmp(Cmaterial_elements[i],"Cay")){
	  Cmaterial_Cay = mxGetPr(element);
	}
	else if(!strcmp(Cmaterial_elements[i],"Caz")){
	  Cmaterial_Caz = mxGetPr(element);
	}
	else if(!strcmp(Cmaterial_elements[i],"Cbx")){
	  Cmaterial_Cbx = mxGetPr(element);
	}
	else if(!strcmp(Cmaterial_elements[i],"Cby")){
	  Cmaterial_Cby = mxGetPr(element);
	}
	else if(!strcmp(Cmaterial_elements[i],"Cbz")){
	  Cmaterial_Cbz = mxGetPr(element);
	}
	else if(!strcmp(Cmaterial_elements[i],"Ccx")){
	  Cmaterial_Ccx = mxGetPr(element);
	}
	else if(!strcmp(Cmaterial_elements[i],"Ccy")){
	  Cmaterial_Ccy = mxGetPr(element);
	}
	else if(!strcmp(Cmaterial_elements[i],"Ccz")){
	  Cmaterial_Ccz = mxGetPr(element);
	}
	else{
	  sprintf(message_buffer, "element Cmaterial.%s not handled",Cmaterial_elements[i]);
	  mexfprintf(message_buffer);
	}
      }
      else
	mexfprintf("Incorrect dimension on Cmaterial.Ca");
    }
    input_counter++;
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure",input_counter);
    mexfprintf(message_buffer);
  } 
  /*Got Cmaterials */

  //fprintf(stderr,"Got Cmaterials\n");
  /*Get Dmaterials */
  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    //check that all fields are present
    if(num_fields != 6){
      sprintf(message_buffer, "Dmaterials should have 6 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
    
    
    for(int i=0;i<6;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, Dmaterial_elements[i]);
      
      ndims = mxGetNumberOfDimensions(element);
      if( ndims == 2 ){
	dimptr_out = mxGetDimensions(element);
	if(dimptr_out[0] != 1){ 
	  sprintf(message_buffer, "Incorrect dimension on Dmaterial.%s",Dmaterial_elements[i]);
	  mexfprintf(message_buffer);
	}
	if(!strcmp(Dmaterial_elements[i],"Dax")){
	  Dmaterial_Dax = mxGetPr(element);
	}
	else if(!strcmp(Dmaterial_elements[i],"Day")){
	  Dmaterial_Day = mxGetPr(element);
	}
	else if(!strcmp(Dmaterial_elements[i],"Daz")){
	  Dmaterial_Daz = mxGetPr(element);
	}
	else if(!strcmp(Dmaterial_elements[i],"Dbx")){
	  Dmaterial_Dbx = mxGetPr(element);
	}
	else if(!strcmp(Dmaterial_elements[i],"Dby")){
	  Dmaterial_Dby = mxGetPr(element);
	}
	else if(!strcmp(Dmaterial_elements[i],"Dbz")){
	  Dmaterial_Dbz = mxGetPr(element);
	}
	else{
	  sprintf(message_buffer, "element Dmaterial.%s not handled",Dmaterial_elements[i]);
	  mexfprintf(message_buffer);
	}
      }
      else
	mexfprintf("Incorrect dimension on Dmaterial.Da");
    }
    input_counter++;
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure",input_counter);
    mexfprintf(message_buffer);
  } 
  /*Got Dmaterials */
  //fprintf(stderr,"Got Dmaterials\n");
  /*Get C */
    
  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    //check that all fields are present
    if( (num_fields != 6) && (num_fields != 9) ){
      sprintf(message_buffer, "C should have 6 or 9 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }

    if( num_fields==9 ){
      is_disp_ml = 1;
    }
    if( num_fields==6 ){
      for(int i=0;i<num_fields;i++){
	element = mxGetField( (mxArray *)prhs[input_counter], 0, C_elements[i]);
      
	ndims = mxGetNumberOfDimensions(element);
	if( ndims == 2 ){
	  dimptr_out = mxGetDimensions(element);
	  if(dimptr_out[0] != 1){ 
	    is_multilayer = 1;
	  }
	  if(!strcmp(C_elements[i],"Cax")){
	    Cax = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements[i],"Cay")){
	    Cay = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements[i],"Caz")){
	    Caz = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements[i],"Cbx")){
	    Cbx = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements[i],"Cby")){
	    Cby = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements[i],"Cbz")){
	    Cbz = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements[i],"Ccx")){
	    Ccx = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements[i],"Ccy")){
	    Ccy = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements[i],"Ccz")){
	    Ccz = mxGetPr(element);
	  }
	  else{
	    sprintf(message_buffer, "element C.%s not handled",C_elements[i]);
	    mexfprintf(message_buffer);
	  }
	}
	else
	  mexfprintf("Incorrect dimension on C");
      }
    }
    else{
      for(int i=0;i<num_fields;i++){
	element = mxGetField( (mxArray *)prhs[input_counter], 0, C_elements_disp_ml[i]);
      
	ndims = mxGetNumberOfDimensions(element);
	if( ndims == 2 ){
	  dimptr_out = mxGetDimensions(element);
	  if(dimptr_out[0] != 1){ 
	    //sprintf(message_buffer, "Incorrect dimension on C.%s",C_elements[i]);
	    //mexfprintf(message_buffer);
	    is_multilayer = 1;
	  }
	  if(!strcmp(C_elements_disp_ml[i],"Cax")){
	    Cax = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements_disp_ml[i],"Cay")){
	    Cay = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements_disp_ml[i],"Caz")){
	    Caz = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements_disp_ml[i],"Cbx")){
	    Cbx = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements_disp_ml[i],"Cby")){
	    Cby = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements_disp_ml[i],"Cbz")){
	    Cbz = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements_disp_ml[i],"Ccx")){
	    Ccx = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements_disp_ml[i],"Ccy")){
	    Ccy = mxGetPr(element);
	  }
	  else if(!strcmp(C_elements_disp_ml[i],"Ccz")){
	    Ccz = mxGetPr(element);
	  }
	  else{
	    sprintf(message_buffer, "element C.%s not handled",C_elements_disp_ml[i]);
	    mexfprintf(message_buffer);
	  }
	}
	else
	  mexfprintf("Incorrect dimension on C");
      }
    }
    input_counter++;
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure",input_counter);
    mexfprintf(message_buffer);
  } 
  /*Got C */
  //fprintf(stderr,"Got C\n");
  /*Get D */
  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    //check that all fields are present
    if(num_fields != 6){
      sprintf(message_buffer, "D should have 6 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
    
    
    for(int i=0;i<6;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, D_elements[i]);
      
      ndims = mxGetNumberOfDimensions(element);
      if( ndims == 2 ){
	dimptr_out = mxGetDimensions(element);
	if(dimptr_out[0] != 1){ 
	  //sprintf(message_buffer, "Incorrect dimension on D.%s",D_elements[i]);
	  //mexfprintf(message_buffer);
	}
	if(!strcmp(D_elements[i],"Dax")){
	  Dax = mxGetPr(element);
	}
	else if(!strcmp(D_elements[i],"Day")){
	  Day = mxGetPr(element);
	}
	else if(!strcmp(D_elements[i],"Daz")){
	  Daz = mxGetPr(element);
	}
	else if(!strcmp(D_elements[i],"Dbx")){
	  Dbx = mxGetPr(element);
	}
	else if(!strcmp(D_elements[i],"Dby")){
	  Dby = mxGetPr(element);
	}
	else if(!strcmp(D_elements[i],"Dbz")){
	  Dbz = mxGetPr(element);
	}
	else{
	  sprintf(message_buffer, "element D.%s not handled",D_elements[i]);
	  mexfprintf(message_buffer);
	}
      }
      else
	mexfprintf("Incorrect dimension on D");
    }
    input_counter++;
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure",input_counter);
    mexfprintf(message_buffer);
  } 
  /*Got D */

  //fprintf(stderr,"Got D\n");
  /*Get freespace*/

  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    //check that all fields are present
    if(num_fields != 6){
      sprintf(message_buffer, "freespace should have 6 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
        
    for(int i=0;i<6;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, freespace_elements[i]);
      
      ndims = mxGetNumberOfDimensions(element);
      if( ndims == 2 ){
	dimptr_out = mxGetDimensions(element);
	if(dimptr_out[0] != 1){ 
	  sprintf(message_buffer, "Incorrect dimension on freespace.%s",freespace_elements[i]);
	  mexfprintf(message_buffer);
	}
	if(!strcmp(freespace_elements[i],"Cbx")){
	  freespace_Cbx = mxGetPr(element);
	}
	else if(!strcmp(freespace_elements[i],"Cby")){
	  freespace_Cby = mxGetPr(element);
	}
	else if(!strcmp(freespace_elements[i],"Cbz")){
	  freespace_Cbz = mxGetPr(element);
	}
	else if(!strcmp(freespace_elements[i],"Dbx")){
	  freespace_Dbx = mxGetPr(element);
	}
	else if(!strcmp(freespace_elements[i],"Dby")){
	  freespace_Dby = mxGetPr(element);
	}
	else if(!strcmp(freespace_elements[i],"Dbz")){
	  freespace_Dbz = mxGetPr(element);
	}
	else{
	  sprintf(message_buffer, "element freespace.%s not handled",freespace_elements[i]);
	  mexfprintf(message_buffer);
	}
      }
      else
	mexfprintf("Incorrect dimension on freespace");
    }
    input_counter++;
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure",input_counter);
    mexfprintf(message_buffer);
  }

  /*Got freespace*/

  //fprintf(stderr,"Got freespace\n");
  /*Get disp_params */

  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    //check that all fields are present
    if(num_fields != 3){
      sprintf(message_buffer, "disp_params should have 3 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
        
    for(int i=0;i<3;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, disp_params_elements[i]);
      
      ndims = mxGetNumberOfDimensions(element);
      if( ndims == 2 ){
	dimptr_out = mxGetDimensions(element);
	if( !(dimptr_out[0] == 1 ||dimptr_out[0] == 0) ){ 
	  sprintf(message_buffer, "Incorrect dimension on disp_params.%s",disp_params_elements[i]);
	  mexfprintf(message_buffer);
	}
	if(!strcmp(disp_params_elements[i],"alpha")){
	  alpha = mxGetPr(element);
	}
	else if(!strcmp(disp_params_elements[i],"beta")){
	  beta = mxGetPr(element);
	}
	else if(!strcmp(disp_params_elements[i],"gamma")){
	  gamma = mxGetPr(element);
	}
	else{
	  sprintf(message_buffer, "element disp_params.%s not handled",disp_params_elements[i]);
	  mexfprintf(message_buffer);
	}
      }
      else
	mexfprintf("Incorrect dimension on disp_params");
    }
    input_counter++;
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure",input_counter);
    mexfprintf(message_buffer);
  }

  
  /*Got disp_params */

  //fprintf(stderr,"Got disp_params\n");
  /*Get delta params*/
  
  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    //check that all fields are present
    if(num_fields != 3){
      sprintf(message_buffer, "delta should have 3 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
    
    for(int i=0;i<3;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, delta_elements[i]);
      
      ndims = mxGetNumberOfDimensions(element);
      if( ndims == 2 ){
	dimptr_out = mxGetDimensions(element);
	if(dimptr_out[0] != 1){ 
	  sprintf(message_buffer, "Incorrect dimension on delta.%s",freespace_elements[i]);
	  mexfprintf(message_buffer);
	}
	if(!strcmp(delta_elements[i],"x")){
	  dx = *mxGetPr( (mxArray *)element);
	}
	else if(!strcmp(delta_elements[i],"y")){
	  dy = *mxGetPr( (mxArray *)element);
	}
	else if(!strcmp(delta_elements[i],"z")){
	  dz = *mxGetPr( (mxArray *)element);
	}
	else{
	  sprintf(message_buffer, "element delta.%s not handled",delta_elements[i]);
	  mexfprintf(message_buffer);
	}
      }
      else
	mexfprintf("Incorrect dimension on delta");
    }
    input_counter++;
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure",input_counter);
    mexfprintf(message_buffer);
  }
  
  /*Got delta params*/

  //fprintf(stderr,"Got delta params\n");
  /*Get interface*/
  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    //check that all fields are present
    if(num_fields != 6){
      sprintf(message_buffer, "interface should have 6 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
    //need to allocate some space for I0
    I0 = (double *)malloc(2*sizeof(double));
    I1 = (double *)malloc(2*sizeof(double));
    J0 = (double *)malloc(2*sizeof(double));
    J1 = (double *)malloc(2*sizeof(double));
    K0 = (double *)malloc(2*sizeof(double));
    K1 = (double *)malloc(2*sizeof(double));
    for(int i=0;i<6;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, interface_fields[i]);
      ndims = mxGetNumberOfDimensions(element);
      if( ndims == 2 ){

	//dimptr = (int *)mxGetDimensions(element);
	dimptr_out = mxGetDimensions(element);
	if( !(dimptr_out[0] == 1 && dimptr_out[1] == 2) ){ 
	  sprintf(message_buffer, "Incorrect dimension on interface.%s (%d,%d,%d)\n",interface_fields[i],(int)dimptr_out[0],(int)dimptr_out[1],(int)mxGetNumberOfElements(element));
	  mexfprintf(message_buffer);
	}
	if(!strcmp(interface_fields[i],"I0")){
	  place_holder = mxGetPr( (mxArray *)element);
	  *I0 = *place_holder - 1.;
	  *(I0+1) = *(place_holder+1);
	}
	else if(!strcmp(interface_fields[i],"I1")){
	  place_holder = mxGetPr( (mxArray *)element);
	  *I1 = *place_holder - 1.;
	  *(I1+1) = *(place_holder+1);
	}
	else if(!strcmp(interface_fields[i],"J0")){
	  place_holder = mxGetPr( (mxArray *)element);
	  *J0 = *place_holder - 1.; 
	  *(J0+1) = *(place_holder+1);
	}
	else if(!strcmp(interface_fields[i],"J1")){
	  place_holder = mxGetPr( (mxArray *)element);
	  *J1 = *place_holder - 1.;
	  *(J1+1) = *(place_holder+1);
	}
	else if(!strcmp(interface_fields[i],"K0")){
	  place_holder = mxGetPr( (mxArray *)element);
	  *K0 = *place_holder - 1.;
	  if(*K0<0)
	    *K0 = 0.;
	  *(K0+1) = *(place_holder+1);
	}
	else if(!strcmp(interface_fields[i],"K1")){
	  place_holder = mxGetPr( (mxArray *)element);
	  *K1 = *place_holder - 1.;
	  if(*K1<0)
	    *K1 = 0.;
	  *(K1+1) = *(place_holder+1);
	}
	else{
	  sprintf(message_buffer, "element interface.%s not handled",interface_fields[i]);
	  mexfprintf(message_buffer);
	}
      }
      else
	mexfprintf("Incorrect dimension on interfaces");
    }
    //printf("%d %d %d %d %d %d\n",(int)*I0,(int)*I1,(int)*J0,(int)*J1,(int)*K0,(int)*K1);  
    input_counter++;
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure",input_counter);
    mexfprintf(message_buffer);
  }

  /*Got interface*/
  
  //fprintf(stderr,"Got interface\n");
  /*Get Isource*/
  //check the dimensions
  if( !mxIsEmpty(prhs[input_counter]) ){
    ndims = mxGetNumberOfDimensions(prhs[input_counter]);
    dimptr_out = mxGetDimensions( (mxArray *)prhs[input_counter]);
    if( (ndims !=3) && (ndims !=2) )
      mexfprintf("Isource should be 3- or 2- dimensional");
    if( ndims ==3 ){
      if( !( (dimptr_out[0]==8) && (dimptr_out[1]==((int)(J1[0]-J0[0]+1))) && (dimptr_out[2]==((int)(K1[0]-K0[0]+1))) ) )
	mexfprintf("Isource has incorresct size");
    }
    else{
      if( !( (dimptr_out[0]==8) && (dimptr_out[1]==((int)(J1[0]-J0[0]+1))) ))
	mexfprintf("Isource has incorrect size");
    }
    if(!mxIsComplex( (mxArray *)prhs[input_counter]))
      mexfprintf("Isource should be complex, use a call of complex(real(Isource),imag(Isource)) in matlab if necessary");
    if( ndims==2 ){
      IsourceR = castMatlab3DArray(mxGetPr( (mxArray *)prhs[input_counter]), dimptr_out[0], dimptr_out[1], 0);
      IsourceI = castMatlab3DArray(mxGetPi( (mxArray *)prhs[input_counter++]), dimptr_out[0], dimptr_out[1], 0);
    }
    else{
      IsourceR = castMatlab3DArray(mxGetPr( (mxArray *)prhs[input_counter]), dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      IsourceI = castMatlab3DArray(mxGetPi( (mxArray *)prhs[input_counter++]), dimptr_out[0], dimptr_out[1], dimptr_out[2]);
    }
  }
  else{
    mexfprintf("Isource is empty\n");
    input_counter++;
  }
  /*Got Isource*/
  //fprintf(stderr,"Got   Isource\n");
  /*Get Jsource*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    ndims = mxGetNumberOfDimensions(prhs[input_counter]);
    dimptr_out = mxGetDimensions( (mxArray *)prhs[input_counter]);
    if( (ndims !=3) && (ndims !=2) )
      mexfprintf("Jsource should be 3- or 2- dimensional");
    if(ndims==3){
      if( !( (dimptr_out[0]==8) && (dimptr_out[1]==((int)(I1[0]-I0[0]+1))) && (dimptr_out[2]==((int)(K1[0]-K0[0]+1))) ) )
	mexfprintf("Jsource has incorrect size");
    }
    else{
      if( !( (dimptr_out[0]==8) && (dimptr_out[1]==((int)(I1[0]-I0[0]+1)))))
	mexfprintf("Jsource has incorrect size");
    }
    if(!mxIsComplex( (mxArray *)prhs[input_counter]))
      mexfprintf("Jsource should be complex, use a call of complex(real(Jsource),imag(Jsource)) in matlab if necessary");
    if( ndims==2 ){
      JsourceR = castMatlab3DArray(mxGetPr( (mxArray *)prhs[input_counter]), dimptr_out[0], dimptr_out[1], 0);
      JsourceI = castMatlab3DArray(mxGetPi( (mxArray *)prhs[input_counter++]), dimptr_out[0], dimptr_out[1], 0);
    }
    else{
      JsourceR = castMatlab3DArray(mxGetPr( (mxArray *)prhs[input_counter]), dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      JsourceI = castMatlab3DArray(mxGetPi( (mxArray *)prhs[input_counter++]), dimptr_out[0], dimptr_out[1], dimptr_out[2]);
    } 
  }
  else{
    mexfprintf("Jsource is empty\n");
    input_counter++;
  }
  /*Got Jsource*/

  //fprintf(stderr,"Got   Jsource\n");
  /*Get Ksource*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    ndims = mxGetNumberOfDimensions(prhs[input_counter]);
    fprintf(stderr,"Ksource-1\n");
    dimptr_out = mxGetDimensions( (mxArray *)prhs[input_counter]);
    fprintf(stderr,"Ksource-2\n");
    if( ndims ==2 ){
      fprintf(stderr,"Ksource-3 (%d)\n",ndims);
      //mexfprintf("Ksource should be 3 dimensional\n");
    }
    if( ndims ==3 ){
      if( !( (dimptr_out[0]==8) && (dimptr_out[1]==((int)(I1[0]-I0[0]+1))) && (dimptr_out[2]==((int)(J1[0]-J0[0]+1))) ) )
	mexfprintf("Ksource has incorresct size\n");
    }
    else if(ndims==2){
	if( !( (dimptr_out[0]==8) && (dimptr_out[1]==((int)(I1[0]-I0[0]+1))) && (0==((int)(J1[0]-J0[0]+1))) ) )
	mexfprintf("Ksource has incorresct size\n");
    }
    fprintf(stderr,"Ksource-4\n");
    if(!mxIsComplex( (mxArray *)prhs[input_counter]))
      mexfprintf("Ksource should be complex, use a call of complex(real(Ksource),imag(Ksource)) in matlab if necessary");
    
    fprintf(stderr,"Ksource-5\n");
    fprintf(stderr,"KsourceR: %d,%d,%d\n",dimptr_out[0], dimptr_out[1], dimptr_out[2]);
    if(ndims==2){
      KsourceR = castMatlab3DArray(mxGetPr( (mxArray *)prhs[input_counter]), dimptr_out[0], dimptr_out[1], 1);
      fprintf(stderr,"Ksource-6a\n");
      KsourceI = castMatlab3DArray(mxGetPi( (mxArray *)prhs[input_counter++]), dimptr_out[0], dimptr_out[1], 1);
      fprintf(stderr,"KsourceR[0][0][0]: %e\n", KsourceR[0][0][0]);
    }
    else{
      KsourceR = castMatlab3DArray(mxGetPr( (mxArray *)prhs[input_counter]), dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      fprintf(stderr,"Ksource-6b\n");
      KsourceI = castMatlab3DArray(mxGetPi( (mxArray *)prhs[input_counter++]), dimptr_out[0], dimptr_out[1], dimptr_out[2]);
    }
  }
  else{
    mexfprintf("Ksource is empty\n");
    input_counter++;
  }
  /*Got Ksource*/
  //fprintf(stderr,"Got   Ksource\n");
  /*Get grid_labels*/
  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    //check that all fields are present
    if(num_fields != 3){
      sprintf(message_buffer, "grid_labels should have 3 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
    for(int i=0;i<3;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, grid_labels_fields[i]);
      ndims = mxGetNumberOfDimensions(element);
      if( ndims == 2 ){
	dimptr_out = mxGetDimensions(element);
	if(dimptr_out[0] != 1){ 
	  sprintf(message_buffer, "Incorrect dimension on Cmaterial.%s",Cmaterial_elements[i]);
	  mexfprintf(message_buffer);
	}
	if(!strcmp(grid_labels_fields[i],"x_grid_labels")){
	  x_grid_labels = mxGetPr( (mxArray *)element);
	}
	else if(!strcmp(grid_labels_fields[i],"y_grid_labels")){
	  y_grid_labels = mxGetPr( (mxArray *)element);
	}
	else if(!strcmp(grid_labels_fields[i],"z_grid_labels")){
	  z_grid_labels = mxGetPr( (mxArray *)element);
	}
	else{
	  sprintf(message_buffer, "element grid_labels.%s not handled",grid_labels_fields[i]);
	  mexfprintf(message_buffer);
	}
      }
      else{
	sprintf(message_buffer, "Incorrect dimension on grid_labels.%s",Cmaterial_elements[i]);
	mexfprintf(message_buffer);
      }
    }
    
    input_counter++;
  }
  /*Get grid_labels*/
  //fprintf(stderr,"Got   grid_labels\n");
  /*Get tvec_E - no longer used*/
  /*
    if(mxIsDouble(prhs[input_counter])){
    tvec_E = mxGetPr(prhs[input_counter]); 
    input_counter++;
    }
    else{
    sprintf(message_buffer, "Expected tvec_E to be a double vector");
    mexfprintf(message_buffer);
    } 
  */
  /*Got tvec_E*/

  /*Get tvec_H - no longer used*/
  /*
    if(mxIsDouble(prhs[input_counter])){
    tvec_H = mxGetPr(prhs[input_counter]); 
    input_counter++;
    }
    else{
    sprintf(message_buffer, "Expected tvec_H to be a double vector");
    mexfprintf(message_buffer);
    } 
  */
  /*Got tvec_H*/

  /*Get omega_an*/

  if(mxIsDouble(prhs[input_counter])){
    omega_an = mxGetPr( (mxArray *)prhs[input_counter]);  
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected omega_an to be a double");
    mexfprintf(message_buffer);
  } 

  /*Got omega_an*/

  /*Get to_l*/

  if(mxIsDouble(prhs[input_counter])){
    to_l = mxGetPr( (mxArray *)prhs[input_counter]);  
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected to_l to be a double");
    mexfprintf(message_buffer);
  } 

  /*Got to_l*/

  /*Get hwhm*/
  
  if(mxIsDouble(prhs[input_counter])){
    hwhm = mxGetPr( (mxArray *)prhs[input_counter]);  
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected hwhm to be a double");
    mexfprintf(message_buffer);
  } 

  /*Got hwhm*/

  /*Get Dxl*/
  
  if(mxIsDouble(prhs[input_counter])){
    Dxl = (int *)malloc(sizeof(int));
    place_holder = mxGetPr( (mxArray *)prhs[input_counter]);
    *Dxl = (int)*place_holder;
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected Dxl to be a double, although i will cast it as an int!");
    mexfprintf(message_buffer);
  } 

  /*Got Dxl*/

  /*Get Dxu*/
  
  if(mxIsDouble(prhs[input_counter])){
    Dxu = (int *)malloc(sizeof(int));
    place_holder = mxGetPr( (mxArray *)prhs[input_counter]);
    *Dxu = (int)*place_holder;
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected Dxu to be a double, although i will cast it as an int!");
    mexfprintf(message_buffer);
  } 

  /*Got Dxu*/

  /*Get Dyl*/
  
  if(mxIsDouble(prhs[input_counter])){
    Dyl = (int *)malloc(sizeof(int));
    place_holder = mxGetPr( (mxArray *)prhs[input_counter]);
    *Dyl = (int)*place_holder;
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected Dyl to be a double, although i will cast it as an int!");
    mexfprintf(message_buffer);
  } 

  /*Got Dyl*/

  /*Get Dyu*/
  
  if(mxIsDouble(prhs[input_counter])){
    Dyu = (int *)malloc(sizeof(int));
    place_holder = mxGetPr( (mxArray *)prhs[input_counter]);
    *Dyu = (int)*place_holder;
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected Dyu to be a double, although i will cast it as an int!");
    mexfprintf(message_buffer);
  } 

  /*Got Dyu*/

  /*Get Dzl*/
  
  if(mxIsDouble(prhs[input_counter])){
    Dzl = (int *)malloc(sizeof(int));
    place_holder = mxGetPr( (mxArray *)prhs[input_counter]);
    *Dzl = (int)*place_holder;
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected Dzl to be a double, although i will cast it as an int!");
    mexfprintf(message_buffer);
  } 

  /*Got Dzl*/

  /*Get Dzu*/
  
  if(mxIsDouble(prhs[input_counter])){
    Dzu = (int *)malloc(sizeof(int));
    place_holder = mxGetPr( (mxArray *)prhs[input_counter]);
    *Dzu = (int)*place_holder;
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected Dzu to be a double, although i will cast it as an int!");
    mexfprintf(message_buffer);
  } 

  /*Got Dzu*/

  /*Get lower_boundary_update
  
    if(mxIsDouble(prhs[input_counter])){
    lower_boundary_update = (int *)malloc(sizeof(int));
    place_holder = mxGetPr( (mxArray *)prhs[input_counter]);
    *lower_boundary_update = (int)*place_holder;
    input_counter++;
    }
    else{
    sprintf(message_buffer, "expected lower_boundary_update to be a double");
    mexfprintf(message_buffer);
    } 
    Got lower_boundary_update*/

  /*Get Nt*/
  if(mxIsDouble(prhs[input_counter])){
    Nt = (int *)malloc(sizeof(int));
    place_holder = mxGetPr( (mxArray *)prhs[input_counter]);  
    *Nt = (int)*place_holder;  
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected Nt to be a double");
    mexfprintf(message_buffer);
  } 
  /*Got Nt*/

  /*Get dt*/
  
  if(mxIsDouble(prhs[input_counter])){
    dt = mxGetPr( (mxArray *)prhs[input_counter]);  
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected dt to be a double");
    mexfprintf(message_buffer);
  } 

  /*Got dt*/

  /*Get tind*/
  if(mxIsDouble(prhs[input_counter])){
    start_tind = (int)(*mxGetPr( (mxArray *)prhs[input_counter]));
    input_counter++;
  }
  else{
    sprintf(message_buffer, "expected start_tind to be a double");
    mexfprintf(message_buffer);
  } 
  /*Got tind*/

  /*Get sourcemode*/
  if(mxIsChar(prhs[input_counter])){
    sourcemodestr = (char *)malloc((1+(int)mxGetNumberOfElements( (mxArray *)prhs[input_counter]))*sizeof(char));
    mxGetString((mxArray *)prhs[input_counter], sourcemodestr, (1+(int)mxGetNumberOfElements( (mxArray *)prhs[input_counter])));
    if( !strncmp(sourcemodestr,"steadystate",11) )
      sourcemode = sm_steadystate;
    else if( !strncmp(sourcemodestr,"pulsed",6) )
      sourcemode = sm_pulsed;
    else{
      sprintf(message_buffer,"value of sourcemode (%s) is invalid\n",sourcemodestr);
      mexfprintf(message_buffer);
    }
    free(sourcemodestr);
    input_counter++;
  }
  else{
    mexfprintf("Expected sourcemode to be a string");
  }
  /*Got sourcemode*/

  /*Get runmode*/
  if(mxIsChar(prhs[input_counter])){
    sourcemodestr = (char *)malloc((1+(int)mxGetNumberOfElements( (mxArray *)prhs[input_counter]))*sizeof(char));
    mxGetString( (mxArray *)prhs[input_counter], sourcemodestr, (1+(int)mxGetNumberOfElements( (mxArray *)prhs[input_counter])));
    if( !strncmp(sourcemodestr,"complete",8) )
      runmode = rm_complete;
    else if( !strncmp(sourcemodestr,"analyse",7) )
      runmode = rm_analyse;
    else{
      sprintf(message_buffer,"value of runmode (%s) is invalid\n",sourcemodestr);
      mexfprintf(message_buffer);
    }
    free(sourcemodestr);
    input_counter++;
  }
  else{
    mexfprintf("Expected sourcemode to be a string");
  }
  /*Got runmode*/

  /*Get exphasorsvolume*/
  if(mxIsDouble(prhs[input_counter])){
    exphasorsvolume = (int)*mxGetPr( (mxArray *)prhs[input_counter++]);

  }
  else{
    sprintf(message_buffer, "expected exphasorsvolume to be a double");
    mexfprintf(message_buffer);
  }
  /*Got exphasorsvolume*/

  /*Get exphasorssurface*/
  if(mxIsDouble(prhs[input_counter])){
    exphasorssurface = (int)*mxGetPr( (mxArray *)prhs[input_counter++]);
  }
  else{
    sprintf(message_buffer, "expected exphasorssurface to be a double");
    mexfprintf(message_buffer);
  }
  /*Got exphasorssurface*/

  /*Get intphasorssurface*/
  if(mxIsDouble(prhs[input_counter])){
    intphasorssurface = (int)*mxGetPr( (mxArray *)prhs[input_counter++]);
  }
  else{
    sprintf(message_buffer, "expected intphasorssurface to be a double");
    mexfprintf(message_buffer);
  }
  /*Got intphasorssurface*/

  /*Get phasorsurface*/
  /*Only do if exphasorssurface is true*/
  if( exphasorssurface && runmode==rm_complete){
    ndims = mxGetNumberOfDimensions(prhs[input_counter]);
    dimptr_out = mxGetDimensions( (mxArray *)prhs[input_counter]);
    if(ndims !=2){
      sprintf(message_buffer, "expected phasorsurface to be a vector of length 6");
      mexfprintf(message_buffer);
    }
    if(dimptr_out[0]*dimptr_out[1] !=6){
      sprintf(message_buffer, "expected phasorsurface to be a vector of length 6");
      mexfprintf(message_buffer);
    }
    //now safe to extract the indices
    for(i=0;i<6;i++){
      cuboid[i] = (int)*(mxGetPr( (mxArray *)prhs[input_counter])+i) - 1;//must go from matlab coords to C
      if(cuboid[i]<0){
	cuboid[i] = 0;
	
      }   
    }
    if(J_tot==0)
      if( cuboid[2] != cuboid[3] ){
	sprintf(message_buffer, "When doind a 2D simulation, J0 should equal J1 in phasorsurface.");
	mexfprintf(message_buffer);
      }

  }
  input_counter++;
  /*Got phasorsurface*/
  //fprintf(stderr,"Got   phasorsurface\n");
  /*Get phasorinc*/
  if(mxIsDouble(prhs[input_counter])){
    double *tmpptr = mxGetPr( (mxArray *)prhs[input_counter++]);
    for(i=0;i<3;i++)
      phasorinc[i] = (int)tmpptr[i];
  }
  else{
    sprintf(message_buffer, "expected phasorinc to be a double");
    mexfprintf(message_buffer);
  }
  /*Got phasorinc*/

  /*Get dimension*/
  mxGetString( (mxArray *)prhs[input_counter],dimension_str,3);
  //now set the dimension integer
  if( !strcmp(dimension_str,"3") )
    dimension = THREE;
  else if( !strcmp(dimension_str,"TE") )
    dimension = TE;
  else
    dimension = TM;
  
  input_counter++;
  /*Got dimension*/

  /*Get conductive_aux */

  if(mxIsStruct(prhs[input_counter])){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    if(num_fields != 3){
      sprintf(message_buffer, "conductive_aux should have 3 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
    for(int i=0;i<3;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, conductive_aux_elements[i]);
      ndims = mxGetNumberOfDimensions(element);
      if( ndims == 2 ){
	dimptr_out = mxGetDimensions(element);
	if( !(dimptr_out[0] == 1 ||dimptr_out[0] == 0) ){ 
	  //sprintf(message_buffer, "Incorrect dimension on conductive_aux.%s",conductive_aux_elements[i]);
	  //mexfprintf(message_buffer);
	}
      }
      if(!strcmp(conductive_aux_elements[i],"rho_x")){
	rho_x = mxGetPr(element);
      }
      else if(!strcmp(conductive_aux_elements[i],"rho_y")){
	rho_y = mxGetPr(element);
      }
      else if(!strcmp(conductive_aux_elements[i],"rho_z")){
	rho_z = mxGetPr(element);
      }
      else{
	sprintf(message_buffer, "element conductive_aux.%s not handled",conductive_aux_elements[i]);
	mexfprintf(message_buffer);
      }
    }
  }
  else{
    sprintf(message_buffer, "Argument %d was expected to be a structure (conductive_aux)",input_counter);
    mexfprintf(message_buffer);
  }
  input_counter++;
  /*Get conductive_aux */

  /*Get dispersive_aux*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    if(mxIsStruct(prhs[input_counter])){
      num_fields = mxGetNumberOfFields(prhs[input_counter]);
      if( num_fields != 9 ){
	sprintf(message_buffer, "dispersive_aux should have 9 elements, it has %d",num_fields);
	mexfprintf(message_buffer);
      }
      for(int i=0;i<9;i++){
	element = mxGetField( (mxArray *)prhs[input_counter], 0, dispersive_aux_elements[i]);
	if(!strcmp(dispersive_aux_elements[i],"alpha")){
	  ml_alpha = mxGetPr(element);
	}
	else if(!strcmp(dispersive_aux_elements[i],"beta")){
	  ml_beta = mxGetPr(element);
	}
	else if(!strcmp(dispersive_aux_elements[i],"gamma")){
	  ml_gamma = mxGetPr(element);
	}
	else if(!strcmp(dispersive_aux_elements[i],"kappa_x")){
	  ml_kappa_x = mxGetPr(element);
	}
	else if(!strcmp(dispersive_aux_elements[i],"kappa_y")){
	  ml_kappa_y = mxGetPr(element);
	}
	else if(!strcmp(dispersive_aux_elements[i],"kappa_z")){
	  ml_kappa_z = mxGetPr(element);
	}
	else if(!strcmp(dispersive_aux_elements[i],"sigma_x")){
	  ml_sigma_x = mxGetPr(element);
	}
	else if(!strcmp(dispersive_aux_elements[i],"sigma_y")){
	  ml_sigma_y = mxGetPr(element);
	}
	else if(!strcmp(dispersive_aux_elements[i],"sigma_z")){
	  ml_sigma_z = mxGetPr(element);
	}
	else{
	  sprintf(message_buffer, "element dispersive_aux.%s not handled",dispersive_aux_elements[i]);
	  mexfprintf(message_buffer);
	}
      }
    }
    else{
      sprintf(message_buffer, "Argument %d was expected to be a structure (dispersive_aux)",input_counter);
      mexfprintf(message_buffer);
    }
  }

  input_counter++;
  /*Got dispersive_aux*/

  /*Get structure*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    ndims = mxGetNumberOfDimensions(prhs[input_counter]);
    dimptr_out = mxGetDimensions((mxArray *)prhs[input_counter]);
    if(ndims!=2){
      //fprintf(stderr,"ndims: %d\n",ndims);
      mexfprintf("structure should be a 2D matrix");
    }
    if( dimptr_out[0] != 2 || dimptr_out[1] != (I_tot+1))
      mexfprintf("structure should have dimension 2 x (I_tot+1) ");
    //castMatlab2DArrayInt(int *array, int nrows, int ncols)
    structure = castMatlab2DArrayInt((int *)mxGetPr((mxArray *)prhs[input_counter]),2,I_tot+1);
    //    fprintf(stderr,"%2d %2d %2d\n%2d %2d %2d\n",structure[0][0],structure[1][0],structure[2][0],structure[0][1],structure[1][1],structure[1][1]);
    
    is_structure = 1;
  }
  else
    is_structure = 0;
  input_counter++;
  
  /*Got structure*/

  /*Get f_ex_vec*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    ndims = mxGetNumberOfDimensions(prhs[input_counter]);
    dimptr_out = mxGetDimensions((mxArray *)prhs[input_counter]);
    fprintf(stderr,"f_ex_vec has ndims=%d, N=%d\n",ndims,dimptr_out[0]);

    if(ndims!=2){
      mexfprintf("f_ex_vec should be an array with N>0 elements");
    }
    if( !( (dimptr_out[0]==1) || (dimptr_out[1]==1) ) )
      mexfprintf("f_ex_vec should be an array with N>0 elements");
    if(dimptr_out[0]>dimptr_out[1])
      N_f_ex_vec=dimptr_out[0];
    else
      N_f_ex_vec=dimptr_out[1];
    f_ex_vec=(double *)mxGetPr((mxArray *)prhs[input_counter]);
  }
  else{
    N_f_ex_vec=1;
    f_ex_vec=(double *)malloc(sizeof(double));
    f_ex_vec[0]=omega_an[0]/2./dcpi;
  }
  input_counter++;
  /*Got f_ex_vec*/

  /*Get exdetintegral*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    if( mxGetNumberOfElements(prhs[input_counter]) != 1)
      fprintf(stderr,"exdetintegral has %d elements, it should only have 1.\n",(int)mxGetNumberOfElements(prhs[input_counter]));
    exdetintegral = (int)*(mxGetPr((mxArray *)prhs[input_counter]));
  }
  else
    exdetintegral = 0;
  input_counter++;
  /*Got exdetintegral*/

  
  if(exdetintegral==1){
    /*Get f_vec*/
    if( !mxIsEmpty(prhs[input_counter]) ){
      if(mxIsStruct(prhs[input_counter])){
	num_fields = mxGetNumberOfFields(prhs[input_counter]);
	if(num_fields != 2){
	  sprintf(message_buffer, "f_vec should have 2 members, it has %d",num_fields);
	  mexfprintf(message_buffer);
	} 
	element = mxGetField( (mxArray *)prhs[input_counter], 0, "fx_vec");
	fx_vec = mxGetPr(element);
	Nfx_vec = mxGetNumberOfElements(element);

	element = mxGetField( (mxArray *)prhs[input_counter], 0, "fy_vec");
	fy_vec = mxGetPr(element);
	Nfy_vec = mxGetNumberOfElements(element);
  
      }
    }
    input_counter++;
    /*Got f_vec*/

    /*Get Pupil*/
    if( !mxIsEmpty(prhs[input_counter]) ){
      ndims = mxGetNumberOfDimensions(prhs[input_counter]);
      dimptr_out = mxGetDimensions(prhs[input_counter]);
      if(ndims!=2)
	fprintf(stderr,"Pupil should be two dimensional\n");
      if( dimptr_out[0]!=Nfx_vec || dimptr_out[1]!=Nfy_vec )
	fprintf(stderr,"Pupil has dimension %dx%d yet it should have dimension %dx%d\n",dimptr_out[0],dimptr_out[1],Nfx_vec,Nfy_vec);
      Pupil = castMatlab2DArray(mxGetPr(prhs[input_counter]),dimptr_out[0],dimptr_out[1]);
    }
    input_counter++;
    /*Got Pupil*/

    /*Get D_tilde*/
    if( !mxIsEmpty(prhs[input_counter]) ){
      if(mxIsStruct(prhs[input_counter])){
	num_fields = mxGetNumberOfFields(prhs[input_counter]);
	if(num_fields != 2){
	  sprintf(message_buffer, "D_tilde should have 2 members, it has %d",num_fields);
	  mexfprintf(message_buffer);
	} 
	element = mxGetField( (mxArray *)prhs[input_counter], 0, "Dx_tilde");
	ndims = mxGetNumberOfDimensions(element);
	dimptr_out = mxGetDimensions(element);
	if(ndims!=3)
	  fprintf(stderr,"Dx_tilde should be three dimensional\n");
	
	Ndetmodes = dimptr_out[0];
	
	if( dimptr_out[1]!=Nfx_vec || dimptr_out[2]!=Nfy_vec )
	  fprintf(stderr,"Dx_tilde has dimension %dx%dx%d yet it should have dimension %dx%dx%d\n",dimptr_out[0],dimptr_out[1],dimptr_out[2],dimptr_out[0],Nfx_vec,Nfy_vec);

	/*Now create Dx_tilde*/
	//fprintf(stderr,"Dx_tilde: %d x %d x %d\n",Ndetmodes,Nfx_vec,Nfy_vec);
	Dx_tilde = (complex<double> ***)malloc(sizeof(complex<double> **)*Nfy_vec);
	for(int j=0;j<Nfy_vec;j++){
	  Dx_tilde[j]=(complex<double> **)malloc(sizeof(complex<double> *)*Nfx_vec);
	  for(int i=0;i<Nfx_vec;i++){
	    Dx_tilde[j][i] = (complex<double> *)malloc(sizeof(complex<double>)*Ndetmodes);
	  }
	}

	D_temp_re = castMatlab3DArray(mxGetPr(element),dimptr_out[0],dimptr_out[1],dimptr_out[2]);
	D_temp_im = castMatlab3DArray(mxGetPi(element),dimptr_out[0],dimptr_out[1],dimptr_out[2]);


	for(int k=0;k<Ndetmodes;k++)
	  for(int j=0;j<Nfy_vec;j++)
	    for(int i=0;i<Nfx_vec;i++){
	      Dx_tilde[j][i][k] = D_temp_re[j][i][k] + I*D_temp_im[j][i][k];
	    }
	//fprintf(stderr,"Dx_tilde[2][3][5]: %e + i%e\n",real(Dx_tilde[4][2][1]),imag(Dx_tilde[4][2][1]));
	freeCastMatlab3DArray(D_temp_re,Nfy_vec);freeCastMatlab3DArray(D_temp_im,Nfy_vec);

	element = mxGetField( (mxArray *)prhs[input_counter], 0, "Dy_tilde");
	ndims = mxGetNumberOfDimensions(element);
	dimptr_out = mxGetDimensions(element);
	if(ndims!=3)
	  fprintf(stderr,"Dy_tilde should be three dimensional\n");
		
	if( dimptr_out[1]!=Nfx_vec || dimptr_out[2]!=Nfy_vec )
	  fprintf(stderr,"Dx_tilde has dimension %dx%dx%d yet it should have dimension %dx%dx%d\n",dimptr_out[0],dimptr_out[1],dimptr_out[2],dimptr_out[0],Nfx_vec,Nfy_vec);

	/*Now create Dy_tilde*/
	
	Dy_tilde = (complex<double> ***)malloc(sizeof(complex<double> **)*Nfy_vec);
	for(int j=0;j<Nfy_vec;j++){
	  Dy_tilde[j]=(complex<double> **)malloc(sizeof(complex<double> *)*Nfx_vec);
	  for(int i=0;i<Nfx_vec;i++){
	    Dy_tilde[j][i] = (complex<double> *)malloc(sizeof(complex<double>)*Ndetmodes);
	  }
	}
	
	D_temp_re = castMatlab3DArray(mxGetPr(element),dimptr_out[0],dimptr_out[1],dimptr_out[2]);
	D_temp_im = castMatlab3DArray(mxGetPi(element),dimptr_out[0],dimptr_out[1],dimptr_out[2]);


	for(int k=0;k<Ndetmodes;k++)
	  for(int j=0;j<Nfy_vec;j++)
	    for(int i=0;i<Nfx_vec;i++){
	      Dy_tilde[j][i][k] = D_temp_re[j][i][k] + I*D_temp_im[j][i][k];
	    }
	freeCastMatlab3DArray(D_temp_re,Nfy_vec);freeCastMatlab3DArray(D_temp_im,Nfy_vec);
      }
    }
    input_counter++;
    /*Got D_tilde*/

    /*Get k_det_obs*/
    if( !mxIsEmpty(prhs[input_counter]) ){
      if( mxGetNumberOfElements(prhs[input_counter]) != 1)
	fprintf(stderr,"k_det_obs has %d elements, it should only have 1.\n",(int)mxGetNumberOfElements(prhs[input_counter]));
      k_det_obs_global = (int)*mxGetPr((mxArray *)prhs[input_counter]) - 1;
    }
    input_counter++;
    /*Got k_det_obs*/    
    //now set z_obs
    z_obs = z_grid_labels[k_det_obs_global];

    
  }//end of if(exdetintegral==1)
  else
    input_counter+=4;//need to advance beyond fields which were not read in as exdetintegral was set to 0
  
  /*Get air_interface*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    air_interface_present=1;
    air_interface = *mxGetPr((mxArray *)prhs[input_counter]);
    fprintf(stderr,"air_interface: %e\nz_obs: %e\n",air_interface,z_obs);
  }
  else{
    air_interface_present=0;
  }
  input_counter++;
  /*Got air_interface*/

  /*Get intmatprops*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    intmatprops = (int)*mxGetPr((mxArray *)prhs[input_counter]);
  }
  else{
    intmatprops = 0;
  }
  input_counter++;
  /*Got intmatprops*/

  /*Get intmethod*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    intmethod = (int)*mxGetPr((mxArray *)prhs[input_counter]);
  }
  else{
    intmethod = 1;
  }
  fprintf(stderr,"intmethod=%d\n",intmethod);
  input_counter++;
  /*Got intmatprops*/

  /*Get tdfield*/
  if(mxIsStruct(prhs[input_counter])){
    fprintf(stderr,"tdfield 01\n");
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
   
    //check that all fields are present
    if(num_fields != 2){
      sprintf(message_buffer, "tdfield should have 2 members, it has %d",num_fields);
      mexfprintf(message_buffer);
    }
    element = mxGetField( (mxArray *)prhs[input_counter], 0, "exi");
    //fprintf(stderr,"isempty ?: (%d)\n", !mxIsEmpty(element));
    if(!mxIsEmpty(element)){
      ndims = mxGetNumberOfDimensions(element);
      dimptr_out = mxGetDimensions(element);
      exi = castMatlab3DArray( mxGetPr( (mxArray *)element ), dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      exi_present = 1;
      fprintf(stderr,"Got tdfield, ndims=%d, dims=(%d,%d,%d)\n",ndims,dimptr_out[0],dimptr_out[1],dimptr_out[2]);
      fprintf(stderr,"ddfield is empty\n");
    }
    else{
      exi_present = 0;
      fprintf(stderr,"exi not present\n");
    }
      
    /*
    for(int iti=0;iti<(20000-1);iti++)
      fprintf(stdout,"%e ",exi[iti][0][511]);
    */
    element = mxGetField( (mxArray *)prhs[input_counter], 0, "eyi");
    if(!mxIsEmpty(element)){
      ndims = mxGetNumberOfDimensions(element);
      dimptr_out = mxGetDimensions(element);
      eyi = castMatlab3DArray( mxGetPr( (mxArray *)element ), dimptr_out[0], dimptr_out[1], dimptr_out[2]);
      eyi_present = 1;
    }
    else{
      eyi_present = 0;
      fprintf(stderr,"eyi not present\n");
    }
	
    input_counter++;
  }
  /*Got tdfield*/

  /*Get tdfdir*/
  //fprintf(stderr,"tdfdir: %d (%d)\n", mxIsChar(prhs[input_counter]),input_counter);
  if(mxIsChar(prhs[input_counter])){
    tdfdirstr = (char *)malloc((1+(int)mxGetNumberOfElements( (mxArray *)prhs[input_counter]))*sizeof(char));
    mxGetString( (mxArray *)prhs[input_counter], tdfdirstr, (1+(int)mxGetNumberOfElements( (mxArray *)prhs[input_counter])));
    //fprintf(stderr,"tdfdirstr = %s\n",tdfdirstr);

    //calculate the Ni_tdf
    for(k=0;k<K_tot;k++)
      if( (k % skip_tdf) == 0)
	Nk_tdf++;
    
    for(i=0;i<I_tot;i++)
      if( (i % skip_tdf) == 0)
	Ni_tdf++;
    fprintf(stderr,"Ni_tdf=%d, Nk_tdf=%d\n",Ni_tdf,Nk_tdf);
    input_counter++;
  }
  /*Got tdfdir*/

  /*Get fieldsample*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    if(mxIsStruct(prhs[input_counter])){
      num_fields = mxGetNumberOfFields(prhs[input_counter]);
      if(num_fields != 4){
	sprintf(message_buffer, "fieldsample should have 4 members, it has %d",num_fields);
	mexfprintf(message_buffer);
      }
      for(int i=0;i<4;i++){
	element = mxGetField( (mxArray *)prhs[input_counter], 0, fieldsample_elements[i]);

	if(!strcmp(fieldsample_elements[i],"i")){
	  fieldsample_i = mxGetPr(element);
	  N_fieldsample_i = mxGetNumberOfElements(element);
	  //fprintf(stderr,"Number of elements in fieldsample_i: %d\n",N_fieldsample_i);
	}
	else if(!strcmp(fieldsample_elements[i],"j")){
	  fieldsample_j = mxGetPr(element);
	  N_fieldsample_j = mxGetNumberOfElements(element);
	}
	else if(!strcmp(fieldsample_elements[i],"k")){
	  fieldsample_k = mxGetPr(element);
	  N_fieldsample_k = mxGetNumberOfElements(element);
	}
	else if(!strcmp(fieldsample_elements[i],"n")){
	  fieldsample_n = mxGetPr(element);
	  N_fieldsample_n = mxGetNumberOfElements(element);
	}
      }
      
    }
    input_counter++;
  }
  else{
    N_fieldsample_i = 0;
    N_fieldsample_j = 0;
    N_fieldsample_k = 0;
    N_fieldsample_n = 0;
    
  }

  /*Get campssample*/
  if( !mxIsEmpty(prhs[input_counter]) ){
    num_fields = mxGetNumberOfFields(prhs[input_counter]);
    fprintf(stderr,"num_fields=%d\n",num_fields);
    if(num_fields != 2){
	sprintf(message_buffer, "campssample should have 2 members, it has %d",num_fields);
	mexfprintf(message_buffer);
    }
    for(int i=0;i<2;i++){
      element = mxGetField( (mxArray *)prhs[input_counter], 0, campssample_elements[i]);
      if(!strcmp(campssample_elements[i],"vertices")){
	if( !mxIsEmpty(element) ){
	  dimptr_out = mxGetDimensions(element);
	  fprintf(stderr,"found vertices (%d x %d)\n", dimptr_out[0], dimptr_out[1]);
	  vertices = castMatlab2DArrayInt((int *)mxGetPr( (mxArray *)element), dimptr_out[0], dimptr_out[1]);
	  //fprintf(stderr,"vertices[1000] = %d %d %d\n",vertices[0][10],vertices[1][10],vertices[2][10]);
	  nvertices =  dimptr_out[0];
	  //convert vertices to index base 0
	  
	  for( int j=0;j<nvertices;j++)
	    for( int k=0;k<3;k++)
	      vertices[k][j] = vertices[k][j] - 1;
	  
	  
	}
	else{
	  nvertices = 0;
	}
	    
      }
      else if(!strcmp(campssample_elements[i],"components")){

	if( !mxIsEmpty(element) ){
	  dimptr_out = mxGetDimensions(element);
	  components = (int *)mxGetPr( (mxArray *)element);
	  if( dimptr_out[0] > dimptr_out[1] )
	    ncomponents = dimptr_out[0];
	  else
	    ncomponents = dimptr_out[1];
	}
	fprintf(stderr,"found components (%d)\n",ncomponents);
      }
    }
    
  }
  else{
    fprintf(stderr, "campssample is empty\n");
  }
    
  /*Got campssample*/
  //fprintf(stderr,"Number of elements in fieldsample_*: %d %d %d %d\n",N_fieldsample_i,N_fieldsample_j,N_fieldsample_k,N_fieldsample_n);
  /*Got fieldsample*/

  /*Deduce the refractive index of the first layer of the multilayer, or of the bulk of homogeneous*/
  refind = sqrt(1./(freespace_Cbx[0]/dt[0]*dx)/eo);
  fprintf(stderr,"refind=%e\n",refind);
  /*Setup temporary storage for detector sensitivity evaluation*/
  if(exdetintegral){
    //These are 2D matrices in row-major order
    Ex_t = (fftw_complex*) fftw_malloc( (J_tot-*Dyl-*Dyu)*(I_tot-*Dxl-*Dxu)*sizeof(fftw_complex));
    Ey_t = (fftw_complex*) fftw_malloc( (J_tot-*Dyl-*Dyu)*(I_tot-*Dxl-*Dxu)*sizeof(fftw_complex));

    pex_t = fftw_plan_dft_2d(I_tot-*Dxl-*Dxu, J_tot-*Dyl-*Dyu, Ex_t, Ex_t, FFTW_FORWARD, FFTW_MEASURE);
    pey_t = fftw_plan_dft_2d(I_tot-*Dxl-*Dxu, J_tot-*Dyl-*Dyu, Ey_t, Ey_t, FFTW_FORWARD, FFTW_MEASURE);
    
    fprintf(stderr,"Ex_t_cm has size %dx%d\n",(J_tot-*Dyl-*Dyu),(I_tot-*Dxl-*Dxu));
    Ex_t_cm = (complex<double> **)malloc(sizeof(complex<double> *)*(J_tot-*Dyl-*Dyu));Ey_t_cm = (complex<double> **)malloc(sizeof(complex<double> *)*(J_tot-*Dyl-*Dyu));
    for(int j=0;j<(J_tot-*Dyl-*Dyu);j++){
      Ex_t_cm[j] = (complex<double> *)malloc(sizeof(complex<double>)*(I_tot-*Dxl-*Dxu));Ey_t_cm[j] = (complex<double> *)malloc(sizeof(complex<double>)*(I_tot-*Dxl-*Dxu));
    }
  }
  
  double f_max=0.;
  for(int ifx=0;ifx<N_f_ex_vec;ifx++)
    if(f_ex_vec[ifx]>f_max)
      f_max=f_ex_vec[ifx];


  if (!useCD){//only perform if using the PSTD method
    //find the largest dimension, i.e., maximum of I_tot, J_tot and K_tot
    max_IJK = I_tot;
    if( J_tot > max_IJK )
      max_IJK = J_tot;
    if ( K_tot > max_IJK )
      max_IJK = K_tot;

    //establish additional memory buffers which are used by the PSTD method
    ca_vec = (double **)malloc( sizeof(double *)*omp_get_max_threads());
    cb_vec = (double **)malloc( sizeof(double *)*omp_get_max_threads());
    cc_vec = (double **)malloc( sizeof(double *)*omp_get_max_threads());

    for(i=0;i<omp_get_max_threads();i++){
      ca_vec[i] = (double *)malloc( sizeof(double)*(max_IJK+1));
      cb_vec[i] = (double *)malloc( sizeof(double)*(max_IJK+1));
      cc_vec[i] = (double *)malloc( sizeof(double)*(max_IJK+1));
    }

    eh_vec = (fftw_complex **) malloc(sizeof(fftw_complex*)*omp_get_max_threads());
    for(i=0;i<omp_get_max_threads();i++)
      *(eh_vec+i) = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*(max_IJK+1));
    for(i = 0;i<(max_IJK+1);i++){
      eh_vec[omp_get_thread_num()][i][0]=0.;
      eh_vec[omp_get_thread_num()][i][1]=0.;
    }
    
    N_e_x = I_tot - 1 + 1;
    N_e_y = J_tot - 1 + 1;
    N_e_z = K_tot - 1 + 1;
    N_h_x = I_tot + 1;
    N_h_y = J_tot + 1;
    N_h_z = K_tot + 1;

    pf_exy = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_exy = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_exz = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_exz = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_eyx = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_eyx = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_eyz = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_eyz = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_ezx = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_ezx = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_ezy = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_ezy = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());

    pf_hxy = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_hxy = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_hxz = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_hxz = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_hyx = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_hyx = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_hyz = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_hyz = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_hzx = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_hzx = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pf_hzy = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());
    pb_hzy = (fftw_plan *) malloc(sizeof(fftw_plan *)*omp_get_max_threads());

    for(i=0;i<omp_get_max_threads();i++){
      pf_exy[i] = fftw_plan_dft_1d(N_e_y, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_exy[i] = fftw_plan_dft_1d(N_e_y, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_exz[i] = fftw_plan_dft_1d(N_e_z, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_exz[i] = fftw_plan_dft_1d(N_e_z, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_eyx[i] = fftw_plan_dft_1d(N_e_x, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_eyx[i] = fftw_plan_dft_1d(N_e_x, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_eyz[i] = fftw_plan_dft_1d(N_e_z, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_eyz[i] = fftw_plan_dft_1d(N_e_z, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_ezx[i] = fftw_plan_dft_1d(N_e_x, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_ezx[i] = fftw_plan_dft_1d(N_e_x, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_ezy[i] = fftw_plan_dft_1d(N_e_y, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_ezy[i] = fftw_plan_dft_1d(N_e_y, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      
      pf_hxy[i] = fftw_plan_dft_1d(N_h_y, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_hxy[i] = fftw_plan_dft_1d(N_h_y, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_hxz[i] = fftw_plan_dft_1d(N_h_z, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_hxz[i] = fftw_plan_dft_1d(N_h_z, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_hyx[i] = fftw_plan_dft_1d(N_h_x, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_hyx[i] = fftw_plan_dft_1d(N_h_x, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_hyz[i] = fftw_plan_dft_1d(N_h_z, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_hyz[i] = fftw_plan_dft_1d(N_h_z, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_hzx[i] = fftw_plan_dft_1d(N_h_x, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_hzx[i] = fftw_plan_dft_1d(N_h_x, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
      pf_hzy[i] = fftw_plan_dft_1d(N_h_y, eh_vec[i], eh_vec[i], FFTW_FORWARD, FFTW_MEASURE);
      pb_hzy[i] = fftw_plan_dft_1d(N_h_y, eh_vec[i], eh_vec[i], FFTW_BACKWARD, FFTW_MEASURE);
    }

    dk_e_x = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(N_e_x));
    dk_e_y = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(N_e_y));
    dk_e_z = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(N_e_z));
    
    dk_h_x = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(N_h_x));
    dk_h_y = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(N_h_y));
    dk_h_z = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(N_h_z));
    
    init_diff_shift_op( -0.5, dk_e_x, N_e_x);
    init_diff_shift_op( -0.5, dk_e_y, N_e_y);
    init_diff_shift_op( -0.5, dk_e_z, N_e_z);

    init_diff_shift_op(  0.5, dk_h_x, N_h_x);
    init_diff_shift_op(  0.5, dk_h_y, N_h_y);
    init_diff_shift_op(  0.5, dk_h_z, N_h_z);
   

  }
  /*Now evaluate Np*/
  //evaluate maximum optical frequency

  Np=(int)floor(1./(2.5*dt[0]*f_max));
  dtp = ((double)Np)*dt[0];
  //fprintf(stderr,"Np=%d, dtp=%e\n",Np,dtp);
  //calculate Npe, the temporal DFT will be evaluated whenever tind incriments by Npe
  for(tind = start_tind; tind < *Nt; tind++)
    if( (tind-start_tind) % Np == 0)
      Npe++;
  fprintf(stderr,"Np=%d, Nt=%d, Npe=%d, f_max=%e,Npraw=%e \n",Np,*Nt,Npe,f_max,2.5*dt[0]*f_max);
  //fprintf(stderr,"Pre 01\n");
  //initialise E_norm and H_norm
  E_norm=(complex<double> *)malloc(N_f_ex_vec*sizeof(complex<double>));
  H_norm=(complex<double> *)malloc(N_f_ex_vec*sizeof(complex<double>));
  for(int ifx=0;ifx<N_f_ex_vec;ifx++){
    E_norm[ifx]=0.;
    H_norm[ifx]=0.;
  }
  
  //fprintf(stderr,"Pre 02\n");

  //fprintf(stderr,"Qos 00 (%d) (%d,%d,%d,%d):\n",J_tot,cuboid[0], cuboid[1],cuboid[4], cuboid[5]);
  /*set up surface mesh if required*/

  if( exphasorssurface && runmode==rm_complete){
    if(J_tot==0)
      conciseCreateBoundary(cuboid[0], cuboid[1],cuboid[4], cuboid[5], 
			    &mx_surface_vertices, &mx_surface_facets);
    else
      conciseTriangulateCuboidSkip( cuboid[0], cuboid[1], cuboid[2], 
				    cuboid[3], cuboid[4], cuboid[5],
				    phasorinc[0],phasorinc[1],phasorinc[2],
				    &mx_surface_vertices, &mx_surface_facets);
    //fprintf(stderr,"Qos 00a:\n");
    //we don't need the facets so destroy the matrix now to save memory
    mxDestroyArray(mx_surface_facets);
    dimptr_out = mxGetDimensions(mx_surface_vertices);
    n_surface_vertices = dimptr_out[0];
    //cast the vertex array as a 2-d integer array
    surface_vertices = castMatlab2DArrayInt((int *)mxGetPr( (mxArray *)mx_surface_vertices), dimptr_out[0], dimptr_out[1]);
    //create space for the complex amplitudes E and H around the surface. These will be in a large complex
    //array with each line being of the form Re(Ex) Im(Ex) Re(Ey) ... Im(Hz). Each line corresponds to the 
    //the vertex with the same line as in surface_vertices
    ndims   = 3;

    dims[0] = n_surface_vertices;
    dims[1] = 6; //one for each component of field
    dims[2] = N_f_ex_vec;

    mx_surface_amplitudes = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX);
    surface_EHr = castMatlab3DArray( mxGetPr( (mxArray *)mx_surface_amplitudes), dims[0], dims[1], dims[2]);
    surface_EHi = castMatlab3DArray( mxGetPi( (mxArray *)mx_surface_amplitudes), dims[0], dims[1], dims[2]);
    //now need to add a command to update the complex amplitudes
  }

  //fprintf(stderr,"Pre 03\n");
  /*Now set up the phasor array, we will have 3 complex output arrays for Ex, Ey and Ez.
    Phasors are extracted over the range Dxl + 3 - 1 to I_tot - Dxu - 1 to avoid pml cells
    see page III.80 for explanation of the following. This has been extended so that interpolation
    is done at the end of the FDTD run and also to handle the case of when there is no PML in place
    more appropriatley*/
  if( *Dxl )
    pind_il = *Dxl + 2;
  else
    pind_il = 0;

  if( *Dxu )
    pind_iu = I_tot - *Dxu - 1;
  else
    pind_iu = I_tot;
  

  if( *Dyl )
    pind_jl = *Dyl + 2;
  else
    pind_jl = 0;
  
  if( *Dyu )
    pind_ju = J_tot - *Dyu - 1;
  else
    pind_ju = J_tot;
  
  
  if( *Dzl )
    pind_kl = *Dzl + 2;
  else
    pind_kl = 0;
  
  if( *Dzu )
    pind_ku = K_tot - *Dzu - 1;
  else
    pind_ku = K_tot;

  //fprintf(stderr,"Pre 04\n");
  //fprintf(stderr,"pind_ju: %d, pind_jl: %d, J_tot: %d\n",pind_ju,pind_jl,J_tot);
  /*     
	 pind_il = 0;
	 pind_iu = I_tot;
	 pind_jl = 0;
	 pind_ju = J_tot;
  */
    
 //fprintf(stderr,"Qos 01:\n");

  /*  dims[0] = I_tot - *Dxu - *Dxl - 3 + 1;
      dims[1] = J_tot - *Dyu - *Dyl - 3 + 1;
      dims[2] = K_tot - *Dzu - *Dzl - 3 + 1;*/
  
  if( runmode == rm_complete && exphasorsvolume ){
    ndims = 3;

    dims[0] = pind_iu - pind_il + 1;
    dims[1] = pind_ju - pind_jl + 1;
    dims[2] = pind_ku - pind_kl + 1;
    
    fprintf(stderr,"dims:(%d,%d,%d)\n",dims[0],dims[1],dims[2]);
    
    plhs[0] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Ex
    plhs[1] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Ey
    plhs[2] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Ez
    
    plhs[3] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Hx
    plhs[4] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Hy
    plhs[5] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Hz

    ExR = castMatlab3DArray(mxGetPr( (mxArray *)plhs[0]), dims[0], dims[1], dims[2]);
    ExI = castMatlab3DArray(mxGetPi( (mxArray *)plhs[0]), dims[0], dims[1], dims[2]);
    
    EyR = castMatlab3DArray(mxGetPr( (mxArray *)plhs[1]), dims[0], dims[1], dims[2]);
    EyI = castMatlab3DArray(mxGetPi( (mxArray *)plhs[1]), dims[0], dims[1], dims[2]);
    
    EzR = castMatlab3DArray(mxGetPr( (mxArray *)plhs[2]), dims[0], dims[1], dims[2]);
    EzI = castMatlab3DArray(mxGetPi( (mxArray *)plhs[2]), dims[0], dims[1], dims[2]);

    HxR = castMatlab3DArray(mxGetPr( (mxArray *)plhs[3]), dims[0], dims[1], dims[2]);
    HxI = castMatlab3DArray(mxGetPi( (mxArray *)plhs[3]), dims[0], dims[1], dims[2]);
    
    HyR = castMatlab3DArray(mxGetPr( (mxArray *)plhs[4]), dims[0], dims[1], dims[2]);
    HyI = castMatlab3DArray(mxGetPi( (mxArray *)plhs[4]), dims[0], dims[1], dims[2]);

    HzR = castMatlab3DArray(mxGetPr( (mxArray *)plhs[5]), dims[0], dims[1], dims[2]);
    HzI = castMatlab3DArray(mxGetPi( (mxArray *)plhs[5]), dims[0], dims[1], dims[2]);
    //fprintf(stderr,"Pre 05\n");
    //fprintf(stderr,"Qos 02:\n");
    //these will ultimately be copies of the phasors used to test convergence
    if(sourcemode==sm_steadystate){
      dummy_array[0] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Ex
      dummy_array[1] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Ey
      dummy_array[2] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Ez
    }
    //fprintf(stderr,"Pre 06\n");
    //fprintf(stderr,"Qos 03:\n");
    if( sourcemode==sm_steadystate ){
      ExR2 = castMatlab3DArray(mxGetPr( (mxArray *)dummy_array[0]), dims[0], dims[1], dims[2]);
      ExI2 = castMatlab3DArray(mxGetPi( (mxArray *)dummy_array[0]), dims[0], dims[1], dims[2]);
    
      EyR2 = castMatlab3DArray(mxGetPr( (mxArray *)dummy_array[1]), dims[0], dims[1], dims[2]);
      EyI2 = castMatlab3DArray(mxGetPi( (mxArray *)dummy_array[1]), dims[0], dims[1], dims[2]);
    
      EzR2 = castMatlab3DArray(mxGetPr( (mxArray *)dummy_array[2]), dims[0], dims[1], dims[2]);
      EzI2 = castMatlab3DArray(mxGetPi( (mxArray *)dummy_array[2]), dims[0], dims[1], dims[2]);
    }
    //fprintf(stderr,"Pre 07\n");
    //this will be a copy of the phasors which are extracted from the previous cycle
    
    //fprintf(stderr,"Qos 04:\n");
    
    //now construct the grid labels
    label_dims[0] = 1;
    label_dims[1] = dims[0];
    plhs[10] = mxCreateNumericArray( 2, (const mwSize *)label_dims, mxDOUBLE_CLASS, mxREAL); //x
    x_grid_labels_out = mxGetPr( (mxArray *)plhs[10]);
    
    label_dims[0] = 1;
    label_dims[1] = dims[1];
    //fprintf(stderr,"plhs[11]: %d,%d\n",label_dims[0],label_dims[1] );
    plhs[11] = mxCreateNumericArray( 2, (const mwSize *)label_dims, mxDOUBLE_CLASS, mxREAL); //y
    y_grid_labels_out = mxGetPr( (mxArray *)plhs[11]);
    
    label_dims[0] = 1;
    label_dims[1] = dims[2];
    plhs[12] = mxCreateNumericArray( 2, (const mwSize *)label_dims, mxDOUBLE_CLASS, mxREAL); //y
    z_grid_labels_out = mxGetPr( (mxArray *)plhs[12]);
    //fprintf(stderr,"Pre 08\n");
  }
  else{
    //initialise to empty matrices
    //fprintf(stderr,"Pre 09\n");
    ndims = 2;
    
    dims[0] = 0;
    dims[1] = 0;
    
    plhs[0] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Ex
    plhs[1] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Ey
    plhs[2] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Ez
    
    plhs[3] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Hx
    plhs[4] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Hy
    plhs[5] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //Hz

    plhs[10] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //x_grid_labels_out
    plhs[11] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //y_grid_labels_out
    plhs[12] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //z_grid_labels_out
  }
  //fprintf(stderr,"Pre 10\n");
  //fprintf(stderr,"Qos 05:\n");
  //plhs[13] -> plhs[15] are the interpolated electric field values
  //plhs[16] -> plhs[18] are the interpolated magnetic field values
 
  //initialise arrays
  if( runmode == rm_complete && exphasorsvolume ){
    initialiseDouble3DArray(ExR, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(ExI, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(EyR, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(EyI, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(EzR, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(EzI, dims[0], dims[1], dims[2]);
    
    initialiseDouble3DArray(HxR, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(HxI, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(HyR, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(HyI, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(HzR, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(HzI, dims[0], dims[1], dims[2]);
  }
//fprintf(stderr,"Pre 11\n");
  if( exdetintegral && runmode==rm_complete){
    ndims = 2;
    dims[0] = 1;
    dims[1] = 1;
    const char *fieldnames[] = {"Idx","Idy"};
    plhs[26] = mxCreateStructArray(ndims,(const mwSize *)dims,2,fieldnames);
 
    ndims = 2;
    dims[0] = Ndetmodes;
    dims[1] = N_f_ex_vec;
  
    mx_Idx = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX);
    Idx_re = castMatlab2DArray(mxGetPr(mx_Idx),dims[0],dims[1]);
    Idx_im = castMatlab2DArray(mxGetPi(mx_Idx),dims[0],dims[1]);
    
    mx_Idy = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX);
    Idy_re = castMatlab2DArray(mxGetPr(mx_Idy),dims[0],dims[1]);
    Idy_im = castMatlab2DArray(mxGetPi(mx_Idy),dims[0],dims[1]);

    Idx = (complex<double> **)malloc(sizeof(complex<double> *)*N_f_ex_vec);
    Idy = (complex<double> **)malloc(sizeof(complex<double> *)*N_f_ex_vec);
    
    for(int ifx=0;ifx<N_f_ex_vec;ifx++){
      Idx[ifx]=(complex<double> *)malloc(sizeof(complex<double>)*Ndetmodes);
      Idy[ifx]=(complex<double> *)malloc(sizeof(complex<double>)*Ndetmodes);
      for(int im=0;im<Ndetmodes;im++){
	Idx[ifx][im]=0.;
	Idy[ifx][im]=0.;
      }
    }

    for(int im=0;im<Ndetmodes;im++){
      for(int ifx=0;ifx<N_f_ex_vec;ifx++){
	Idx_re[ifx][im]=0.;Idx_im[ifx][im]=0.;Idy_re[ifx][im]=0.;Idy_im[ifx][im]=0.;
      }
    }

    mxSetField(plhs[26],0,"Idx",mx_Idx);
    mxSetField(plhs[26],0,"Idy",mx_Idy);
  }
  else{
    ndims = 2;
    dims[0] = 0;
    dims[1] = 0;
    
    plhs[26] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX);
  }
//fprintf(stderr,"Pre 12\n");
  if(runmode == rm_complete && sourcemode==sm_steadystate && exphasorsvolume){
    initialiseDouble3DArray(ExR2, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(ExI2, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(EyR2, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(EyI2, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(EzR2, dims[0], dims[1], dims[2]);
    initialiseDouble3DArray(EzI2, dims[0], dims[1], dims[2]);
  }
  //fprintf(stderr,"Pre 13\n");
  /*This is just for efficiency */
  K = K_tot - Dxl[0] - Dxu[0];

  /*Now set up the phasor arrays for storing the fdtd version of the input fields,
    these will be used in a boot strapping procedure. Calculated over a complete 
    xy-plane. */
  
  ndims = 2;
  dims[0] = I_tot;
  dims[1] = J_tot + 1;
  plhs[6] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //x electric field source phasor - boot strapping
  iwave_lEx_Rbs = castMatlab2DArray(mxGetPr( (mxArray *)plhs[6]), dims[0], dims[1]);
  iwave_lEx_Ibs = castMatlab2DArray(mxGetPi( (mxArray *)plhs[6]), dims[0], dims[1]);
  initialiseDouble2DArray(iwave_lEx_Rbs, dims[0], dims[1]);
  initialiseDouble2DArray(iwave_lEx_Ibs, dims[0], dims[1]);

  dims[0] = I_tot + 1;
  dims[1] = J_tot;
  plhs[7] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //y electric field source phasor - boot strapping  
  iwave_lEy_Rbs = castMatlab2DArray(mxGetPr( (mxArray *)plhs[7]), dims[0], dims[1]);
  iwave_lEy_Ibs = castMatlab2DArray(mxGetPi( (mxArray *)plhs[7]), dims[0], dims[1]);
  initialiseDouble2DArray(iwave_lEy_Rbs, dims[0], dims[1]);
  initialiseDouble2DArray(iwave_lEy_Ibs, dims[0], dims[1]);
  
  dims[0] = I_tot + 1;
  dims[1] = J_tot;
  plhs[8] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //x magnetic field source phasor - boot strapping

  iwave_lHx_Rbs = castMatlab2DArray(mxGetPr( (mxArray *)plhs[8]), dims[0], dims[1]);
  iwave_lHx_Ibs = castMatlab2DArray(mxGetPi( (mxArray *)plhs[8]), dims[0], dims[1]);
  initialiseDouble2DArray(iwave_lHx_Rbs, dims[0], dims[1]);
  initialiseDouble2DArray(iwave_lHx_Ibs, dims[0], dims[1]);

  dims[0] = I_tot;
  dims[1] = J_tot + 1;
  plhs[9] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); //y magnetic field source phasor - boot strapping
  iwave_lHy_Rbs = castMatlab2DArray(mxGetPr( (mxArray *)plhs[9]), dims[0], dims[1]);
  iwave_lHy_Ibs = castMatlab2DArray(mxGetPi( (mxArray *)plhs[9]), dims[0], dims[1]);
  initialiseDouble2DArray(iwave_lHy_Rbs, dims[0], dims[1]);
  initialiseDouble2DArray(iwave_lHy_Ibs, dims[0], dims[1]);
  
  /*start dispersive*/

  //work out if we have any disperive materials
  is_disp = is_dispersive(materials,gamma, dt[0], I_tot, J_tot, K_tot);
  //work out if we have conductive background
  is_cond =  is_conductive(rho_x,rho_y,rho_z,I_tot,J_tot,K_tot);
  //work out if we have a dispersive background
  if(is_disp_ml)
    is_disp_ml = is_dispersive_ml(ml_gamma, K_tot);
  //  fprintf(stderr,"is_disp:%d, is_cond%d, is_disp_ml: %d\n",is_disp,is_cond,is_disp_ml);
  //if we have dispersive materials we need to create additional field variables
  if(is_disp || is_disp_ml){
    allocate_auxilliary_mem(I_tot, J_tot, K_tot,
			    &Exy_nm1, &Exz_nm1, 
			    &Eyx_nm1, &Eyz_nm1,  
			    &Ezx_nm1, &Ezy_nm1,
			    &Jxy, &Jxz, 
			    &Jyx, &Jyz,  
			    &Jzx, &Jzy,
			    &Jxy_nm1, &Jxz_nm1, 
			    &Jyx_nm1, &Jyz_nm1,  
			    &Jzx_nm1, &Jzy_nm1);
  }
//fprintf(stderr,"Pre 14\n");
  if(is_cond){
    allocate_auxilliary_mem_conductive(I_tot, J_tot, K_tot,
				       &Jcxy, &Jcxz, 
				       &Jcyx, &Jcyz,  
				       &Jczx, &Jczy);
  }
  /*end dispersive*/

  /*setup the output array for the sampled field*/
  if( !((N_fieldsample_i == 0) || (N_fieldsample_j == 0) || (N_fieldsample_k == 0) || (N_fieldsample_n == 0)) ){
    ndims   = 4;
    dims[0] = N_fieldsample_i;
    dims[1] = N_fieldsample_j;
    dims[2] = N_fieldsample_k;
    dims[3] = N_fieldsample_n;

    mx_fieldsample = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
    fieldsample = castMatlab4DArray( mxGetPr(mx_fieldsample),N_fieldsample_i, N_fieldsample_j,N_fieldsample_k,N_fieldsample_n);
    //these variables are temporary storage to reduce the need for interpolation during the algorithm
    mx_fieldsample_x = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
    fieldsample_x = castMatlab4DArray( mxGetPr(mx_fieldsample),N_fieldsample_i, N_fieldsample_j,N_fieldsample_k,N_fieldsample_n);
    mx_fieldsample_y = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
    fieldsample_y = castMatlab4DArray( mxGetPr(mx_fieldsample),N_fieldsample_i, N_fieldsample_j,N_fieldsample_k,N_fieldsample_n);
    mx_fieldsample_z = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
    fieldsample_z = castMatlab4DArray( mxGetPr(mx_fieldsample),N_fieldsample_i, N_fieldsample_j,N_fieldsample_k,N_fieldsample_n);
    
  }
  else{
    ndims   = 4;
    dims[0] = 0;
    dims[1] = 0;
    dims[2] = 0;
    dims[3] = 0;
    
    mx_fieldsample = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
  }
  plhs[27] = mx_fieldsample;

  if(nvertices>0){
      ndims   = 3;
      dims[0] = nvertices;
      dims[1] = ncomponents;
      dims[2] = N_f_ex_vec;
      mx_camplitudes = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); 
      camplitudesR = castMatlab3DArray(mxGetPr(mx_camplitudes),dims[0],dims[1],dims[2]);
      camplitudesI = castMatlab3DArray(mxGetPi(mx_camplitudes),dims[0],dims[1],dims[2]);
      
  }
  else{
    ndims   = 3;
    dims[0] = 0;
    dims[1] = 0;
    dims[2] = 0;
    mx_camplitudes = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxCOMPLEX); 
    
  }
  plhs[28] = mx_camplitudes;
  //fprintf(stderr,"Pre 15\n");
  /*end of setup the output array for the sampled field*/

  
  
  /*set up the parameters for the phasor convergence procedure*/
  /*First we set dt so that an integer number of time periods fits within a sinusoidal period
   */
  double Nsteps_tmp, dt_old;
  if(sourcemode==sm_steadystate){
    dt_old = dt[0];
    Nsteps_tmp = ceil(2.*dcpi/omega_an[0]/dt[0]*3);
    dt[0] = 2.*dcpi/omega_an[0]*3/Nsteps_tmp;
  }
  
  //fprintf(stderr,"Pre 16\n");
  if(sourcemode==sm_steadystate && runmode == rm_complete)
    fprintf(stderr,"Changing dt from %.10e to %.10e\n",dt_old,dt[0]);
  Nsteps = (int)round(Nsteps_tmp);
  //fprintf(stderr,"Pre 17\n");
  //Nsteps = (int)(floor(3*2.*dcpi/(omega_an[0]*dt[0])) + 1.);//the number of time steps in a sinusoidal period
  dft_counter = 0;
    //fprintf(stderr,"Pre 18\n");
  /*Nt should be an integer number of Nsteps in the case of steady-state operation*/
  if(sourcemode==sm_steadystate && runmode == rm_complete)
    if(*Nt/Nsteps*Nsteps != *Nt){
      fprintf(stderr,"Changing the value of Nt from %d to",*Nt);
      *Nt = *Nt/Nsteps*Nsteps;
      fprintf(stderr," %d for correct phasor extraction\n",*Nt);
    }
  //fprintf(stderr,"Pre 19\n");

  if( (runmode==rm_complete)&&(sourcemode==sm_steadystate))
    printf("Nsteps: %d \n",Nsteps);

  /*An optimization step in the 2D (J_tot==0) case, try to work out if we have either 
    of TE or TM, ie, not both*/
  int ksource_nz[4];
  for (int icomp=0;icomp<4;icomp++)
    ksource_nz[icomp]=0;
  //fprintf(stderr,"Pre 20\n");
  if(J_tot==0){
    
    
    for (int icomp=0;icomp<4;icomp++)
      for(i=0;i<(I_tot+1);i++){
	ksource_nz[icomp] = ksource_nz[icomp] || (fabs(KsourceI[0][i-((int)I0[0])][icomp])>1.0e-15 )|| (fabs(KsourceR[0][i-((int)I0[0])][icomp])>1.0e-15 );
      }
       
    //for (int icomp=0;icomp<4;icomp++)
    //	fprintf(stderr,"ksource_nz[%d] = %d\n",icomp,ksource_nz[icomp]);
    
  }
  //fprintf(stderr,"Pre 21\n");
  /*
    In the J_tot==0 2D version, the 'TE' case involves components Ey, Hx and Hz
    'TM' case involves components Ex, Ez and Hy

    Ey and Hx receive an input from the source condition only if ksource_nz[2] or ksource_nz[1]
    are non-zero. 

    Ex and Hy receive an input from the source condition only if ksource_nz[3] or ksource_nz[0]
    are non-zero. 

    The idea is to use an alternative upper limit to the loop over j when we have J_tot==0. We currently have the followingloops 
    in place for each of the component updates.

    From the analysis below, we see that in all cases, the TE update has the following loop on j:
    for(j=0;j<max(J_tot,1);j++)
    whilst the TM case has:
    for(j=0;j<(J_tot+1);j++)
    
    So we can create variables
    J_tot_p1_bound     
    and
    J_tot_bound 

    which would take the following values
    3D:
    J_tot_p1_bound = J_tot + 1;
    J_tot_bound = J_tot;

    2D:
    TE:
    J_tot_bound = 1;
    Not TE:
    J_tot_bound = 0;

    TM:
    J_tot_p1_bound = 1;
    Not TM:
    J_tot_p1_bound = 0;

    Exy: Not involved in 2D
    for(k=0;k<(K_tot+1);k++)
    for(j=1;j<J_tot;j++)
    for(i=0;i<I_tot;i++){

    Exz: TM
    for(k=1;k<K_tot;k++)
    for(j=0;j<(J_tot+1);j++)
    for(i=0;i<I_tot;i++){

    Eyx: TE
    for(k=0;k<(K_tot+1);k++)
    for(j=0;j<max(J_tot,1);j++)
    for(i=1;i<I_tot;i++){
    
    Eyz: TE
    for(k=1;k<K_tot;k++)
    for(j=0;j<max(J_tot,1);j++)
    for(i=0;i<(I_tot+1);i++){

    Ezx: TM
    for(k=0;k<K_tot;k++)
    for(j=0;j<(J_tot+1);j++)
    for(i=1;i<I_tot;i++){

    Ezy: Not involved in 2D
    for(k=0;k<K_tot;k++)
    for(j=1;j<J_tot;j++)
    for(i=0;i<(I_tot+1);i++){

    Hxy:  Not involved in 2D
    for(k=0;k<K_tot;k++)
    for(j=0;j<J_tot;j++)
    for(i=0;i<(I_tot+1);i++)

    Hxz: TE
    for(k=0;k<K_tot;k++)
    for(j=0;j<max(J_tot,1);j++)
    for(i=0;i<(I_tot+1);i++){
    
    Hyx: TM
    for(k=0;k<K_tot;k++)
    for(j=0;j<(J_tot+1);j++)
    for(i=0;i<I_tot;i++){

    Hyz: TM
    for(k=0;k<K_tot;k++){
    for(j=0;j<(J_tot+1);j++)
    for(i=0;i<I_tot;i++){

    Hzx: TE
    for(k=0;k<(K_tot+1);k++)
    for(j=0;j<max(J_tot,1);j++)
    for(i=0;i<I_tot;i++){

    Hzy: Not involved in 2D
    for(k=0;k<(K_tot+1);k++)
    for(j=0;j<J_tot;j++)
    for(i=0;i<I_tot;i++){
  */    
  //implement the above
  int J_tot_bound = J_tot;
  int J_tot_p1_bound = J_tot+1;
  if(J_tot==0){
    //TE case
    if( ksource_nz[2] || ksource_nz[1] || eyi_present )
      J_tot_bound = 1;
    else
      J_tot_bound = 0;

    //TM case
    if( ksource_nz[3] || ksource_nz[0] || exi_present)
      J_tot_p1_bound = 1;
    else
      J_tot_p1_bound = 0;
  }
  //fprintf(stderr,"Pre 22a\n");
  //fprintf(stderr,"Pre 22, %d\n",strcmp(tdfdirstr,""));
  /*setup time domain field export matrices*/
  if( strcmp(tdfdirstr,"") ){
    ndims = 2;
    dims[0] = Ni_tdf;
    dims[1] = Nk_tdf;
    ex_tdf_array = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
    ex_tdf = castMatlab2DArray( mxGetPr( (mxArray *)ex_tdf_array), dims[0], dims[1] );
  }
  
//fprintf(stderr,"Pre 23\n");
    
  /*Start of FDTD iteration*/
  //open a file for logging the times
  /*The times of the E and H fields at the point where update equations are applied. 
    time_H is actually the time of the H field when the E field consistency update is 
    applied and vice versa. time_E > time_H below since after the E field consistency 
    update the E field will have advanced one time step.

    The interpretation of time is slightly complicated in the following. In what follows 
    I write (tind*dt,(tind+1/2)*dt) to mean the time at which we currently know the 
    electric (tind*dt) and magnetic ( (tind+1/2)*dt ) fields. 
  
    Times before                Operation         Times after    
    (tind*dt,(tind+1/2)*dt)     Extract phasors   (tind*dt,(tind+1/2)*dt)
    (tind*dt,(tind+1/2)*dt)     E field update    ( (tind+1)*dt,(tind+1/2)*dt)
    ((tind+1)*dt,(tind+1/2)*dt) H field update    ( (tind+1)*dt,(tind+3/2)*dt)
    ((tind+1)*dt,(tind+3/2)*dt) Normalisation extraction
   
    We note that the extractPhasorENorm uses (tind+1)*dt and extractPhasorHNorm uses 
    (tind+1/2)*dt to perform the update equation in the DFT. This seems incorrect
    at first but we note that they take the terms fte and fth as inputs respectively. 
    When one notes that fte is calculated using time_E and fth using time_H we see 
    that this indexing is correct, ie, time_E = (tind+1)*dt and time_H = (tind+1/2)*dt.
  */
//fprintf(stderr,"Pre 24\n");
  double time_E;
  double time_H;
  t0 = (double)time(NULL);
  //    fprintf(stderr,"start_tind: %d\n",start_tind);
  //  fprintf(stdout,"dz: %e, c: %e, dz/c: %e\n",dz,light_v,dz/light_v);
  //Begin of main iteration loop 
  if(TIME_MAIN_LOOP)
    time_ml_0 = omp_get_wtime();
  for(tind = start_tind; tind < *Nt; tind++){
    //fprintf(stderr,"Pos 00:\n");
    time_E = ( (double)(tind + 1) )*dt[0];
    time_H = time_E - *dt/2.;
    //Extract phasors
    time_0=omp_get_wtime();
    if((dft_counter == Nsteps)&&(runmode==rm_complete)&&(sourcemode==sm_steadystate) && exphasorsvolume){//runmode=complete,sourcemode=steadystate
      dft_counter = 0;
      double tol = checkPhasorConvergence(ExR,ExI,EyR,EyI,EzR,EzI,
					  ExR2,ExI2,EyR2,EyI2,EzR2,EzI2,
					  Exy,Exz,Eyx,Eyz,Ezx,Ezy,
					  pind_il,pind_iu,
					  pind_jl,pind_ju,
					  pind_kl,pind_ku,
					  tind,*omega_an, *dt, *Nt);
      
      
      //      mexPrintf("tol: %.5e \n",tol);
      fprintf(stderr,"tol: %.5e \n",tol);
      
      if( tol<TOL )
	break;//required accuracy obtained
      
      copyPhasors(mxGetPr( (mxArray *)plhs[0]),mxGetPi( (mxArray *)plhs[0]),mxGetPr( (mxArray *)plhs[1]),mxGetPi( (mxArray *)plhs[1]),mxGetPr( (mxArray *)plhs[2]),mxGetPi( (mxArray *)plhs[2]),
		  mxGetPr( (mxArray *)dummy_array[0]),mxGetPi( (mxArray *)dummy_array[0]),mxGetPr( (mxArray *)dummy_array[1]),mxGetPi( (mxArray *)dummy_array[1]),
		  mxGetPr( (mxArray *)dummy_array[2]),mxGetPi( (mxArray *)dummy_array[2]),  (int)mxGetNumberOfElements( (mxArray *)plhs[0]));
      
      //clean the phasors
      dims[0] = pind_iu - pind_il + 1;
      dims[1] = pind_ju - pind_jl + 1;
      dims[2] = pind_ku - pind_kl + 1;
      
      /*
	dims[0] = I_tot - *Dxu - *Dxl - 3 + 1;
	dims[1] = J_tot - *Dyu - *Dyl - 3 + 1;
	dims[2] = K_tot - *Dzu - *Dzl - 3 + 1;
      */
      
      initialiseDouble3DArray(ExR, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(ExI, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(EyR, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(EyI, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(EzR, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(EzI, dims[0], dims[1], dims[2]);

      initialiseDouble3DArray(HxR, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(HxI, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(HyR, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(HyI, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(HzR, dims[0], dims[1], dims[2]);
      initialiseDouble3DArray(HzI, dims[0], dims[1], dims[2]);

      if( exphasorssurface ){
	initialiseDouble3DArray(surface_EHr, n_surface_vertices, 6, N_f_ex_vec);
	initialiseDouble3DArray(surface_EHi, n_surface_vertices, 6, N_f_ex_vec);
      }
      //cleanphasors
    }
    //fprintf(stderr,"Pos 01:\n");

    if((sourcemode==sm_steadystate)&&(runmode==rm_complete) && exphasorsvolume){
      
      extractPhasorsVolume( ExR,ExI,EyR,EyI,EzR,EzI,
			    Exy,Exz,Eyx,Eyz,Ezx,Ezy,
			    pind_il,pind_iu,
			    pind_jl,pind_ju,
			    pind_kl,pind_ku,
			    dft_counter,*omega_an, *dt, Nsteps);
      extractPhasorsVolumeH(HxR,HxI,HyR,HyI,HzR,HzI,
			    Hxy,Hxz,Hyx,Hyz,Hzx,Hzy,
			    pind_il,pind_iu,
			    pind_jl,pind_ju,
			    pind_kl,pind_ku,
			    dft_counter,*omega_an, *dt, Nsteps);
      
      if( exphasorssurface ){
	if(intphasorssurface){
	  for(int ifx=0;ifx<N_f_ex_vec;ifx++)
	    extractPhasorsSurface( surface_EHr[ifx], surface_EHi[ifx],  
				   Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
				   Exy, Exz, Eyx, Eyz, Ezx, Ezy,
				   surface_vertices, n_surface_vertices, dft_counter, f_ex_vec[ifx]*2*dcpi, *dt, Nsteps, dimension,J_tot,intmethod );
	  dft_counter++;
	}
	else{
	  for(int ifx=0;ifx<N_f_ex_vec;ifx++)
	    extractPhasorsSurfaceNoInterpolation( surface_EHr[ifx], surface_EHi[ifx],  
						  Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
						  Exy, Exz, Eyx, Eyz, Ezx, Ezy,
						  surface_vertices, n_surface_vertices, dft_counter, f_ex_vec[ifx]*2*dcpi, *dt, Nsteps, dimension,J_tot );
	  dft_counter++;
	}
      }

    }
    else if((sourcemode==sm_pulsed)&&(runmode==rm_complete) && exphasorsvolume){
      if(TIME_EXEC){
	time_1=omp_get_wtime();
	secs= time_1-time_0;
	fprintf(stdout,"%.03e ",secs);
	time_0=time_1;
      }
      /*
	for(k=0;k<(K_tot+1);k++){
	fprintf(stdout,"%e %e %e %e %e %e %e %e %e %e %e %e ",Exy[k][13][13],Exz[k][13][13],Eyx[k][13][13],Eyz[k][13][13],Ezx[k][13][13],Ezy[k][13][13],Hxy[k][13][13],Hxz[k][13][13],Hyx[k][13][13],Hyz[k][13][13],Hzx[k][13][13],Hzy[k][13][13]);
	}
	fprintf(stdout,"\n");
      */
      /*for(k=0;k<(K_tot+1);k++)
	fprintf(stdout,"%e ",Exy[k][13][13]+Exz[k][13][13]);
	fprintf(stdout,"\n");*/

      if( (tind-start_tind) % Np == 0){
	extractPhasorsVolume(ExR,ExI,EyR,EyI,EzR,EzI,
			     Exy,Exz,Eyx,Eyz,Ezx,Ezy,
			     pind_il,pind_iu,
			     pind_jl,pind_ju,
			     pind_kl,pind_ku,
			     tind,*omega_an, *dt, Npe);
	//fprintf(stderr,"Pos 01a:\n");
	extractPhasorsVolumeH(HxR,HxI,HyR,HyI,HzR,HzI,
			      Hxy,Hxz,Hyx,Hyz,Hzx,Hzy,
			      pind_il,pind_iu,
			      pind_jl,pind_ju,
			      pind_kl,pind_ku,
			      tind,*omega_an, *dt, Npe);
      }
      if(TIME_EXEC){
	time_1=omp_get_wtime();
	secs= time_1-time_0;
	fprintf(stdout,"%.03e ",secs);
	time_0=time_1;
      }
      //fprintf(stderr,"Pos 01b:\n");
    }
    /*extract fieldsample*/
    if(!((N_fieldsample_i == 0) || (N_fieldsample_j == 0) || (N_fieldsample_k == 0) || (N_fieldsample_n == 0))){
      //if( (tind-start_tind) % Np == 0){
      if(1){
#pragma omp parallel default(shared)  private(Ex_temp,Ey_temp,Ez_temp)
	{

#pragma omp for
	  for(int kt=0; kt< N_fieldsample_k; kt++)
	    for(int jt=0; jt< N_fieldsample_j ; jt++)
	      for(int it=0; it< N_fieldsample_i; it++){
		////fprintf(stderr,"Pos fs 1\n");
		interpolateTimeDomainFieldCentralEBandLimited( Exy, Exz, Eyx, Eyz, Ezx, Ezy, (int)fieldsample_i[it] + Dxl[0] - 1, (int)fieldsample_j[jt] + Dyl[0] - 1,(int)fieldsample_k[kt] + Dzl[0] - 1, &Ex_temp, &Ey_temp, &Ez_temp);
		//fprintf(stderr,"Pos fs 2\n");
		for(int nt=0; nt< N_fieldsample_n; nt++)
		  fieldsample[nt][kt][jt][it] = fieldsample[nt][kt][jt][it] + pow( Ex_temp*Ex_temp + Ey_temp*Ey_temp + Ez_temp*Ez_temp, fieldsample_n[nt]/2.)/Nt[0];
		//fprintf(stderr,"%d %d %d %d -> %d %d %d (%d) %d [%d %d]\n",nt,kt,jt,it,(int)fieldsample_n[nt], (int)fieldsample_i[it] + Dxl[0] - 1, (int)fieldsample_j[jt] + Dyl[0] - 1, Dyl[0],(int)fieldsample_k[kt] + Dzl[0] - 1 , Nsteps, (int)fieldsample_n[nt] - 2);

		/*
		fieldsample_x[nt][kt][jt][it] = fieldsample_x[nt][kt][jt][it] + pow( Exz[ (int)fieldsample_k[kt] + Dzl[0] - 1 ][ (int)fieldsample_j[jt] + Dyl[0] - 1][ (int)fieldsample_i[it] + Dxl[0] - 1] + Exy[ (int)fieldsample_k[kt] + Dzl[0] - 1][ (int)fieldsample_j[jt] + Dyl[0] - 1][ (int)fieldsample_i[it] + Dxl[0] - 1],  (int)fieldsample_n[nt])/Nsteps;

		fieldsample_y[nt][kt][jt][it] = fieldsample_y[nt][kt][jt][it] + pow( Eyx[ (int)fieldsample_k[kt] + Dzl[0] - 1 ][ (int)fieldsample_j[jt] + Dyl[0] - 1][ (int)fieldsample_i[it] + Dxl[0] - 1] + Eyz[ (int)fieldsample_k[kt] + Dzl[0] - 1][ (int)fieldsample_j[jt] + Dyl[0] - 1][ (int)fieldsample_i[it] + Dxl[0] - 1],  (int)fieldsample_n[nt])/Nsteps;

		fieldsample_z[nt][kt][jt][it] = fieldsample_z[nt][kt][jt][it] + pow( Ezx[ (int)fieldsample_k[kt] + Dzl[0] - 1 ][ (int)fieldsample_j[jt] + Dyl[0] - 1][ (int)fieldsample_i[it] + Dxl[0] - 1] + Ezy[ (int)fieldsample_k[kt] + Dzl[0] - 1][ (int)fieldsample_j[jt] + Dyl[0] - 1][ (int)fieldsample_i[it] + Dxl[0] - 1],  (int)fieldsample_n[nt])/Nsteps;
		*/
	      }
	}
      }
    }
    
    /*end extract fieldsample*/
    
    //fprintf(stderr,"Pos 02:\n");
    if( sourcemode==sm_pulsed && runmode==rm_complete && exphasorssurface ){
      if( (tind-start_tind) % Np == 0 ){
	if(intphasorssurface)
	  for(int ifx=0;ifx<N_f_ex_vec;ifx++)
	    extractPhasorsSurface( surface_EHr[ifx], surface_EHi[ifx],  
				   Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
				   Exy, Exz, Eyx, Eyz, Ezx, Ezy,
				   surface_vertices, n_surface_vertices, tind, 
				   f_ex_vec[ifx]*2*dcpi, *dt, Npe, dimension,J_tot,intmethod );
	else
	  for(int ifx=0;ifx<N_f_ex_vec;ifx++)
	    extractPhasorsSurfaceNoInterpolation( surface_EHr[ifx], surface_EHi[ifx],  
				   Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
				   Exy, Exz, Eyx, Eyz, Ezx, Ezy,
				   surface_vertices, n_surface_vertices, tind, 
				   f_ex_vec[ifx]*2*dcpi, *dt, Npe, dimension,J_tot );

	  
      }
    }

   if( sourcemode==sm_pulsed && runmode==rm_complete && (nvertices > 0) ){
     //     fprintf(stderr,"loc 01 (%d,%d,%d)\n",tind,start_tind,Np);
      if( (tind-start_tind) % Np == 0 ){
	//	fprintf(stderr,"loc 02\n");
	if( nvertices > 0){
	  //fprintf(stderr,"loc 03\n");
	  //	  fprintf(stderr,"EPV 01\n");
	  for(int ifx=0;ifx<N_f_ex_vec;ifx++)
	    extractPhasorsVertices( camplitudesR[ifx], camplitudesI[ifx],  
				    Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
				    Exy, Exz, Eyx, Eyz, Ezx, Ezy,
				    vertices, nvertices, components, ncomponents, tind, 
				    f_ex_vec[ifx]*2*dcpi, *dt, Npe, dimension,J_tot,intmethod );
	}
      }
    }

    
    //fprintf(stderr,"Pos 02a:\n");
    if( sourcemode==sm_pulsed && runmode==rm_complete && exdetintegral ){
      if( (tind-start_tind) % Np == 0 ){
	//First need to sum up the Ex and Ey values on a plane ready for FFT, remember that Ex_t and Ey_t are in row-major format whilst Exy etc. are in column major format
	for(j=*Dyl;j<(J_tot-*Dyu);j++)
	  for(i=*Dxl;i<(I_tot-*Dxu);i++){
	    Ex_t[j-*Dyl+(i-*Dxl)*(J_tot-*Dyu-*Dyl)][0] = Exy[k_det_obs_global][j][i] + Exz[k_det_obs_global][j][i];Ex_t[j-*Dyl+(i-*Dxl)*(J_tot-*Dyu-*Dyl)][1]=0.;
	    Ey_t[j-*Dyl+(i-*Dxl)*(J_tot-*Dyu-*Dyl)][0] = Eyx[k_det_obs_global][j][i] + Eyz[k_det_obs_global][j][i];Ey_t[j-*Dyl+(i-*Dxl)*(J_tot-*Dyu-*Dyl)][1]=0.;
	  }
	//fprintf(stderr,"Pos 02a [1] (%d,%d,%d,%d):\n",*Dyl,J_tot-*Dyu,*Dxl,I_tot-*Dxu);
	fftw_execute(pex_t);fftw_execute(pey_t);
	//fprintf(stderr,"Pos 02a [2]:\n");
	//Iterate over each mode
	for(int im=0;im<Ndetmodes;im++){
	  //Now go back to column-major
	  for(j=0;j<(J_tot-*Dyu-*Dyl);j++)
	    for(i=0;i<(I_tot-*Dxu-*Dxl);i++){
	      Ex_t_cm[j][i] = Ex_t[j+i*(J_tot-*Dyu-*Dyl)][0] + I*Ex_t[j+i*(J_tot-*Dyu-*Dyl)][1];
	      Ey_t_cm[j][i] = Ey_t[j+i*(J_tot-*Dyu-*Dyl)][0] + I*Ey_t[j+i*(J_tot-*Dyu-*Dyl)][1];
	    }
	  //fprintf(stderr,"Pos 02a [3]:\n");
	  //Now multiply the pupil, mostly the pupil is non-zero in only a elements
	  for(j=0;j<(J_tot-*Dyu-*Dyl);j++)
	    for(i=0;i<(I_tot-*Dxu-*Dxl);i++){
	      Ex_t_cm[j][i] = Ex_t_cm[j][i]*Pupil[j][i]*Dx_tilde[j][i][im];
	      Ey_t_cm[j][i] = Ey_t_cm[j][i]*Pupil[j][i]*Dy_tilde[j][i][im];
	    }
	  //fprintf(stderr,"Pos 02a [4]:\n");
	  //now iterate over each frequency to extract phasors at
#pragma omp parallel default(shared)  private(lambda_an_t,Idxt,Idyt,i,j,kprop,phaseTermE,cphaseTermE)
	  {
#pragma omp for
	    for(int ifx=0;ifx<N_f_ex_vec;ifx++){
	      //wavelength in air
	      lambda_an_t = light_v/f_ex_vec[ifx];
	      //fprintf(stdout,"lambda_an_t = %e, light_v = %e, z_obs = %e\n",lambda_an_t,light_v,z_obs);
	      Idxt=0.;Idyt=0.;
	  
	      //now loop over all angular frequencies
	      for(j=0;j<(J_tot-*Dyu-*Dyl);j++)
		for(i=0;i<(I_tot-*Dxu-*Dxl);i++){
		  if( (lambda_an_t*fx_vec[i]*lambda_an_t*fx_vec[i]+lambda_an_t*fy_vec[j]*lambda_an_t*fy_vec[j]) < 1){

		    if(!air_interface_present){
		      /*This had to be fixed since we must take into account the refractive index of the medium.

		       */
		      kprop = exp(I*z_obs*2.*dcpi/lambda_an_t*refind*sqrt(1. - pow(lambda_an_t*fx_vec[i]/refind,2.) - pow(lambda_an_t*fy_vec[j]/refind,2.) ));
		      //fprintf(stdout,"%d %d %e %e %e %e %e %e %e\n",i,j,fx_vec[i],fy_vec[j],real(kprop),imag(kprop),z_obs,dcpi,lambda_an_t);
		    }
		    else{
		      kprop = exp(I*(-air_interface+z_obs)*2.*dcpi/lambda_an_t*refind*sqrt(1.-pow(lambda_an_t*fx_vec[i]/refind,2.)-pow(lambda_an_t*fy_vec[j]/refind,2.)))*exp(I*air_interface*2.*dcpi/lambda_an_t*sqrt(1.-pow(lambda_an_t*fx_vec[i],2.) - pow(lambda_an_t*fy_vec[j],2.)));
		    }
		  }
		  else
		    kprop = 0.;

		  Idxt+=Ex_t_cm[j][i]*kprop;
		  Idyt+=Ey_t_cm[j][i]*kprop;
		}
	      phaseTermE = fmod(f_ex_vec[ifx]*2.*dcpi*((double) tind)*dt[0], 2*dcpi);
	      cphaseTermE = exp(phaseTermE * I) * 1./((double) Npe);

	      Idx[ifx][im]+=Idxt*cphaseTermE;
	      Idy[ifx][im]+=Idyt*cphaseTermE;
	  
	    }//end of loop on frequencies
	  }//end of pragma omp parallel
	}//end of loop over each mode
      }
    }//end of section for calculating detector function

    //fprintf(stderr,"Pos 02b:\n");
    if(runmode==rm_complete)
      if(dimension==THREE){
	extractPhasorsPlane( iwave_lEx_Rbs, iwave_lEx_Ibs, iwave_lEy_Rbs, iwave_lEy_Ibs, 
			     iwave_lHx_Rbs, iwave_lHx_Ibs, iwave_lHy_Rbs, iwave_lHy_Ibs, 
			     Exz, Eyz, Hxz, Hyz,
			     Exy, Eyx, Hxy, Hyx,
			     I_tot, J_tot, ((int)*K0) + 1, tind, *omega_an, *dt, *Nt);//extract the phasors just above the line
      }
    //fprintf(stderr,"Pos 02c:\n");
   
    //Update equations for the E field

    /*There are two options for determing the update coefficients for the FDTD cell:

      1) If cell (i,j,k) is either free space or PML:
         
      materials[k][j][i] will be set to 0. In this case the update parameter used will 
      be given by Cay[j], Cby[j] etc depending on which update equation is being implemented.

      2) if cell (i,j,k) is composed of a scattering type material then materials[k][j][i] will be 
      non-zero and will be an index into Cmaterial_Cay and Cmaterial_Cby etc depending on which 
      update equation is being implemented.

    */

    int array_ind;
    //fprintf(stderr,"I_tot=%d, J_tot=%d, K_tot=%d\n",I_tot,J_tot,K_tot);
    if(TIME_EXEC){
      time_1=omp_get_wtime();
      secs= time_1-time_0;
      fprintf(stdout,"%.03e ",secs);
      time_0=time_1;
    }
    //fprintf(stderr,"Dimension = %d\n",dimension);
    /*
      for(k=0;k<(K_tot+1);k++)
      fprintf(stdout,"%e ",Exy[k][13][13]+Exz[k][13][13]);
      fprintf(stdout,"\n");
    */
#pragma omp parallel default(shared)  private(i,j,k,rho,k_loc,array_ind,Ca,Cb,Cc,alpha_l,beta_l,gamma_l,kappa_l,sigma_l,Enp1,Jnp1)//,ca_vec,cb_vec,cc_vec,eh_vec)
    {
      if(dimension==THREE || dimension==TE){
	if( useCD ){//FDTD, Exy
#pragma omp for
	  for(k=0;k<(K_tot+1);k++)
	    for(j=1;j<J_tot;j++)
	      for(i=0;i<I_tot;i++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = j;
		else
		  array_ind = (J_tot+1)*k_loc+j;

		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k][j][i+1]){
		  //fprintf(stdout,"(%d,%d,%d,%d)\n",i,j,k,tind);
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Cay[array_ind];
		    Cb = Cby[array_ind];
		    if(is_disp_ml)
		      Cc = Ccy[array_ind];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Cay[materials[k][j][i]-1];
		    Cb = Cmaterial_Cby[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccy[materials[k][j][i]-1];
		  }
	    
		  if(intmatprops){
		    if( !materials[k][j][i+1] ){
		      Ca = Ca + Cay[array_ind];
		      Cb = Cb + Cby[array_ind];
		      if(is_disp_ml)
			Cc = Cc + Ccy[array_ind];
		    }
		    else{
		      Ca = Ca + Cmaterial_Cay[materials[k][j][i+1]-1];
		      Cb = Cb + Cmaterial_Cby[materials[k][j][i+1]-1];
		      Cc = Cc + Cmaterial_Ccy[materials[k][j][i+1]-1];
		    }
		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Cay[array_ind];
		  Cb = Cby[array_ind];
		  if(is_disp_ml)
		    Cc = Ccy[array_ind];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_y[array_ind];
		}
	    
		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_y[array_ind];
		  kappa_l = ml_kappa_y[array_ind];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k][j][i+1]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k][j][i+1]){
		      alpha_l += alpha[materials[k][j][i+1]-1];
		      beta_l +=  beta[materials[k][j][i+1]-1];
		      gamma_l += gamma[materials[k][j][i+1]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];

		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}


		Enp1 = Ca*Exy[k][j][i]+Cb*(Hzy[k][j][i] + Hzx[k][j][i] - Hzy[k][j-1][i] - Hzx[k][j-1][i]);
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Exy_nm1[k][j][i] - 1./2.*Cb*dy*((1+alpha_l)*Jxy[k][j][i] + beta_l*Jxy_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dy*Jcxy[k][j][i];
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jxy[k][j][i] + beta_l*Jxy_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Exy_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Exy[k][j][i];
	  
		  Exy_nm1[k][j][i] = Exy[k][j][i];
		  Jxy_nm1[k][j][i] = Jxy[k][j][i];
		  Jxy[k][j][i]     = Jnp1;
	   
		  //	    fprintf(stderr,"(%d,%d,%d): %e\n",i,j,k,Jxy[k][j][i]);
		}

		if(is_cond && rho){
		  Jcxy[k][j][i] -= rho*(Enp1 + Exy[k][j][i]);
		}
	  
		Exy[k][j][i] = Enp1;
	      }
	}//FDTD, Exy
	else{//PSTD, Exy
//fprintf(stderr,"Pos 02d:\n");
#pragma omp for
	  for(k=0;k<(K_tot+1);k++)
	    for(i=0;i<I_tot;i++){
	      for(j=1;j<J_tot;j++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = j;
		else
		  array_ind = (J_tot+1)*k_loc+j;

		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k][j][i+1]){
		  //fprintf(stdout,"(%d,%d,%d,%d)\n",i,j,k,tind);
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Cay[array_ind];
		    Cb = Cby[array_ind];
		    if(is_disp_ml)
		      Cc = Ccy[array_ind];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Cay[materials[k][j][i]-1];
		    Cb = Cmaterial_Cby[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccy[materials[k][j][i]-1];
		  }

		  if(intmatprops){
		    if( !materials[k][j][i+1] ){
		      Ca = Ca + Cay[array_ind];
		      Cb = Cb + Cby[array_ind];
		      if(is_disp_ml)
			Cc = Cc + Ccy[array_ind];
		    }
		    else{
		      Ca = Ca + Cmaterial_Cay[materials[k][j][i+1]-1];
		      Cb = Cb + Cmaterial_Cby[materials[k][j][i+1]-1];
		      Cc = Cc + Cmaterial_Ccy[materials[k][j][i+1]-1];
		    }
		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Cay[array_ind];
		  Cb = Cby[array_ind];
		  if(is_disp_ml)
		    Cc = Ccy[array_ind];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_y[array_ind];
		}
	    
		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_y[array_ind];
		  kappa_l = ml_kappa_y[array_ind];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k][j][i+1]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k][j][i+1]){
		      alpha_l += alpha[materials[k][j][i+1]-1];
		      beta_l +=  beta[materials[k][j][i+1]-1];
		      gamma_l += gamma[materials[k][j][i+1]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];

		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}


		//Enp1 = Ca*Exy[k][j][i]+Cb*(Hzy[k][j][i] + Hzx[k][j][i] - Hzy[k][j-1][i] - Hzx[k][j-1][i]);
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Exy_nm1[k][j][i] - 1./2.*Cb*dy*((1+alpha_l)*Jxy[k][j][i] + beta_l*Jxy_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dy*Jcxy[k][j][i];
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jxy[k][j][i] + beta_l*Jxy_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Exy_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Exy[k][j][i];
	  
		  Exy_nm1[k][j][i] = Exy[k][j][i];
		  Jxy_nm1[k][j][i] = Jxy[k][j][i];
		  Jxy[k][j][i]     = Jnp1;
	   
		  //	    fprintf(stderr,"(%d,%d,%d): %e\n",i,j,k,Jxy[k][j][i]);
		}

		if(is_cond && rho){
		  Jcxy[k][j][i] -= rho*(Enp1 + Exy[k][j][i]);
		}
	  
		eh_vec[omp_get_thread_num()][j][0] = Hzy[k][j][i] + Hzx[k][j][i];eh_vec[omp_get_thread_num()][j][1] = 0.;
		ca_vec[omp_get_thread_num()][j-1] = Ca;
		cb_vec[omp_get_thread_num()][j-1] = Cb;
	      }
	      if(J_tot>1){
		j = 0;
		eh_vec[omp_get_thread_num()][j][0] = Hzy[k][j][i] + Hzx[k][j][i];eh_vec[omp_get_thread_num()][j][1] = 0.;
		first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				  dk_e_y, N_e_y , pf_exy[omp_get_thread_num()], pb_exy[omp_get_thread_num()]);

	      
		//fprintf(stdout,"(%d,%d) %d (of %d)\n",i,k,omp_get_thread_num(),omp_get_num_threads());

		for(j=1;j<J_tot;j++){
		  Exy[k][j][i] = ca_vec[omp_get_thread_num()][j-1]*Exy[k][j][i] + cb_vec[omp_get_thread_num()][j-1]*eh_vec[omp_get_thread_num()][j][0]/((double) N_e_y);
		}
	      }
	    }
	}//PSTD, Exy
      
	/*
	  if(is_disp){
	  i=36;
	  j=36;
	  k=36;

	  fprintf(stdout,"%e %e",Jxy[k][j][i],Exy[k][j][i]);
	  }
	*/
  
	//fprintf(stderr,"Pos 04:\n");
	//Exz updates
	if( useCD ){//FDTD, Exz
#pragma omp for
	  for(k=1;k<K_tot;k++)
	    for(j=0;j<J_tot_p1_bound;j++)
	      for(i=0;i<I_tot;i++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k][j][i+1]){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Caz[k_loc];
		    Cb = Cbz[k_loc];
		    if(is_disp_ml)
		      Cc = Ccz[k_loc];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Caz[materials[k][j][i]-1];
		    Cb = Cmaterial_Cbz[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccz[materials[k][j][i]-1];
		  }
	    
		  if(intmatprops){
		    if( !materials[k][j][i+1] ){
		      Ca = Ca + Caz[k_loc];
		      Cb = Cb + Cbz[k_loc];
		      if(is_disp_ml)
			Cc = Cc + Ccz[k_loc];
		    }
		    else{
		      Ca = Ca + Cmaterial_Caz[materials[k][j][i+1]-1];
		      Cb = Cb + Cmaterial_Cbz[materials[k][j][i+1]-1];
		      Cc = Cc + Cmaterial_Ccz[materials[k][j][i+1]-1];
		    }
		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Caz[k_loc];
		  Cb = Cbz[k_loc];
		  if(is_disp_ml)
		    Cc = Ccz[k_loc];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_z[k_loc];
		}
	    
		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_z[k_loc];
		  kappa_l = ml_kappa_z[k_loc];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k][j][i+1]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k][j][i+1]){
		      alpha_l += alpha[materials[k][j][i+1]-1];
		      beta_l +=  beta[materials[k][j][i+1]-1];
		      gamma_l += gamma[materials[k][j][i+1]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
		  
		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}
		/*if( materials[k][j][i] || materials[k][j][i+1])
		  fprintf(stdout,"(%d,%d,%d), Ca= %e, Cb=%e, is_cond:%d, rho: %e, is_disp: %d, is_disp_ml: %d\n",i,j,k,Ca,Cb,is_cond,rho,is_disp,is_disp_ml);
		  if(tind==0)
		  fprintf(stdout,"%d %d %e %e\n",i,k,Ca, Cb);*/
		Enp1 = Ca*Exz[k][j][i]+Cb*(Hyx[k-1][j][i] + Hyz[k-1][j][i] - Hyx[k][j][i] - Hyz[k][j][i]);
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Exz_nm1[k][j][i] - 1./2.*Cb*dz*((1+alpha_l)*Jxz[k][j][i] + beta_l*Jxz_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dz*Jcxz[k][j][i];
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jxz[k][j][i] + beta_l*Jxz_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Exz_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Exz[k][j][i];
		  Exz_nm1[k][j][i] = Exz[k][j][i];
		  Jxz_nm1[k][j][i] = Jxz[k][j][i];
		  Jxz[k][j][i]     = Jnp1;
	  
		}
	    
		if(is_cond && rho){
		  Jcxz[k][j][i] -= rho*(Enp1 + Exz[k][j][i]);
		}

		Exz[k][j][i] = Enp1;

	      }
	}//FDTD, Exz
	else{//PSTD, Exz
	  //#pragma omp for
	  for(j=0;j<J_tot_p1_bound;j++)
	    #pragma omp for
	    for(i=0;i<I_tot;i++){
	      for(k=1;k<K_tot;k++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k][j][i+1]){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Caz[k_loc];
		    Cb = Cbz[k_loc];
		    if(is_disp_ml)
		      Cc = Ccz[k_loc];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Caz[materials[k][j][i]-1];
		    Cb = Cmaterial_Cbz[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccz[materials[k][j][i]-1];
		  }
		  if(intmatprops){
		    if( !materials[k][j][i+1] ){
		      Ca = Ca + Caz[k_loc];
		      Cb = Cb + Cbz[k_loc];
		      if(is_disp_ml)
			Cc = Cc + Ccz[k_loc];
		    }
		    else{
		      Ca = Ca + Cmaterial_Caz[materials[k][j][i+1]-1];
		      Cb = Cb + Cmaterial_Cbz[materials[k][j][i+1]-1];
		      Cc = Cc + Cmaterial_Ccz[materials[k][j][i+1]-1];
		    }
		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Caz[k_loc];
		  Cb = Cbz[k_loc];
		  if(is_disp_ml)
		    Cc = Ccz[k_loc];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_z[k_loc];
		}
	    
		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_z[k_loc];
		  kappa_l = ml_kappa_z[k_loc];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k][j][i+1]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k][j][i+1]){
		      alpha_l += alpha[materials[k][j][i+1]-1];
		      beta_l +=  beta[materials[k][j][i+1]-1];
		      gamma_l += gamma[materials[k][j][i+1]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
		  
		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}
		/*if( materials[k][j][i] || materials[k][j][i+1])
		  fprintf(stdout,"(%d,%d,%d), Ca= %e, Cb=%e, is_cond:%d, rho: %e, is_disp: %d, is_disp_ml: %d\n",i,j,k,Ca,Cb,is_cond,rho,is_disp,is_disp_ml);
		  if(tind==0)
		  fprintf(stdout,"%d %d %e %e\n",i,k,Ca, Cb);*/
		//Enp1 = Ca*Exz[k][j][i]+Cb*(Hyx[k-1][j][i] + Hyz[k-1][j][i] - Hyx[k][j][i] - Hyz[k][j][i]);
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Exz_nm1[k][j][i] - 1./2.*Cb*dz*((1+alpha_l)*Jxz[k][j][i] + beta_l*Jxz_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dz*Jcxz[k][j][i];
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jxz[k][j][i] + beta_l*Jxz_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Exz_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Exz[k][j][i];
		  Exz_nm1[k][j][i] = Exz[k][j][i];
		  Jxz_nm1[k][j][i] = Jxz[k][j][i];
		  Jxz[k][j][i]     = Jnp1;
	  
		}
	    
		if(is_cond && rho){
		  Jcxz[k][j][i] -= rho*(Enp1 + Exz[k][j][i]);
		}
		
		eh_vec[omp_get_thread_num()][k][0] = Hyx[k][j][i] + Hyz[k][j][i];eh_vec[omp_get_thread_num()][k][1] = 0.;
		ca_vec[omp_get_thread_num()][k-1] = Ca;
		cb_vec[omp_get_thread_num()][k-1] = Cb;

	      }
	      k=0;
	      eh_vec[omp_get_thread_num()][k][0] = Hyx[k][j][i] + Hyz[k][j][i];eh_vec[omp_get_thread_num()][k][1] = 0.;
	      /*
		if (tind==1 & i==25 & j==25){
		for(k=0;k<N_e_z;k++)
		fprintf(stdout,"%e ",dk_e_z[k][0]);
		fprintf(stdout,"\n\n");
		for(k=0;k<N_e_z;k++)
		fprintf(stdout,"%e ",dk_e_z[k][1]);
		fprintf(stdout,"\n\n");
		for(k=0;k<N_e_z;k++)
		fprintf(stdout,"%e ",eh_vec[omp_get_thread_num()][k][0]);
		fprintf(stdout,"\n\n");
		for(k=0;k<N_e_z;k++)
		fprintf(stdout,"%e ",eh_vec[omp_get_thread_num()][k][1]);
		}
	      */
	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_e_z, N_e_z , pf_exz[omp_get_thread_num()], pb_exz[omp_get_thread_num()]);
	      /*
		if (tind==1 & i==25 & j==25){
		fprintf(stdout,"\n\n");
		for(k=0;k<N_e_z;k++)
		fprintf(stdout,"%e ",eh_vec[k][0]);
		fprintf(stdout,"\n\n");
		for(k=0;k<N_e_z;k++)
		fprintf(stdout,"%e ",eh_vec[k][1]);
		}
	      */
	      for(k=1;k<K_tot;k++){
		Exz[k][j][i] = ca_vec[omp_get_thread_num()][k-1]*Exz[k][j][i] - cb_vec[omp_get_thread_num()][k-1]*eh_vec[omp_get_thread_num()][k][0]/((double) N_e_z);
	      }
	      
	    }
	  
	}//PSTD, Exz
		
	//fprintf(stderr,"Pos 05:\n");
	//Eyx updates
	if( useCD ){//FDTD, Eyx
#pragma omp for			
	  for(k=0;k<(K_tot+1);k++)
	    for(j=0;j<J_tot_bound;j++)
	      for(i=1;i<I_tot;i++){
		rho = 0.;
		k_loc = k;
		if( is_structure ){
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		}
		if( !is_multilayer )
		  array_ind = i;
		else
		  array_ind = (I_tot+1)*k_loc+i;

		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k][min(J_tot,j+1)][i] ){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Cax[array_ind];
		    Cb = Cbx[array_ind];
		    if(is_disp_ml)
		      Cc = Ccx[array_ind];
		    else
		      Cc = 0;
		  }
		  else{
		    Ca = Cmaterial_Cax[materials[k][j][i]-1];
		    Cb = Cmaterial_Cbx[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccx[materials[k][j][i]-1];
		  }
		  if(intmatprops){
		    if( !materials[k][min(J_tot,j+1)][i] ){
		      Ca = Ca + Cax[array_ind];
		      Cb = Cb + Cbx[array_ind];
		      if(is_disp_ml)
			Cc = Cc + Ccx[array_ind];
		    }
		    else{
		      Ca = Ca + Cmaterial_Cax[materials[k][min(J_tot,j+1)][i]-1];
		      Cb = Cb + Cmaterial_Cbx[materials[k][min(J_tot,j+1)][i]-1];
		      Cc = Cc + Cmaterial_Ccx[materials[k][min(J_tot,j+1)][i]-1];
		    }

		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Cax[array_ind];
		  Cb = Cbx[array_ind];
		  if(is_disp_ml)
		    Cc = Ccx[array_ind];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_x[array_ind];
		}

		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_x[array_ind];
		  kappa_l = ml_kappa_x[array_ind];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k][min(J_tot,j+1)][i]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k][min(J_tot,j+1)][i]){
		      alpha_l += alpha[materials[k][min(J_tot,j+1)][i]-1];
		      beta_l +=  beta[materials[k][min(J_tot,j+1)][i]-1];
		      gamma_l += gamma[materials[k][min(J_tot,j+1)][i]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
		  
		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		
		  }
		}


		Enp1 = Ca*Eyx[k][j][i]+Cb*(Hzx[k][j][i-1] + Hzy[k][j][i-1] - Hzx[k][j][i] - Hzy[k][j][i]);
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Eyx_nm1[k][j][i] - 1./2.*Cb*dx*((1+alpha_l)*Jyx[k][j][i] + beta_l*Jyx_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dx*Jcyx[k][j][i];
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jyx[k][j][i] + beta_l*Jyx_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Eyx_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Eyx[k][j][i];
		  Eyx_nm1[k][j][i] = Eyx[k][j][i];
		  Jyx_nm1[k][j][i] = Jyx[k][j][i];
		  Jyx[k][j][i]     = Jnp1;
		}
		if(is_cond && rho){
		  Jcyx[k][j][i] -= rho*(Enp1 + Eyx[k][j][i]);
		}
	    
		Eyx[k][j][i] = Enp1;
	      }
	}//FDTD, Eyx
	else{//PSTD, Eyx
#pragma omp for
	  for(k=0;k<(K_tot+1);k++)
	    for(j=0;j<J_tot_bound;j++){
	      for(i=1;i<I_tot;i++){
		rho = 0.;
		k_loc = k;
		if( is_structure ){
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		}
		if( !is_multilayer )
		  array_ind = i;
		else
		  array_ind = (I_tot+1)*k_loc+i;

		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k][min(J_tot,j+1)][i] ){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Cax[array_ind];
		    Cb = Cbx[array_ind];
		    if(is_disp_ml)
		      Cc = Ccx[array_ind];
		    else
		      Cc = 0;
		  }
		  else{
		    Ca = Cmaterial_Cax[materials[k][j][i]-1];
		    Cb = Cmaterial_Cbx[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccx[materials[k][j][i]-1];
		  }
		  if(intmatprops){
		    if( !materials[k][min(J_tot,j+1)][i] ){
		      Ca = Ca + Cax[array_ind];
		      Cb = Cb + Cbx[array_ind];
		      if(is_disp_ml)
			Cc = Cc + Ccx[array_ind];
		    }
		    else{
		      Ca = Ca + Cmaterial_Cax[materials[k][min(J_tot,j+1)][i]-1];
		      Cb = Cb + Cmaterial_Cbx[materials[k][min(J_tot,j+1)][i]-1];
		      Cc = Cc + Cmaterial_Ccx[materials[k][min(J_tot,j+1)][i]-1];
		    }

		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Cax[array_ind];
		  Cb = Cbx[array_ind];
		  if(is_disp_ml)
		    Cc = Ccx[array_ind];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_x[array_ind];
		}

		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_x[array_ind];
		  kappa_l = ml_kappa_x[array_ind];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k][min(J_tot,j+1)][i]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k][min(J_tot,j+1)][i]){
		      alpha_l += alpha[materials[k][min(J_tot,j+1)][i]-1];
		      beta_l +=  beta[materials[k][min(J_tot,j+1)][i]-1];
		      gamma_l += gamma[materials[k][min(J_tot,j+1)][i]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
		  
		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		
		  }
		}


		//Enp1 = Ca*Eyx[k][j][i]+Cb*(Hzx[k][j][i-1] + Hzy[k][j][i-1] - Hzx[k][j][i] - Hzy[k][j][i]);
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Eyx_nm1[k][j][i] - 1./2.*Cb*dx*((1+alpha_l)*Jyx[k][j][i] + beta_l*Jyx_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dx*Jcyx[k][j][i];
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jyx[k][j][i] + beta_l*Jyx_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Eyx_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Eyx[k][j][i];
		  Eyx_nm1[k][j][i] = Eyx[k][j][i];
		  Jyx_nm1[k][j][i] = Jyx[k][j][i];
		  Jyx[k][j][i]     = Jnp1;
		}
		if(is_cond && rho){
		  Jcyx[k][j][i] -= rho*(Enp1 + Eyx[k][j][i]);
		}
	      
		eh_vec[omp_get_thread_num()][i][0] = Hzx[k][j][i] + Hzy[k][j][i];eh_vec[omp_get_thread_num()][i][1] = 0.;
		ca_vec[omp_get_thread_num()][i-1] = Ca;
		cb_vec[omp_get_thread_num()][i-1] = Cb;
			    
	      }
	      i=0;
	      eh_vec[omp_get_thread_num()][i][0] = Hzx[k][j][i] + Hzy[k][j][i];eh_vec[omp_get_thread_num()][i][1] = 0.;
	      
	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_e_x, N_e_x , pf_eyx[omp_get_thread_num()], pb_eyx[omp_get_thread_num()]);

	      for(i=1;i<I_tot;i++){
		Eyx[k][j][i] = ca_vec[omp_get_thread_num()][i-1]*Eyx[k][j][i] - cb_vec[omp_get_thread_num()][i-1]*eh_vec[omp_get_thread_num()][i][0]/((double) N_e_x);
		//Eyx[k][j][i] = Enp1;
	      }
	    }
	}//PSTD, Eyx

	//fprintf(stderr,"Pos 06:\n");
	//Eyz updates
	if( useCD ){//FDTD, Eyz
#pragma omp for
	  for(k=1;k<K_tot;k++)
	    for(j=0;j<J_tot_bound;j++)
	      for(i=0;i<(I_tot+1);i++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( materials[k][j][i] || materials[k][min(J_tot,j+1)][i] ){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Caz[k_loc];
		    Cb = Cbz[k_loc];
		    if(is_disp_ml)
		      Cc = Ccz[k_loc];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Caz[materials[k][j][i]-1];
		    Cb = Cmaterial_Cbz[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccz[materials[k][j][i]-1];
		  }
	    
		  if(intmatprops){
		    if( !materials[k][min(J_tot,j+1)][i] ){
		      Ca = Ca + Caz[k_loc];
		      Cb = Cb + Cbz[k_loc];
		      if(is_disp_ml)
			Cc = Cc + Ccz[k_loc];
		    }
		    else{
		      Ca = Ca + Cmaterial_Caz[materials[k][min(J_tot,j+1)][i]-1];
		      Cb = Cb + Cmaterial_Cbz[materials[k][min(J_tot,j+1)][i]-1];
		      Cc = Cc + Cmaterial_Ccz[materials[k][min(J_tot,j+1)][i]-1];
		    }
	  
		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Caz[k_loc];
		  Cb = Cbz[k_loc];
		  if(is_disp_ml)
		    Cc = Ccz[k_loc];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_z[k_loc];
		}

		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_z[k_loc];
		  kappa_l = ml_kappa_z[k_loc];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k][min(J_tot,j+1)][i]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k][min(J_tot,j+1)][i]){
		      alpha_l += alpha[materials[k][min(J_tot,j+1)][i]-1];
		      beta_l +=  beta[materials[k][min(J_tot,j+1)][i]-1];
		      gamma_l += gamma[materials[k][min(J_tot,j+1)][i]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
		  
		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}

		//fprintf(stderr,"[%d %d %d]Ca: %e, Cb: %e, Cc: %e, alpha: %e, beta: %e, gamme: %e\n",i,j,k,Ca,Cb,Cc,alpha_l,beta_l,gamma_l);
		Enp1 = Ca*Eyz[k][j][i]+Cb*(Hxy[k][j][i] + Hxz[k][j][i] - Hxy[k-1][j][i] - Hxz[k-1][j][i]); 
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Eyz_nm1[k][j][i] - 1./2.*Cb*dz*((1+alpha_l)*Jyz[k][j][i] + beta_l*Jyz_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dz*Jcyz[k][j][i];
    
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jyz[k][j][i] + beta_l*Jyz_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Eyz_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Eyz[k][j][i];
		  Eyz_nm1[k][j][i] = Eyz[k][j][i];
		  Jyz_nm1[k][j][i] = Jyz[k][j][i];
		  Jyz[k][j][i]     = Jnp1;
		  //	      if(i==40 && j==40 && k==40)
		  //		fprintf(outfile,"%e %e %e\n",Eyz_nm1[k][j][i],Jyz_nm1[k][j][i],Jyz[k][j][i]);
		  //	      fflush(outfile);
		}
		if(is_cond && rho){
		  Jcyz[k][j][i] -= rho*(Enp1 + Eyz[k][j][i]);
		}

		Eyz[k][j][i] = Enp1;
	  
	      }
	}//FDTD, Eyz
	else{//PSTD, Eyz
#pragma omp for
	  for(j=0;j<J_tot_bound;j++)
	    for(i=0;i<(I_tot+1);i++){
	      for(k=1;k<K_tot;k++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( materials[k][j][i] || materials[k][min(J_tot,j+1)][i] ){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Caz[k_loc];
		    Cb = Cbz[k_loc];
		    if(is_disp_ml)
		      Cc = Ccz[k_loc];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Caz[materials[k][j][i]-1];
		    Cb = Cmaterial_Cbz[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccz[materials[k][j][i]-1];
		  }
	    
		  if(intmatprops){
		    if( !materials[k][min(J_tot,j+1)][i] ){
		      Ca = Ca + Caz[k_loc];
		      Cb = Cb + Cbz[k_loc];
		      if(is_disp_ml)
			Cc = Cc + Ccz[k_loc];
		    }
		    else{
		      Ca = Ca + Cmaterial_Caz[materials[k][min(J_tot,j+1)][i]-1];
		      Cb = Cb + Cmaterial_Cbz[materials[k][min(J_tot,j+1)][i]-1];
		      Cc = Cc + Cmaterial_Ccz[materials[k][min(J_tot,j+1)][i]-1];
		    }
	  
		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Caz[k_loc];
		  Cb = Cbz[k_loc];
		  if(is_disp_ml)
		    Cc = Ccz[k_loc];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_z[k_loc];
		}

		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_z[k_loc];
		  kappa_l = ml_kappa_z[k_loc];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k][min(J_tot,j+1)][i]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k][min(J_tot,j+1)][i]){
		      alpha_l += alpha[materials[k][min(J_tot,j+1)][i]-1];
		      beta_l +=  beta[materials[k][min(J_tot,j+1)][i]-1];
		      gamma_l += gamma[materials[k][min(J_tot,j+1)][i]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
		  
		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}

		//fprintf(stderr,"[%d %d %d]Ca: %e, Cb: %e, Cc: %e, alpha: %e, beta: %e, gamme: %e\n",i,j,k,Ca,Cb,Cc,alpha_l,beta_l,gamma_l);
		//Enp1 = Ca*Eyz[k][j][i]+Cb*(Hxy[k][j][i] + Hxz[k][j][i] - Hxy[k-1][j][i] - Hxz[k-1][j][i]); 
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Eyz_nm1[k][j][i] - 1./2.*Cb*dz*((1+alpha_l)*Jyz[k][j][i] + beta_l*Jyz_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dz*Jcyz[k][j][i];
    
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jyz[k][j][i] + beta_l*Jyz_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Eyz_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Eyz[k][j][i];
		  Eyz_nm1[k][j][i] = Eyz[k][j][i];
		  Jyz_nm1[k][j][i] = Jyz[k][j][i];
		  Jyz[k][j][i]     = Jnp1;
		  //	      if(i==40 && j==40 && k==40)
		  //		fprintf(outfile,"%e %e %e\n",Eyz_nm1[k][j][i],Jyz_nm1[k][j][i],Jyz[k][j][i]);
		  //	      fflush(outfile);
		}
		if(is_cond && rho){
		  Jcyz[k][j][i] -= rho*(Enp1 + Eyz[k][j][i]);
		}

		eh_vec[omp_get_thread_num()][k][0] = Hxy[k][j][i] + Hxz[k][j][i];eh_vec[omp_get_thread_num()][k][1] = 0.;
		ca_vec[omp_get_thread_num()][k-1] = Ca;
		cb_vec[omp_get_thread_num()][k-1] = Cb;
	  
	      }
	      k = 0;
	      eh_vec[omp_get_thread_num()][k][0] = Hxy[k][j][i] + Hxz[k][j][i];eh_vec[omp_get_thread_num()][k][1] = 0.;
	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_e_z, N_e_z , pf_eyz[omp_get_thread_num()], pb_eyz[omp_get_thread_num()]);
	      

	      for(k=1;k<K_tot;k++){
		Eyz[k][j][i] = ca_vec[omp_get_thread_num()][k-1]*Eyz[k][j][i] + cb_vec[omp_get_thread_num()][k-1]*eh_vec[omp_get_thread_num()][k][0]/((double) N_e_z);
		//Eyz[k][j][i] = Enp1;
	      }
	    }
	}//PSTD, Eyz
      }//if(dimension==THREE || dimension==TE)

      //fprintf(stderr,"Pos 07:\n");
      if(dimension==THREE || dimension==TE){
	if( useCD ){//FDTD, Ezx
#pragma omp for
	  //Ezx updates
	  for(k=0;k<K_tot;k++)
	    for(j=0;j<J_tot_p1_bound;j++)
	      for(i=1;i<I_tot;i++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = i;
		else
		  array_ind = (I_tot+1)*k_loc+i;

		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k+1][j][i]){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Cax[array_ind];
		    Cb = Cbx[array_ind];
		    if(is_disp_ml)
		      Cc = Ccx[array_ind];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Cax[materials[k][j][i]-1];
		    Cb = Cmaterial_Cbx[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccx[materials[k][j][i]-1];
		  }

		  if(intmatprops){
		    if( !materials[k+1][j][i] ){
		      Ca = Ca + Cax[array_ind];
		      Cb = Cb + Cbx[array_ind];
		      if(is_disp_ml)
			Cc = Cc + Ccx[array_ind];
		    }
		    else{
		      Ca = Ca + Cmaterial_Cax[materials[k+1][j][i]-1];
		      Cb = Cb + Cmaterial_Cbx[materials[k+1][j][i]-1];
		      Cc = Cc + Cmaterial_Ccx[materials[k+1][j][i]-1];
		    }

		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Cax[array_ind];
		  Cb = Cbx[array_ind];
		  if(is_disp_ml)
		    Cc = Ccx[array_ind];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_x[array_ind];
		}

		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_x[array_ind];
		  kappa_l = ml_kappa_x[array_ind];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k+1][j][i]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k+1][j][i]){
		      alpha_l += alpha[materials[k+1][j][i]-1];
		      beta_l +=  beta[materials[k+1][j][i]-1];
		      gamma_l += gamma[materials[k+1][j][i]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
 
		    }
		
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}
	    
		/*if( materials[k][j][i] || materials[k][j][i+1])
		  fprintf(stdout,"(%d,%d,%d), Ca= %e, Cb=%e, is_cond:%d, rho: %e, is_disp: %d, is_disp_ml: %d\n",i,j,k,Ca,Cb,is_cond,rho,is_disp,is_disp_ml);*/
		Enp1 = Ca*Ezx[k][j][i]+Cb*(Hyx[k][j][i] + Hyz[k][j][i] - Hyx[k][j][i-1] - Hyz[k][j][i-1]);
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Ezx_nm1[k][j][i] - 1./2.*Cb*dx*((1+alpha_l)*Jzx[k][j][i] + beta_l*Jzx_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dx*Jczx[k][j][i];
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jzx[k][j][i] + beta_l*Jzx_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Ezx_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Ezx[k][j][i];
		  Ezx_nm1[k][j][i] = Ezx[k][j][i];
		  Jzx_nm1[k][j][i] = Jzx[k][j][i];
		  Jzx[k][j][i]     = Jnp1;
		}
		if(is_cond && rho){
		  Jczx[k][j][i] -= rho*(Enp1 + Ezx[k][j][i]);
		}

		Ezx[k][j][i] = Enp1;
	      }
	}//FDTD, Ezx
	else{//PSTD, Ezx
#pragma omp for
	  for(k=0;k<K_tot;k++)
	    for(j=0;j<J_tot_p1_bound;j++){
	      for(i=1;i<I_tot;i++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = i;
		else
		  array_ind = (I_tot+1)*k_loc+i;

		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k+1][j][i]){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Cax[array_ind];
		    Cb = Cbx[array_ind];
		    if(is_disp_ml)
		      Cc = Ccx[array_ind];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Cax[materials[k][j][i]-1];
		    Cb = Cmaterial_Cbx[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccx[materials[k][j][i]-1];
		  }
	    
	  	  if(intmatprops){
		    if( !materials[k+1][j][i] ){
		      Ca = Ca + Cax[array_ind];
		      Cb = Cb + Cbx[array_ind];
		      if(is_disp_ml)
			Cc = Cc + Ccx[array_ind];
		    }
		    else{
		      Ca = Ca + Cmaterial_Cax[materials[k+1][j][i]-1];
		      Cb = Cb + Cmaterial_Cbx[materials[k+1][j][i]-1];
		      Cc = Cc + Cmaterial_Ccx[materials[k+1][j][i]-1];
		    }

		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
		}
		else{
		  Ca = Cax[array_ind];
		  Cb = Cbx[array_ind];
		  if(is_disp_ml)
		    Cc = Ccx[array_ind];
		  else
		    Cc = 0.;
		  if(is_cond)
		    rho = rho_x[array_ind];
		}

		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_x[array_ind];
		  kappa_l = ml_kappa_x[array_ind];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k+1][j][i]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k+1][j][i]){
		      alpha_l += alpha[materials[k+1][j][i]-1];
		      beta_l +=  beta[materials[k+1][j][i]-1];
		      gamma_l += gamma[materials[k+1][j][i]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
 
		    }
		
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}
	    
		/*if( materials[k][j][i] || materials[k][j][i+1])
		  fprintf(stdout,"(%d,%d,%d), Ca= %e, Cb=%e, is_cond:%d, rho: %e, is_disp: %d, is_disp_ml: %d\n",i,j,k,Ca,Cb,is_cond,rho,is_disp,is_disp_ml);*/
		//Enp1 = Ca*Ezx[k][j][i]+Cb*(Hyx[k][j][i] + Hyz[k][j][i] - Hyx[k][j][i-1] - Hyz[k][j][i-1]);
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Ezx_nm1[k][j][i] - 1./2.*Cb*dx*((1+alpha_l)*Jzx[k][j][i] + beta_l*Jzx_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dx*Jczx[k][j][i];
		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jzx[k][j][i] + beta_l*Jzx_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Ezx_nm1[k][j][i]);
		  Jnp1 += sigma_l/eo*gamma_l*Ezx[k][j][i];
		  Ezx_nm1[k][j][i] = Ezx[k][j][i];
		  Jzx_nm1[k][j][i] = Jzx[k][j][i];
		  Jzx[k][j][i]     = Jnp1;
		}
		if(is_cond && rho){
		  Jczx[k][j][i] -= rho*(Enp1 + Ezx[k][j][i]);
		}

		eh_vec[omp_get_thread_num()][i][0] = Hyx[k][j][i] + Hyz[k][j][i];eh_vec[omp_get_thread_num()][i][1] = 0.;
		ca_vec[omp_get_thread_num()][i-1] = Ca;
		cb_vec[omp_get_thread_num()][i-1] = Cb;

	      }
	      i=0;
	      eh_vec[omp_get_thread_num()][i][0] = Hyx[k][j][i] + Hyz[k][j][i];eh_vec[omp_get_thread_num()][i][1] = 0.;
	  
	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_e_x, N_e_x , pf_ezx[omp_get_thread_num()], pb_ezx[omp_get_thread_num()]);

	      for(i=1;i<I_tot;i++){
		Ezx[k][j][i] = ca_vec[omp_get_thread_num()][i-1]*Ezx[k][j][i] + cb_vec[omp_get_thread_num()][i-1]*eh_vec[omp_get_thread_num()][i][0]/((double) N_e_x);
		//Ezx[k][j][i] = Enp1;
	      }
	    }
	}//PSTD, Ezx
      }//(dimension==THREE || dimension==TE)
      else{
#pragma omp for
	//Ezx updates
	for(k=0;k<=K_tot;k++)
	  for(j=0;j<(J_tot+1);j++)
	    for(i=1;i<I_tot;i++){
	      rho = 0.;
	      k_loc = k;
	      if( is_structure )
		if( k>Dzl[0] && k<(Dzl[0]+K) ){
		  if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		    k_loc = k - structure[i][1];
		  else if( (k-structure[i][1])>=(K+Dzl[0]) )
		    k_loc = Dzl[0]+K-1;
		  else
		    k_loc = Dzl[0]+1;
		}
	      if( !is_multilayer )
		array_ind = i;
	      else
		array_ind = (I_tot+1)*k_loc+i;

	      //use the average of material parameters between nodes
	      if( !materials[k][j][i] ){
		Ca = Cax[array_ind];
		Cb = Cbx[array_ind];
		if(is_disp_ml)
		  Cc = Ccx[array_ind];
		else
		  Cc = 0.;
		if(is_cond)
		  rho = rho_x[i];
	      }
	      else{
		rho = 0.;
		Ca = Cmaterial_Cax[materials[k][j][i]-1];
		Cb = Cmaterial_Cbx[materials[k][j][i]-1];
		Cc = Cmaterial_Ccx[materials[k][j][i]-1];
	      }
	    
	      alpha_l = 0.;
	      beta_l  = 0.;
	      gamma_l = 0.;
	      kappa_l = 1.;
	      sigma_l = 0.;

	      
	      if(is_disp|| is_disp_ml){
		sigma_l = ml_sigma_x[array_ind];
		kappa_l = ml_kappa_x[array_ind];
		alpha_l = ml_alpha[k_loc];
		beta_l = ml_beta[k_loc];
		gamma_l = ml_gamma[k_loc];
	      
		if( materials[k][j][i] ){
		  alpha_l = alpha[materials[k][j][i]-1];
		  beta_l  = beta[materials[k][j][i]-1];
		  gamma_l = gamma[materials[k][j][i]-1];

		}
		else{
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		}
	      }

	      Enp1 = Ca*Ezx[k][j][i]+Cb*(Hyx[k][j][i] + Hyz[k][j][i] - Hyx[k][j][i-1] - Hyz[k][j][i-1]);
	      if( (is_disp || is_disp_ml) && gamma_l)
		Enp1 += Cc*Ezx_nm1[k][j][i] - 1./2.*Cb*dx*((1+alpha_l)*Jzx[k][j][i] + beta_l*Jzx_nm1[k][j][i]);
	      if(is_cond && rho)
		Enp1 += Cb*dx*Jczx[k][j][i];
	
	      if( (is_disp || is_disp_ml) && gamma_l){
		Jnp1  = alpha_l*Jzx[k][j][i] + beta_l*Jzx_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Ezx_nm1[k][j][i]);
		Jnp1 += sigma_l/eo*gamma_l*Ezx[k][j][i];
		Ezx_nm1[k][j][i] = Ezx[k][j][i];
		Jzx_nm1[k][j][i] = Jzx[k][j][i];
		Jzx[k][j][i]     = Jnp1;
	      }
	      if(is_cond && rho){
		Jczx[k][j][i] -= rho*(Enp1 + Ezx[k][j][i]);
	      }
	    
	      Ezx[k][j][i] = Enp1;
	    }
      }
      //fprintf(stderr,"Pos 08:\n");
      if(dimension==THREE || dimension==TE){
	if( useCD ){//FDTD, Ezy
#pragma omp for
	  //Ezy updates
	  for(k=0;k<K_tot;k++)
	    for(j=1;j<J_tot;j++)
	      for(i=0;i<(I_tot+1);i++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = j;
		else
		  array_ind = (J_tot+1)*k_loc+j;

		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k+1][j][i]){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Cay[array_ind];
		    Cb = Cby[array_ind];
		    if(is_disp_ml)
		      Cc = Ccy[array_ind];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Cay[materials[k][j][i]-1];
		    Cb = Cmaterial_Cby[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccy[materials[k][j][i]-1];
		  }
	    
		  if(intmatprops){
		    if( !materials[k+1][j][i] ){
		      Ca = Ca + Cay[array_ind];
		      Cb = Cb + Cby[array_ind];
		      if(is_disp_ml)
			Cc = Cc + Ccy[array_ind];
		    }
		    else{
		      Ca = Ca + Cmaterial_Cay[materials[k+1][j][i]-1];
		      Cb = Cb + Cmaterial_Cby[materials[k+1][j][i]-1];
		      Cc = Cc + Cmaterial_Ccy[materials[k+1][j][i]-1];
		    }	
		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
	    
		}
		else{
		  Ca = Cay[array_ind];
		  Cb = Cby[array_ind];
		  if(is_disp_ml)
		    Cc = Ccy[array_ind];
		  else
		    Cc = 0;
		  if(is_cond)
		    rho = rho_y[array_ind];
		}

		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_y[array_ind];
		  kappa_l = ml_kappa_y[array_ind];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k+1][j][i]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k+1][j][i]){
		      alpha_l += alpha[materials[k+1][j][i]-1];
		      beta_l +=  beta[materials[k+1][j][i]-1];
		      gamma_l += gamma[materials[k+1][j][i]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}



		Enp1 = Ca*Ezy[k][j][i]+Cb*(Hxy[k][j-1][i] + Hxz[k][j-1][i] - Hxy[k][j][i] - Hxz[k][j][i]); 
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Ezy_nm1[k][j][i] - 1./2.*Cb*dy*((1+alpha_l)*Jzy[k][j][i] + beta_l*Jzy_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dy*Jczy[k][j][i];

		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jzy[k][j][i] + beta_l*Jzy_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Ezy_nm1[k][j][i]);

		  Jnp1 += sigma_l/eo*gamma_l*Ezy[k][j][i];
		  Ezy_nm1[k][j][i] = Ezy[k][j][i];
		  Jzy_nm1[k][j][i] = Jzy[k][j][i];
		  Jzy[k][j][i]     = Jnp1;
		}
		if(is_cond && rho){
		  Jczy[k][j][i] -= rho*(Enp1 + Ezy[k][j][i]);
		}
		Ezy[k][j][i] = Enp1;
	      }
	}//FDTD, Ezy
	else{//PSTD, Ezy
#pragma omp for
	  //Ezy updates
	  for(k=0;k<K_tot;k++)
	    for(i=0;i<(I_tot+1);i++){
	      for(j=1;j<J_tot;j++){
		rho = 0.;
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = j;
		else
		  array_ind = (J_tot+1)*k_loc+j;

		//use the average of material parameters between nodes
		if( materials[k][j][i] || materials[k+1][j][i]){
		  rho = 0.;
		  if( !materials[k][j][i] ){
		    Ca = Cay[array_ind];
		    Cb = Cby[array_ind];
		    if(is_disp_ml)
		      Cc = Ccy[array_ind];
		    else
		      Cc = 0.;
		  }
		  else{
		    Ca = Cmaterial_Cay[materials[k][j][i]-1];
		    Cb = Cmaterial_Cby[materials[k][j][i]-1];
		    Cc = Cmaterial_Ccy[materials[k][j][i]-1];
		  }
	    
		  if(intmatprops){
		    if( !materials[k+1][j][i] ){
		      Ca = Ca + Cay[array_ind];
		      Cb = Cb + Cby[array_ind];
		      if(is_disp_ml)
			Cc = Cc + Ccy[array_ind];
		    }
		    else{
		      Ca = Ca + Cmaterial_Cay[materials[k+1][j][i]-1];
		      Cb = Cb + Cmaterial_Cby[materials[k+1][j][i]-1];
		      Cc = Cc + Cmaterial_Ccy[materials[k+1][j][i]-1];
		    }	
		    Ca = Ca/2.;
		    Cb = Cb/2.;
		    Cc = Cc/2.;
		  }
	    
		}
		else{
		  Ca = Cay[array_ind];
		  Cb = Cby[array_ind];
		  if(is_disp_ml)
		    Cc = Ccy[array_ind];
		  else
		    Cc = 0;
		  if(is_cond)
		    rho = rho_y[array_ind];
		}

		alpha_l = 0.;
		beta_l  = 0.;
		gamma_l = 0.;
		kappa_l = 1.;
		sigma_l = 0.;

		if(is_disp || is_disp_ml){
		  sigma_l = ml_sigma_y[array_ind];
		  kappa_l = ml_kappa_y[array_ind];
		  alpha_l = ml_alpha[k_loc];
		  beta_l = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		  if( materials[k][j][i] || materials[k+1][j][i]){
		    if(materials[k][j][i]){
		      alpha_l = alpha[materials[k][j][i]-1];
		      beta_l =  beta[materials[k][j][i]-1];
		      gamma_l = gamma[materials[k][j][i]-1];
		    }
		    else{
		      alpha_l = ml_alpha[k_loc];
		      beta_l = ml_beta[k_loc];
		      gamma_l = ml_gamma[k_loc];
		    }

		    if(materials[k+1][j][i]){
		      alpha_l += alpha[materials[k+1][j][i]-1];
		      beta_l +=  beta[materials[k+1][j][i]-1];
		      gamma_l += gamma[materials[k+1][j][i]-1];
		    }
		    else{
		      alpha_l += ml_alpha[k_loc];
		      beta_l += ml_beta[k_loc];
		      gamma_l += ml_gamma[k_loc];
		    }
		    alpha_l = alpha_l/2.;
		    beta_l = beta_l/2.;
		    gamma_l = gamma_l/2.;
		  }
		}



		//Enp1 = Ca*Ezy[k][j][i]+Cb*(Hxy[k][j-1][i] + Hxz[k][j-1][i] - Hxy[k][j][i] - Hxz[k][j][i]); 
		if( (is_disp || is_disp_ml) && gamma_l)
		  Enp1 += Cc*Ezy_nm1[k][j][i] - 1./2.*Cb*dy*((1+alpha_l)*Jzy[k][j][i] + beta_l*Jzy_nm1[k][j][i]);
		if(is_cond && rho)
		  Enp1 += Cb*dy*Jczy[k][j][i];

		if( (is_disp || is_disp_ml) && gamma_l){
		  Jnp1  = alpha_l*Jzy[k][j][i] + beta_l*Jzy_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Ezy_nm1[k][j][i]);

		  Jnp1 += sigma_l/eo*gamma_l*Ezy[k][j][i];
		  Ezy_nm1[k][j][i] = Ezy[k][j][i];
		  Jzy_nm1[k][j][i] = Jzy[k][j][i];
		  Jzy[k][j][i]     = Jnp1;
		}
		if(is_cond && rho){
		  Jczy[k][j][i] -= rho*(Enp1 + Ezy[k][j][i]);
		}

		eh_vec[omp_get_thread_num()][j][0] = Hxy[k][j][i] + Hxz[k][j][i];eh_vec[omp_get_thread_num()][j][1] = 0.;
		ca_vec[omp_get_thread_num()][j-1] = Ca;
		cb_vec[omp_get_thread_num()][j-1] = Cb;
	    
	      }
	      if(J_tot>1){
		j = 0;
		eh_vec[omp_get_thread_num()][j][0] = Hxy[k][j][i] + Hxz[k][j][i];eh_vec[omp_get_thread_num()][j][1] = 0.;
		first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				  dk_e_y, N_e_y , pf_ezy[omp_get_thread_num()], pb_ezy[omp_get_thread_num()]);
	      }
	      for(j=1;j<J_tot;j++){
		Ezy[k][j][i] = ca_vec[omp_get_thread_num()][j-1]*Ezy[k][j][i] - cb_vec[omp_get_thread_num()][j-1]*eh_vec[omp_get_thread_num()][j][0]/((double) N_e_y);
		//Ezy[k][j][i] = Enp1;
	      }
	    }	
	}//PSTD, Ezy
      }//(dimension==THREE || dimension==TE)
      else{
#pragma omp for
	for(k=0;k<=K_tot;k++)
	  for(j=1;j<J_tot;j++)
	    for(i=0;i<(I_tot+1);i++){
	      rho = 0.;
	      k_loc = k;
	      if( is_structure )
		if( k>Dzl[0] && k<(Dzl[0]+K) ){
		  if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		    k_loc = k - structure[i][1];
		  else if( (k-structure[i][1])>=(K+Dzl[0]) )
		    k_loc = Dzl[0]+K-1;
		  else
		    k_loc = Dzl[0]+1;
		}
	      if( !is_multilayer )
		array_ind = j;
	      else
		array_ind = (J_tot+1)*k_loc+j;

	      //use the average of material parameters between nodes
	      if( !materials[k][j][i] ){
		Ca = Cay[array_ind];
		Cb = Cby[array_ind];
		if(is_disp_ml)
		  Cc = Ccy[array_ind];
		else
		  Cc = 0.;
		if(is_cond)
		  rho = rho_y[array_ind];
	      }
	      else{
		rho = 0.;
		Ca = Cmaterial_Cay[materials[k][j][i]-1];
		Cb = Cmaterial_Cby[materials[k][j][i]-1];
		Cc = Cmaterial_Ccy[materials[k][j][i]-1];
	      }

	      alpha_l = 0.;
	      beta_l  = 0.;
	      gamma_l = 0.;
	      kappa_l = 1.;
	      sigma_l = 0.;

	      if(is_disp || is_disp_ml){
		kappa_l = ml_kappa_y[array_ind];
		sigma_l = ml_sigma_y[array_ind];
		alpha_l = ml_alpha[k_loc];
		beta_l  = ml_beta[k_loc];
		gamma_l = ml_gamma[k_loc];
	      
		if( !materials[k][j][i] ){
		  alpha_l = 0.;
		  beta_l  = 0.;
		  gamma_l = 0.;
		}
		else{
		  alpha_l = ml_alpha[k_loc];
		  beta_l  = ml_beta[k_loc];
		  gamma_l = ml_gamma[k_loc];
		}
	      }
	    

	    


	      Enp1 = Ca*Ezy[k][j][i]+Cb*(Hxy[k][j-1][i] + Hxz[k][j-1][i] - Hxy[k][j][i] - Hxz[k][j][i]); 
	      if( (is_disp || is_disp_ml) && gamma_l)
		Enp1 += Cc*Ezy_nm1[k][j][i] - 1./2.*Cb*dy*((1+alpha_l)*Jzy[k][j][i] + beta_l*Jzy_nm1[k][j][i]);
	      if(is_cond && rho)
		Enp1 += Cb*dy*Jczy[k][j][i];
	  
	      if( (is_disp || is_disp_ml) && gamma_l){
		Jnp1  = alpha_l*Jzy[k][j][i] + beta_l*Jzy_nm1[k][j][i] + kappa_l*gamma_l/(2.*dt[0])*(Enp1 - Ezy_nm1[k][j][i]);	      
	      	    
		Jnp1 += sigma_l/eo*gamma_l*Ezy[k][j][i];
		Ezy_nm1[k][j][i] = Ezy[k][j][i];
		Jzy_nm1[k][j][i] = Jzy[k][j][i];
		Jzy[k][j][i]     = Jnp1;
	      }
	      if(is_cond && rho){
		Jczy[k][j][i] -= rho*(Enp1 + Ezy[k][j][i]);
	      }

	      Ezy[k][j][i] = Enp1;
	    }
      }
    }//end of parallel section
    //fprintf(stderr,"Pos 09:\n");
    if(TIME_EXEC){
      time_1=omp_get_wtime();
      secs= time_1-time_0;
      fprintf(stdout,"%.03e ",secs);
      time_0=time_1;
    }
    /********************/
  
    //update terms for self consistency across scattered/total interface - E updates##
    if(sourcemode == sm_steadystate){//steadystate
      commonPhase = exp(-I*fmod(omega_an[0]*time_H,2.*dcpi));
      commonAmplitude = linearRamp(time_H, 1./(*omega_an/(2*dcpi)), ramp_width);
      for(k=((int)K0[0]);k<=((int)K1[0]);k++)
	for(j=((int)J0[0]);j<=((int)J1[0]);j++){
	  if( (int)I0[1] ){//Perform across I0

	    if( !is_multilayer )
	      array_ind = (int)I0[0];
	    else
	      array_ind = (I_tot+1)*k+(int)I0[0];

	    if( k < ((int)K1[0]) || dimension==TM ){
	      Ezx[k][j][(int)I0[0]] = Ezx[k][j][(int)I0[0]] - Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][2] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][2]));
	      if(is_cond)
		Jczx[k][j][(int)I0[0]] += rho_x[array_ind]*Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][2] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][2]));
	      if(is_disp_ml)
		Jzx[k][j][(int)I0[0]] += ml_kappa_x[array_ind]*ml_gamma[k]/(2.*dt[0])*Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][2] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][2]));
	    }
	    if( j < ((int)J1[0]) ){	 
	      Eyx[k][j][(int)I0[0]] = Eyx[k][j][(int)I0[0]] + Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][3] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][3]));
	      if(is_cond)
		Jcyx[k][j][(int)I0[0]] -= rho_x[array_ind]*Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][3] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][3]));
	      if(is_disp_ml)
		Jyx[k][j][(int)I0[0]] -= ml_kappa_x[array_ind]*ml_gamma[k]/(2.*dt[0])*Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][3] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][3]));
	    }
	  }
	  if( (int)I1[1] ){//Perform across I1

	    if( !is_multilayer )
	      array_ind = (int)I1[0];
	    else
	      array_ind = (I_tot+1)*k+(int)I1[0];

	    if( k < ((int)K1[0]) || dimension==TM ){
	      Ezx[k][j][(int)I1[0]] = Ezx[k][j][(int)I1[0]] + Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][6] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][6]));
	      if(is_cond)
		Jczx[k][j][(int)I1[0]] -= rho_x[array_ind]*Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][6] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][6]));
	      if(is_disp_ml)
		Jzx[k][j][(int)I1[0]] -= ml_kappa_x[array_ind]*ml_gamma[k]/(2.*dt[0])*Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][6] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][6]));
	    }
	    if( j < ((int)J1[0]) ){	 
	      Eyx[k][j][(int)I1[0]] = Eyx[k][j][(int)I1[0]] - Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][7] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][7]));
	      if(is_cond)
		Jcyx[k][j][(int)I1[0]] += rho_x[array_ind]*Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][7] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][7]));
	      if(is_disp_ml)
		Jyx[k][j][(int)I1[0]] += ml_kappa_x[array_ind]*ml_gamma[k]/(2.*dt[0])*Cbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][7] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][7]));
	    }
	  }
	}
      
      for(k=((int)K0[0]);k<=((int)K1[0]);k++)
	for(i=((int)I0[0]);i<=((int)I1[0]);i++){
	  if( (int)J0[1] ){//Perform across J0
	    if( k < ((int)K1[0]) || dimension==TM ){

	      if( !is_multilayer )
		array_ind = (int)J0[0];
	      else
		array_ind = (J_tot+1)*k+(int)J0[0];
	 
	      Ezy[k][((int)J0[0])][i] = Ezy[k][((int)J0[0])][i] + Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][2] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][2]));
	      if(is_cond)
		Jczy[k][((int)J0[0])][i] -= rho_y[array_ind]*Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][2] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][2]));
	      if(is_disp_ml)
		Jzy[k][((int)J0[0])][i] -= ml_kappa_y[array_ind]*ml_gamma[k]/(2.*dt[0])*Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][2] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][2]));
	    }
	    if( i < ((int)I1[0]) ){	 
	      Exy[k][((int)J0[0])][i] = Exy[k][((int)J0[0])][i] - Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][3] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][3]));
	      if(is_cond)
		Jcxy[k][((int)J0[0])][i] += rho_y[array_ind]*Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][3] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][3]));
	      if(is_disp_ml)
		Jxy[k][((int)J0[0])][i] += ml_kappa_y[array_ind]*ml_gamma[k]/(2.*dt[0])*Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][3] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][3]));
	    }
	  }
	  if( (int)J1[1] ){//Perform across J1

	    if( !is_multilayer )
	      array_ind = (int)J1[0];
	    else
	      array_ind = (J_tot+1)*k+(int)J1[0];

	    if( k < ((int)K1[0]) || dimension==TM ){	 
	      Ezy[k][((int)J1[0])][i] = Ezy[k][((int)J1[0])][i] - Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][6] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][6]));
	      if(is_cond)
		Jczy[k][((int)J1[0])][i] += rho_y[array_ind]*Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][6] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][6]));
	      if(is_disp_ml)
		Jzy[k][((int)J1[0])][i] -= ml_kappa_y[array_ind]*ml_gamma[k]/(2.*dt[0])*Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][6] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][6]));
	    }
	    if( i < ((int)I1[0]) ){	 
	      Exy[k][((int)J1[0])][i] = Exy[k][((int)J1[0])][i] + Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][7] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][7]));
	      if(is_cond)
		Jcxy[k][((int)J1[0])][i] -= rho_y[array_ind]*Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][7] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][7]));
	      if(is_disp_ml)
		Jxy[k][((int)J1[0])][i] += ml_kappa_y[array_ind]*ml_gamma[k]/(2.*dt[0])*Cby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][7] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][7]));
	    }
	  }
	}
      
      for(j=((int)J0[0]);j<=((int)J1[0]);j++)
	for(i=((int)I0[0]);i<=((int)I1[0]);i++){
	  if( (int)K0[1] ){//Perform across K0
	    if( j < ((int)J1[0]) ){	 
	      Eyz[((int)K0[0])][j][i] = Eyz[((int)K0[0])][j][i] - Cbz[(int)K0[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2]));
	      if(is_cond)
		Jcyz[((int)K0[0])][j][i] += rho_z[((int)K0[0])]*Cbz[(int)K0[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2]));
	      if(is_disp_ml)
		Jyz[((int)K0[0])][j][i] -= ml_kappa_z[((int)K0[0])]*ml_gamma[k]/(2.*dt[0])*Cbz[(int)K0[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2]));
	    }
	    if( i < ((int)I1[0]) ){	 
	      Exz[((int)K0[0])][j][i] = Exz[((int)K0[0])][j][i] + Cbz[(int)K0[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][3] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][3]));
	      if(is_cond)
		Jcxz[((int)K0[0])][j][i]  -= rho_z[((int)K0[0])]*Cbz[(int)K0[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][3] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][3]));
	      if(is_disp_ml)
		Jxz[((int)K0[0])][j][i] += ml_kappa_z[((int)K0[0])]*ml_gamma[k]/(2.*dt[0])*Cbz[(int)K0[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][3] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][3]));
	    }
	  }
	  if( (int)K1[1] ){//Perform across K1
	    if( j < ((int)J1[0]) ){	 
	      Eyz[((int)K1[0])][j][i] = Eyz[((int)K1[0])][j][i] + Cbz[(int)K1[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][6] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][6]));
	      if(is_cond)
		Jcyz[((int)K1[0])][j][i] -= rho_z[((int)K1[0])]*Cbz[(int)K1[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][6] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][6]));
	      if(is_disp_ml)
		Jyz[((int)K1[0])][j][i] += ml_kappa_z[((int)K1[0])]*ml_gamma[k]/(2.*dt[0])*Cbz[(int)K1[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][6] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][6]));
	    }
	    if( i < ((int)I1[0]) ){	 
	      Exz[((int)K1[0])][j][i] = Exz[((int)K1[0])][j][i] - Cbz[(int)K1[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][7] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][7]));
	      if(is_cond)
		Jcxz[((int)K1[0])][j][i] += rho_z[((int)K1[0])]*Cbz[(int)K1[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][7] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][7]));
	      if(is_disp_ml)
		Jxz[((int)K1[0])][j][i] -= ml_kappa_z[((int)K1[0])]*ml_gamma[k]/(2.*dt[0])*Cbz[(int)K1[0]]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][7] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][7]));
	    }
	  }
	}
      fth = real(commonAmplitude*commonPhase);
    }
    else if(sourcemode==1){//pulsed
      
      if(J_tot==0){
	j=0;
	for(i=0;i<(I_tot+1);i++){
	  Eyz[(int)K0[0]][j][i] = Eyz[(int)K0[0]][j][i] - Cbz[(int)K0[0]]*real((KsourceR[0][i-((int)I0[0])][2] + I*KsourceI[0][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2));
	  //Eyz[(int)K0[0]][j][i] = Eyz[(int)K0[0]][j][i] - Cbz[(int)K0[0]]*real((KsourceR[0][i-((int)I0[0])][2] + I*KsourceI[0][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2));
	  if(is_cond)
	    Jcyz[(int)K0[0]][j][i] += rho_z[(int)K0[0]]*Cbz[(int)K0[0]]*real((KsourceR[0][i-((int)I0[0])][2] + I*KsourceI[0][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2));
	  //Jcyz[(int)K0[0]][j][i] += rho_z[(int)K0[0]]*Cbz[(int)K0[0]]*real((KsourceR[0][i-((int)I0[0])][2] + I*KsourceI[0][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2));
	  if(is_disp_ml){
	    Jyz[(int)K0[0]][j][i] -= ml_kappa_z[(int)K0[0]]*ml_gamma[(int)K0[0]]/(2.*dt[0])*Cbz[(int)K0[0]]*real((KsourceR[0][i-((int)I0[0])][2] + I*KsourceI[0][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2));
	    //Jyz[(int)K0[0]][j][i] -= ml_kappa_z[(int)K0[0]]*ml_gamma[(int)K0[0]]/(2.*dt[0])*Cbz[(int)K0[0]]*real((KsourceR[0][i-((int)I0[0])][2] + I*KsourceI[0][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2));
	    
	  }
	}
      }
      else
	for(j=0;j<J_tot;j++)
	  for(i=0;i<(I_tot+1);i++){
	    /*
	      if(i==41 & j==41)
	      fprintf(stderr,"Cbz = %.10e, Re(K) = %.10e, Im(K) = %.10e, time_H= %.10e, to_l[0]=%.10e, dz/light_v/2=%.10e, hwhm = %.10e, dE=%.10e\n",Cbz[(int)K0[0]],KsourceR[j-((int)J0[0])][i-((int)I0[0])][2],KsourceI[j-((int)J0[0])][i-((int)I0[0])][2],time_H,to_l[0],dz/light_v/2,hwhm[0],Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2)));
	    */
	    Eyz[(int)K0[0]][j][i] = Eyz[(int)K0[0]][j][i] - Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2));
	    //Eyz[(int)K0[0]][j][i] = Eyz[(int)K0[0]][j][i] - Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2));
	    if(is_cond)
	      Jcyz[(int)K0[0]][j][i] += rho_z[(int)K0[0]]*Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2));
	    //Jcyz[(int)K0[0]][j][i] += rho_z[(int)K0[0]]*Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2));
	    if(is_disp_ml){
	      Jyz[(int)K0[0]][j][i] -= ml_kappa_z[(int)K0[0]]*ml_gamma[(int)K0[0]]/(2.*dt[0])*Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2));
	      //Jyz[(int)K0[0]][j][i] -= ml_kappa_z[(int)K0[0]]*ml_gamma[(int)K0[0]]/(2.*dt[0])*Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][2] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][2])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2));
	  
	    }
	  }
      for(j=0;j<(J_tot+1);j++)
	for(i=0;i<I_tot;i++){
	  Exz[(int)K0[0]][j][i] = Exz[(int)K0[0]][j][i] + Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][3] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][3])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2 ));
	  //Exz[(int)K0[0]][j][i] = Exz[(int)K0[0]][j][i] + Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][3] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][3])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2 ));
	  if(is_cond)
	    Jcxz[(int)K0[0]][j][i] -= rho_z[(int)K0[0]]*Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][3] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][3])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2 ));
	  //Jcxz[(int)K0[0]][j][i] -= rho_z[(int)K0[0]]*Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][3] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][3])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2 ));
	  if(is_disp_ml)
	    Jxz[(int)K0[0]][j][i] += ml_kappa_z[(int)K0[0]]*ml_gamma[(int)K0[0]]/(2.*dt[0])*Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][3] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][3])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2 ));
	  //Jxz[(int)K0[0]][j][i] += ml_kappa_z[(int)K0[0]]*ml_gamma[(int)K0[0]]/(2.*dt[0])*Cbz[(int)K0[0]]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][3] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][3])*(-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2 ));
	}
      //fth = real((-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2));
      fth = real((-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0] + dz/light_v/2.)/(hwhm[0]),2));
      //fth = real((-1.0*I)*exp(-I*fmod(omega_an[0]*(time_H - to_l[0]),2.*dcpi)))*exp( -1.0*dcpi*pow((time_H - to_l[0])/(hwhm[0]),2));

    }
    //fprintf(stderr,"Pos 10:\n");
    /**Debugging**/
    //    for(k=0;k<(K_tot+1);k++)
    //      fprintf(outfile,"%e ",Jxz[k][30][30]+Jxy[k][30][30]);
    //for(k=0;k<(K_tot+1);k++)
    //    fprintf(outfile,"%e ",Exz[k][30][30]+Exy[k][30][30]);
    //   fprintf(outfile,"\n");
    /**End Debugging**/
    //fprintf(stderr,"Pos 11a:\n");

    //end of source terms
    if(TIME_EXEC){
      time_1=omp_get_wtime();
      secs= time_1-time_0;
      fprintf(stdout,"%.03e ",secs);
      time_0=time_1;
    }
  
    /********************/
    //begin parallel
#pragma omp parallel default(shared)  private(i,j,k,k_loc,array_ind)//,ca_vec,cb_vec,cc_vec,eh_vec)
    {
      if(dimension==THREE || dimension==TE){
	if( useCD ){//FDTD, Hxz
#pragma omp for
	  //Hxz updates
	  for(k=0;k<K_tot;k++)
	    for(j=0;j<J_tot_bound;j++)
	      for(i=0;i<(I_tot+1);i++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }

		if(  !materials[k][j][i])
		  Hxz[k][j][i] = Daz[k_loc]*Hxz[k][j][i]+Dbz[k_loc]*(Eyx[k+1][j][i] + Eyz[k+1][j][i] - Eyx[k][j][i] - Eyz[k][j][i]);
		else
		  Hxz[k][j][i] = Dmaterial_Daz[materials[k][j][i]-1]*Hxz[k][j][i]+Dmaterial_Dbz[materials[k][j][i]-1]*(Eyx[k+1][j][i] + Eyz[k+1][j][i] - Eyx[k][j][i] - Eyz[k][j][i]);
	
	      }
	}//FDTD, Hxz
	else{//PSTD, Hxz
#pragma omp for
	  //Hxz updates
	  for(j=0;j<J_tot_bound;j++)
	    for(i=0;i<(I_tot+1);i++){
	      for(k=0;k<K_tot;k++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }

		if(  !materials[k][j][i]){
		  ca_vec[omp_get_thread_num()][k]=Daz[k_loc];
		  cb_vec[omp_get_thread_num()][k]=Dbz[k_loc];
		  //Hxz[k][j][i] = Daz[k_loc]*Hxz[k][j][i]+Dbz[k_loc]*(Eyx[k+1][j][i] + Eyz[k+1][j][i] - Eyx[k][j][i] - Eyz[k][j][i]);
		}
		else{
		  ca_vec[omp_get_thread_num()][k]=Dmaterial_Daz[materials[k][j][i]-1];
		  cb_vec[omp_get_thread_num()][k]=Dmaterial_Dbz[materials[k][j][i]-1];
		  //Hxz[k][j][i] = Dmaterial_Daz[materials[k][j][i]-1]*Hxz[k][j][i]+Dmaterial_Dbz[materials[k][j][i]-1]*(Eyx[k+1][j][i] + Eyz[k+1][j][i] - Eyx[k][j][i] - Eyz[k][j][i]);
		}
	    
		eh_vec[omp_get_thread_num()][k][0] = Eyx[k][j][i] + Eyz[k][j][i];eh_vec[omp_get_thread_num()][k][1] = 0.;

	      }
	      k = K_tot;
	      eh_vec[omp_get_thread_num()][k][0] = Eyx[k][j][i] + Eyz[k][j][i];eh_vec[omp_get_thread_num()][k][1] = 0.;
	    
	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_h_z, N_h_z , pf_hxz[omp_get_thread_num()], pb_hxz[omp_get_thread_num()]);

	      for(k=0;k<K_tot;k++){
		Hxz[k][j][i] = ca_vec[omp_get_thread_num()][k]*Hxz[k][j][i] + cb_vec[omp_get_thread_num()][k]*eh_vec[omp_get_thread_num()][k][0]/((double) N_h_z);
	      }
	    }
	
	}//PSTD, Hxz
        
	if( useCD ){//FDTD, Hxy
#pragma omp for
	  //Hxy updates
	  for(k=0;k<K_tot;k++)
	    for(j=0;j<J_tot;j++)
	      for(i=0;i<(I_tot+1);i++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = j;
		else
		  array_ind = (J_tot+1)*k_loc+j;
		if(  !materials[k][j][i])
		  Hxy[k][j][i] = Day[array_ind]*Hxy[k][j][i]+Dby[array_ind]*(Ezy[k][j][i] + Ezx[k][j][i] - Ezy[k][j+1][i] - Ezx[k][j+1][i]);
		else
		  Hxy[k][j][i] = Dmaterial_Day[materials[k][j][i]-1]*Hxy[k][j][i]+Dmaterial_Dby[materials[k][j][i]-1]*(Ezy[k][j][i] + Ezx[k][j][i] - Ezy[k][j+1][i] - Ezx[k][j+1][i]);
	      }
	}//FDTD, Hxy
	else{//PSTD, Hxy
#pragma omp for
	  //Hxy updates
	  for(k=0;k<K_tot;k++)
	    for(i=0;i<(I_tot+1);i++){
	      for(j=0;j<J_tot;j++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = j;
		else
		  array_ind = (J_tot+1)*k_loc+j;
		if(  !materials[k][j][i]){
		  ca_vec[omp_get_thread_num()][j] = Day[array_ind];
		  cb_vec[omp_get_thread_num()][j] = Dby[array_ind];
		  //		Hxy[k][j][i] = Day[array_ind]*Hxy[k][j][i]+Dby[array_ind]*(Ezy[k][j][i] + Ezx[k][j][i] - Ezy[k][j+1][i] - Ezx[k][j+1][i]);
		}
		else{
		  ca_vec[omp_get_thread_num()][j] = Dmaterial_Day[materials[k][j][i]-1];
		  cb_vec[omp_get_thread_num()][j] = Dmaterial_Dby[materials[k][j][i]-1];
		  //		Hxy[k][j][i] = Dmaterial_Day[materials[k][j][i]-1]*Hxy[k][j][i]+Dmaterial_Dby[materials[k][j][i]-1]*(Ezy[k][j][i] + Ezx[k][j][i] - Ezy[k][j+1][i] - Ezx[k][j+1][i]);
		}
	      	      
		eh_vec[omp_get_thread_num()][j][0] = Ezy[k][j][i] + Ezx[k][j][i];eh_vec[omp_get_thread_num()][j][1] = 0.;
	      
	      }
	      j=J_tot;
	      eh_vec[omp_get_thread_num()][j][0] = Ezy[k][j][i] + Ezx[k][j][i];eh_vec[omp_get_thread_num()][j][1] = 0.;

	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_h_y, N_h_y , pf_hxy[omp_get_thread_num()], pb_hxy[omp_get_thread_num()]);
	    	    
	      for(j=0;j<J_tot;j++){
		Hxy[k][j][i] = ca_vec[omp_get_thread_num()][j]*Hxy[k][j][i] - cb_vec[omp_get_thread_num()][j]*eh_vec[omp_get_thread_num()][j][0]/((double) N_h_y);
	      }

	      /*
		if( i==12 && k==24){
		fprintf(stdout,"tind: %d\n",tind);
		fprintf(stdout,"Da: ");
		for(j=0;j<J_tot;j++)
		fprintf(stdout,"%e ",ca_vec[omp_get_thread_num()][j]);
		fprintf(stdout,"\nDb: ");
		for(j=0;j<J_tot;j++)
		fprintf(stdout,"%e ",cb_vec[omp_get_thread_num()][j]);
	      
		fprintf(stdout,"\neh_vec: ");
		for(j=0;j<J_tot;j++)
		fprintf(stdout,"%e ",eh_vec[omp_get_thread_num()][j][0]/((double) N_e_y));
		fprintf(stdout,"\n");
		}
	      */
	    }
	}//PSTD, Hxy

	if( useCD ){//FDTD, Hyx
#pragma omp for
	  //Hyx updates
	  for(k=0;k<K_tot;k++)
	    for(j=0;j<J_tot_p1_bound;j++)
	      for(i=0;i<I_tot;i++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = i;
		else
		  array_ind = (I_tot+1)*k_loc+i;
		if(  !materials[k][j][i])
		  Hyx[k][j][i] = Dax[array_ind]*Hyx[k][j][i]+Dbx[array_ind]*(Ezx[k][j][i+1] + Ezy[k][j][i+1] - Ezx[k][j][i] - Ezy[k][j][i]);  
		else{
		  Hyx[k][j][i] = Dmaterial_Dax[materials[k][j][i]-1]*Hyx[k][j][i]+Dmaterial_Dbx[materials[k][j][i]-1]*(Ezx[k][j][i+1] + Ezy[k][j][i+1] - Ezx[k][j][i] - Ezy[k][j][i]);  
		}
	    
	      }
	}//FDTD, Hyx
	else{//PSTD, Hyx
#pragma omp for
	  //Hyx updates
	  for(k=0;k<K_tot;k++)
	    for(j=0;j<J_tot_p1_bound;j++){
	      for(i=0;i<I_tot;i++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = i;
		else
		  array_ind = (I_tot+1)*k_loc+i;
		if(  !materials[k][j][i]){
		  ca_vec[omp_get_thread_num()][i] = Dax[array_ind];
		  cb_vec[omp_get_thread_num()][i] = Dbx[array_ind];
		  //		Hyx[k][j][i] = Dax[array_ind]*Hyx[k][j][i]+Dbx[array_ind]*(Ezx[k][j][i+1] + Ezy[k][j][i+1] - Ezx[k][j][i] - Ezy[k][j][i]);  
		}
		else{
		  ca_vec[omp_get_thread_num()][i] = Dmaterial_Dax[materials[k][j][i]-1];
		  cb_vec[omp_get_thread_num()][i] = Dmaterial_Dbx[materials[k][j][i]-1];
		  //	Hyx[k][j][i] = Dmaterial_Dax[materials[k][j][i]-1]*Hyx[k][j][i]+Dmaterial_Dbx[materials[k][j][i]-1]*(Ezx[k][j][i+1] + Ezy[k][j][i+1] - Ezx[k][j][i] - Ezy[k][j][i]);  
		}

		eh_vec[omp_get_thread_num()][i][0] = Ezx[k][j][i] + Ezy[k][j][i];eh_vec[omp_get_thread_num()][i][1] = 0.;
	      
	      }
	      i = I_tot;
	      eh_vec[omp_get_thread_num()][i][0] = Ezx[k][j][i] + Ezy[k][j][i];eh_vec[omp_get_thread_num()][i][1] = 0.;

	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_h_x, N_h_x , pf_hyx[omp_get_thread_num()], pb_hyx[omp_get_thread_num()]);

	      for(i=0;i<I_tot;i++){
		Hyx[k][j][i] = ca_vec[omp_get_thread_num()][i]*Hyx[k][j][i] + cb_vec[omp_get_thread_num()][i]*eh_vec[omp_get_thread_num()][i][0]/((double) N_h_x);
	      }
	    }
	}//PSTD, Hyx

	if( useCD ){//FDTD, Hyz
#pragma omp for
	  //Hyz updates
	  for(k=0;k<K_tot;k++){
	    for(j=0;j<J_tot_p1_bound;j++)
	      for(i=0;i<I_tot;i++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if(  !materials[k][j][i]){
		  /*if(tind==0)
		    fprintf(stdout,"%d %d %e %e\n",i,k,Daz[k_loc], Dbz[k_loc]);*/
		  Hyz[k][j][i] = Daz[k_loc]*Hyz[k][j][i]+Dbz[k_loc]*(Exy[k][j][i] + Exz[k][j][i] - Exy[k+1][j][i] - Exz[k+1][j][i]);    
		}
		else{
		  /*if(tind==0)
		    fprintf(stdout,"%d %d %e %e\n",i,k,Dmaterial_Daz[materials[k][j][i]-1],Dmaterial_Dbz[materials[k][j][i]-1]);*/
		  Hyz[k][j][i] = Dmaterial_Daz[materials[k][j][i]-1]*Hyz[k][j][i]+Dmaterial_Dbz[materials[k][j][i]-1]*(Exy[k][j][i] + Exz[k][j][i] - Exy[k+1][j][i] - Exz[k+1][j][i]); 
		}
	      } 
	  }
	}//FDTD, Hyz
	else{//PSTD, Hyz
	  //#pragma omp for
	  //Hyz updates
	  for(j=0;j<J_tot_p1_bound;j++)
	    #pragma omp for
	    for(i=0;i<I_tot;i++){
	      for(k=0;k<K_tot;k++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if(  !materials[k][j][i]){
		  ca_vec[omp_get_thread_num()][k] = Daz[k_loc];
		  cb_vec[omp_get_thread_num()][k] = Dbz[k_loc];
		  /*if(tind==0)
		    fprintf(stdout,"%d %d %e %e\n",i,k,Daz[k_loc], Dbz[k_loc]);*/
		  //Hyz[k][j][i] = Daz[k_loc]*Hyz[k][j][i]+Dbz[k_loc]*(Exy[k][j][i] + Exz[k][j][i] - Exy[k+1][j][i] - Exz[k+1][j][i]);    
		}
		else{
		  ca_vec[omp_get_thread_num()][k] = Dmaterial_Daz[materials[k][j][i]-1];
		  cb_vec[omp_get_thread_num()][k] = Dmaterial_Dbz[materials[k][j][i]-1];
		  /*if(tind==0)
		    fprintf(stdout,"%d %d %e %e\n",i,k,Dmaterial_Daz[materials[k][j][i]-1],Dmaterial_Dbz[materials[k][j][i]-1]);*/
		  //Hyz[k][j][i] = Dmaterial_Daz[materials[k][j][i]-1]*Hyz[k][j][i]+Dmaterial_Dbz[materials[k][j][i]-1]*(Exy[k][j][i] + Exz[k][j][i] - Exy[k+1][j][i] - Exz[k+1][j][i]); 
		}
	      
		eh_vec[omp_get_thread_num()][k][0] = Exy[k][j][i] + Exz[k][j][i];eh_vec[omp_get_thread_num()][k][1] = 0.;
	      
	      }
	      k=K_tot;
	      eh_vec[omp_get_thread_num()][k][0] = Exy[k][j][i] + Exz[k][j][i];eh_vec[omp_get_thread_num()][k][1] = 0.;
	    
	      /*
		if( i==12 & j==12 ){
		for(k=0;k<K_tot;k++)
		fprintf(stdout,"%.10e ",eh_vec[omp_get_thread_num()][k][0]);
		fprintf(stdout,"\n");
		}
	      */

	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_h_z, N_h_z , pf_hyz[omp_get_thread_num()], pb_hyz[omp_get_thread_num()]);

	      for(k=0;k<K_tot;k++){
		Hyz[k][j][i] = ca_vec[omp_get_thread_num()][k]*Hyz[k][j][i] - cb_vec[omp_get_thread_num()][k]*eh_vec[omp_get_thread_num()][k][0]/((double) N_h_z);
	      }
	    }
 	
	}//PSTD, Hyz
      }//(dimension==THREE || dimension==TE)
      else{
	  
#pragma omp for
	for(k=0;k<=K_tot;k++)
	  for(j=0;j<J_tot;j++)
	    for(i=0;i<(I_tot+1);i++)
	      if(  !materials[k][j][i])
		Hxz[k][j][i] = 0.;
	      else
		Hxz[k][j][i] = 0.;

#pragma omp for
	//Hxy update
	for(k=0;k<=K_tot;k++)
	  for(j=0;j<J_tot;j++)
	    for(i=0;i<(I_tot+1);i++){
	      k_loc = k;
	      if( is_structure )
		if( k>Dzl[0] && k<(Dzl[0]+K) ){
		  if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		    k_loc = k - structure[i][1];
		  else if( (k-structure[i][1])>=(K+Dzl[0]) )
		    k_loc = Dzl[0]+K-1;
		  else
		    k_loc = Dzl[0]+1;
		}
	      if( !is_multilayer )
		array_ind = j;
	      else
		array_ind = (J_tot+1)*k_loc+j;
	      if(  !materials[k][j][i])
		Hxy[k][j][i] = Day[array_ind]*Hxy[k][j][i]+Dby[array_ind]*(Ezy[k][j][i] + Ezx[k][j][i] - Ezy[k][j+1][i] - Ezx[k][j+1][i]);
	      else
		Hxy[k][j][i] = Dmaterial_Day[materials[k][j][i]-1]*Hxy[k][j][i]+Dmaterial_Dby[materials[k][j][i]-1]*(Ezy[k][j][i] + Ezx[k][j][i] - Ezy[k][j+1][i] - Ezx[k][j+1][i]);
	    }

#pragma omp for
	//Hyx update
	for(k=0;k<=K_tot;k++)
	  for(j=0;j<(J_tot+1);j++)
	    for(i=0;i<I_tot;i++){
	      k_loc = k;
	      if( is_structure )
		if( k>Dzl[0] && k<(Dzl[0]+K) ){
		  if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		    k_loc = k - structure[i][1];
		  else if( (k-structure[i][1])>=(K+Dzl[0]) )
		    k_loc = Dzl[0]+K-1;
		  else
		    k_loc = Dzl[0]+1;
		}
	      if( !is_multilayer )
		array_ind = i;
	      else
		array_ind = (I_tot+1)*k_loc+i;
	      if(  !materials[k][j][i])
		Hyx[k][j][i] = Dax[array_ind]*Hyx[k][j][i]+Dbx[array_ind]*(Ezx[k][j][i+1] + Ezy[k][j][i+1] - Ezx[k][j][i] - Ezy[k][j][i]);  
	      else
		Hyx[k][j][i] = Dmaterial_Dax[materials[k][j][i]-1]*Hyx[k][j][i]+Dmaterial_Dbx[materials[k][j][i]-1]*(Ezx[k][j][i+1] + Ezy[k][j][i+1] - Ezx[k][j][i] - Ezy[k][j][i]); 
	    }

#pragma omp for
	for(k=0;k<=K_tot;k++){
	  for(j=0;j<(J_tot+1);j++)
	    for(i=0;i<I_tot;i++)
	      if(  !materials[k][j][i])
		Hyz[k][j][i] = 0.;
	      else
		Hyz[k][j][i] = 0.;
	}
      }
    
      if(dimension==THREE || dimension==TE){
	if( useCD ){//FDTD, Hzy
#pragma omp for
	  //Hzy update
	  for(k=0;k<(K_tot+1);k++)
	    for(j=0;j<J_tot;j++)
	      for(i=0;i<I_tot;i++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = j;
		else
		  array_ind = (J_tot+1)*k_loc+j;
		if(  !materials[k][j][i])
		  Hzy[k][j][i] = Day[array_ind]*Hzy[k][j][i]+Dby[array_ind]*(Exy[k][j+1][i] + Exz[k][j+1][i] - Exy[k][j][i] - Exz[k][j][i]); 
		else
		  Hzy[k][j][i] = Dmaterial_Day[materials[k][j][i]-1]*Hzy[k][j][i]+Dmaterial_Dby[materials[k][j][i]-1]*(Exy[k][j+1][i] + Exz[k][j+1][i] - Exy[k][j][i] - Exz[k][j][i]); 
	      }
	}//FDTD, Hzy
	else{//PSTD, Hzy
#pragma omp for
	  //Hzy update
	  for(k=0;k<(K_tot+1);k++)
	    for(i=0;i<I_tot;i++){
	      for(j=0;j<J_tot;j++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = j;
		else
		  array_ind = (J_tot+1)*k_loc+j;
		if(  !materials[k][j][i]){
		  ca_vec[omp_get_thread_num()][j] = Day[array_ind];
		  cb_vec[omp_get_thread_num()][j] = Dby[array_ind];
		  //	      Hzy[k][j][i] = Day[array_ind]*Hzy[k][j][i]+Dby[array_ind]*(Exy[k][j+1][i] + Exz[k][j+1][i] - Exy[k][j][i] - Exz[k][j][i]); 
		}
		else{
		  ca_vec[omp_get_thread_num()][j] = Dmaterial_Day[materials[k][j][i]-1];
		  cb_vec[omp_get_thread_num()][j] = Dmaterial_Dby[materials[k][j][i]-1];
		  //	      Hzy[k][j][i] = Dmaterial_Day[materials[k][j][i]-1]*Hzy[k][j][i]+Dmaterial_Dby[materials[k][j][i]-1]*(Exy[k][j+1][i] + Exz[k][j+1][i] - Exy[k][j][i] - Exz[k][j][i]); 
		}

		eh_vec[omp_get_thread_num()][j][0] = Exy[k][j][i] + Exz[k][j][i];eh_vec[omp_get_thread_num()][j][1] = 0.;
	      }
	      j=J_tot;
	      eh_vec[omp_get_thread_num()][j][0] = Exy[k][j][i] + Exz[k][j][i];eh_vec[omp_get_thread_num()][j][1] = 0.;

	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_h_y, N_h_y , pf_hzy[omp_get_thread_num()], pb_hzy[omp_get_thread_num()]);

	      for(j=0;j<J_tot;j++){
		Hzy[k][j][i] = ca_vec[omp_get_thread_num()][j]*Hzy[k][j][i] + cb_vec[omp_get_thread_num()][j]*eh_vec[omp_get_thread_num()][j][0]/((double) N_h_y);
	      }
	    }
	}//PSTD, Hzy

	if( useCD ){//FDTD, Hzx
#pragma omp for
	  //Hzx update
	  for(k=0;k<(K_tot+1);k++)
	    for(j=0;j<J_tot_bound;j++)
	      for(i=0;i<I_tot;i++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = i;
		else
		  array_ind = (I_tot+1)*k_loc+i;
		if(  !materials[k][j][i])
		  Hzx[k][j][i] = Dax[array_ind]*Hzx[k][j][i]+Dbx[array_ind]*(Eyx[k][j][i] + Eyz[k][j][i] - Eyx[k][j][i+1] - Eyz[k][j][i+1]);
		else
		  Hzx[k][j][i] = Dmaterial_Dax[materials[k][j][i]-1]*Hzx[k][j][i]+Dmaterial_Dbx[materials[k][j][i]-1]*(Eyx[k][j][i] + Eyz[k][j][i] - Eyx[k][j][i+1] - Eyz[k][j][i+1]);
	      }
	}//FDTD, Hzx
	else{//PSTD, Hzx
#pragma omp for
	  //Hzx update
	  for(k=0;k<(K_tot+1);k++)
	    for(j=0;j<J_tot_bound;j++){
	      for(i=0;i<I_tot;i++){
		k_loc = k;
		if( is_structure )
		  if( k>Dzl[0] && k<(Dzl[0]+K) ){
		    if( (k-structure[i][1])<(K+Dzl[0]) && (k-structure[i][1])>Dzl[0] )
		      k_loc = k - structure[i][1];
		    else if( (k-structure[i][1])>=(K+Dzl[0]) )
		      k_loc = Dzl[0]+K-1;
		    else
		      k_loc = Dzl[0]+1;
		  }
		if( !is_multilayer )
		  array_ind = i;
		else
		  array_ind = (I_tot+1)*k_loc+i;
		if(  !materials[k][j][i]){
		  //		Hzx[k][j][i] = Dax[array_ind]*Hzx[k][j][i]+Dbx[array_ind]*(Eyx[k][j][i] + Eyz[k][j][i] - Eyx[k][j][i+1] - Eyz[k][j][i+1]);
		  ca_vec[omp_get_thread_num()][i] = Dax[array_ind];
		  cb_vec[omp_get_thread_num()][i] = Dbx[array_ind];
		}
		else{
		  //		Hzx[k][j][i] = Dmaterial_Dax[materials[k][j][i]-1]*Hzx[k][j][i]+Dmaterial_Dbx[materials[k][j][i]-1]*(Eyx[k][j][i] + Eyz[k][j][i] - Eyx[k][j][i+1] - Eyz[k][j][i+1]);
		  ca_vec[omp_get_thread_num()][i] = Dmaterial_Dax[materials[k][j][i]-1];
		  cb_vec[omp_get_thread_num()][i] = Dmaterial_Dbx[materials[k][j][i]-1];
		}

		eh_vec[omp_get_thread_num()][i][0] = Eyx[k][j][i] + Eyz[k][j][i];eh_vec[omp_get_thread_num()][i][1] = 0.;
	      }
	      i=I_tot;
	      eh_vec[omp_get_thread_num()][i][0] = Eyx[k][j][i] + Eyz[k][j][i];eh_vec[omp_get_thread_num()][i][1] = 0.;

	    
	      first_derivative( eh_vec[omp_get_thread_num()], eh_vec[omp_get_thread_num()],
				dk_h_x, N_h_x , pf_hzx[omp_get_thread_num()], pb_hzx[omp_get_thread_num()]);

	      for(i=0;i<I_tot;i++){
		Hzx[k][j][i] = ca_vec[omp_get_thread_num()][i]*Hzx[k][j][i] - cb_vec[omp_get_thread_num()][i]*eh_vec[omp_get_thread_num()][i][0]/((double) N_h_x);
	      }
	    
	    }
	}//PSTD, Hzx
      }//(dimension==THREE || dimension==TE)
    }//end parallel
    if(TIME_EXEC){
      time_1=omp_get_wtime();
      secs= time_1-time_0;
      fprintf(stdout,"%.03e ",secs);
      time_0=time_1;
    }
  
    //fprintf(stderr,"Pos 11b:\n");
    //update terms for self consistency across scattered/total interface - E updates
    if(sourcemode == sm_steadystate){//steadystate
      commonPhase = exp(-I*fmod(omega_an[0]*time_E,2.*dcpi));
      commonAmplitude = linearRamp(time_E, 1./(*omega_an/(2*dcpi)), ramp_width);
      for(k=((int)K0[0]);k<=((int)K1[0]);k++)
	for(j=((int)J0[0]);j<=((int)J1[0]);j++){
	  if( (int)I0[1] ){//Perform across I0

	    if( !is_multilayer )
	      array_ind = (int)I0[0] - 1;
	    else
	      array_ind = (I_tot+1)*k+(int)I0[0] - 1;

	    if( j < ((int)J1[0]) )
	      Hzx[k][j][((int)I0[0])-1] = Hzx[k][j][((int)I0[0])-1] + Dbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][0] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][0]));
	    if( k < ((int)K1[0]) || dimension==TM )	 	 
	      Hyx[k][j][((int)I0[0])-1] = Hyx[k][j][((int)I0[0])-1] - Dbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][1] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][1]));
	    
	  }
	  if( (int)I1[1] ){//Perform across I1

	    if( !is_multilayer )
	      array_ind = (int)I1[0];
	    else
	      array_ind = (I_tot+1)*k+(int)I1[0];

	    if( j < ((int)J1[0]) )
	      Hzx[k][j][((int)I1[0])] = Hzx[k][j][((int)I1[0])] - Dbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][4] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][4]));
	    if( k < ((int)K1[0]) || dimension==TM )	 
	      Hyx[k][j][((int)I1[0])] = Hyx[k][j][((int)I1[0])] + Dbx[array_ind]*real(commonAmplitude*commonPhase*(IsourceR[k-((int)K0[0])][j-((int)J0[0])][5] + I*IsourceI[k-((int)K0[0])][j-((int)J0[0])][5]));
	  }
	}
      
      for(k=((int)K0[0]);k<=((int)K1[0]);k++)
	for(i=((int)I0[0]);i<=((int)I1[0]);i++){
	  if( (int)J0[1] ){//Perform across J0

	    if( !is_multilayer )
	      array_ind = (int)J0[0];
	    else
	      array_ind = (J_tot+1)*k+(int)J0[0];
	 
	    if( i < ((int)I1[0]) )	 
	      Hzy[k][((int)J0[0])-1][i] = Hzy[k][((int)J0[0])-1][i] - Dby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][0] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][0]));

	    if( k < ((int)K1[0]) || dimension==TM )	 
	      Hxy[k][((int)J0[0])-1][i] = Hxy[k][((int)J0[0])-1][i] + Dby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][1] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][1]));
	  }
	  if( (int)J1[1] ){//Perform across J1

	    if( !is_multilayer )
	      array_ind = (int)J1[0];
	    else
	      array_ind = (J_tot+1)*k+(int)J1[0];

	    if( i < ((int)I1[0]) )	 
	      Hzy[k][((int)J1[0])][i] = Hzy[k][((int)J1[0])][i] + Dby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][4] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][4]));
	    if( k < ((int)K1[0]) || dimension==TM )	 
	      Hxy[k][((int)J1[0])][i] = Hxy[k][((int)J1[0])][i] - Dby[array_ind]*real(commonAmplitude*commonPhase*(JsourceR[k-((int)K0[0])][i-((int)I0[0])][5] + I*JsourceI[k-((int)K0[0])][i-((int)I0[0])][5]));
	  }
	}
      
      for(j=((int)J0[0]);j<=((int)J1[0]);j++)
	for(i=((int)I0[0]);i<=((int)I1[0]);i++){
	  if( (int)K0[1] ){//Perform across K0
	    if( i < ((int)I1[0]) )	 
	      Hyz[((int)K0[0])-1][j][i] = Hyz[((int)K0[0])-1][j][i] + Dbz[((int)K0[0])-1]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][0] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][0]));
	    if( j < ((int)J1[0]) )	 
	      Hxz[((int)K0[0])-1][j][i] = Hxz[((int)K0[0])-1][j][i] - Dbz[((int)K0[0])-1]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][1] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][1]));
	  }
	  if( (int)K1[1] ){//Perform across K1
	    if( i < ((int)I1[0]) )	 
	      Hyz[((int)K1[0])][j][i] = Hyz[((int)K1[0])][j][i] - Dbz[((int)K1[0])]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][4] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][4]));
	    if( j < ((int)J1[0]) )	 
	      Hxz[((int)K1[0])][j][i] = Hxz[((int)K1[0])][j][i] + Dbz[((int)K1[0])]*real(commonAmplitude*commonPhase*(KsourceR[j-((int)J0[0])][i-((int)I0[0])][5] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][5]));
	  }
	}
      fte = real(commonAmplitude*commonPhase);
    }
    else if(sourcemode==1){//pulsed
      //fprintf(stderr,"Pos 11c\n");
      if(J_tot==0){
	//fprintf(stderr,"Pos 11d\n");
	j=0;
	for(i=0;i<(I_tot+1);i++){
	  Hxz[((int)K0[0])-1][j][i] = Hxz[((int)K0[0])-1][j][i] - Dbz[((int)K0[0])-1]*real((KsourceR[0][i-((int)I0[0])][1] + I*KsourceI[0][i-((int)I0[0])][1])*(-1.*I)*exp(-I*fmod(omega_an[0]*(time_E - to_l[0]),2*dcpi)))*exp(-1.*dcpi*pow((time_E - to_l[0])/(hwhm[0]),2. ));
	  //broadband source term
	  if(eyi_present)
	    Hxz[((int)K0[0])-1][j][i] = Hxz[((int)K0[0])-1][j][i] - Dbz[((int)K0[0])-1]*eyi[tind][j][i];
	}
	//fprintf(stderr,"Pos 11e\n");
	for(i=0;i<I_tot;i++){
	  Hyz[((int)K0[0])-1][j][i] = Hyz[((int)K0[0])-1][j][i] + Dbz[((int)K0[0])-1]*real((KsourceR[0][i-((int)I0[0])][0] + I*KsourceI[0][i-((int)I0[0])][0])*(-1.*I)*exp(-I*fmod(omega_an[0]*(time_E - to_l[0]),2*dcpi)))*exp(-1.*dcpi*pow((time_E - to_l[0])/(hwhm[0]) ,2.)); 
	  //broadband source term
	  if(exi_present)
	    Hyz[((int)K0[0])-1][j][i] = Hyz[((int)K0[0])-1][j][i] + Dbz[((int)K0[0])-1]*exi[tind][j][i];
	  //if(i==511)
	  //  fprintf(stdout,"%e\n",Dbz[((int)K0[0])-1]*exi[tind][j][i]);
	}
	//fprintf(stderr,"Pos 11f\n");
      }
      else{
	//fprintf(stderr,"Pos 11g\n");
	for(j=0;j<J_tot;j++)
	  for(i=0;i<(I_tot+1);i++){
	    Hxz[((int)K0[0])-1][j][i] = Hxz[((int)K0[0])-1][j][i] - Dbz[((int)K0[0])-1]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][1] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][1])*(-1.*I)*exp(-I*fmod(omega_an[0]*(time_E - to_l[0]),2*dcpi)))*exp(-1.*dcpi*pow((time_E - to_l[0])/(hwhm[0]),2. ));
	    //broadband source term
	    if(eyi_present)
	      Hxz[((int)K0[0])-1][j][i] = Hxz[((int)K0[0])-1][j][i] - Dbz[((int)K0[0])-1]*eyi[tind][j][i];
	  }
	//fprintf(stderr,"Pos 11h\n");
	for(j=0;j<(J_tot+1);j++)
	  for(i=0;i<I_tot;i++){
	    Hyz[((int)K0[0])-1][j][i] = Hyz[((int)K0[0])-1][j][i] + Dbz[((int)K0[0])-1]*real((KsourceR[j-((int)J0[0])][i-((int)I0[0])][0] + I*KsourceI[j-((int)J0[0])][i-((int)I0[0])][0])*(-1.*I)*exp(-I*fmod(omega_an[0]*(time_E - to_l[0]),2*dcpi)))*exp(-1.*dcpi*pow((time_E - to_l[0])/(hwhm[0]) ,2.)); 
	    //broadband source term
	    if(exi_present)
	      Hyz[((int)K0[0])-1][j][i] = Hyz[((int)K0[0])-1][j][i] + Dbz[((int)K0[0])-1]*exi[tind][j][i];
	  }
	//fprintf(stderr,"Pos 11i\n");
      }
      fte = real((-1.*I)*exp(-I*fmod(omega_an[0]*(time_E - to_l[0]),2*dcpi)))*exp(-1.*dcpi*pow((time_E - to_l[0])/(hwhm[0]) ,2.));
      //fprintf(stderr,"Pos 11j\n");
    }
    if(TIME_EXEC){
      time_1=omp_get_wtime();
      secs= time_1-time_0;
      fprintf(stdout,"%.03e ",secs);
      time_0=time_1;
    }
  
    if( exphasorssurface || exphasorsvolume || exdetintegral || (nvertices > 0) ){
      if(sourcemode==sm_steadystate){
	extractPhasorENorm(&E_norm_an, fte, tind, *omega_an, *dt, Nsteps);
	extractPhasorHNorm(&H_norm_an, fth, tind, *omega_an, *dt, Nsteps);
	for(int ifx=0;ifx<N_f_ex_vec;ifx++){
	  extractPhasorENorm(&E_norm[ifx], fte, tind, f_ex_vec[ifx]*2*dcpi, *dt, Nsteps);
	  extractPhasorHNorm(&H_norm[ifx], fth, tind, f_ex_vec[ifx]*2*dcpi, *dt, Nsteps);
	}
      }
      else{
	if( (tind-start_tind) % Np == 0){
	  extractPhasorENorm(&E_norm_an, fte, tind, *omega_an, *dt, Npe);
	  extractPhasorHNorm(&H_norm_an, fth, tind, *omega_an, *dt, Npe);
	  for(int ifx=0;ifx<N_f_ex_vec;ifx++){
	    extractPhasorENorm(&E_norm[ifx], fte, tind, f_ex_vec[ifx]*2*dcpi, *dt, Npe);
	    extractPhasorHNorm(&H_norm[ifx], fth, tind, f_ex_vec[ifx]*2*dcpi, *dt, Npe);
	  }
	}
	
	
      }
      
    }
    if(TIME_EXEC){
      time_1=omp_get_wtime();
      secs= time_1-time_0;
      fprintf(stdout,"%.03e\n",secs);
      time_0=time_1;
    }
    
    if(  (((double)time(NULL)) - t0)>1 ){
 
      maxfield = 0.;
      for(k=0;k<(K_tot+1);k++)
	for(j=0;j<(J_tot+1);j++)
	  for(i=0;i<(I_tot+1);i++){
	    tempfield = fabs(Exy[k][j][i]+Exz[k][j][i]);
	    if( maxfield < tempfield )
	      maxfield = tempfield;
	    tempfield = fabs(Eyx[k][j][i]+Eyz[k][j][i]);
	    if( maxfield < tempfield )
	      maxfield = tempfield;
	    tempfield = fabs(Ezx[k][j][i]+Ezy[k][j][i]);	
	    if( maxfield < tempfield )
	      maxfield = tempfield;
	    tempfield = fabs(Hxy[k][j][i]+Hxz[k][j][i]);	
	    if( maxfield < tempfield )
	      maxfield = tempfield;
	    tempfield = fabs(Hyx[k][j][i]+Hyz[k][j][i]);	
	    if( maxfield < tempfield )
	      maxfield = tempfield;
	    tempfield = fabs(Hzx[k][j][i]+Hzy[k][j][i]);	
	    if( maxfield < tempfield )
	      maxfield = tempfield;
	  }
fprintf(stdout,"Iterating: %d %e\n",tind,maxfield);
 
//     fprintf(stdout,"Iterating: %d\n",tind);
      t0 = double(time(NULL));
    }

    if((sourcemode==sm_steadystate)&&(tind == (Nt[0]-1))&&(runmode==rm_complete)&& exphasorsvolume ){
      fprintf(stdout, "Iteration limit reached, setting output fields to last complete DFT\n");
      copyPhasors(mxGetPr( (mxArray *)dummy_array[0]),mxGetPi( (mxArray *)dummy_array[0]),mxGetPr( (mxArray *)dummy_array[1]),mxGetPi( (mxArray *)dummy_array[1]),mxGetPr( (mxArray *)dummy_array[2]),mxGetPi( (mxArray *)dummy_array[2]),
		  mxGetPr( (mxArray *)plhs[0]),mxGetPi( (mxArray *)plhs[0]),mxGetPr( (mxArray *)plhs[1]),mxGetPi( (mxArray *)plhs[1]),mxGetPr( (mxArray *)plhs[2]),mxGetPi( (mxArray *)plhs[2]),
		  (int)mxGetNumberOfElements( (mxArray *)plhs[0]));
    }

    fflush(stdout);
    //fprintf(stderr,"%s %d %d\n",tdfdirstr, strcmp(tdfdirstr,""),!strcmp(tdfdirstr,""));
    if( strcmp(tdfdirstr,"") ){
      //fprintf(stderr,"tind:%d\n",tind);
      if((tind % Np)==0){
      MATFile *toutfile;
      char toutputfilename[512];
      int kc=0;
      int ic=0;
      //fprintf(stderr,"Pos td01\n");
      for(i=0;i<I_tot;i++){
	kc = 0;
	if( (i % skip_tdf) == 0){
	  for(k=0;k<K_tot;k++)
	    if( (k % skip_tdf) == 0){
	      ex_tdf[kc++][ic] = Exy[k][0][i] + Exz[k][0][i];
	      //fprintf(stderr,"%d %d\n",kc,ic);
	    }
	ic++;
	}
      }
      //fprintf(stderr,"Pos td02\n");
      
      sprintf(toutputfilename,"%s/ex_%06d.mat",tdfdirstr,tind);
      //fprintf(stderr,"Pos td03\n");
      fprintf(stderr,"time domain output: %s\n",toutputfilename);
      toutfile = matOpen(toutputfilename, "w");
      //fprintf(stderr,"Pos td04\n");
      matPutVariable(toutfile, "ex_tdf", (mxArray *)ex_tdf_array);
      //fprintf(stderr,"Pos td05\n");
      matClose(toutfile);
      //fprintf(stderr,"Pos td06\n");

      }
    }

    /*write out fdtdgrid to a file*/
    /*
     MATFile *toutfile;
     char toutputfilename[100];
     if(tind % Np == 0){
     //if(tind <= 1000){
       sprintf(toutputfilename,"tdata/fdtdgrid_%04d.mat",tind);
       toutfile = matOpen(toutputfilename, "w");
       matPutVariable(toutfile, "fdtdgrid", (mxArray *)prhs[0]);
       matClose(toutfile);
       } 
    */
    /*write out fdtdgrid to a file*/

  }//end of main iteration loop
  if(TIME_MAIN_LOOP){
    time_ml_1 = omp_get_wtime();
    fprintf(stdout,"# Time elasped in main loop: %e\n",time_ml_1-time_ml_0);
  }
  //save state of fdtdgrid

  //fprintf(stderr,"Pos 12\n");
  if(runmode == rm_complete && exphasorsvolume){
    normaliseVolume(ExR,ExI,EyR,EyI,EzR,EzI,
		    pind_il,pind_iu,
		    pind_jl,pind_ju,
		    pind_kl,pind_ku,
		    E_norm_an);

    normaliseVolume(HxR,HxI,HyR,HyI,HzR,HzI,
		    pind_il,pind_iu,
		    pind_jl,pind_ju,
		    pind_kl,pind_ku,
		    H_norm_an);
  }
 
  //fprintf(stderr,"Pos 13\n");
  if(runmode == rm_complete && exphasorssurface)
    for(int ifx=0;ifx<N_f_ex_vec;ifx++){
      normaliseSurface( surface_EHr[ifx], surface_EHi[ifx],  
			surface_vertices, n_surface_vertices, E_norm[ifx], H_norm[ifx]);
      //fprintf(stderr,"E_norm[%d]: %e %e\n",ifx,real(E_norm[ifx]),imag(E_norm[ifx]));
    }

if(runmode == rm_complete && (nvertices>0) )
   for(int ifx=0;ifx<N_f_ex_vec;ifx++){
      normaliseVertices( camplitudesR[ifx], camplitudesI[ifx],  
			vertices, nvertices,
			 components, ncomponents,
			 E_norm[ifx], H_norm[ifx]);
      //fprintf(stderr,"E_norm[%d]: %e %e\n",ifx,real(E_norm[ifx]),imag(E_norm[ifx]));
    }


  //fprintf(stderr,"Pos 14\n");
  if( sourcemode==sm_pulsed && runmode==rm_complete && exdetintegral ){
    for(int im=0;im<Ndetmodes;im++)
      for(int ifx=0;ifx<N_f_ex_vec;ifx++){
	Idx[ifx][im]=Idx[ifx][im]/E_norm[ifx];
	Idy[ifx][im]=Idy[ifx][im]/E_norm[ifx];

	Idx_re[ifx][im] = real(Idx[ifx][im]);
	Idx_im[ifx][im] = imag(Idx[ifx][im]);
	
	Idy_re[ifx][im] = real(Idy[ifx][im]);
	Idy_im[ifx][im] = imag(Idy[ifx][im]);
	
      }
  }
  
  //now find the maximum absolute value of residual field in the grid
  for(k=0;k<(K_tot+1);k++)
    for(j=0;j<(J_tot+1);j++)
      for(i=0;i<(I_tot+1);i++){
	tempfield = fabs(Exy[k][j][i]+Exz[k][j][i]);
	if( maxfield < tempfield )
	  maxfield = tempfield;
	tempfield = fabs(Eyx[k][j][i]+Eyz[k][j][i]);
	if( maxfield < tempfield )
	  maxfield = tempfield;
	tempfield = fabs(Ezx[k][j][i]+Ezy[k][j][i]);	
	if( maxfield < tempfield )
	  maxfield = tempfield;
	tempfield = fabs(Hxy[k][j][i]+Hxz[k][j][i]);	
	if( maxfield < tempfield )
	  maxfield = tempfield;
	tempfield = fabs(Hyx[k][j][i]+Hyz[k][j][i]);	
	if( maxfield < tempfield )
	  maxfield = tempfield;
	tempfield = fabs(Hzx[k][j][i]+Hzy[k][j][i]);	
	if( maxfield < tempfield )
	  maxfield = tempfield;
      }
  
  //fprintf(stderr,"Pos 15\n");
  //noe set the output
  ndims   = 2;
  dims[0] = 1;
  dims[1] = 1;
  plhs[25] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
  *mxGetPr( (mxArray *)plhs[25]) = maxfield;
  
  if( runmode == rm_complete && exphasorsvolume )
    setGridLabels(x_grid_labels     ,y_grid_labels     ,z_grid_labels,
		  x_grid_labels_out ,y_grid_labels_out ,z_grid_labels_out,
		  pind_il           ,pind_iu           ,pind_jl          ,pind_ju ,pind_kl ,pind_ku);

  //fprintf(stderr,"Pos 15_m1\n");
  if( runmode==rm_complete && exphasorsvolume){
    //now interpolate over the extracted phasors
    if(dimension==THREE){
      fprintf(stderr,"mxInterpolateFieldCentralE: %d %d %d \n",pind_iu - pind_il - 1,pind_ju - pind_jl - 1,pind_ku - pind_kl - 1);
      //fprintf(stderr,"Pos 15_m1a\n");
      mxInterpolateFieldCentralE( plhs[0]   , plhs[1]  , plhs[2]  ,
				  &plhs[13] ,&plhs[14] , &plhs[15],
				  2, pind_iu - pind_il - 1, 2, pind_ju - pind_jl - 1, 2, pind_ku - pind_kl - 1);
      //fprintf(stderr,"Pos 15_m1b\n");
      
    }
    else if(dimension==TE)
      mxInterpolateFieldCentralE_TE( plhs[0]   , plhs[1]  , plhs[2]  ,
				     &plhs[13] ,&plhs[14] , &plhs[15],
				     2, pind_iu - pind_il - 1, 2, pind_ju - pind_jl - 1, 0, 0);
    else
      mxInterpolateFieldCentralE_TM( plhs[0]   , plhs[1]  , plhs[2]  ,
				     &plhs[13] ,&plhs[14] , &plhs[15],
				     2, pind_iu - pind_il - 1, 2, pind_ju - pind_jl - 1, 0, 0);
    if(dimension==THREE)
      mxInterpolateFieldCentralH( plhs[3]   , plhs[4]  , plhs[5],
				  &plhs[16] ,&plhs[17] , &plhs[18], 
				  2, pind_iu - pind_il - 1, 2, pind_ju - pind_jl - 1, 2, pind_ku - pind_kl - 1);
    else if(dimension==TE)
      mxInterpolateFieldCentralH_TE( plhs[3]   , plhs[4]  , plhs[5],
				     &plhs[16] ,&plhs[17] , &plhs[18], 
				     2, pind_iu - pind_il - 1, 2, pind_ju - pind_jl - 1, 0, 0);
    else
      mxInterpolateFieldCentralH_TM( plhs[3]   , plhs[4]  , plhs[5],
				     &plhs[16] ,&plhs[17] , &plhs[18], 
				     2, pind_iu - pind_il - 1, 2, pind_ju - pind_jl - 1, 0, 0);

    //fprintf(stderr,"Pos 15a\n");
    //now set up the grid labels for the interpolated fields
    label_dims[0] = 1;
    label_dims[1] = pind_iu - pind_il - 2;
    plhs[19] = mxCreateNumericArray( 2, (const mwSize *)label_dims, mxDOUBLE_CLASS, mxREAL); //x
    //fprintf(stderr,"Pos 15b\n");
    label_dims[0] = 1;
    label_dims[1] = pind_ju - pind_jl - 2;
    if(label_dims[1]<1)
      label_dims[1] = 1;
    //fprintf(stderr,"creating plhs[20]: %d,%d\n",label_dims[0],label_dims[1]);
    plhs[20] = mxCreateNumericArray( 2, (const mwSize *)label_dims, mxDOUBLE_CLASS, mxREAL); //y
    //fprintf(stderr,"Pos 15c\n");
    label_dims[0] = 1;
    if(dimension==THREE)
      label_dims[1] = pind_ku - pind_kl - 2;
    else
      label_dims[1] = 1;
    //fprintf(stderr,"Pos 15d\n");
    plhs[21] = mxCreateNumericArray( 2, (const mwSize *)label_dims, mxDOUBLE_CLASS, mxREAL); //z
    //fprintf(stderr,"Pos 15e\n");
    if(dimension==THREE){
      //fprintf(stderr,"Pos 15a-1\n");
      setGridLabels(x_grid_labels_out ,y_grid_labels_out ,z_grid_labels_out,
		    mxGetPr( (mxArray *)plhs[19]) ,mxGetPr( (mxArray *)plhs[20]) ,mxGetPr( (mxArray *)plhs[21]),
		    2, pind_iu - pind_il - 1, 2, pind_ju - pind_jl - 1, 2, pind_ku - pind_kl - 1);
    }
    else
      setGridLabels(x_grid_labels_out ,y_grid_labels_out ,z_grid_labels_out,
		    mxGetPr( (mxArray *)plhs[19]) ,mxGetPr( (mxArray *)plhs[20]) ,mxGetPr( (mxArray *)plhs[21]),
		    2, pind_iu - pind_il - 1, 2, pind_ju - pind_jl - 1, 0, 0);
    //fprintf(stderr,"Pos 15f\n");
  }
  else{
    mwSize *emptydims;
    emptydims = (mwSize *)malloc(2*sizeof(mwSize));
    int emptyloop;
    emptydims[0] = 0;
    emptydims[1] = 0;
    for(emptyloop=13;emptyloop<=18;emptyloop++)
      plhs[emptyloop] = mxCreateNumericArray( 2, (const mwSize *)emptydims, mxDOUBLE_CLASS, mxCOMPLEX);
    for(emptyloop=19;emptyloop<=21;emptyloop++)
      plhs[emptyloop] = mxCreateNumericArray( 2, (const mwSize *)emptydims, mxDOUBLE_CLASS, mxCOMPLEX);
    free(emptydims);
  
  }


  //fprintf(stderr,"Pos 16\n");
  /*Now export 3 matrices, a vertex list, a matrix of complex amplitudes at 
    these vertices and a list of facets*/
  if( exphasorssurface && runmode==rm_complete ){
    //first regenerate the mesh since we threw away the facet list before iterating
    mxArray *dummy_vertex_list;
    if(J_tot==0)
      conciseCreateBoundary(cuboid[0], cuboid[1],cuboid[4], cuboid[5], 
			    &dummy_vertex_list, &mx_surface_facets);
    else
      conciseTriangulateCuboidSkip( cuboid[0], cuboid[1], cuboid[2], cuboid[3], cuboid[4], cuboid[5], 
				    phasorinc[0],phasorinc[1],phasorinc[2],
				    &dummy_vertex_list, &mx_surface_facets);
    mxDestroyArray(dummy_vertex_list);
    mxArray *vertex_list;
    double **vertex_list_ptr;
    ndims   = 2;
    dims[0] = n_surface_vertices;
    dims[1] = 3;
    vertex_list = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
    vertex_list_ptr = castMatlab2DArray( mxGetPr( (mxArray *)vertex_list), dims[0], dims[1]);
    
    //now populate the vertex list
    for(i=0;i<n_surface_vertices;i++){
      
      vertex_list_ptr[0][i] = x_grid_labels[ surface_vertices[0][i] ];
      vertex_list_ptr[1][i] = y_grid_labels[ surface_vertices[1][i] ];
      vertex_list_ptr[2][i] = z_grid_labels[ surface_vertices[2][i] ];
    }
    //assign outputs
    plhs[22] = vertex_list;
    plhs[23] = mx_surface_amplitudes;
    plhs[24] = mx_surface_facets;
    
    freeCastMatlab2DArray(vertex_list_ptr);
  }
  else{//still set outputs
    ndims   = 2;
    dims[0] = 0;
    dims[1] = 0;
    plhs[22] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
    plhs[23] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
    plhs[24] = mxCreateNumericArray( ndims, (const mwSize *)dims, mxDOUBLE_CLASS, mxREAL);
  }


  /*End of FDTD iteration*/

  //fprintf(stderr,"Pos 17\n");
  /*Free the additional data structures used to cast the matlab arrays*/
  if( exphasorssurface && runmode==rm_complete ){
    freeCastMatlab2DArrayInt(surface_vertices);
    freeCastMatlab3DArray(surface_EHr,N_f_ex_vec);
    freeCastMatlab3DArray(surface_EHi,N_f_ex_vec);

    mxDestroyArray(mx_surface_vertices);
  }

  if(nvertices>0){
    freeCastMatlab2DArrayInt(vertices);
    freeCastMatlab3DArray(camplitudesR,N_f_ex_vec);
    freeCastMatlab3DArray(camplitudesI,N_f_ex_vec);
    
  }

  if(exdetintegral==1){
    freeCastMatlab2DArray(Pupil);
    freeCastMatlab2DArray(Idx_re);freeCastMatlab2DArray(Idx_im);
    freeCastMatlab2DArray(Idy_re);freeCastMatlab2DArray(Idy_im);
    for(int ifx=0;ifx<N_f_ex_vec;ifx++){
      free(Idx[ifx]);free(Idy[ifx]);
    }
    free(Idx);free(Idy);
    for(int j=0;j<Nfy_vec;j++){
      for(int i=0;i<Nfx_vec;i++){
	free(Dx_tilde[j][i]);free(Dy_tilde[j][i]);
      }
      free(Dx_tilde[j]);free(Dy_tilde[j]);
    }
    free(Dx_tilde);free(Dy_tilde);
        
    fftw_free(Ex_t);fftw_free(Ey_t);
    fftw_destroy_plan(pey_t);
    fftw_destroy_plan(pex_t);
      
    /*
      for(int j=0;j<(J_tot-*Dyl-*Dyu);j++){
      free(Ex_t_cm[j]);free(Ey_t_cm[j]);
      }
      //fprintf(stderr,"Position 9\n");
      free(Ex_t_cm);free(Ey_t_cm);
      //fprintf(stderr,"Position 10\n");
    */
  }
  if(exi_present)
    freeCastMatlab3DArray(exi,*Nt);
  if(eyi_present)
    freeCastMatlab3DArray(eyi,*Nt);

   if( strcmp(tdfdirstr,"") )
     freeCastMatlab2DArray(ex_tdf);
 

  //fprintf(stderr,"Pos 18\n");
  if(dimension==THREE){
    freeCastMatlab3DArray(Exy,K_tot+1);
    freeCastMatlab3DArray(Exz,K_tot+1);
    freeCastMatlab3DArray(Eyx,K_tot+1);
    freeCastMatlab3DArray(Eyz,K_tot+1);
    freeCastMatlab3DArray(Ezx,K_tot+1);
    freeCastMatlab3DArray(Ezy,K_tot+1);

    freeCastMatlab3DArray(Hxy,K_tot+1);
    freeCastMatlab3DArray(Hxz,K_tot+1);
    freeCastMatlab3DArray(Hyx,K_tot+1);
    freeCastMatlab3DArray(Hyz,K_tot+1);
    freeCastMatlab3DArray(Hzx,K_tot+1);
    freeCastMatlab3DArray(Hzy,K_tot+1);
  }
  else{
    freeCastMatlab3DArray(Exy,0);
    freeCastMatlab3DArray(Exz,0);
    freeCastMatlab3DArray(Eyx,0);
    freeCastMatlab3DArray(Eyz,0);
    freeCastMatlab3DArray(Ezx,0);
    freeCastMatlab3DArray(Ezy,0);
    
    freeCastMatlab3DArray(Hxy,0);
    freeCastMatlab3DArray(Hxz,0);
    freeCastMatlab3DArray(Hyx,0);
    freeCastMatlab3DArray(Hyz,0);
    freeCastMatlab3DArray(Hzx,0);
    freeCastMatlab3DArray(Hzy,0);
  }

  //fprintf(stderr,"Pos 19\n");
  //this should be fixed to take into account the change to steady state matrix size
  //when we have a PML layer of zero thickness
  if( runmode == rm_complete && exphasorsvolume ){
    if( dimension==THREE ){
      freeCastMatlab3DArray(ExR,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(ExI,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(EyR,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(EyI,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(EzR,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(EzI,K_tot - *Dzu - *Dzl - 3 + 1);
    
      freeCastMatlab3DArray(HxR,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(HxI,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(HyR,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(HyI,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(HzR,K_tot - *Dzu - *Dzl - 3 + 1);
      freeCastMatlab3DArray(HzI,K_tot - *Dzu - *Dzl - 3 + 1);
    }
    else{
      freeCastMatlab3DArray(ExR,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(ExI,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(EyR,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(EyI,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(EzR,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(EzI,pind_ku - pind_kl + 1);
      
      freeCastMatlab3DArray(HxR,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(HxI,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(HyR,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(HyI,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(HzR,pind_ku - pind_kl + 1);
      freeCastMatlab3DArray(HzI,pind_ku - pind_kl + 1);
    }
  }

  //fprintf(stderr,"Pos 20\n");
  if( (int)I0[1] || (int)I1[1] ){
    freeCastMatlab3DArray(IsourceI,((int)(K1[0]-K0[0]+1.)));
    freeCastMatlab3DArray(IsourceR,((int)(K1[0]-K0[0]+1.)));
  }
  if( (int)J0[1] || (int)J1[1] ){
    freeCastMatlab3DArray(JsourceI,((int)(K1[0]-K0[0]+1.)));
    freeCastMatlab3DArray(JsourceR,((int)(K1[0]-K0[0]+1.)));
  }
  if( (int)K0[1] || (int)K1[1] ){
    freeCastMatlab3DArray(KsourceI,((int)(J1[0]-J0[0]+1.)));
    freeCastMatlab3DArray(KsourceR,((int)(J1[0]-J0[0]+1.)));
  }

if( !((N_fieldsample_i == 0) || (N_fieldsample_j == 0) || (N_fieldsample_k == 0) || (N_fieldsample_n == 0)) ){
  freeCastMatlab4DArray( fieldsample, N_fieldsample_k,N_fieldsample_n);
  freeCastMatlab4DArray( fieldsample_x, N_fieldsample_k,N_fieldsample_n);
  freeCastMatlab4DArray( fieldsample_y, N_fieldsample_k,N_fieldsample_n);
  freeCastMatlab4DArray( fieldsample_z, N_fieldsample_k,N_fieldsample_n);
 }

  freeCastMatlab2DArray(iwave_lEx_Rbs);
  freeCastMatlab2DArray(iwave_lEx_Ibs);
  freeCastMatlab2DArray(iwave_lEy_Rbs);
  freeCastMatlab2DArray(iwave_lEy_Ibs);
  
  freeCastMatlab2DArray(iwave_lHx_Rbs);
  freeCastMatlab2DArray(iwave_lHx_Ibs);
  freeCastMatlab2DArray(iwave_lHy_Rbs);
  freeCastMatlab2DArray(iwave_lHy_Ibs);

  //fprintf(stderr,"Pos 21\n");
  if(is_structure)
    freeCastMatlab2DArrayInt(structure);

  if(dimension==THREE)
    freeCastMatlab3DArrayUint8(materials, material_nlayers);
  else
    freeCastMatlab3DArrayUint8(materials, 0);
  /*Free the additional memory which was allocated to store integers which were passed as doubles*/

  if (!useCD){
    for(i=0;i<omp_get_max_threads();i++){
      free(ca_vec[i]);
      free(cb_vec[i]);
      free(cc_vec[i]);
    }
    free(ca_vec);
    free(cb_vec);
    free(cc_vec);

    for(i=0;i<omp_get_max_threads();i++)
      fftw_free(*(eh_vec+i));
    free(eh_vec);

    for(i=0;i<omp_get_max_threads();i++){
         
      fftw_destroy_plan(pf_exy[i]);fftw_destroy_plan(pb_exy[i]);
      fftw_destroy_plan(pf_exz[i]);fftw_destroy_plan(pb_exz[i]);
      fftw_destroy_plan(pf_eyx[i]);fftw_destroy_plan(pb_eyx[i]);
      fftw_destroy_plan(pf_eyz[i]);fftw_destroy_plan(pb_eyz[i]);
      fftw_destroy_plan(pf_ezx[i]);fftw_destroy_plan(pb_ezx[i]);
      fftw_destroy_plan(pf_ezy[i]);fftw_destroy_plan(pb_ezy[i]);
      
      fftw_destroy_plan(pf_hxy[i]);fftw_destroy_plan(pb_hxy[i]);
      fftw_destroy_plan(pf_hxz[i]);fftw_destroy_plan(pb_hxz[i]);
      fftw_destroy_plan(pf_hyx[i]);fftw_destroy_plan(pb_hyx[i]);
      fftw_destroy_plan(pf_hyz[i]);fftw_destroy_plan(pb_hyz[i]);
      fftw_destroy_plan(pf_hzx[i]);fftw_destroy_plan(pb_hzx[i]);
      fftw_destroy_plan(pf_hzy[i]);fftw_destroy_plan(pb_hzy[i]);
    }

    free(pf_exy);free(pb_exy);
    free(pf_exz);free(pb_exz);
    free(pf_eyx);free(pb_eyx);
    free(pf_eyz);free(pb_eyz);
    free(pf_ezx);free(pb_ezx);
    free(pf_ezy);free(pb_ezy);

    free(pf_hxy);free(pb_hxy);
    free(pf_hxz);free(pb_hxz);
    free(pf_hyx);free(pb_hyx);
    free(pf_hyz);free(pb_hyz);
    free(pf_hzx);free(pb_hzx);
    free(pf_hzy);free(pb_hzy);

    fftw_free(dk_e_x);
    fftw_free(dk_e_y);
    fftw_free(dk_e_z);

    fftw_free(dk_h_x);
    fftw_free(dk_h_y);
    fftw_free(dk_h_z);
    
  }

  free(E_norm);
  free(H_norm);
  free(Dxl);
  free(Dxu);
  free(Dyl);
  free(Dyu);
  free(Dzl);
  free(Dzu);
  //  free(lower_boundary_update);
  free(Nt);
  free(I0);
  free(I1);
  free(J0);
  free(J1);
  free(K0);
  free(K1);
  free(dims);
  free(label_dims);
  if(is_disp || is_disp_ml){
    destroy_auxilliary_mem(I_tot, J_tot, K_tot,
			   &Exy_nm1, &Exz_nm1, 
			   &Eyx_nm1, &Eyz_nm1,  
			   &Ezx_nm1, &Ezy_nm1,
			   &Jxy, &Jxz, 
			   &Jyx, &Jyz,  
			   &Jzx, &Jzy,
			   &Jxy_nm1, &Jxz_nm1, 
			   &Jyx_nm1, &Jyz_nm1,  
			   &Jzx_nm1, &Jzy_nm1);
   
  }

  //fprintf(stderr,"Pos 22\n");
  if(is_cond){
    destroy_auxilliary_mem_conductive(I_tot, J_tot, K_tot,
				      &Jcxy, &Jcxz, 
				      &Jcyx, &Jcyz,  
				      &Jczx, &Jczy);
  }

  if(sourcemode==sm_steadystate && runmode==rm_complete){
    mxDestroyArray(dummy_array[0]);
    mxDestroyArray(dummy_array[1]);
    mxDestroyArray(dummy_array[2]);
  }
  if(poutfile)
    fclose(outfile);
  //  fclose(eyfile);
  //  fclose(jyfile);
  //must destroy mx_surface_amplitudes
}
  
/*Sets the contents of the 3 dimensional double array to zero
  inArray - pointer to the array
  i_lim - number of elements along the i dimension of the array
  j_lim - number of elements along the j dimension of the array
  k_lim - number of elements along the k dimension of the array

  The array is assumed to be indexed according to inArray[k][j][i]

*/

void initialiseDouble3DArray(double ***inArray, int i_lim, int j_lim, int k_lim){
  for(int k_var=0;k_var<k_lim;k_var++)  
    for(int j_var=0;j_var<j_lim;j_var++)
      for(int i_var=0;i_var<i_lim;i_var++)
	inArray[k_var][j_var][i_var] = 0.0;
}

/*Sets the contents of the 2 dimensional double array to zero
  inArray - pointer to the array
  i_lim - number of elements along the i dimension of the array
  j_lim - number of elements along the j dimension of the array
  
  The array is assumed to be indexed according to inArray[j][i]

*/

void initialiseDouble2DArray(double **inArray, int i_lim, int j_lim){
  for(int j_var=0;j_var<j_lim;j_var++)  
    for(int i_var=0;i_var<i_lim;i_var++)
      inArray[j_var][i_var] = 0.0;
}

//i_l is the index into the fdtd grid which is the first non-pml cell in the i direction
//i_u is the index into the fdtd grid which is the last non-pml cell in the i direction
//
//result gives field according to the exp(-iwt) convention
void extractPhasorsVolume(double ***ExR, double ***ExI, double ***EyR, double ***EyI, double ***EzR, double ***EzI,
			  double ***Exy, double ***Exz, double ***Eyx, double ***Eyz, double ***Ezx, double ***Ezy,
			  int i_l, int i_u, int j_l, int j_u, int k_l, int k_u, int n, double omega, double dt, int Nt){

  complex<double> phaseTerm, subResult;
  double ex_m, ey_m, ez_m;


  
  phaseTerm = fmod(omega*((double) n)*dt, 2*dcpi);
  //  fprintf(stdout,"phi: %.10e, dt: %.10e, omega: %.10e\n",omega*((double) n)*dt,dt,omega);
#pragma omp parallel default(shared)  private(ex_m,ey_m,ez_m,subResult)
  {
#pragma omp for
    for(int k=k_l; k<= k_u; k++)
      for(int j=j_l; j<= j_u; j++)
	for(int i=i_l; i<= i_u; i++){

	  /*
	    ex_m = 0.5 * (Exy[k][j][i] + Exz[k][j][i] + Exy[k][j][i-1] + Exz[k][j][i-1]);
	    ey_m = 0.5 * (Eyx[k][j][i] + Eyz[k][j][i] + Eyx[k][j-1][i] + Eyz[k][j-1][i]);
	    ez_m = 0.5 * (Ezx[k][j][i] + Ezy[k][j][i] + Ezx[k-1][j][i] + Ezy[k-1][j][i]);
	  */

	  ex_m = Exy[k][j][i] + Exz[k][j][i];
	  ey_m = Eyx[k][j][i] + Eyz[k][j][i];
	  ez_m = Ezx[k][j][i] + Ezy[k][j][i];

	  subResult = ex_m * exp(phaseTerm * I) * 1./((double) Nt);
	
	  ExR[k-k_l][j-j_l][i-i_l] = ExR[k-k_l][j-j_l][i-i_l] + real(subResult);
	  ExI[k-k_l][j-j_l][i-i_l] = ExI[k-k_l][j-j_l][i-i_l] + imag(subResult);

	  subResult = ey_m * exp(phaseTerm * I) * 1./((double) Nt);
	
	  EyR[k-k_l][j-j_l][i-i_l] = EyR[k-k_l][j-j_l][i-i_l] + real(subResult);
	  EyI[k-k_l][j-j_l][i-i_l] = EyI[k-k_l][j-j_l][i-i_l] + imag(subResult);

	  subResult = ez_m * exp(phaseTerm * I) * 1./((double) Nt);
	
	  EzR[k-k_l][j-j_l][i-i_l] = EzR[k-k_l][j-j_l][i-i_l] + real(subResult);
	  EzI[k-k_l][j-j_l][i-i_l] = EzI[k-k_l][j-j_l][i-i_l] + imag(subResult);
	
	
	
	}
  }//end of parallel region
  
}

void normaliseSurface( double **surface_EHr, double **surface_EHi ,
		       int **surface_vertices, int n_surface_vertices,  complex<double> Enorm , complex<double> Hnorm ){
  double norm_r, norm_i, denom, temp_r, temp_i; 

  norm_r = real(Enorm);
  norm_i = imag(Enorm);
  denom = norm_r*norm_r + norm_i*norm_i;

  for(int vindex = 0; vindex<n_surface_vertices; vindex++)
    for(int i=0; i<3; i++){
      temp_r = surface_EHr[i][vindex];
      temp_i = surface_EHi[i][vindex];
      
      surface_EHr[i][vindex] = (norm_r*temp_r + norm_i*temp_i)/denom;
      surface_EHi[i][vindex] = (norm_r*temp_i - norm_i*temp_r)/denom;
    }

  norm_r = real(Hnorm);
  norm_i = imag(Hnorm);
  denom = norm_r*norm_r + norm_i*norm_i;

  for(int vindex = 0; vindex<n_surface_vertices; vindex++)
    for(int i=3; i<6; i++){
      temp_r = surface_EHr[i][vindex];
      temp_i = surface_EHi[i][vindex];
      
      surface_EHr[i][vindex] = (norm_r*temp_r + norm_i*temp_i)/denom;
      surface_EHi[i][vindex] = (norm_r*temp_i - norm_i*temp_r)/denom;
    }
}


void normaliseVertices( double **EHr, double **EHi ,
			int **vertices, int nvertices,
			int *components, int ncomponents,
			complex<double> Enorm , complex<double> Hnorm ){
  
  double norm_r, norm_i, denom, temp_r, temp_i;
  int ii;

  norm_r = real(Enorm);
  norm_i = imag(Enorm);
  denom = norm_r*norm_r + norm_i*norm_i;

  for(int vindex = 0; vindex<nvertices; vindex++)
    for(int i=0; i<3; i++){
      ii = find( components, ncomponents, i+1);
      if( ii>=0 ){
	temp_r = EHr[ii][vindex];
	temp_i = EHi[ii][vindex];
            
	EHr[ii][vindex] = (norm_r*temp_r + norm_i*temp_i)/denom;
	EHi[ii][vindex] = (norm_r*temp_i - norm_i*temp_r)/denom;
      }
    }

  norm_r = real(Hnorm);
  norm_i = imag(Hnorm);
  denom = norm_r*norm_r + norm_i*norm_i;

  for(int vindex = 0; vindex<nvertices; vindex++)
    for(int i=3; i<6; i++){
      ii = find( components, ncomponents, i+1);
      if( ii>=0 ){
	temp_r = EHr[ii][vindex];
	temp_i = EHi[ii][vindex];
      
	EHr[ii][vindex] = (norm_r*temp_r + norm_i*temp_i)/denom;
	EHi[ii][vindex] = (norm_r*temp_i - norm_i*temp_r)/denom;
      }
    }
}

void normaliseVolume(double ***ExR, double ***ExI, double ***EyR, double ***EyI, double ***EzR, double ***EzI,
		     int i_l, int i_u, int j_l, int j_u, int k_l, int k_u,  complex<double> norm){
  double norm_r, norm_i, denom, temp_r, temp_i; 

  norm_r = real(norm);
  norm_i = imag(norm);
  denom = norm_r*norm_r + norm_i*norm_i;

  for(int k=k_l; k<= k_u; k++)
    for(int j=j_l; j<= j_u; j++)
      for(int i=i_l; i<= i_u; i++){

	temp_r = ExR[k-k_l][j-j_l][i-i_l];
	temp_i = ExI[k-k_l][j-j_l][i-i_l];
	ExR[k-k_l][j-j_l][i-i_l] = (norm_r*temp_r + norm_i*temp_i)/denom;
	ExI[k-k_l][j-j_l][i-i_l] = (norm_r*temp_i - norm_i*temp_r)/denom;

	temp_r = EyR[k-k_l][j-j_l][i-i_l];
	temp_i = EyI[k-k_l][j-j_l][i-i_l];
	EyR[k-k_l][j-j_l][i-i_l] = (norm_r*temp_r + norm_i*temp_i)/denom;
	EyI[k-k_l][j-j_l][i-i_l] = (norm_r*temp_i - norm_i*temp_r)/denom;

	temp_r = EzR[k-k_l][j-j_l][i-i_l];
	temp_i = EzI[k-k_l][j-j_l][i-i_l];
	EzR[k-k_l][j-j_l][i-i_l] = (norm_r*temp_r + norm_i*temp_i)/denom;
	EzI[k-k_l][j-j_l][i-i_l] = (norm_r*temp_i - norm_i*temp_r)/denom;
      }

}

void extractPhasorENorm(complex<double> *Enorm, double ft, int n, double omega, double dt, int Nt){
  *Enorm += ft*exp( fmod(omega*((double) (n+1))*dt, 2*dcpi) * I) * 1./((double) Nt);
}

void extractPhasorHNorm(complex<double> *Hnorm, double ft, int n, double omega, double dt, int Nt){
  *Hnorm += ft*exp( fmod(omega*((double) n + 0.5)*dt, 2*dcpi) * I) * 1./((double) Nt);
}

//these indices are set according to the electric part of the pml - since the index of the 
//first cell is different in the case of the electric update eqns.

//i_l is the index into the fdtd grid which is the first non-pml cell in the i direction
//i_u is the index into the fdtd grid which is the last non-pml cell in the i direction
//
//result gives field according to the exp(-iwt) convention
void extractPhasorsVolumeH(double ***HxR, double ***HxI, double ***HyR, double ***HyI, double ***HzR, double ***HzI,
			   double ***Hxy, double ***Hxz, double ***Hyx, double ***Hyz, double ***Hzx, double ***Hzy,
			   int i_l, int i_u, int j_l, int j_u, int k_l, int k_u, int n, double omega, double dt, int Nt){

  complex<double> phaseTerm, subResult;
  double hx_m, hy_m, hz_m;
  
  //a + 0.5 is added because we always know H half a time step
  //after we know E.
  phaseTerm = fmod(omega*((double) n + 0.5)*dt, 2*dcpi);
#pragma omp parallel default(shared)  private(hx_m,hy_m,hz_m,subResult)
  {
#pragma omp for
    for(int k=k_l; k<= k_u; k++)
      for(int j=j_l; j<= j_u; j++)
	for(int i=i_l; i<= i_u; i++){
	
	  hx_m = Hxy[k][j][i] + Hxz[k][j][i];
	  hy_m = Hyx[k][j][i] + Hyz[k][j][i];
	  hz_m = Hzx[k][j][i] + Hzy[k][j][i];
		
	  subResult = hx_m * exp(phaseTerm * I) * 1./((double) Nt);
	
	  HxR[k-k_l][j-j_l][i-i_l] = HxR[k-k_l][j-j_l][i-i_l] + real(subResult);
	  HxI[k-k_l][j-j_l][i-i_l] = HxI[k-k_l][j-j_l][i-i_l] + imag(subResult);

	  subResult = hy_m * exp(phaseTerm * I) * 1./((double) Nt);
	
	  HyR[k-k_l][j-j_l][i-i_l] = HyR[k-k_l][j-j_l][i-i_l] + real(subResult);
	  HyI[k-k_l][j-j_l][i-i_l] = HyI[k-k_l][j-j_l][i-i_l] + imag(subResult);

	  subResult = hz_m * exp(phaseTerm * I) * 1./((double) Nt);
	
	  HzR[k-k_l][j-j_l][i-i_l] = HzR[k-k_l][j-j_l][i-i_l] + real(subResult);
	  HzI[k-k_l][j-j_l][i-i_l] = HzI[k-k_l][j-j_l][i-i_l] + imag(subResult);
		
	}
  }//end parallel region
}

void extractPhasorsSurface( double **surface_EHr, double **surface_EHi,  
			    double ***Hxy, double ***Hxz, double ***Hyx, double ***Hyz, double ***Hzx, double ***Hzy,
			    double ***Exy, double ***Exz, double ***Eyx, double ***Eyz, double ***Ezx, double ***Ezy,
			    int **surface_vertices, int n_surface_vertices, int n, double omega, double dt, int Nt, int dimension,int J_tot,int intmethod ){
  int vindex;
  double Ex, Ey, Ez, Hx, Hy, Hz;
  complex<double> phaseTermE, phaseTermH, subResultE, subResultH, cphaseTermE, cphaseTermH ;

  phaseTermE = fmod(omega*((double) n)*dt, 2*dcpi);
  phaseTermH  = fmod(omega*((double) n + 0.5)*dt, 2*dcpi);

  cphaseTermH = exp(phaseTermH * I) * 1./((double) Nt);
  cphaseTermE = exp(phaseTermE * I) * 1./((double) Nt);
  
  //loop over every vertex in the list 
#pragma omp parallel default(shared)  private(Ex, Ey, Ez, Hx, Hy, Hz,phaseTermE, phaseTermH, subResultE, subResultH,vindex)
  {
#pragma omp for
    for(vindex = 0; vindex<n_surface_vertices; vindex++){
      //    fprintf(stderr,"vindex: %d: (%d %d %d)\n",vindex,surface_vertices[0][vindex],surface_vertices[1][vindex],surface_vertices[2][vindex]);
      if(dimension==THREE)
	if(J_tot==0){
	  interpolateTimeDomainFieldCentralE_2Dy( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
						  surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
						  &Ex, &Ey, &Ez);
	}
	else
	  if( intmethod==1 )
			  interpolateTimeDomainFieldCentralE( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
							      surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
							      &Ex, &Ey, &Ez);
	  else
	    interpolateTimeDomainFieldCentralEBandLimited( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
							   surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
							   &Ex, &Ey, &Ez);
      else if(dimension==TE)
	interpolateTimeDomainFieldCentralE_TE( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
					       surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
					       &Ex, &Ey, &Ez);
      else
	interpolateTimeDomainFieldCentralE_TM( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
					       surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
					       &Ex, &Ey, &Ez);
      //    fprintf(stderr,"1st interp donezn");
      if(dimension==THREE)
	if(J_tot==0){
	  interpolateTimeDomainFieldCentralH_2Dy( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
						  surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
						  &Hx, &Hy, &Hz);
	}
	else
	  if( intmethod==1 )
			  interpolateTimeDomainFieldCentralH( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
							      surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
							      &Hx, &Hy, &Hz);
	  else
	    interpolateTimeDomainFieldCentralHBandLimited( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
							   surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
							   &Hx, &Hy, &Hz);
      else if(dimension==TE)
	interpolateTimeDomainFieldCentralH_TE( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
					       surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
					       &Hx, &Hy, &Hz);
      else
	interpolateTimeDomainFieldCentralH_TM( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
					       surface_vertices[0][vindex], surface_vertices[1][vindex], surface_vertices[2][vindex],
					       &Hx, &Hy, &Hz);
      //    fprintf(stderr,"2nd interp donezn");
  
      /*Ex and Hx*/
      subResultH = Hx * cphaseTermH;//exp(phaseTermH * I) * 1./((double) Nt);
      subResultE = Ex * cphaseTermE;//exp(phaseTermE * I) * 1./((double) Nt);

      //now update the master array
      surface_EHr[0][vindex] = surface_EHr[0][vindex] + real(subResultE);
      surface_EHi[0][vindex] = surface_EHi[0][vindex] + imag(subResultE);
    
      surface_EHr[3][vindex] = surface_EHr[3][vindex] + real(subResultH);
      surface_EHi[3][vindex] = surface_EHi[3][vindex] + imag(subResultH);

      /*Ey and Hy*/
      subResultH = Hy * cphaseTermH;//exp(phaseTermH * I) * 1./((double) Nt);
      subResultE = Ey * cphaseTermE;//exp(phaseTermE * I) * 1./((double) Nt);

      //now update the master array
      surface_EHr[1][vindex] = surface_EHr[1][vindex] + real(subResultE);
      surface_EHi[1][vindex] = surface_EHi[1][vindex] + imag(subResultE);
    
      surface_EHr[4][vindex] = surface_EHr[4][vindex] + real(subResultH);
      surface_EHi[4][vindex] = surface_EHi[4][vindex] + imag(subResultH);

    
      /*Ez and Hz*/
      subResultH = Hz * cphaseTermH;//exp(phaseTermH * I) * 1./((double) Nt);
      subResultE = Ez * cphaseTermE;//exp(phaseTermE * I) * 1./((double) Nt);

      //now update the master array
      surface_EHr[2][vindex] = surface_EHr[2][vindex] + real(subResultE);
      surface_EHi[2][vindex] = surface_EHi[2][vindex] + imag(subResultE);
    
      surface_EHr[5][vindex] = surface_EHr[5][vindex] + real(subResultH);
      surface_EHi[5][vindex] = surface_EHi[5][vindex] + imag(subResultH);
    }
  }//end parallel region
}



void extractPhasorsVertices( double **EHr, double **EHi,  
			     double ***Hxy, double ***Hxz, double ***Hyx, double ***Hyz, double ***Hzx, double ***Hzy,
			     double ***Exy, double ***Exz, double ***Eyx, double ***Eyz, double ***Ezx, double ***Ezy,
			     int **vertices, int nvertices, int *components, int ncomponents,
			     int n, double omega, double dt, int Nt, int dimension,int J_tot,int intmethod ){
  int vindex;
  double Ex, Ey, Ez, Hx, Hy, Hz;
  complex<double> phaseTermE, phaseTermH, subResultE, subResultH, cphaseTermE, cphaseTermH ;

  phaseTermE = fmod(omega*((double) n)*dt, 2*dcpi);
  phaseTermH  = fmod(omega*((double) n + 0.5)*dt, 2*dcpi);

  cphaseTermH = exp(phaseTermH * I) * 1./((double) Nt);
  cphaseTermE = exp(phaseTermE * I) * 1./((double) Nt);
  
  //loop over every vertex in the list 
#pragma omp parallel default(shared)  private(Ex, Ey, Ez, Hx, Hy, Hz,phaseTermE, phaseTermH, subResultE, subResultH,vindex)
  {
#pragma omp for
    for(vindex = 0; vindex<nvertices; vindex++){
      //fprintf(stderr,"vindex: %d: (%d %d %d)\n",vindex,vertices[0][vindex],vertices[1][vindex],vertices[2][vindex]);
      if(dimension==THREE)
	if(J_tot==0){
	  interpolateTimeDomainFieldCentralE_2Dy( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
						  vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
						  &Ex, &Ey, &Ez);
	}
	else
	  if( intmethod==1 )
	    interpolateTimeDomainFieldCentralE( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
						vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
						&Ex, &Ey, &Ez);
	  else
	    interpolateTimeDomainFieldCentralEBandLimited( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
							   vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
							   &Ex, &Ey, &Ez);
      else if(dimension==TE)
	interpolateTimeDomainFieldCentralE_TE( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
					       vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
					       &Ex, &Ey, &Ez);
      else
	interpolateTimeDomainFieldCentralE_TM( Exy, Exz, Eyx, Eyz, Ezx, Ezy,
					       vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
					       &Ex, &Ey, &Ez);
      //    fprintf(stderr,"1st interp donezn");
      if(dimension==THREE)
	if(J_tot==0){
	  interpolateTimeDomainFieldCentralH_2Dy( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
						  vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
						  &Hx, &Hy, &Hz);
	}
	else
	  if( intmethod==1 )
			  interpolateTimeDomainFieldCentralH( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
							      vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
							      &Hx, &Hy, &Hz);
	  else
	    interpolateTimeDomainFieldCentralHBandLimited( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
							   vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
							   &Hx, &Hy, &Hz);
      else if(dimension==TE)
	interpolateTimeDomainFieldCentralH_TE( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
					       vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
					       &Hx, &Hy, &Hz);
      else
	interpolateTimeDomainFieldCentralH_TM( Hxy, Hxz, Hyx, Hyz, Hzx, Hzy,
					       vertices[0][vindex], vertices[1][vindex], vertices[2][vindex],
					       &Hx, &Hy, &Hz);
      //    fprintf(stderr,"2nd interp donezn");
  
      /*Ex and Hx*/
      subResultH = Hx * cphaseTermH;//exp(phaseTermH * I) * 1./((double) Nt);
      subResultE = Ex * cphaseTermE;//exp(phaseTermE * I) * 1./((double) Nt);

      //now update the master array
      if( find(components, ncomponents, 1) >= 0 ){
	EHr[find(components, ncomponents, 1)][vindex] = EHr[find(components, ncomponents, 1)][vindex] + real(subResultE);
	EHi[find(components, ncomponents, 1)][vindex] = EHi[find(components, ncomponents, 1)][vindex] + imag(subResultE);
      }
      if( find(components, ncomponents, 4) >= 0 ){
	EHr[find(components, ncomponents, 4)][vindex] = EHr[find(components, ncomponents, 4)][vindex] + real(subResultH);
	EHi[find(components, ncomponents, 4)][vindex] = EHi[find(components, ncomponents, 4)][vindex] + imag(subResultH);
      }

      /*Ey and Hy*/
      subResultH = Hy * cphaseTermH;//exp(phaseTermH * I) * 1./((double) Nt);
      subResultE = Ey * cphaseTermE;//exp(phaseTermE * I) * 1./((double) Nt);

      //now update the master array
      if( find(components, ncomponents, 2) >= 0){
	EHr[find(components, ncomponents, 2)][vindex] = EHr[find(components, ncomponents, 2)][vindex] + real(subResultE);
	EHi[find(components, ncomponents, 2)][vindex] = EHi[find(components, ncomponents, 2)][vindex] + imag(subResultE);
      }
      if( find(components, ncomponents, 5) >= 0 ){
	EHr[find(components, ncomponents, 5)][vindex] = EHr[find(components, ncomponents, 5)][vindex] + real(subResultH);
	EHi[find(components, ncomponents, 5)][vindex] = EHi[find(components, ncomponents, 5)][vindex] + imag(subResultH);
      }

    
      /*Ez and Hz*/
      subResultH = Hz * cphaseTermH;//exp(phaseTermH * I) * 1./((double) Nt);
      subResultE = Ez * cphaseTermE;//exp(phaseTermE * I) * 1./((double) Nt);

      //now update the master array
      if( find(components, ncomponents, 3) >= 0 ){
	EHr[find(components, ncomponents, 3)][vindex] = EHr[find(components, ncomponents, 3)][vindex] + real(subResultE);
	EHi[find(components, ncomponents, 3)][vindex] = EHi[find(components, ncomponents, 3)][vindex] + imag(subResultE);
      }
      if( find(components, ncomponents, 6) >= 0 ){
	EHr[find(components, ncomponents, 6)][vindex] = EHr[find(components, ncomponents, 6)][vindex] + real(subResultH);
	EHi[find(components, ncomponents, 6)][vindex] = EHi[find(components, ncomponents, 6)][vindex] + imag(subResultH);
      }
    }
  }//end parallel region
}



void extractPhasorsSurfaceNoInterpolation( double **surface_EHr, double **surface_EHi,  
					   double ***Hxy, double ***Hxz, double ***Hyx, double ***Hyz, double ***Hzx, double ***Hzy,
					   double ***Exy, double ***Exz, double ***Eyx, double ***Eyz, double ***Ezx, double ***Ezy,
					   int **surface_vertices, int n_surface_vertices, int n, double omega, double dt, int Nt, int dimension,int J_tot ){
  int vindex;
  double Ex, Ey, Ez, Hx, Hy, Hz;
  complex<double> phaseTermE, phaseTermH, subResultE, subResultH, cphaseTermE, cphaseTermH ;

  phaseTermE = fmod(omega*((double) n)*dt, 2*dcpi);
  phaseTermH  = fmod(omega*((double) n + 0.5)*dt, 2*dcpi);

  cphaseTermH = exp(phaseTermH * I) * 1./((double) Nt);
  cphaseTermE = exp(phaseTermE * I) * 1./((double) Nt);
  
  //loop over every vertex in the list 
#pragma omp parallel default(shared)  private(Ex, Ey, Ez, Hx, Hy, Hz,phaseTermE, phaseTermH, subResultE, subResultH,vindex)
  {
#pragma omp for
    for(vindex = 0; vindex<n_surface_vertices; vindex++){
      //    fprintf(stderr,"vindex: %d: (%d %d %d)\n",vindex,surface_vertices[0][vindex],surface_vertices[1][vindex],surface_vertices[2][vindex]);
      Ex = Exy[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]] + Exz[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]];
      Ey = Eyx[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]] + Eyz[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]];
      Ez = Ezx[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]] + Ezy[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]];
    
      Hx = Hxy[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]] + Hxz[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]];
      Hy = Hyx[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]] + Hyz[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]];
      Hz = Hzx[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]] + Hzy[surface_vertices[2][vindex]][surface_vertices[1][vindex]][surface_vertices[0][vindex]];
    
      /*Ex and Hx*/
      subResultH = Hx * cphaseTermH;//exp(phaseTermH * I) * 1./((double) Nt);
      subResultE = Ex * cphaseTermE;//exp(phaseTermE * I) * 1./((double) Nt);

      //now update the master array
      surface_EHr[0][vindex] = surface_EHr[0][vindex] + real(subResultE);
      surface_EHi[0][vindex] = surface_EHi[0][vindex] + imag(subResultE);
    
      surface_EHr[3][vindex] = surface_EHr[3][vindex] + real(subResultH);
      surface_EHi[3][vindex] = surface_EHi[3][vindex] + imag(subResultH);

      /*Ey and Hy*/
      subResultH = Hy * cphaseTermH;//exp(phaseTermH * I) * 1./((double) Nt);
      subResultE = Ey * cphaseTermE;//exp(phaseTermE * I) * 1./((double) Nt);

      //now update the master array
      surface_EHr[1][vindex] = surface_EHr[1][vindex] + real(subResultE);
      surface_EHi[1][vindex] = surface_EHi[1][vindex] + imag(subResultE);
    
      surface_EHr[4][vindex] = surface_EHr[4][vindex] + real(subResultH);
      surface_EHi[4][vindex] = surface_EHi[4][vindex] + imag(subResultH);

    
      /*Ez and Hz*/
      subResultH = Hz * cphaseTermH;//exp(phaseTermH * I) * 1./((double) Nt);
      subResultE = Ez * cphaseTermE;//exp(phaseTermE * I) * 1./((double) Nt);

      //now update the master array
      surface_EHr[2][vindex] = surface_EHr[2][vindex] + real(subResultE);
      surface_EHi[2][vindex] = surface_EHi[2][vindex] + imag(subResultE);
    
      surface_EHr[5][vindex] = surface_EHr[5][vindex] + real(subResultH);
      surface_EHi[5][vindex] = surface_EHi[5][vindex] + imag(subResultH);
    }
  }//end parallel region
}

void extractPhasorsPlane( double **iwave_lEx_Rbs, double **iwave_lEx_Ibs, double **iwave_lEy_Rbs, double **iwave_lEy_Ibs, 
			  double **iwave_lHx_Rbs, double **iwave_lHx_Ibs, double **iwave_lHy_Rbs, double **iwave_lHy_Ibs, 
			  double ***Exz, double ***Eyz, double ***Hxz, double ***Hyz,
			  double ***Exy, double ***Eyx, double ***Hxy, double ***Hyx,
			  int I_tot, int J_tot, int K1, int n, double omega, double dt, int Nt){

  complex<double> phaseTerm = 0., subResult = 0.;

  
  phaseTerm = fmod(omega*((double) n)*dt, 2*dcpi);
  int i, j;
  
  for( j=0; j< J_tot; j++)
    for( i=0; i<(I_tot+1); i++){
      
     
      //Eyz
      subResult = (Eyz[K1][j][i] + Eyx[K1][j][i]) * exp(phaseTerm * I) * 1./((double) Nt);

      iwave_lEy_Rbs[j][i] = iwave_lEy_Rbs[j][i] + real(subResult);
      iwave_lEy_Ibs[j][i] = iwave_lEy_Ibs[j][i] + imag(subResult);
      
      //Hxz
      subResult = (Hxz[K1-1][j][i]+Hxy[K1][j][i]) * exp(phaseTerm * I) * 1./((double) Nt);
      
      iwave_lHx_Rbs[j][i] = iwave_lHx_Rbs[j][i] + real(subResult);
      iwave_lHx_Ibs[j][i] = iwave_lHx_Ibs[j][i] + imag(subResult);
    }
    
  for( j=0; j<(J_tot+1); j++)
    for( i=0; i<I_tot; i++){
      
            
      //Exz
      subResult = (Exz[K1][j][i] + Exy[K1][j][i]) * exp(phaseTerm * I) * 1./((double) Nt);
      
      iwave_lEx_Rbs[j][i] = iwave_lEx_Rbs[j][i] + real(subResult);
      iwave_lEx_Ibs[j][i] = iwave_lEx_Ibs[j][i] + imag(subResult);
      
      //Hyz
      subResult = (Hyz[K1-1][j][i] + Hyx[K1][j][i]) * exp(phaseTerm * I) * 1./((double) Nt);
      
      iwave_lHy_Rbs[j][i] = iwave_lHy_Rbs[j][i] + real(subResult);
      iwave_lHy_Ibs[j][i] = iwave_lHy_Ibs[j][i] + imag(subResult);
      
    }
}

/*Implements a linear ramp which has the properties:
  
  ramp(t) = 1 if t > rampwidth*period
  = t/(rampwidth*period) otherwise

  t - the current time at which to evaluate the ramp
  period - the period of the monochormatic sinusoidal excitation
  rampwidth - the fraction or number of periods with of the ramp

*/
double linearRamp(double t, double period, double rampwidth){
  
  if(t > period*rampwidth)
    return 1.;
  else
    return t/(period*rampwidth);
}

//i_l is the index into the fdtd grid which is the first non-pml cell in the i direction
//i_u is the index into the fdtd grid which is the last non-pml cell in the i direction
double checkPhasorConvergence(double ***ExR, double ***ExI, double ***EyR, double ***EyI, double ***EzR, double ***EzI,
			      double ***ExR2, double ***ExI2, double ***EyR2, double ***EyI2, double ***EzR2, double ***EzI2,
			      double ***Exy, double ***Exz, double ***Eyx, double ***Eyz, double ***Ezx, double ***Ezy,
			      int i_l, int i_u, int j_l, int j_u, int k_l, int k_u, int n, double omega, double dt, int Nt){

  //first find the maximum absolute value
  double maxabs = 0., tmaxabs = 0.;
  double maxdiff = 0.;
  
  for(int k=k_l; k<= k_u; k++)
    for(int j=j_l; j<= j_u; j++)
      for(int i=i_l; i<= i_u; i++){
	
	tmaxabs = complexAbs(ExR[k-k_l][j-j_l][i-i_l] + I*ExI[k-k_l][j-j_l][i-i_l]);
	if(tmaxabs > maxabs)
	  maxabs = tmaxabs;
		
	tmaxabs = complexAbs(EyR[k-k_l][j-j_l][i-i_l] + I*EyI[k-k_l][j-j_l][i-i_l]);
	if(tmaxabs > maxabs)
	  maxabs = tmaxabs;

	tmaxabs = complexAbs(EzR[k-k_l][j-j_l][i-i_l] + I*EzI[k-k_l][j-j_l][i-i_l]);
	if(tmaxabs > maxabs)
	  maxabs = tmaxabs;
	
      }
  //now find the largest difference (in absolute value) between phasors
  for(int k=k_l; k<= k_u; k++)
    for(int j=j_l; j<= j_u; j++)
      for(int i=i_l; i<= i_u; i++){

    
	
	tmaxabs = complexAbs(ExR[k-k_l][j-j_l][i-i_l] - ExR2[k-k_l][j-j_l][i-i_l] + I*ExI[k-k_l][j-j_l][i-i_l] - I*ExI2[k-k_l][j-j_l][i-i_l]);
	if(tmaxabs > maxdiff)
	  maxdiff = tmaxabs;
	
	tmaxabs = complexAbs(EyR[k-k_l][j-j_l][i-i_l] - EyR2[k-k_l][j-j_l][i-i_l] + I*EyI[k-k_l][j-j_l][i-i_l] - I*EyI2[k-k_l][j-j_l][i-i_l]);
	if(tmaxabs > maxdiff)
	  maxdiff = tmaxabs;
	
	tmaxabs = complexAbs(EzR[k-k_l][j-j_l][i-i_l] - EzR2[k-k_l][j-j_l][i-i_l] + I*EzI[k-k_l][j-j_l][i-i_l] - I*EzI2[k-k_l][j-j_l][i-i_l]);
	if(tmaxabs > maxdiff)
	  maxdiff = tmaxabs;
	
      }

  return maxdiff/maxabs;
}


/* Returns the absolute value of complex number z*/
double complexAbs(complex<double> z){
  return sqrt(real(z)*real(z) + imag(z)*imag(z));
}


/*Copy the phasors from E* to E*2*/
void copyPhasors(double *ExR, double *ExI, double *EyR, double *EyI, double *EzR, double *EzI,
		 double *ExR2, double *ExI2, double *EyR2, double *EyI2, double *EzR2, double *EzI2,
		 int nelements){

  memcpy(ExR2, ExR, nelements*sizeof(double));
  memcpy(ExI2, ExI, nelements*sizeof(double));
  memcpy(EyR2, EyR, nelements*sizeof(double));
  memcpy(EyI2, EyI, nelements*sizeof(double));
  memcpy(EzR2, EzR, nelements*sizeof(double));
  memcpy(EzI2, EzI, nelements*sizeof(double));
 
}

/*Load up the output grid labels*/
void setGridLabels(double *x_labels_in , double * y_labels_in , double *z_labels_in ,
		   double *x_labels_out, double * y_labels_out, double *z_labels_out,
		   int i_l, int i_u, int j_l, int j_u, int k_l, int k_u){
  //fprintf(stderr,"Entered: setGridLabels\n");
  //fprintf(stderr,"setGridLabels: %d,%d\n",j_u,j_l);
  for(int i=i_l; i<= i_u; i++){
    x_labels_out[i-i_l] = x_labels_in[i];
  }
  for(int j=j_l; j<= j_u; j++){
    y_labels_out[j-j_l] = y_labels_in[j];
  }
  for(int k=k_l; k<= k_u; k++){
    z_labels_out[k-k_l] = z_labels_in[k];
  }
}


/* These functions are used by the dispersive component of the code*/

/*Work out if there are any non-zero alpha values*/
int is_dispersive(unsigned char ***materials,double *gamma, double dt, int I_tot, int J_tot, int K_tot){
  int max_mat = 0;

  //first find the number of entries in alpha
  for(int k=0;k<(K_tot+1);k++)
    for(int j=0;j<(J_tot+1);j++)
      for(int i=0;i<(I_tot+1);i++){
	if(materials[k][j][i] > max_mat)
	  max_mat = materials[k][j][i];
      }
  //now see if there are any non zero alphas
  for(int i=0;i<max_mat;i++){
    if( fabs(gamma[i]/dt) > 1e-15 ){
      return 1;
    }
  }
  return 0;
}

/*work out if we have a conductive background*/
int is_conductive(double *rho_x, double *rho_y, double *rho_z, int I_tot, int J_tot, int K_tot){
  
  for(int i=0;i<(I_tot+1);i++)
    if( fabs(rho_x[i]) > 1e-15 )
      return 1;
  for(int j=0;j<(J_tot+1);j++)
    if( fabs(rho_y[j]) > 1e-15 )
      return 1;
  for(int k=0;k<(K_tot+1);k++)
    if( fabs(rho_z[k]) > 1e-15 )
      return 1;
    
  return 0;
}

/*work out if we have a dispersive background*/
int is_dispersive_ml(double *ml_gamma, int K_tot){
  for( int i=0;i<K_tot;i++)
    if( fabs(ml_gamma[i]) > 1e-15 )
      return 1;
  return 0;
}

/*Allocate auxilliary memory for En-1 and J*/
void allocate_auxilliary_mem(int I_tot, int J_tot, int K_tot,
			     double ****Exy, double ****Exz, 
			     double ****Eyx, double ****Eyz,  
			     double ****Ezx, double ****Ezy,
			     double ****Jxy, double ****Jxz, 
			     double ****Jyx, double ****Jyz,  
			     double ****Jzx, double ****Jzy,
			     double ****Jxy2, double ****Jxz2, 
			     double ****Jyx2, double ****Jyz2,  
			     double ****Jzx2, double ****Jzy2){
  
  
  cons3dArray(Exy, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Exz, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Eyx, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Eyz, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Ezx, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Ezy, I_tot+1, J_tot+1, K_tot+1);
  
  cons3dArray(Jxy, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jxz, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jyx, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jyz, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jzx, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jzy, I_tot+1, J_tot+1, K_tot+1);

  cons3dArray(Jxy2, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jxz2, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jyx2, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jyz2, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jzx2, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jzy2, I_tot+1, J_tot+1, K_tot+1);


  for(int k=0;k<(K_tot+1);k++)
    for(int j=0;j<(J_tot+1);j++)
      for(int i=0;i<(I_tot+1);i++){
	(*Exy)[k][j][i] = 0.;
	(*Exz)[k][j][i] = 0.;
	(*Eyx)[k][j][i] = 0.;
	(*Eyz)[k][j][i] = 0.;
	(*Ezx)[k][j][i] = 0.;
	(*Ezy)[k][j][i] = 0.;

	(*Jxy)[k][j][i] = 0.;
	(*Jxz)[k][j][i] = 0.;
	(*Jyx)[k][j][i] = 0.;
	(*Jyz)[k][j][i] = 0.;
	(*Jzx)[k][j][i] = 0.;
	(*Jzy)[k][j][i] = 0.;

	(*Jxy2)[k][j][i] = 0.;
	(*Jxz2)[k][j][i] = 0.;
	(*Jyx2)[k][j][i] = 0.;
	(*Jyz2)[k][j][i] = 0.;
	(*Jzx2)[k][j][i] = 0.;
	(*Jzy2)[k][j][i] = 0.;
      }
  
    
}

/*Allocate auxilliary memory for J in case of dispersive background*/
void allocate_auxilliary_mem_conductive(int I_tot, int J_tot, int K_tot,
					double ****Jxy, double ****Jxz, 
					double ****Jyx, double ****Jyz,  
					double ****Jzx, double ****Jzy){

  cons3dArray(Jxy, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jxz, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jyx, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jyz, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jzx, I_tot+1, J_tot+1, K_tot+1);
  cons3dArray(Jzy, I_tot+1, J_tot+1, K_tot+1);

  for(int k=0;k<(K_tot+1);k++)
    for(int j=0;j<(J_tot+1);j++)
      for(int i=0;i<(I_tot+1);i++){
	
	(*Jxy)[k][j][i] = 0.;
	(*Jxz)[k][j][i] = 0.;
	(*Jyx)[k][j][i] = 0.;
	(*Jyz)[k][j][i] = 0.;
	(*Jzx)[k][j][i] = 0.;
	(*Jzy)[k][j][i] = 0.;
      }
}

void destroy_auxilliary_mem_conductive(int I_tot, int J_tot, int K_tot,
				       double ****Jxy, double ****Jxz, 
				       double ****Jyx, double ****Jyz,  
				       double ****Jzx, double ****Jzy){

  destroy3DArray(Jxy, J_tot+1, K_tot+1);
  destroy3DArray(Jxz, J_tot+1, K_tot+1);
  destroy3DArray(Jyx, J_tot+1, K_tot+1);
  destroy3DArray(Jyz, J_tot+1, K_tot+1);
  destroy3DArray(Jzx, J_tot+1, K_tot+1);
  destroy3DArray(Jzy, J_tot+1, K_tot+1);


}

/*Destroy the previously allocated memory*/
void destroy_auxilliary_mem(int I_tot, int J_tot, int K_tot,
			    double ****Exy, double ****Exz, 
			    double ****Eyx, double ****Eyz,  
			    double ****Ezx, double ****Ezy,
			    double ****Jxy, double ****Jxz, 
			    double ****Jyx, double ****Jyz,  
			    double ****Jzx, double ****Jzy,
			    double ****Jxy2, double ****Jxz2, 
			    double ****Jyx2, double ****Jyz2,  
			    double ****Jzx2, double ****Jzy2){
  

  destroy3DArray(Exy, J_tot+1, K_tot+1);
  destroy3DArray(Exz, J_tot+1, K_tot+1);
  destroy3DArray(Eyx, J_tot+1, K_tot+1);
  destroy3DArray(Eyz, J_tot+1, K_tot+1);
  destroy3DArray(Ezx, J_tot+1, K_tot+1);
  destroy3DArray(Ezy, J_tot+1, K_tot+1);
  
  destroy3DArray(Jxy, J_tot+1, K_tot+1);
  destroy3DArray(Jxz, J_tot+1, K_tot+1);
  destroy3DArray(Jyx, J_tot+1, K_tot+1);
  destroy3DArray(Jyz, J_tot+1, K_tot+1);
  destroy3DArray(Jzx, J_tot+1, K_tot+1);
  destroy3DArray(Jzy, J_tot+1, K_tot+1);

  destroy3DArray(Jxy2, J_tot+1, K_tot+1);
  destroy3DArray(Jxz2, J_tot+1, K_tot+1);
  destroy3DArray(Jyx2, J_tot+1, K_tot+1);
  destroy3DArray(Jyz2, J_tot+1, K_tot+1);
  destroy3DArray(Jzx2, J_tot+1, K_tot+1);
  destroy3DArray(Jzy2, J_tot+1, K_tot+1);


}


/*check if the integer b appears in the vector a and return the index into a. Returns -1 if b cannot be found
 */
int find(int *a, int na, int b){
  int res = -1;

  for(int i=0;i<na;i++)
    if( a[i]== b )
      res = i;

  return res;


}
