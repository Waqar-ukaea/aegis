#ifndef equData__
#define equData__

#include <stdio.h>
#include <vector>
#include <fstream>
#include "alglib/interpolation.h"



class equData{
  private:
  // Variables 

  // Methods

  // Read 1D array from eqdsk
  std::vector<double> read_array(int n, std::string varName);

  // Read 2D array from eqdsk
  std::vector<std::vector<double>> read_2darray(int nx, int ny, std::string varName);

  // Write singular line out in EQDSK format
  int eqdsk_line_out(std::ofstream &file, double element, int counter);

  // Write out eqdsk arrays
  void eqdsk_write_array(std::ofstream &file, std::vector<double> array, int counter);

  // Set sign to determine if psi decreasing or increasing away from centre
  void set_rsig();


  public:

  // File streams
  std::ifstream eqdsk_file;
  std::ofstream eqdsk_out;

  // eqdsk data
  std::string header; // string containing eqdsk header information
  int nw; // Number of horizontal R points in grid
  int nh; // Number of vertical Z points in grid
  int nbdry; // Number of boundary points
  int nlim; // Number of limiter points

  double rdim; // EFIT Horizontal dimension in metre of computational box
  double zdim; // EFIT Vertical dimension in metre of computational box
  double rcentr; // EFIT Radial dimension at centre of plasma
  double rgrid; // EFIT Minimum R in metre of rectangular computational box
  double zmid; // EFIT centre of computational box in metres
  double rmaxis; // EFIT plasma centre (magnetic axis) in metre (R at central psi extremum)
  double zmaxis; // EFIT plasma centre (magnetic axis) in metre (Z at central psi extremum)
  double psimag1; // EFIT poloidal flux at the magnetic axis (Wb/rad)
  double psibdry1; // EFIT poloidal flux at the boundary (Wb/rad)
  double bcentr; // EFIT vacuum toroidal field at r=rcentr IGNORED BY SMARDDA
  double cpasma; // EFIT computed plasma current in A IGNORED BY SMARDDA
  double psimag2; // EFIT poloidal flux at the magnetic axis (Wb/rad) IGNORED BY SMARDDA
  double psibdry2; // EFIT poloidal flux at the boundary (Wb/rad) IGNORED BY SMARDDA
  double xdum; // empty dummy variables in data

  std::vector<double> fpol; // fpol size(nw) f(psi) in SMARDDA
  std::vector<double> pres; // pres size(nw)
  std::vector<double> ffprime; // ffprime(nw)
  std::vector<double> pprime; // pprime(nw)
  std::vector<std::vector<double>> psi; // psi(R,Z) size(nw*nh)
  std::vector<double> qpsi; // qpsi(nw)
  std::vector<double> rbdry; // Boundary points in R
  std::vector<double> zbdry; // Boundary points in Z
  std::vector<double> rlim; // Limiter points in R  
  std::vector<double> zlim; // Limiter points in Z  


  // attributes from SMARDDA
  int nr; // nw-1 (used for finite difference)
  int nz; // nh-1 (used for finite difference)

  double psiaxis; // centre flux?
  double psiqbdry; // bdry flux?
  double psinorm; // normalised psi (fabs(psiqbdry-psiaxis)/2)
  double dpsi; // 1d psi finite difference
  double rcen; // rcen calculated from centre()
  double zcen; // zcen calculated from centre()
  double rsig; // sign of dpsi/dr value (+1 -> Increase outwards, -1 -> Decrease outwards)


  double rmin; // min R value in equillibrium data
  double zmin; // min Z value in equillibrium data
  double rmax; // max R value in equillibrium data
  double zmax; // max Z value in equillibrium data

  double dr; // step size in R (rmax-rmin)/nr
  double dz; // step size in Z (zmax-zmin)/nz

  // alglib grids
  alglib::real_1d_array r_grid; // 1D grid R[nw] with spacing = dr (KNOTS)
  alglib::real_1d_array z_grid; // 1D grid Z[nh] with spacing = dz (KNOTS)
  alglib::real_1d_array psi_1dgrid; // 1D grid psi[nw] with spacing = dpsi (KNOTS)
  alglib::real_1d_array psi_grid; // 2D grid psi(R,Z) (FUNCTION VALUES)
  alglib::real_1d_array f_grid; // 1D grid for I(psi) (FUNCTION VALUES)
 


  // alglib splines
  alglib::spline2dinterpolant psiSpline; // 2d spline interpolant for Psi(R,Z)
  alglib::spline1dinterpolant fSpline; // 1d spline interpolant for f(psi) or I(psi)

  // Methods

  // Read eqdsk file
  void read_eqdsk(std::string filename);

  // Write out eqdsk data back out in eqdsk format
  void write_eqdsk_out();

  // Initialise the 1D arrays and 2d spline functions
  void init_interp_splines();

  // Write out psi(R,Z) data for gnuplotting
  void gnuplot_out();

  // Find central psi extrema
  void centre();

  // Create 2d spline structures for R(psi,theta) and Z(psi,theta)
  void rz_splines();

  // Caculate B field vector (in toroidal polars) at given position
  // set string to "polar" if position vector already in polars
  std::vector<double> b_field(std::vector<double> position, 
                              std::string startingFrom);

};

#endif

                                          

