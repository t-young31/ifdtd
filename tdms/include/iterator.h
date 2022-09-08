#include "complex"
#include "field.h"
#include "vector_collection.h"
#include "grid_labels.h"


void extractPhasorsPlane( double **iwave_lEx_Rbs, double **iwave_lEx_Ibs, double **iwave_lEy_Rbs, double **iwave_lEy_Ibs, 
			  double **iwave_lHx_Rbs, double **iwave_lHx_Ibs, double **iwave_lHy_Rbs, double **iwave_lHy_Ibs, 
			  double ***Exz, double ***Eyz, double ***Hxz, double ***Hyz,
			  double ***Exy, double ***Eyx, double ***Hxy, double ***Hyx,
			  int I_tot, int J_tot, int K1, int n, double omega, double dt, int Nt);

void initialiseDouble3DArray(double ***inArray, int i_lim, int j_lim, int k_lim);

void initialiseDouble2DArray(double **inArray, int i_lim, int j_lim);

double linearRamp(double t, double period, double rampwidth);

double checkPhasorConvergence(ElectricField &A, ElectricField &B);

void copyPhasors(ElectricField &from, ElectricField &to, int nelements);

void setGridLabels(GridLabels &input_labels, GridLabels &output_labels,
                   int i_l, int i_u, int j_l, int j_u, int k_l, int k_u);

void extractPhasorsSurface( double **surface_EHr, double **surface_EHi, MagneticSplitField &H, ElectricSplitField &E,
			    int **surface_vertices, int n_surface_vertices, int n, double omega, double dt, int Nt, int dimension,int J_tot,int intmethod );

void extractPhasorsSurfaceNoInterpolation( double **surface_EHr, double **surface_EHi, MagneticSplitField &H, ElectricSplitField &E,
					   int **surface_vertices, int n_surface_vertices, int n, double omega, double dt, int Nt, int dimension,int J_tot );

int is_dispersive(unsigned char ***materials,double *gamma, double dt, int I_tot, int J_tot, int K_tot);

void extractPhasorENorm(std::complex<double> *Enorm, double ft, int n, double omega, double dt, int Nt);

void extractPhasorHNorm(std::complex<double> *Hnorm, double ft, int n, double omega, double dt, int Nt);

void normaliseSurface( double **surface_EHr, double **surface_EHi ,
		       int **surface_vertices, int n_surface_vertices,  std::complex<double> Enorm , std::complex<double> Hnorm );

void normaliseVertices( double **EHr, double **EHi ,
			int **vertices, int nvertices,
			int *components, int ncomponents,
			std::complex<double> Enorm , std::complex<double> Hnorm );

bool is_conductive(const XYZVectors &rho, int I_tot, int J_tot, int K_tot);

bool is_dispersive_ml(const DispersiveMultiLayer &ml, int K_tot);

void extractPhasorsVertices( double **EHr, double **EHi, MagneticSplitField &H, ElectricSplitField &E,
			     int **vertices, int nvertices, int *components, int ncomponents,
			     int n, double omega, double dt, int Nt, int dimension,int J_tot,int intmethod );

int find(int *a, int na, int b);
