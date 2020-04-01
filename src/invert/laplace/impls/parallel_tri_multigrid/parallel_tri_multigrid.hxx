/**************************************************************************
 * Perpendicular Laplacian inversion. Parallel code using FFT
 * and tridiagonal solver.
 *
 **************************************************************************
 * Copyright 2010 B.D.Dudson, S.Farley, M.V.Umansky, X.Q.Xu
 *
 * Contact: Ben Dudson, bd512@york.ac.uk
 *
 * This file is part of BOUT++.
 *
 * BOUT++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BOUT++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.
 *
 **************************************************************************/

class LaplaceParallelTriMG;

#ifndef __PARALLEL_TRI_MG_H__
#define __PARALLEL_TRI_MG_H__

#include <invert_laplace.hxx>
#include <dcomplex.hxx>
#include <options.hxx>

class LaplaceParallelTriMG : public Laplacian {
public:
  LaplaceParallelTriMG(Options *opt = nullptr, const CELL_LOC loc = CELL_CENTRE, Mesh *mesh_in = nullptr);
  ~LaplaceParallelTriMG(){};

  using Laplacian::setCoefA;
  void setCoefA(const Field2D &val) override {
    ASSERT1(val.getLocation() == location);
    ASSERT1(localmesh == val.getMesh());
    A = val;
  }
  using Laplacian::setCoefC;
  void setCoefC(const Field2D &val) override {
    ASSERT1(val.getLocation() == location);
    ASSERT1(localmesh == val.getMesh());
    C = val;
  }
  using Laplacian::setCoefD;
  void setCoefD(const Field2D &val) override {
    ASSERT1(val.getLocation() == location);
    ASSERT1(localmesh == val.getMesh());
    D = val;
  }
  using Laplacian::setCoefEx;
  void setCoefEx(const Field2D &UNUSED(val)) override {
    throw BoutException("LaplaceParallelTriMG does not have Ex coefficient");
  }
  using Laplacian::setCoefEz;
  void setCoefEz(const Field2D &UNUSED(val)) override {
    throw BoutException("LaplaceParallelTriMG does not have Ez coefficient");
  }

  using Laplacian::solve;
  FieldPerp solve(const FieldPerp &b) override;
  FieldPerp solve(const FieldPerp &b, const FieldPerp &x0) override;
  //FieldPerp solve(const FieldPerp &b, const FieldPerp &x0, const FieldPerp &b0 = 0.0);

  BoutReal getMeanIterations() const { return ipt_mean_its; }
  void resetMeanIterations() { ipt_mean_its = 0; }

  void get_initial_guess(const int jy, const int kz, Matrix<dcomplex> &r,
      Tensor<dcomplex> &lowerGuardVector, Tensor<dcomplex> &upperGuardVector,
      Matrix<dcomplex> &xk1d);
  void check_diagonal_dominance(const Array<dcomplex> &a, const Array<dcomplex> &b,
      const Array<dcomplex> &c, const int ncx, const int jy, const int kz);
  bool is_diagonally_dominant(const dcomplex al, const dcomplex au, const dcomplex bl, const dcomplex bu, const int jy, const int kz);

  void resetSolver();

  bool all(const Array<bool>);
  bool any(const Array<bool>);

  void get_errors(Array<BoutReal> &error_rel, Array<BoutReal> &error_abs, const Matrix<dcomplex> x,const Matrix<dcomplex> xlast);


  void refine(Matrix<dcomplex> &xloc, Matrix<dcomplex> &xloclast);

  struct Level {

  public:

    Tensor<dcomplex> upperGuardVector, lowerGuardVector;
    Matrix<dcomplex> al, bl, au, bu;
    Matrix<dcomplex> alold, blold, auold, buold;
    Matrix<dcomplex> r1, r2, r3, r4, r5, r6, r7, r8;
    Array<dcomplex> rl, ru;
    Array<dcomplex> rlold, ruold;
    Matrix<dcomplex> minvb;
    Matrix<dcomplex> avec, bvec, cvec, rvec, residual, soln, solnlast;
    Array<dcomplex> acomm, bcomm, ccomm;

    int index_in;
    int index_out;
    int err;
    MPI_Comm comm;
    int xproc;
    int yproc;
    int myproc;
    int xs, xe, ncx;
    int current_level;

  };

  void levels_info(const Level l);
  void init(Level &level, const Level lup, const int ncx, const int xs, const int xe, const int current_level);
  void init(Level &level, const int ncx, const int jy, const Matrix<dcomplex> avec, const Matrix<dcomplex> bvec, const Matrix<dcomplex> cvec, const Matrix<dcomplex> bcmplx, const int xs, const int xe, const int current_level);

  void jacobi(Level &level, const int jy, Matrix<dcomplex> &xloc, Matrix<dcomplex> &xloclast);
  void jacobi_full_system(Level &level);
  void gauss_seidel_full_system(Level &level);
  void gauss_seidel_red_black_full_system(Level &level);
  void refine_full_system(Level &level, Matrix<dcomplex> &fine_error);

  void coarsen(const Level level, Matrix<dcomplex> &xloc, Matrix<dcomplex> &xloclast, int jy);
  void coarsen_full_system(Level &level, const Matrix<dcomplex> fine_residual);

  void calculate_residual_full_system(Level &level);
  void calculate_total_residual(BoutReal &total, const Level level);
  //void calculate_total_residual(Array<BoutReal> &total, const Level level);
  void update_solution(Level &l, const Matrix<dcomplex> &fine_error);
  void reconstruct_full_solution(Level &level, const int jy, Matrix<dcomplex> &halos);

private:
  // The coefficents in
  // D*grad_perp^2(x) + (1/C)*(grad_perp(C))*grad_perp(x) + A*x = b
  Field2D A, C, D;

  // Flag to state whether this is the first time the solver is called
  // on the point (jy,kz).
  Matrix<bool> first_call;

  // Save previous x in Fourier space
  Tensor<dcomplex> x0saved;

  /// Solver tolerances
  BoutReal rtol, atol;

  /// Maximum number of iterations
  int maxits;

  /// Maximum number of coarse grids
  int max_level;

  /// Maximum number of iterations per grid
  int max_cycle;

  /// Mean number of iterations taken by the solver
  BoutReal ipt_mean_its;

  /// Counter for the number of times the solver has been called
  int ncalls;

  /// Flag for method selection
  bool new_method;

  /// If true, use previous timestep's solution as initial guess for next step
  /// If false, use the approximate solution of the system (neglecting the
  /// coupling terms between processors) as the initial guess.
  /// The first timestep always uses the approximate solution.
  bool use_previous_timestep;

  //Tensor<dcomplex> upperGuardVector, lowerGuardVector;
///  Matrix<dcomplex> al, bl, au, bu;
///  Matrix<dcomplex> alold, blold, auold, buold;
///  Matrix<dcomplex> r1, r2, r3, r4, r5, r6, r7, r8;
  bool store_coefficients;

  int nmode;
  int index_in;
  int index_out;
  int proc_in;
  int proc_out;

};


#endif // __PARALLEL_TRI_H__