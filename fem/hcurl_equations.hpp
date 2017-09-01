#ifndef FILE_HCURL_EQUATIONS
#define FILE_HCURL_EQUATIONS

/*********************************************************************/
/* File:   hcurl_equations.hpp                                       */
/* Author: Joachim Schoeberl                                         */
/* Date:   10. Feb. 2002                                             */
/*********************************************************************/


namespace ngfem
{


  /*
    
  Maxwell integrators:


  Finite Element Integrators for H(curl)

  Mapping with covariant transformation

  Requires H(curl) finite elements
  */





  /// Identity operator, covariant transformation
  template <int D, typename FEL = HCurlFiniteElement<D> >
  class DiffOpIdEdge : public DiffOp<DiffOpIdEdge<D, FEL> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = D };
    enum { DIM_ELEMENT = D };
    enum { DIM_DMAT = D };
    enum { DIFFORDER = 0 };

    static constexpr bool SUPPORT_PML = true;
    template <typename MIP, typename MAT>
    static void GenerateMatrix (const FiniteElement & fel, 
				const MIP & mip,
				MAT && mat, LocalHeap & lh)
    {
      GenerateMatrix2 (fel, mip, SliceIfPossible<double> (Trans(mat)), lh);
    }

    template <typename AFEL, typename MIP, typename MAT>
    static void GenerateMatrix2 (const AFEL & fel, const MIP & mip,
				MAT && mat, LocalHeap & lh)
    {
      HeapReset hr(lh);
      mat = static_cast<const FEL&>(fel).GetShape(mip.IP(), lh) * mip.GetJacobianInverse();
    }

    template <typename AFEL>
    static void GenerateMatrix2 (const AFEL & fel, 
                                 const MappedIntegrationPoint<D,D> & mip,
                                 SliceMatrix<> mat, LocalHeap & lh)
    {
      static_cast<const FEL&> (fel).CalcMappedShape (mip, mat);  
    }


    static void GenerateMatrixIR (const FiniteElement & fel, 
                                  const MappedIntegrationRule<D,D> & mir,
                                  SliceMatrix<double,ColMajor> mat, LocalHeap & lh)
    {
      static_cast<const FEL&> (fel).CalcMappedShape (mir, Trans(mat));
    }

    static void GenerateMatrixSIMDIR (const FiniteElement & fel,
                                      const SIMD_BaseMappedIntegrationRule & mir,
                                      BareSliceMatrix<SIMD<double>> mat)
    {
      static_cast<const FEL&>(fel).CalcMappedShape (mir, mat);      
    }


    template <typename FEL1, typename MIP, class TVX, class TVY>
    static void Apply (const FEL1 & fel, const MIP & mip,
		       const TVX & x, TVY && y,
		       LocalHeap & lh) 
    {
      typedef typename TVX::TSCAL TSCAL;
      HeapReset hr(lh);
      Vec<D,TSCAL> hx;
      hx = Trans (static_cast<const FEL&> (fel).GetShape (mip.IP(), lh)) * x;
      y = Trans (mip.GetJacobianInverse()) * hx;
    }

    template <typename FEL1, class TVX, class TVY>
    static void Apply (const FEL1 & fel, const MappedIntegrationPoint<D,D> & mip,
		       const TVX & x, TVY && y,
		       LocalHeap & lh) 
    {
      HeapReset hr(lh);
      FlatMatrixFixWidth<D> shape(fel.GetNDof(), lh);
      static_cast<const FEL&> (fel).CalcMappedShape (mip, shape);
      y = Trans(shape) * x;
    }


    template <typename FEL1, typename MIP, class TVX, class TVY>
    static void ApplyTrans (const FEL1 & fel, const MIP & mip,
			    const TVX & x, TVY & y,
			    LocalHeap & lh) 
    {
      typedef typename TVX::TSCAL TSCAL;
      HeapReset hr(lh);
      Vec<D,TSCAL> hx;
      hx = mip.GetJacobianInverse() * x;
      y = static_cast<const FEL&> (fel).GetShape (mip.IP(),lh) * hx;
    }

    template <typename FEL1, class TVX, class TVY>
    static void ApplyTrans (const FEL1 & fel, const MappedIntegrationPoint<D,D> & mip,
			    const TVX & x, TVY & y,
			    LocalHeap & lh) 
    {
      HeapReset hr(lh);
      FlatMatrixFixWidth<D> shape(fel.GetNDof(), lh);
      static_cast<const FEL&> (fel).CalcMappedShape (mip, shape);
      y = shape * x;
    }


    static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<double> x, BareSliceMatrix<SIMD<double>> y)
    {
      static_cast<const FEL&> (fel).Evaluate (mir, x, y);
    }    

    static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<Complex> x, BareSliceMatrix<SIMD<Complex>> y)
    {
      static_cast<const FEL&> (fel).Evaluate (mir, x, y);
    }    

    static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<double>> y, BareSliceVector<double> x)
    {
       static_cast<const FEL&> (fel).AddTrans (mir, y, x);
    }    
    
    static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<Complex>> y, BareSliceVector<Complex> x)
    {
       static_cast<const FEL&> (fel).AddTrans (mir, y, x);
    }    
    
  };








  /// Operator $curl$, Piola-transformation
  template <int D, typename FEL = HCurlFiniteElement<D> >
  class DiffOpCurlEdge : public DiffOp<DiffOpCurlEdge<D, FEL> >
  {
  };

  template <typename FEL> class DiffOpCurlEdge<2,FEL> 
    : public DiffOp<DiffOpCurlEdge<2, FEL> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = 2 };
    enum { DIM_ELEMENT = 2 };
    enum { DIM_DMAT = 1 };
    enum { DIFFORDER = 1 };

    static string Name() { return "curl"; }

    static constexpr bool SUPPORT_PML = true;

    template <typename AFEL, typename MIP, typename MAT>
    static void GenerateMatrix (const AFEL & fel, const MIP & mip,
				MAT & mat, LocalHeap & lh)
    {
      mat = 1.0/mip.GetJacobiDet() * 
	Trans (static_cast<const FEL&> (fel).GetCurlShape(mip.IP(), lh));
    }
    
    static void GenerateMatrixSIMDIR (const FiniteElement & fel,
                                      const SIMD_BaseMappedIntegrationRule & mir,
                                      BareSliceMatrix<SIMD<double>> mat)
    {
      static_cast<const FEL&>(fel).CalcMappedCurlShape (mir, mat);      
    }

    template <typename AFEL, typename MIP, class TVX, class TVY>
    static void Apply (const AFEL & fel, const MIP & mip,
		       const TVX & x, TVY && y,
		       LocalHeap & lh) 
    {
      y = (1.0/mip.GetJacobiDet()) * 
	(Trans (static_cast<const FEL&>(fel).GetCurlShape(mip.IP(), lh)) * x);
    }

    using DiffOp<DiffOpCurlEdge<2> >::ApplySIMDIR;        
    static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<double> x, BareSliceMatrix<SIMD<double>> y)
    {
      static_cast<const FEL&> (fel).EvaluateCurl (mir, x, y);
    }    

    using DiffOp<DiffOpCurlEdge<2> >::AddTransSIMDIR;        
    static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<double>> y, BareSliceVector<double> x)
    {
       static_cast<const FEL&> (fel).AddCurlTrans (mir, y, x);
    }    
  };

  template <typename FEL> class DiffOpCurlEdge<3,FEL> : public DiffOp<DiffOpCurlEdge<3,FEL> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = 3 };
    enum { DIM_ELEMENT = 3 };
    enum { DIM_DMAT = 3 };
    enum { DIFFORDER = 1 };

    static string Name() { return "curl"; }

    static constexpr bool SUPPORT_PML = true;


    template <typename MIP, typename MAT>
    static void GenerateMatrix (const FiniteElement & fel, 
				const MIP & mip,
				MAT && mat, LocalHeap & lh)
    {
      GenerateMatrix2 (fel, mip, SliceIfPossible<double> (Trans(mat)), lh);
    }

    template <typename AFEL, typename MIP, typename MAT>
    static void GenerateMatrix2 (const AFEL & fel, const MIP & mip,
				MAT && mat, LocalHeap & lh)
    {
      // cout << "diffopcurl: slow matrix" << endl;
      // cout << "mtype = " << typeid(mat).name() << endl;

      mat = (1.0/mip.GetJacobiDet())
        * (static_cast<const FEL&>(fel).GetCurlShape(mip.IP(), lh) * Trans(mip.GetJacobian()));
    }
    
    template <typename AFEL>
    static void GenerateMatrix2 (const AFEL & fel, 
                                 const MappedIntegrationPoint<3,3> & mip,
                                 SliceMatrix<> mat, LocalHeap & lh)
    {
      static_cast<const FEL&> (fel).CalcMappedCurlShape (mip, mat);
    }

    static void GenerateMatrixIR (const FiniteElement & fel, 
                                  const MappedIntegrationRule<3,3> & mir,
                                  SliceMatrix<double,ColMajor> mat, LocalHeap & lh)
    {
      static_cast<const FEL&> (fel).CalcMappedCurlShape (mir, Trans(mat));
    }
    
    static void GenerateMatrixSIMDIR (const FiniteElement & fel,
                                      const SIMD_BaseMappedIntegrationRule & mir,
                                      BareSliceMatrix<SIMD<double>> mat)
    {
      static_cast<const FEL&>(fel).CalcMappedCurlShape (mir, mat);      
    }


    template <typename AFEL, typename MIP, class TVX, class TVY>
    static void Apply (const AFEL & fel, const MIP & mip,
		       const TVX & x, TVY && y,
		       LocalHeap & lh) 
    {
      // typedef typename TVX::TSCAL TSCAL;
      // Vec<3,TSCAL> hx;
      auto hx = static_cast<const FEL&>(fel).EvaluateCurlShape (mip.IP(), x, lh);
      y = (1.0/mip.GetJacobiDet()) * (mip.GetJacobian() * hx);
    }


    template <typename AFEL, typename MIP, class TVX, class TVY>
    static void ApplyTrans (const AFEL & fel, const MIP & mip,
			    const TVX & x, TVY & y,
			    LocalHeap & lh) 
    {
      typedef typename TVX::TSCAL TSCAL;

      Vec<3,TSCAL> hx;
      hx = (1.0/mip.GetJacobiDet()) * (Trans (mip.GetJacobian()) * x);
      y = static_cast<const FEL&>(fel).GetCurlShape(mip.IP(), lh) * hx;
    }

    using DiffOp<DiffOpCurlEdge<3> >::ApplySIMDIR;        
    static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<double> x, BareSliceMatrix<SIMD<double>> y)
    {
      static_cast<const FEL&> (fel).EvaluateCurl (mir, x, y);
    }    

    using DiffOp<DiffOpCurlEdge<3> >::AddTransSIMDIR;        
    static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<double>> y, BareSliceVector<double> x)
    {
       static_cast<const FEL&> (fel).AddCurlTrans (mir, y, x);
    }    
    
  };

















  // \int_{C} v.\tau
  template <int D>
  class DiffOpTangentialComponentEdge : public DiffOp<DiffOpTangentialComponentEdge<D> >
  {
  public:
    enum { DIM = D };
    enum { DIM_SPACE = D };
    enum { DIM_ELEMENT = D };
    enum { DIM_DMAT = 1 };
    enum { DIFFORDER = 0 };
    static constexpr bool SUPPORT_PML = true;

    template <typename FEL, typename MIP, typename MAT>
    static void GenerateMatrix (const FEL & fel, const MIP & mip,
				MAT & mat, LocalHeap & lh)
    {
      Vec<D> tv = mip.GetTV();
      Vec<D> tv_JI = mip.GetJacobianInverse () * tv;
   
      mat = Trans ( fel.GetShape(mip.IP(), lh) * tv_JI );
    }
  };

  
  /// Identity on codim 2
  template <int D, typename FEL = HCurlFiniteElement<D-2> >
  class DiffOpIdBBoundaryEdge : public DiffOp<DiffOpIdBBoundaryEdge<D,FEL> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = D };
    enum { DIM_ELEMENT = D-2 };
    enum { DIM_DMAT = D };
    enum { DIFFORDER = 0 };
    static constexpr bool SUPPORT_PML = true;

    template <typename FEL1, typename MIP, typename MAT>
    static void GenerateMatrix (const FEL1 & fel, const MIP & mip,
				MAT & mat, LocalHeap & lh)
    {
      mat = Trans (mip.GetJacobianInverse ()) * 
	Trans (static_cast<const FEL&> (fel).GetShape(mip.IP(),lh));
    }

    template <typename FEL1, typename MIP, class TVX, class TVY>
    static void Apply (const FEL1 & fel, const MIP & mip,
		       const TVX & x, TVY & y,
		       LocalHeap & lh) 
    {
      typedef typename TVX::TSCAL TSCAL;

      Vec<DIM_ELEMENT,TSCAL> hx;
      hx = Trans (static_cast<const FEL&> (fel).GetShape (mip.IP(),lh)) * x;
      y = Trans (mip.GetJacobianInverse()) * hx;
    }

    template <typename FEL1, typename MIP, class TVX, class TVY>
    static void ApplyTrans (const FEL1 & fel, const MIP & mip,
			    const TVX & x, TVY & y,
			    LocalHeap & lh) 
    {
      typedef typename TVX::TSCAL TSCAL;

      Vec<DIM_ELEMENT,TSCAL> hx;
      hx = mip.GetJacobianInverse() * x;
      y = static_cast<const FEL&> (fel).GetShape (mip.IP(),lh) * hx;

      /*
      FlatMatrixFixWidth<DIM_ELEMENT> mshape (y.Height(), &hv(0)); 
      FlatMatrix<> mshape2 (y.Height(), DIM_ELEMENT, &hv(0)); 
      y = mshape2 * hx; 
      */
    }

    static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<double> x, BareSliceMatrix<SIMD<double>> y)
    {
      static_cast<const FEL&> (fel).Evaluate (mir, x, y);
    }    
           
    static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<double>> y, BareSliceVector<double> x)
    {
       static_cast<const FEL&> (fel).AddTrans (mir, y, x);
    }    
    
    
    static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<Complex> x, BareSliceMatrix<SIMD<Complex>> y)
    {
      static_cast<const FEL&> (fel).Evaluate (mir, x, y);
    }    
           
    static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<Complex>> y, BareSliceVector<Complex> x)
    {
       static_cast<const FEL&> (fel).AddTrans (mir, y, x);
    }    
    
    
  };
  /// Identity on boundary
  template <int D, typename FEL = HCurlFiniteElement<D-1> >
  class DiffOpIdBoundaryEdge : public DiffOp<DiffOpIdBoundaryEdge<D,FEL> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = D };
    enum { DIM_ELEMENT = D-1 };
    enum { DIM_DMAT = D };
    enum { DIFFORDER = 0 };

    static constexpr bool SUPPORT_PML = true;
    template <typename FEL1, typename MIP, typename MAT>
    static void GenerateMatrix (const FEL1 & fel, const MIP & mip,
				MAT & mat, LocalHeap & lh)
    {
      mat = Trans (mip.GetJacobianInverse ()) * 
	Trans (static_cast<const FEL&> (fel).GetShape(mip.IP(),lh));
    }

    static void GenerateMatrixSIMDIR (const FiniteElement & fel,
                                      const SIMD_BaseMappedIntegrationRule & mir,
                                      BareSliceMatrix<SIMD<double>> mat)
    {
      static_cast<const FEL&>(fel).CalcMappedShape (mir, mat);      
    }

    template <typename FEL1, typename MIP, class TVX, class TVY>
    static void Apply (const FEL1 & fel, const MIP & mip,
		       const TVX & x, TVY & y,
		       LocalHeap & lh) 
    {
      typedef typename TVX::TSCAL TSCAL;

      Vec<D-1,TSCAL> hx;
      hx = Trans (static_cast<const FEL&> (fel).GetShape (mip.IP(),lh)) * x;
      y = Trans (mip.GetJacobianInverse()) * hx;
    }

    template <typename FEL1, typename MIP, class TVX, class TVY>
    static void ApplyTrans (const FEL1 & fel, const MIP & mip,
			    const TVX & x, TVY & y,
			    LocalHeap & lh) 
    {
      typedef typename TVX::TSCAL TSCAL;

      Vec<DIM_ELEMENT,TSCAL> hx;
      hx = mip.GetJacobianInverse() * x;
      y = static_cast<const FEL&> (fel).GetShape (mip.IP(),lh) * hx;

      /*
      FlatMatrixFixWidth<DIM_ELEMENT> mshape (y.Height(), &hv(0)); 
      FlatMatrix<> mshape2 (y.Height(), DIM_ELEMENT, &hv(0)); 
      y = mshape2 * hx; 
      */
    }
  
    static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<double> x, BareSliceMatrix<SIMD<double>> y)
    {
      static_cast<const FEL&> (fel).Evaluate (mir, x, y);
    }    
           
    static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<double>> y, BareSliceVector<double> x)
    {
       static_cast<const FEL&> (fel).AddTrans (mir, y, x);
    }    
    
    
    static void ApplySIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<Complex> x, BareSliceMatrix<SIMD<Complex>> y)
    {
      static_cast<const FEL&> (fel).Evaluate (mir, x, y);
    }    
           
    static void AddTransSIMDIR (const FiniteElement & fel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<Complex>> y, BareSliceVector<Complex> x)
    {
       static_cast<const FEL&> (fel).AddTrans (mir, y, x);
    }    
    
  };



  /// Curl on boundary
  template <typename FEL = HCurlFiniteElement<2> > 
  class DiffOpCurlBoundaryEdge : public DiffOp<DiffOpCurlBoundaryEdge<FEL> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = 3 };
    enum { DIM_ELEMENT = 2 };
    enum { DIM_DMAT = 1 };
    enum { DIFFORDER = 1 };

    static constexpr bool SUPPORT_PML = true;
    template <typename AFEL, typename MIP, typename MAT>
    static void GenerateMatrix (const AFEL & fel, const MIP & mip,
				MAT & mat, LocalHeap & lh)
    {
      mat = 1.0/mip.GetJacobiDet() * 
	Trans (static_cast<const FEL&>(fel).GetCurlShape(mip.IP(),lh));
    }


    template <typename AFEL, typename MIP, class TVX, class TVY>
    static void Apply (const AFEL & fel, const MIP & mip,
		       const TVX & x, TVY & y,
		       LocalHeap & lh) 
    {
      y = (1.0/mip.GetJacobiDet()) * 
	(Trans (static_cast<const FEL&> (fel).GetCurlShape(mip.IP(),lh)) * x);
    }

    template <typename AFEL, typename MIP, class TVX, class TVY>
    static void ApplyTrans (const AFEL & fel, const MIP & mip,
			    const TVX & x, TVY & y,
			    LocalHeap & lh) 
    {
      y = static_cast<const FEL&>(fel).GetCurlShape(mip.IP(),lh) * ((1.0/mip.GetJacobiDet()) * x);
    
    }
  };



template <typename FEL = HCurlFiniteElement<2> >
class DiffOpCurlBoundaryEdgeVec : public DiffOp<DiffOpCurlBoundaryEdgeVec<FEL> >
{
public:
  enum { DIM = 1 };
  enum { DIM_SPACE = 3 };
  enum { DIM_ELEMENT = 2 };
  enum { DIM_DMAT = 3 };
  enum { DIFFORDER = 1 };

    static constexpr bool SUPPORT_PML = true;
  static const FEL & Cast (const FiniteElement & fel) 
  { return static_cast<const FEL&> (fel); }

  template <typename AFEL, typename MIP, typename MAT>
  static void GenerateMatrix (const AFEL & fel, const MIP & mip,
			      MAT & mat, LocalHeap & lh)
  {
    auto scaled_nv = (1.0/mip.GetJacobiDet()) * mip.GetNV();
    mat = scaled_nv * Trans(Cast(fel).GetCurlShape (mip.IP(), lh));
  }

  template <typename AFEL, typename MIP, class TVX, class TVY> 
  static void Apply (const AFEL & fel, const MIP & mip,
		     const TVX & x, TVY & y,
		     LocalHeap & lh)
  {
    y = ( (1.0/mip.GetJacobiDet())*(InnerProduct (Cast(fel).GetCurlShape (mip.IP(), lh), x) )) * mip.GetNV();
  }

  template <typename AFEL, typename MIP, class TVX, class TVY>
  static void ApplyTrans (const AFEL & fel, const MIP & mip,
			  const TVX & x, TVY & y,
			  LocalHeap & lh)
  {
    y = ((1.0/mip.GetJacobiDet())* InnerProduct (x, mip.GetNV()) ) * Cast(fel).GetCurlShape (mip.IP(), lh);
  }
};







  // bilinearform integrators



  /*
  /// 
  template <int D, typename FEL = HCurlFiniteElement<D> >
  class CurlCurlEdgeIntegrator 
    : public T_BDBIntegrator<DiffOpCurlEdge<D>, DiagDMat<DIM_CURL_TRAIT<D>::DIM>, FEL>
  {
    typedef  T_BDBIntegrator<DiffOpCurlEdge<D>, DiagDMat<DIM_CURL_TRAIT<D>::DIM>, FEL> BASE;
  public:
    using BASE::T_BDBIntegrator;
    virtual string Name () const { return "CurlCurlEdge"; }
  };
  */

  template <int D>
  using CurlCurlEdgeIntegrator = T_BDBIntegrator<DiffOpCurlEdge<D>, DiagDMat<DIM_CURL_TRAIT<D>::DIM>, HCurlFiniteElement<D>>;
  


  /// 
  class CurlCurlBoundaryEdgeIntegrator 
    : public T_BDBIntegrator<DiffOpCurlBoundaryEdge<>, DiagDMat<1>, HCurlFiniteElement<2> >
  {
    typedef T_BDBIntegrator<DiffOpCurlBoundaryEdge<>, DiagDMat<1>, HCurlFiniteElement<2> > BASE;
  public:
    using T_BDBIntegrator<DiffOpCurlBoundaryEdge<>, DiagDMat<1>, HCurlFiniteElement<2> >::T_BDBIntegrator;
    ///
    virtual bool BoundaryForm () const { return 1; }
    ///
    virtual string Name () const { return "CurlCurlBoundaryEdge"; }
  };

  /// 
  template <int D, typename FEL = HCurlFiniteElement<D> >
  class CurlCurlEdgeOrthoIntegrator 
    : public T_BDBIntegrator<DiffOpCurlEdge<D>, OrthoDMat<DIM_CURL_TRAIT<D>::DIM>, FEL>
  {
    typedef  T_BDBIntegrator<DiffOpCurlEdge<D>, OrthoDMat<DIM_CURL_TRAIT<D>::DIM>, FEL> BASE;
  public:
    using T_BDBIntegrator<DiffOpCurlEdge<D>, OrthoDMat<DIM_CURL_TRAIT<D>::DIM>, FEL>::T_BDBIntegrator;
    ///
    virtual string Name () const { return "CurlCurlEdgeOrtho"; }
  };



  /*
  ///
  template <int D, typename FEL = HCurlFiniteElement<D> >
  class MassEdgeIntegrator 
    : public T_BDBIntegrator<DiffOpIdEdge<D>, DiagDMat<D>, FEL>
  {
    typedef  T_BDBIntegrator<DiffOpIdEdge<D>, DiagDMat<D>, FEL> BASE;
  public:
    using BASE::T_BDBIntegrator;
    ///
    virtual string Name () const { return "MassEdge"; }
  };
  */

  template <int D>
  using MassEdgeIntegrator = T_BDBIntegrator<DiffOpIdEdge<D>, DiagDMat<D>, HCurlFiniteElement<D>>;


  ///
  template <int D, typename FEL = HCurlFiniteElement<D> >
  class MassEdgeOrthoIntegrator 
    : public T_BDBIntegrator<DiffOpIdEdge<D>, OrthoDMat<D>, FEL>
  {
  public:
    using T_BDBIntegrator<DiffOpIdEdge<D>, OrthoDMat<D>, FEL>::T_BDBIntegrator;
    /*
    ///
    MassEdgeOrthoIntegrator (CoefficientFunction * coeff1,
			     CoefficientFunction * coeff2)
      : T_BDBIntegrator<DiffOpIdEdge<D>, OrthoDMat<D>, FEL> (OrthoDMat<D> (coeff1, coeff2))
    { ; }

    MassEdgeOrthoIntegrator (CoefficientFunction * coeff1,
			     CoefficientFunction * coeff2,
			     CoefficientFunction * coeff3)
      : T_BDBIntegrator<DiffOpIdEdge<D>, OrthoDMat<D>, FEL> (OrthoDMat<D> (coeff1, coeff2, coeff3))
    { ; }

    static Integrator * Create (Array<CoefficientFunction*> & coeffs)
    {
      if (D == 2)
	return new MassEdgeOrthoIntegrator (coeffs[0], coeffs[1]);
      else
	return new MassEdgeOrthoIntegrator (coeffs[0], coeffs[1], coeffs[2]);
    }
    */  
    ///
    virtual string Name () const { return "MassEdgeOrtho"; }
  };





  ///
  template <int D, typename FEL = HCurlFiniteElement<D-1> >
  class RobinEdgeIntegrator 
    : public T_BDBIntegrator<DiffOpIdBoundaryEdge<D,FEL>, DiagDMat<D>, FEL>
  {
    typedef T_BDBIntegrator<DiffOpIdBoundaryEdge<D,FEL>, DiagDMat<D>, FEL> BASE;
  public:
    using T_BDBIntegrator<DiffOpIdBoundaryEdge<D,FEL>, DiagDMat<D>, FEL>::T_BDBIntegrator;
    virtual string Name () const { return "RobinEdge"; }
  };





  // Linearform integrators 

  template <int D, typename FEL = HCurlFiniteElement<D> >
  class NGS_DLL_HEADER SourceEdgeIntegrator
    : public T_BIntegrator<DiffOpIdEdge<D>, DVec<D>, FEL>
  {
    typedef T_BIntegrator<DiffOpIdEdge<D>, DVec<D>, FEL> BASE;
  public:
    using T_BIntegrator<DiffOpIdEdge<D>, DVec<D>, FEL>::T_BIntegrator;
    virtual string Name () const { return "SourceEdge"; }
  };








  ///
  template <int D, typename FEL = HCurlFiniteElement<D> >
  class TangentialSourceEdgeIntegrator 
    : public T_BIntegrator<DiffOpIdEdge<D>, TVec<D>, FEL>
  {
  public:
    using  T_BIntegrator<DiffOpIdEdge<D>, TVec<D>, FEL>::T_BIntegrator;
    ///
    virtual string Name () const { return "TangentialSourceEdge"; }
  };
  

  ///
  template <int D, typename FEL = HCurlFiniteElement<D-1> >
  class NeumannEdgeIntegrator
    : public T_BIntegrator<DiffOpIdBoundaryEdge<D,FEL>, DVec<D>, FEL>
  {
    typedef T_BIntegrator<DiffOpIdBoundaryEdge<D,FEL>, DVec<D>, FEL> BASE;
  public:
    using T_BIntegrator<DiffOpIdBoundaryEdge<D,FEL>, DVec<D>, FEL>::T_BIntegrator;
    ///
    /*
    NeumannEdgeIntegrator (CoefficientFunction * coeff1,
			   CoefficientFunction * coeff2,
			   CoefficientFunction * coeff3)
      : T_BIntegrator<DiffOpIdBoundaryEdge<D,FEL>,DVec<D>, FEL> 
    (DVec<D> (coeff1, coeff2, coeff3))
    { ; }
    NeumannEdgeIntegrator (CoefficientFunction * coeff1,
			   CoefficientFunction * coeff2)
      : T_BIntegrator<DiffOpIdBoundaryEdge<D,FEL>,DVec<D>, FEL> 
    (DVec<D> (coeff1, coeff2))
    { ; }

    NeumannEdgeIntegrator (Array<CoefficientFunction*> & coeffs)
      : T_BIntegrator<DiffOpIdBoundaryEdge<D,FEL>,DVec<D>, FEL> 
    (DVec<D> (coeffs))
    { ; }
    */

    /*
    static Integrator * Create (Array<CoefficientFunction*> & coeffs)
    {
      if (D == 3)
	return new NeumannEdgeIntegrator<3> (coeffs[0], coeffs[1], coeffs[2]);
      else
	return new NeumannEdgeIntegrator<2> (coeffs[0], coeffs[1]);
    }
    */
    ///
    virtual bool BoundaryForm () const { return 1; }
    ///
    virtual string Name () const { return "NeumannEdge"; }
  };






  ///
  template <int D, typename FEL = HCurlFiniteElement<D> >
  class CurlEdgeIntegrator 
    : public T_BIntegrator<DiffOpCurlEdge<D>, DVec<DIM_CURL_TRAIT<D>::DIM>, FEL>
  {
    typedef  T_BIntegrator<DiffOpCurlEdge<D>, DVec<DIM_CURL_TRAIT<D>::DIM>, FEL> BASE;
  public:
    using T_BIntegrator<DiffOpCurlEdge<D>, DVec<DIM_CURL_TRAIT<D>::DIM>, FEL>::T_BIntegrator;
    /*
    ///
    CurlEdgeIntegrator (CoefficientFunction * coeff1)
      : T_BIntegrator<DiffOpCurlEdge<D>, DVec<DIM_CURL_TRAIT<D>::DIM>, FEL> 
    (DVec<DIM_CURL_TRAIT<D>::DIM> (coeff1))
    { ; }

    CurlEdgeIntegrator (CoefficientFunction * coeffx,
			CoefficientFunction * coeffy,
			CoefficientFunction * coeffz)
      : T_BIntegrator<DiffOpCurlEdge<D,FEL>, DVec<DIM_CURL_TRAIT<D>::DIM>, FEL> 
    (DVec<DIM_CURL_TRAIT<D>::DIM> (coeffx, coeffy, coeffz))
    { ; }

    CurlEdgeIntegrator (Array<CoefficientFunction*> & coeffs)
      : T_BIntegrator<DiffOpCurlEdge<D,FEL>, DVec<DIM_CURL_TRAIT<D>::DIM>, FEL> 
    (DVec<DIM_CURL_TRAIT<D>::DIM> (coeffs))
    { ; }
    */


    /*
    static Integrator * Create (Array<CoefficientFunction*> & coeffs)
    {
      if (D == 2)
	return new CurlEdgeIntegrator<2> (coeffs[0]);
      else
	return new CurlEdgeIntegrator<3> (coeffs[0], coeffs[1], coeffs[2]);
    }
    */

    ///
    virtual bool BoundaryForm () const { return 0; }
    ///
    virtual string Name () const { return "CurlEdge"; }
  };




  ///
  template <typename FEL = HCurlFiniteElement<2> >
  class CurlBoundaryEdgeIntegrator 
    : public T_BIntegrator<DiffOpCurlBoundaryEdge<FEL>, DVec<1>, FEL>
  {
    typedef T_BIntegrator<DiffOpCurlBoundaryEdge<FEL>, DVec<1>, FEL> BASE;
  public:
    using T_BIntegrator<DiffOpCurlBoundaryEdge<FEL>, DVec<1>, FEL>::T_BIntegrator;
    /*
    ///
    CurlBoundaryEdgeIntegrator (CoefficientFunction * coeff1)
      : T_BIntegrator<DiffOpCurlBoundaryEdge<FEL>, DVec<1>, FEL> 
    (DVec<1> (coeff1))
    { ; }

    static Integrator * Create (Array<CoefficientFunction*> & coeffs)
    {
      return new CurlBoundaryEdgeIntegrator (coeffs[0]);
    }
    */
    ///
    virtual bool BoundaryForm () const { return 1; }
    ///
    virtual string Name () const { return "CurlBoundaryEdge"; }
  };




  
#ifdef FILE_HCURL_EQUATIONS_CPP
#define HCURL_EQUATIONS_EXTERN
#else
#define HCURL_EQUATIONS_EXTERN extern
#endif

  HCURL_EQUATIONS_EXTERN template class NGS_DLL_HEADER T_DifferentialOperator<DiffOpIdEdge<2> >;
  HCURL_EQUATIONS_EXTERN template class NGS_DLL_HEADER T_DifferentialOperator<DiffOpIdEdge<3> >;
  HCURL_EQUATIONS_EXTERN template class NGS_DLL_HEADER T_DifferentialOperator<DiffOpIdBoundaryEdge<2> >;
  HCURL_EQUATIONS_EXTERN template class NGS_DLL_HEADER T_DifferentialOperator<DiffOpIdBoundaryEdge<3> >;
  HCURL_EQUATIONS_EXTERN template class NGS_DLL_HEADER T_DifferentialOperator<DiffOpCurlEdge<2> >;
  HCURL_EQUATIONS_EXTERN template class NGS_DLL_HEADER T_DifferentialOperator<DiffOpCurlEdge<3> >;
  HCURL_EQUATIONS_EXTERN template class NGS_DLL_HEADER T_DifferentialOperator<DiffOpCurlBoundaryEdge<> >;

  // HCURL_EQUATIONS_EXTERN template class MassEdgeIntegrator<2>;
  // HCURL_EQUATIONS_EXTERN template class MassEdgeIntegrator<3>;

  HCURL_EQUATIONS_EXTERN template class RobinEdgeIntegrator<2>;
  HCURL_EQUATIONS_EXTERN template class RobinEdgeIntegrator<3>;

  // HCURL_EQUATIONS_EXTERN template class CurlCurlEdgeIntegrator<2>;
  // HCURL_EQUATIONS_EXTERN template class CurlCurlEdgeIntegrator<3>;

  // HCURL_EQUATIONS_EXTERN template class MassEdgeAnisotropicIntegrator<3>;
  
  HCURL_EQUATIONS_EXTERN template class T_BDBIntegrator<DiffOpIdEdge<2>, DiagDMat<2>, HCurlFiniteElement<2>>;
  HCURL_EQUATIONS_EXTERN template class T_BDBIntegrator<DiffOpIdEdge<3>, DiagDMat<3>, HCurlFiniteElement<3>>;
  // HCURL_EQUATIONS_EXTERN template class T_BDBIntegrator<DiffOpIdEdge<3>, SymDMat<3>, HCurlFiniteElement<3>>;
  HCURL_EQUATIONS_EXTERN template class T_BDBIntegrator<DiffOpIdBoundaryEdge<2>, DiagDMat<2>, HCurlFiniteElement<1>>;
  HCURL_EQUATIONS_EXTERN template class T_BDBIntegrator<DiffOpIdBoundaryEdge<3>, DiagDMat<3>, HCurlFiniteElement<2>>;
  HCURL_EQUATIONS_EXTERN template class T_BDBIntegrator<DiffOpCurlEdge<2>, DiagDMat<1>, HCurlFiniteElement<2>>;
  HCURL_EQUATIONS_EXTERN template class T_BDBIntegrator<DiffOpCurlEdge<3>, DiagDMat<3>, HCurlFiniteElement<3>>;
  HCURL_EQUATIONS_EXTERN template class T_BDBIntegrator<DiffOpCurlBoundaryEdge<>, DiagDMat<1>, HCurlFiniteElement<2> >;

  HCURL_EQUATIONS_EXTERN template class SourceEdgeIntegrator<2, HCurlFiniteElement<2>>;
  HCURL_EQUATIONS_EXTERN template class SourceEdgeIntegrator<3, HCurlFiniteElement<3>>;

  HCURL_EQUATIONS_EXTERN template class T_BIntegrator<DiffOpIdEdge<2>, DVec<2>, HCurlFiniteElement<2>>;
  HCURL_EQUATIONS_EXTERN template class T_BIntegrator<DiffOpIdEdge<3>, DVec<3>, HCurlFiniteElement<3>>;
  HCURL_EQUATIONS_EXTERN template class T_BIntegrator<DiffOpIdBoundaryEdge<2>, DVec<2>, HCurlFiniteElement<1>>;
  HCURL_EQUATIONS_EXTERN template class T_BIntegrator<DiffOpIdBoundaryEdge<3>, DVec<3>, HCurlFiniteElement<2>>;
  HCURL_EQUATIONS_EXTERN template class T_BIntegrator<DiffOpCurlEdge<2>, DVec<1>, HCurlFiniteElement<2>>;
  HCURL_EQUATIONS_EXTERN template class T_BIntegrator<DiffOpCurlEdge<3>, DVec<3>, HCurlFiniteElement<3>>;


  /*
    HCURL_EQUATIONS_EXTERN template class 
    HCURL_EQUATIONS_EXTERN template class 
  */
}

#endif
