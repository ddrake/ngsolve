#ifndef FILE_THDIVFE
#define FILE_THDIVFE

/*********************************************************************/
/* File:   thdivfe.hpp                                               */
/* Author: Joachim Schoeberl                                         */
/* Date:   5. Jul. 2001                                              */
/*********************************************************************/

namespace ngfem
{


  template <int D, typename SCAL>
  inline SCAL Dot (const AutoDiff<D,SCAL> & u, const AutoDiff<D,SCAL> & v)
  {
    SCAL sum = 0;
    for (int i = 0; i < D; i++)
      sum += u.DValue(i) * v.DValue(i);
    return sum;
  }


  // rotated gradient
  template <int DIM> class DuRot;

  template <> class DuRot<2>
  {

  public:
    const AutoDiff<2> u;

    DuRot (const AutoDiff<2> au)
      : u(au) { ; }

    Vec<2> Value () const
    {
      Vec<2> val;
      val(0) = u.DValue(1);
      val(1) = -u.DValue(0);
      return val;
    }

    /*
    Vec<DIM_CURL> CurlValue () const
    {
      return Vec<DIM> (0.0);
    }
    */
  };





  template <int DIM, typename SCAL>
  class Class_uDvDw_Cyclic
  {
  public:
    const AutoDiff<DIM,SCAL> u, v, w;
    Class_uDvDw_Cyclic (const AutoDiff<DIM,SCAL> au, 
                        const AutoDiff<DIM,SCAL> av,
                        const AutoDiff<DIM,SCAL> aw)
      : u(au), v(av), w(aw) { ; }
  };

  template <int DIM, typename SCAL>
  INLINE Class_uDvDw_Cyclic<DIM,SCAL> 
  uDvDw_Cyclic (AutoDiff<DIM,SCAL> u, AutoDiff<DIM,SCAL> v, AutoDiff<DIM,SCAL> w)
  { return Class_uDvDw_Cyclic<DIM,SCAL> (u,v,w); }
  
  template <int DIM, typename SCAL>
  INLINE Class_uDvDw_Cyclic<DIM,SCAL> 
  uDvDw_Cyclic (AutoDiffRec<DIM,SCAL> u, AutoDiffRec<DIM,SCAL> v, AutoDiffRec<DIM,SCAL> w)
  { return Class_uDvDw_Cyclic<DIM,SCAL> (u,v,w); }


  template <int DIM, typename SCAL>
  class Class_Du_Cross_Dv
  {
  public:
    const AutoDiff<DIM,SCAL> u, v;
    Class_Du_Cross_Dv (const AutoDiff<DIM,SCAL> au, 
                       const AutoDiff<DIM,SCAL> av)
      : u(au), v(av) { ; }
  };

  template <int DIM, typename SCAL>
  INLINE Class_Du_Cross_Dv<DIM,SCAL> 
  Du_Cross_Dv (AutoDiff<DIM,SCAL> u, AutoDiff<DIM,SCAL> v)
  { return Class_Du_Cross_Dv<DIM,SCAL> (u,v); }

  template <int DIM, typename SCAL>
  INLINE Class_Du_Cross_Dv<DIM,SCAL> 
  Du_Cross_Dv (AutoDiffRec<DIM,SCAL> u, AutoDiffRec<DIM,SCAL> v)
  { return Class_Du_Cross_Dv<DIM,SCAL> (u,v); }


  template <int DIM, typename SCAL>
  class Class_wDu_Cross_Dv
  {
  public:
    const AutoDiff<DIM,SCAL> u, v, w;
    Class_wDu_Cross_Dv (const AutoDiff<DIM,SCAL> au, 
                        const AutoDiff<DIM,SCAL> av,
                        const AutoDiff<DIM,SCAL> aw)
      : u(au), v(av), w(aw) { ; }
  };

  template <int DIM, typename SCAL>
  INLINE Class_wDu_Cross_Dv<DIM,SCAL> 
  wDu_Cross_Dv(AutoDiff<DIM,SCAL> u, AutoDiff<DIM,SCAL> v, AutoDiff<DIM,SCAL> w)
  { return Class_wDu_Cross_Dv<DIM,SCAL> (u,v,w); }

  template <int DIM, typename SCAL>
  INLINE Class_wDu_Cross_Dv<DIM,SCAL> 
  wDu_Cross_Dv(AutoDiffRec<DIM,SCAL> u, AutoDiffRec<DIM,SCAL> v, AutoDiffRec<DIM,SCAL> w)
  { return Class_wDu_Cross_Dv<DIM,SCAL> (u,v,w); }


  
  template <int DIM, typename SCAL>
  class Class_uDvDw_minus_DuvDw
  {
  public:
    const AutoDiff<DIM,SCAL> u, v, w;
    Class_uDvDw_minus_DuvDw (const AutoDiff<DIM,SCAL> au, 
                             const AutoDiff<DIM,SCAL> av,
                             const AutoDiff<DIM,SCAL> aw)
      : u(au), v(av), w(aw) { ; }
  };

  template <int DIM, typename SCAL>
  INLINE Class_uDvDw_minus_DuvDw<DIM,SCAL> 
  uDvDw_minus_DuvDw (AutoDiff<DIM,SCAL> u, AutoDiff<DIM,SCAL> v, AutoDiff<DIM,SCAL> w)
  { return Class_uDvDw_minus_DuvDw<DIM,SCAL> (u,v,w); }

  template <int DIM, typename SCAL>
  INLINE Class_uDvDw_minus_DuvDw<DIM,SCAL> 
  uDvDw_minus_DuvDw (AutoDiffRec<DIM,SCAL> u, AutoDiffRec<DIM,SCAL> v, AutoDiffRec<DIM,SCAL> w)
  { return Class_uDvDw_minus_DuvDw<DIM,SCAL> (u,v,w); }


  template <int DIM, typename SCAL>
  class Class_curl_uDvw_minus_Duvw
  {
  public:
    const AutoDiff<DIM,SCAL> u, v, w;
    Class_curl_uDvw_minus_Duvw (const AutoDiff<DIM,SCAL> au, 
                                const AutoDiff<DIM,SCAL> av,
                                const AutoDiff<DIM,SCAL> aw)
      : u(au), v(av), w(aw) { ; }
  };

  template <int DIM, typename SCAL>
  INLINE Class_curl_uDvw_minus_Duvw<DIM,SCAL> 
  curl_uDvw_minus_Duvw (AutoDiff<DIM,SCAL> u, AutoDiff<DIM,SCAL> v, AutoDiff<DIM,SCAL> w)
  { return Class_curl_uDvw_minus_Duvw<DIM,SCAL> (u,v,w); }

  template <int DIM, typename SCAL>
  INLINE Class_curl_uDvw_minus_Duvw<DIM,SCAL> 
  curl_uDvw_minus_Duvw (AutoDiffRec<DIM,SCAL> u, AutoDiffRec<DIM,SCAL> v, AutoDiffRec<DIM,SCAL> w)
  { return Class_curl_uDvw_minus_Duvw<DIM,SCAL> (u,v,w); }




  template <int DIM, typename SCAL = double> class THDiv2Shape
  {
  public:
    INLINE operator Vec<DIM,SCAL> () { return 0.0; }
  };


  template <typename SCAL> class THDiv2Shape<2,SCAL>
  {
    Vec<2,SCAL> data;
  public:
    INLINE THDiv2Shape (Class_Du<2,SCAL> uv)
    {
      data = Vec<2,SCAL> (uv.u.DValue(1), -uv.u.DValue(0));
    }
    
    INLINE THDiv2Shape (Class_uDv<2,SCAL> uv)
    {
      data = Vec<2,SCAL> (-uv.u.Value()*uv.v.DValue(1), 
                          uv.u.Value()*uv.v.DValue(0));
    }

    INLINE THDiv2Shape (const Class_uDv_minus_vDu<2,SCAL> & uv) 
    { 
      data(0) = -uv.u.Value() * uv.v.DValue(1) + uv.u.DValue(1) * uv.v.Value();
      data(1) =  uv.u.Value() * uv.v.DValue(0) - uv.u.DValue(0) * uv.v.Value();
    }

    INLINE THDiv2Shape (const Class_wuDv_minus_wvDu<2,SCAL> & uv) 
    { 
      data[0] = -uv.u.Value() * uv.v.DValue(1) + uv.u.DValue(1) * uv.v.Value();
      data[1] =  uv.u.Value() * uv.v.DValue(0) - uv.u.DValue(0) * uv.v.Value();
      data[0] *= uv.w.Value();
      data[1] *= uv.w.Value();
    }
    
    INLINE operator Vec<2,SCAL> () const { return data; }
  };


  template <typename SCAL> class THDiv2Shape<3,SCAL>
  {
    Vec<3,SCAL> data;
  public:

    INLINE THDiv2Shape (const Class_uDvDw_Cyclic<3,SCAL> & uvw) 
    { 
      /*
      AutoDiff<3,SCAL> hv =
        uvw.u.Value() * Cross (uvw.v, uvw.w) +
        uvw.v.Value() * Cross (uvw.w, uvw.u) +
        uvw.w.Value() * Cross (uvw.u, uvw.v);

      for (int i = 0; i < 3; i++)
        data[i] = hv.DValue(i);
      */
      AutoDiff<3,SCAL> p1 = Cross (uvw.v, uvw.w);
      AutoDiff<3,SCAL> p2 = Cross (uvw.w, uvw.u);
      AutoDiff<3,SCAL> p3 = Cross (uvw.u, uvw.v);

      for (int i = 0; i < 3; i++)
        data[i] =
          uvw.u.Value() * p1.DValue(i) + 
          uvw.v.Value() * p2.DValue(i) + 
          uvw.w.Value() * p3.DValue(i);
    }

    INLINE THDiv2Shape (const Class_Du_Cross_Dv<3,SCAL> & uv) 
    { 
      AutoDiff<3,SCAL> hv = Cross (uv.u, uv.v);
      for (int i = 0; i < 3; i++)
        data[i] = hv.DValue(i);
    }

    INLINE THDiv2Shape (const Class_wDu_Cross_Dv<3,SCAL> & uvw) 
    { 
      AutoDiff<3,SCAL> hv = Cross (uvw.u, uvw.v);
      for (int i = 0; i < 3; i++)
        data[i] = uvw.w.Value() * hv.DValue(i);
    }


    INLINE THDiv2Shape (const Class_uDvDw_minus_DuvDw<3,SCAL> & uvw) 
    { 
      /*
      AutoDiff<3,SCAL> hv =
        uvw.u.Value() * Cross (uvw.v, uvw.w) +
        uvw.v.Value() * Cross (uvw.w, uvw.u);

      for (int i = 0; i < 3; i++)
        data[i] = hv.DValue(i);
      */
      AutoDiff<3,SCAL> p1 = Cross (uvw.v, uvw.w);
      AutoDiff<3,SCAL> p2 = Cross (uvw.w, uvw.u);

      for (int i = 0; i < 3; i++)
        data[i] =
          uvw.u.Value() * p1.DValue(i) + 
          uvw.v.Value() * p2.DValue(i);
    }

    INLINE THDiv2Shape (const Class_curl_uDvw_minus_Duvw<3,SCAL> & uvw) 
    { 
      AutoDiff<3,SCAL> hv = Cross (uvw.u*uvw.w, uvw.v) - Cross (uvw.v*uvw.w, uvw.u);
      for (int i = 0; i < 3; i++)
        data[i] = hv.DValue(i);
    }

    INLINE operator Vec<3,SCAL> () const { return data; }
  };



  /*
  // 2D 
  template <int DIM>
  class HDivShapeElement
  {
    double * data;
  public:
    HDivShapeElement (double * adata) : data(adata) { ; }

    void operator= (THDiv2Shape<DIM> hd2vec)
    {
      Vec<DIM> v = hd2vec;
      for (int j = 0; j < DIM; j++)
        data[j] = v(j);
    }
  };


  template <int DIM>
  class HDivEvaluateShapeElement
  {
    const double * coefs;
    Vec<DIM> & sum;
  public:
    HDivEvaluateShapeElement (const double * acoefs, Vec<DIM> & asum)
      : coefs(acoefs), sum(asum) { ; }


    void operator= (THDiv2Shape<DIM> hd2vec)
    {
      sum += *coefs * Vec<DIM> (hd2vec);
    }
  };
  */






  template <int DIM, typename SCAL = double> class THDiv2DivShape
  {
  public:
    INLINE operator SCAL () const { return SCAL(0.0); }
  };

  template <typename SCAL> class THDiv2DivShape<2,SCAL>
  {
    SCAL data;
  public:
    INLINE THDiv2DivShape (Class_Du<2,SCAL> uv)
    {
      data = SCAL(0.0);
    }
    
    INLINE THDiv2DivShape (Class_uDv<2,SCAL> uv)
    {
      AutoDiff<1,SCAL> hd = Cross (uv.u, uv.v);
      data = -hd.DValue(0);
    }

    INLINE THDiv2DivShape (const Class_uDv_minus_vDu<2,SCAL> & uv) 
    { 
      data = -2*uv.u.DValue(0) * uv.v.DValue(1) 
        + 2*uv.u.DValue(1) * uv.v.DValue(0);
    }

    INLINE THDiv2DivShape (const Class_wuDv_minus_wvDu<2,SCAL> & uv) 
    { 
      AutoDiff<1,SCAL> hd = Cross (uv.u*uv.w, uv.v) + Cross(uv.u, uv.v*uv.w);
      data = -hd.DValue(0);
    }
    
    INLINE operator SCAL () const { return data; }
    INLINE SCAL Get() const { return data; }
  };


  template <typename SCAL> class THDiv2DivShape<3,SCAL>
  {
    SCAL data;
  public:

    INLINE THDiv2DivShape (const Class_uDvDw_Cyclic<3,SCAL> & uvw) 
    { 
      data = 
        Dot (uvw.u, Cross (uvw.v, uvw.w)) +
        Dot (uvw.v, Cross (uvw.w, uvw.u)) +
        Dot (uvw.w, Cross (uvw.u, uvw.v));
    }

    INLINE THDiv2DivShape (const Class_Du_Cross_Dv<3,SCAL> & uv) 
    { 
      data = 0.0;
    }

    INLINE THDiv2DivShape (const Class_wDu_Cross_Dv<3,SCAL> & uvw) 
    { 
      data = Dot (uvw.w, Cross (uvw.u, uvw.v));
    }

    INLINE THDiv2DivShape (const Class_uDvDw_minus_DuvDw<3,SCAL> & uvw) 
    { 
      data = 
        Dot (uvw.u, Cross (uvw.v, uvw.w)) +
        Dot (uvw.v, Cross (uvw.w, uvw.u));
    }

    INLINE THDiv2DivShape (const Class_curl_uDvw_minus_Duvw<3,SCAL> & uvw) 
    { 
      data = 0.0;
    }

    INLINE operator SCAL () const { return data; }
    INLINE SCAL Get() const { return data; }    
  };




  /*
  template <int DIM, typename SCAL = double>
  class HDivDivShapeElement
  {
    SCAL * data;
  public:
    HDivDivShapeElement (SCAL * adata) : data(adata) { ; }


    void operator= (const THDiv2DivShape<DIM,SCAL> & hd2dshape)
    {
      *data = hd2dshape;
    }
  };

  template <int DIM>
  class HDivShapeAssign
  {
    double * dshape;
  public:
    HDivShapeAssign (FlatMatrixFixWidth<DIM> mat)
    { dshape = &mat(0,0); }
    
    HDivShapeElement<DIM> operator[] (int i) const
    { return HDivShapeElement<DIM> (dshape + i*DIM); }
  };


  template <int DIM>
  class HDivDivShapeAssign
  {
    SliceVector<> dshape;
  public:
    HDivDivShapeAssign (SliceVector<>  mat)
      : dshape(mat) { ; }

    HDivDivShapeElement<DIM> operator[] (int i) const
    { return HDivDivShapeElement<DIM> (&dshape(i)); }
  };

  template <int DIM>
  class HDivEvaluateShape
  {
    const double * coefs;
    Vec<DIM> sum;
  public:
    HDivEvaluateShape (FlatVector<> acoefs)
    { coefs = &acoefs(0); sum = 0.0; }

    HDivEvaluateShapeElement<DIM> operator[] (int i) 
    { return HDivEvaluateShapeElement<DIM> (coefs+i, sum); }

    Vec<DIM> Sum() { return sum; }
  };
  */





  







  template <class FEL, ELEMENT_TYPE ET>
  class T_HDivFiniteElement 
    : public HDivFiniteElement<ET_trait<ET>::DIM>
    
  {
    enum { DIM = ET_trait<ET>::DIM };

  public:

    virtual void CalcShape (const IntegrationPoint & ip, 
			    SliceMatrix<> shape) const;
    
    virtual void CalcDivShape (const IntegrationPoint & ip, 
			       SliceVector<> divshape) const;

#ifndef FASTCOMPILE
    virtual void CalcMappedShape (const MappedIntegrationPoint<DIM,DIM> & mip,
				  SliceMatrix<> shape) const;

    virtual void CalcMappedShape (const MappedIntegrationRule<DIM,DIM> & mip,
				  SliceMatrix<> shape) const;

    virtual void CalcMappedShape (const SIMD<MappedIntegrationPoint<DIM,DIM>> & mip,
				  BareSliceMatrix<SIMD<double>> shape) const;

    virtual void CalcMappedShape (const SIMD_BaseMappedIntegrationRule & mir, 
                                  BareSliceMatrix<SIMD<double>> shapes) const;

    using HDivFiniteElement<ET_trait<ET>::DIM>::CalcMappedDivShape;
    virtual void CalcMappedDivShape (const SIMD_BaseMappedIntegrationRule & mir, 
                                     BareSliceMatrix<SIMD<double>> divshapes) const;

    virtual void Evaluate (const IntegrationRule & ir, 
			   FlatVector<double> coefs, 
			   FlatMatrixFixWidth<DIM> vals) const;

    virtual void EvaluateTrans (const IntegrationRule & ir, 
                                FlatMatrixFixWidth<DIM> vals,
                                FlatVector<double> coefs) const;


    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, BareSliceVector<> coefs, BareSliceMatrix<SIMD<double>> values) const;
    virtual void AddTrans (const SIMD_BaseMappedIntegrationRule & ir, BareSliceMatrix<SIMD<double>> values,
                           BareSliceVector<> coefs) const;

    virtual void EvaluateDiv (const SIMD_BaseMappedIntegrationRule & ir, BareSliceVector<> coefs, BareVector<SIMD<double>> values) const;
    
    virtual void AddDivTrans (const SIMD_BaseMappedIntegrationRule & ir, BareVector<SIMD<double>> values,
                              BareSliceVector<> coefs) const;
    
    
#endif
  };

}


#endif
