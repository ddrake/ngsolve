/*********************************************************************/
/* File:   basematrix.cpp                                            */
/* Author: Joachim Schoeberl                                         */
/* Date:   25. Mar. 2000                                             */
/*********************************************************************/

/* 
   base class in matrix hierarchy
*/

#include <la.hpp>

namespace ngla
{
  BaseMatrix :: BaseMatrix()
    : paralleldofs (NULL)
  {
    ;
  }

  BaseMatrix :: BaseMatrix (shared_ptr<ParallelDofs> aparalleldofs)
    : paralleldofs ( aparalleldofs )
  {     
    ;
  }
  
  BaseMatrix :: ~BaseMatrix ()
  {
    ;
  }

  int BaseMatrix :: VHeight() const
  {
    throw Exception (string("BaseMatrix::VHeight not overloaded, type = ")+typeid(*this).name());
  }
  
  int BaseMatrix :: VWidth() const
  {
    throw Exception (string("BaseMatrix::VWidth not overloaded, type = ")+typeid(*this).name());
  }

  BaseVector & BaseMatrix :: AsVector()
  {
    throw Exception (string("BaseMatrix::AsVector not overloaded, type = ")+typeid(*this).name());
  }

  const BaseVector & BaseMatrix :: AsVector() const
  {
    throw Exception (string("BaseMatrix::AsVector const not overloaded, type = ")+typeid(*this).name());    
  }
  
  void BaseMatrix :: SetZero()
  {
    AsVector() = 0;
  }

  ostream & BaseMatrix :: Print (ostream & ost) const
  {
    return (ost << "Print base-matrix" << endl);
  }

  void BaseMatrix :: MemoryUsage (Array<MemoryUsageStruct*> & mu) const
  { ; }

  shared_ptr<BaseMatrix> BaseMatrix :: CreateMatrix () const
  {
    throw Exception (string("BaseMatrix::CreateMatrix not overloaded, type = ")+typeid(*this).name());        
  }

  AutoVector BaseMatrix :: CreateVector () const
  {
    throw Exception (string("BaseMatrix::CreateVector not overloaded, type = ")+typeid(*this).name());            
  }

  AutoVector BaseMatrix :: CreateRowVector () const
  {
    return CreateVector();
  }

  AutoVector BaseMatrix :: CreateColVector () const
  {
    return CreateVector();
  }




  
  void BaseMatrix :: Mult (const BaseVector & x, BaseVector & y) const
  {
    y = 0;
    MultAdd (1, x, y);
  }

  void BaseMatrix :: MultAdd (double s, const BaseVector & x, BaseVector & y) const
  {
    //    cout << "Warning: BaseMatrix::MultAdd(double), this = " << typeid(*this).name() << endl;
    auto temp = y.CreateVector();
    Mult (x, *temp);
    y += s * *temp;
  }

  void BaseMatrix :: MultAdd (Complex s, const BaseVector & x, BaseVector & y) const 
  {
    stringstream err;
    err << "BaseMatrix::MultAdd (Complex) called, type = " 
	<< typeid(*this).name();
    throw Exception (err.str());
  }
  
  void BaseMatrix :: MultTransAdd (double s, const BaseVector & x, BaseVector & y) const
  {
    cout << "warning: BaseMatrix::MultTransAdd(double) calls MultAdd, ";
    cout << "type = " << typeid(*this).name() << endl;
    MultAdd (s, x, y);
    return;

    stringstream err;
    err << "BaseMatrix::MultTransAdd (double) called, type = " 
	<< typeid(*this).name();
    throw Exception (err.str());
  }

  void BaseMatrix :: MultTransAdd (Complex s, const BaseVector & x, BaseVector & y) const
  {
    //    cout << "warning: BaseMatrix::MultTransAdd(complex) calls MultAdd" << endl;
    MultAdd (s, x, y);
    return;

    stringstream err;
    err << "BaseMatrix::MultTransAdd (Complex) called, type = " 
	<< typeid(*this).name();
    throw Exception (err.str());
  }

   // to split mat x vec for symmetric matrices
  void BaseMatrix :: MultAdd1 (double s, const BaseVector & x, BaseVector & y,
			       const BitArray * ainner,
			       const Array<int> * acluster) const
  {
    MultAdd (s, x, y);
  }

  void BaseMatrix :: MultAdd2 (double s, const BaseVector & x, BaseVector & y,
			       const BitArray * ainner,
			       const Array<int> * acluster) const
  {
    ;
  }
  



  shared_ptr<BaseMatrix> BaseMatrix :: InverseMatrix (shared_ptr<BitArray> subset) const 
  {
    cerr << "BaseMatrix::InverseMatrix not available" << endl;
    return NULL;
  }
  
  shared_ptr<BaseMatrix> BaseMatrix :: InverseMatrix (shared_ptr<const Array<int>> clusters) const
  {
    cerr << "BaseMatrix::InverseMatrix not available" << endl;
    return NULL;
  }
  
  INVERSETYPE BaseMatrix :: SetInverseType ( INVERSETYPE ainversetype ) const
  {
    cerr << "BaseMatrix::SetInverseType not available" << endl;
    return SPARSECHOLESKY;
  }
  
  INVERSETYPE BaseMatrix :: SetInverseType ( string ainversetype ) const
  {
    cerr << "BaseMatrix::SetInverseType not available" << endl;
    return SPARSECHOLESKY;
  }
  
  INVERSETYPE BaseMatrix :: GetInverseType () const
  {
    cerr << "BaseMatrix::GetInverseType not available" << endl;
    return SPARSECHOLESKY;
  }

  void BaseMatrix :: DoArchive (Archive & ar)
  {
    ;
  }


  template<>
  S_BaseMatrix<double> :: S_BaseMatrix () 
  { ; }


  template<>
  S_BaseMatrix<double> :: ~S_BaseMatrix () 
  { ; }

  S_BaseMatrix<Complex> :: S_BaseMatrix () 
  { ; }

  S_BaseMatrix<Complex> :: ~S_BaseMatrix () 
  { ; }


  void S_BaseMatrix<Complex> :: 
  MultAdd (double s, const BaseVector & x, BaseVector & y) const 
  {
    MultAdd (Complex(s), x, y);
  }

  void S_BaseMatrix<Complex> :: 
  MultAdd (Complex s, const BaseVector & x, BaseVector & y) const 
  {
    stringstream err;
    err << "S_BaseMatrix<Complex>::MultAdd (Complex) called, type = " 
	<< typeid(*this).name();
    throw Exception (err.str());
  }
  
  void S_BaseMatrix<Complex> :: 
  MultTransAdd (double s, const BaseVector & x, BaseVector & y) const
  {
    MultTransAdd (Complex(s), x, y);
  }

  void S_BaseMatrix<Complex> :: 
  MultTransAdd (Complex s, const BaseVector & x, BaseVector & y) const
  {
    stringstream err;
    err << "S_BaseMatrix<Complex>::MultTransAdd (Complex) called, type = " 
	<< typeid(*this).name();
    throw Exception (err.str());
  }


  void VMatVecExpr :: CheckSize (BaseVector & dest_vec) const
  {
    if (m.Height() != dest_vec.Size() || m.Width() != x.Size())
      throw Exception (ToString ("matrix-vector: size does not fit\n") +
                       "matrix-type = " + typeid(m).name() + "\n" +
                       "matrix:     " + ToString(m.Height()) + " x " + ToString(m.Width()) + "\n"
                       "vector in : " + ToString(x.Size()) + "\n"
                       "vector res: " + ToString(dest_vec.Size()));

  }



  string GetInverseName (INVERSETYPE type)
  {
    switch (type)
      {
      case PARDISO:         return "pardiso";
      case PARDISOSPD:      return "pardisospd";
      case SPARSECHOLESKY:  return "sparsecholesky";
      case SUPERLU:         return "superlu";
      case SUPERLU_DIST:    return "superlu_dist";
      case MUMPS:           return "mumps";
      case MASTERINVERSE:   return "masterinverse";
      case UMFPACK:         return "umfpack";
      }
    return "";
  }

}
