#ifndef FILE_NGBLA
#define FILE_NGBLA

// #ifdef USE_BLAS
// extern "C"
// {
// #include <mkl_cblas.h>
// }
// #endif


#include <ngstd.hpp>


/// namespace for basic linear algebra
namespace ngbla
{
  using namespace std;
  using namespace ngstd;

  using ngstd::CArray;

#ifdef USE_MYCOMPLEX
  typedef MyComplex<double> Complex;
  inline double fabs (Complex v) { return ngstd::abs (v); }
#else
  // typedef std::complex<double> Complex;
  inline double fabs (Complex v) { return std::abs (v); }
#endif

  using ngstd::MyAtomicAdd;
  inline void MyAtomicAdd (Complex & x, Complex y)
  {
    auto real = y.real();
    ngstd::MyAtomicAdd (reinterpret_cast<double(&)[2]>(x)[0], real);
    auto imag = y.imag();
    ngstd::MyAtomicAdd (reinterpret_cast<double(&)[2]>(x)[1], imag);
  }

  inline bool IsComplex(double v) { return false; }
  inline bool IsComplex(Complex v) { return true; }
}


#ifdef PARALLEL
namespace ngstd
{
  template <>
  class MPI_Traits<ngbla::Complex>
  {
  public:
    /// gets the MPI datatype
    static MPI_Datatype MPIType () 
    { 
      // return MPI_C_DOUBLE_COMPLEX;   // no MPI_SUM defined ??
      return MPI_DOUBLE_COMPLEX;
    }
  };
}
#endif




#include "expr.hpp"
#include "vector.hpp"
#include "matrix.hpp"
#include "avector.hpp"
#include "cholesky.hpp"
#include "symmetricmatrix.hpp"
#include "bandmatrix.hpp"
#include "tensor.hpp"

#include "cuda_bla.hpp"

namespace ngbla
{

  /// Computes eigenvalues and vectors of the symmetric matrix mat.
  extern NGS_DLL_HEADER void CalcEigenSystem (const FlatMatrix<double> & mat,
			       FlatVector<double> & lami,
			       FlatMatrix<double> & eigenvecs);



  /// Computes the Schur Complement.
  extern NGS_DLL_HEADER void CalcSchurComplement (const FlatMatrix<double> a, 
				   FlatMatrix<double> s,
				   const BitArray & used,
				   LocalHeap & lh);
}

#include "ng_lapack.hpp"

#endif
