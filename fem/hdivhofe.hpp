#ifndef FILE_HDIVHOFE_
#define FILE_HDIVHOFE_ 

/*********************************************************************/
/* File:   hdivhofe.hpp                                              */
/* Author: A. Becirovic, S. Zaglmayr, J. Schoeberl                   */
/* Date:   15. Feb. 2003                                             */
/*********************************************************************/


#include "thdivfe.hpp"

namespace ngfem
{
  





  template <int D>
  class HDivHighOrderNormalFiniteElement : public HDivNormalFiniteElement<D>
  {
  protected:
    INT<D> order_inner;    
  public:
    ///
    HDivHighOrderNormalFiniteElement ()
      : HDivNormalFiniteElement<D> (-1, -1) { ; } 

    void SetOrderInner (int oi) { order_inner = oi; }
    void SetOrderInner (INT<D> oi) { order_inner = oi; }
    
    virtual void ComputeNDof () = 0;
  };


  
  template <typename FEL, ELEMENT_TYPE ET>
  class T_HDivHighOrderNormalFiniteElement 
    : public HDivHighOrderNormalFiniteElement<ET_trait<ET>::DIM>,
      public VertexOrientedFE<ET>
  {
  protected:
    enum { DIM = ET_trait<ET>::DIM };
    using VertexOrientedFE<ET>::vnums;
  public:
    T_HDivHighOrderNormalFiniteElement ()
    {
      for (int i = 0; i < ET_trait<ET>::N_VERTEX; i++)
        vnums[i] = i;
    }

    virtual ELEMENT_TYPE ElementType() const { return ET; }

    int EdgeOrientation (int enr) const
    {
      const EDGE * edges = ElementTopology::GetEdges (this->ElementType());
      return (vnums[edges[enr][1]] > vnums[edges[enr][0]]) ? 1 : -1;
    }

    virtual void CalcShape (const IntegrationPoint & ip,
                            FlatVector<> shape) const
    {
      TIP<DIM,double> tip = ip;
      static_cast<const FEL*> (this) -> T_CalcShape (tip, shape);
    }

    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & bmir,
                           BareSliceVector<> coefs,
                           BareSliceMatrix<SIMD<double>> values) const
    {
      auto & mir = static_cast<const SIMD_MappedIntegrationRule<DIM,DIM+1>&> (bmir);
      for (size_t i = 0; i < mir.Size(); i++)
        {
          auto & mip = mir[i];
          auto nv = mip.GetNV();
          /*
          SIMD<double> sum = 0.0;
          for (size_t j = 0; j < DIM+1; j++)
            sum += nv[j] * values(j,i);
          SIMD<double> val = sum / mip.GetJacobiDet();

          TIP<DIM,SIMD<double>> tip = mip.IP().template TIp<DIM>();
          static_cast<const FEL*> (this) ->
            T_CalcShape (tip, SBLambda([&] (int nr, SIMD<double> shape)
                                       {
                                         coefs(nr) += HSum(shape*val);
                                       }));
          */
          SIMD<double> sum(0.0);
          TIP<DIM,SIMD<double>> tip = mip.IP().template TIp<DIM>();
          static_cast<const FEL*> (this) ->
            T_CalcShape (tip, SBLambda([&] (int nr, SIMD<double> shape)
                                       {
                                         sum += coefs(nr) * shape;
                                       }));
          sum /= mip.GetJacobiDet();
          for (size_t j = 0; j < DIM+1; j++)
            values(j,i) = nv[j] * sum;
        }
    }

    
    virtual void AddTrans (const SIMD_BaseMappedIntegrationRule & bmir,
                           BareSliceMatrix<SIMD<double>> values,
                           BareSliceVector<> coefs) const
    {
      auto & mir = static_cast<const SIMD_MappedIntegrationRule<DIM,DIM+1>&> (bmir);
      for (size_t i = 0; i < mir.Size(); i++)
        {
          auto & mip = mir[i];
          auto nv = mip.GetNV();
          
          SIMD<double> sum = 0.0;
          for (size_t j = 0; j < DIM+1; j++)
            sum += nv[j] * values(j,i);
          SIMD<double> val = sum / mip.GetJacobiDet();

          TIP<DIM,SIMD<double>> tip = mip.IP().template TIp<DIM>();
          static_cast<const FEL*> (this) ->
            T_CalcShape (tip, SBLambda([&] (int nr, SIMD<double> shape)
                                       {
                                         coefs(nr) += HSum(shape*val);
                                       }));
        }
    }
  };

  

  template <class T_ORTHOPOL = TrigExtensionMonomial>
  class HDivHighOrderNormalSegm : public T_HDivHighOrderNormalFiniteElement<HDivHighOrderNormalSegm<T_ORTHOPOL>,
                                                                            ET_SEGM>
  {
    typedef T_HDivHighOrderNormalFiniteElement<HDivHighOrderNormalSegm<T_ORTHOPOL>, ET_SEGM> BASE;
    using BASE::order_inner;
    using BASE::order;
    using BASE::ndof;
    using BASE::vnums;
  public:
    HDivHighOrderNormalSegm (int aorder);
    virtual void ComputeNDof();
    /*
    virtual void CalcShape (const IntegrationPoint & ip,
                            FlatVector<> shape) const;
    */
    template<typename Tx, typename TFA>  
    INLINE void T_CalcShape (TIP<1,Tx> ip, TFA & shape) const
    { 
      AutoDiff<1,Tx> x (ip.x, 0);
      AutoDiff<1,Tx> lam[2] = { x, 1-x };
      
      INT<2> e = ET_trait<ET_SEGM>::GetEdgeSort (0, vnums);	  
      
      shape[0] = -lam[e[0]].DValue(0);
      
      int ii = 1;
      IntLegNoBubble::
        EvalMult (order_inner[0]-1,
                  lam[e[1]]-lam[e[0]], lam[e[0]]*lam[e[1]],
                  SBLambda ([&] (int nr, AutoDiff<1,Tx> val)
                            {
                              shape[ii++] = -val.DValue(0);
                            }));
    }
  };

  template <class T_ORTHOPOL = TrigExtensionMonomial>
  class HDivHighOrderNormalTrig : public T_HDivHighOrderNormalFiniteElement<HDivHighOrderNormalTrig<T_ORTHOPOL>, ET_TRIG>
  {
    typedef T_HDivHighOrderNormalFiniteElement<HDivHighOrderNormalTrig<T_ORTHOPOL>, ET_TRIG> BASE;
    using BASE::order_inner;
    using BASE::order;
    using BASE::ndof;
    using BASE::vnums;
  public:
    HDivHighOrderNormalTrig (int aorder);
    virtual void ComputeNDof();
    /*
    virtual void CalcShape (const IntegrationPoint & ip,
                            FlatVector<> shape) const;
    */
    
    template<typename Tx, typename TFA>  
    INLINE void T_CalcShape (TIP<2,Tx> ip, TFA & shape) const
    {
      Tx x = ip.x;
      Tx y = ip.y;

      Tx lami[3];

      lami[0] = x;
      lami[1] = y;
      lami[2] = 1-x-y;
      
      Mat<3,2, Tx> dlami(0.0);
      dlami(0,0) = 1.;
      dlami(1,1) = 1.;
      dlami(2,0) = -1.;
      dlami(2,1) = -1.;

      int ii, is, ie, iop;
      
      int p = order_inner[0];
      ii = 1;
      
      int fav[3];
      for(int i=0;i<3;i++) fav[i] = i;
      
      //Sort vertices first edge op minimal vertex
      double fswap = 1;
      if(vnums[fav[0]] > vnums[fav[1]]) { swap(fav[0],fav[1]); fswap *= -1; }
      if(vnums[fav[1]] > vnums[fav[2]]) { swap(fav[1],fav[2]); fswap *= -1; }
      if(vnums[fav[0]] > vnums[fav[1]]) { swap(fav[0],fav[1]); fswap *= -1; }
      
      is = fav[0]; ie = fav[1]; iop = fav[2];
      
      AutoDiff<2, Tx> ls = lami[is];
      AutoDiff<2, Tx> le = lami[ie];
      AutoDiff<2, Tx> lo = lami[iop];
      
      //AutoDiff<3> lsle = lami[is]*lami[ie];
      for (int j = 0; j < 2; j++)
        {
          ls.DValue(j) = dlami(is,j);
          le.DValue(j) = dlami(ie,j);
          lo.DValue(j) = dlami(iop,j);
        }
      
      Vec<2,Tx> nedelec;
      for (int j = 0; j < 2; j++)
        nedelec(j) = ls.Value()*le.DValue(j) - le.Value()*ls.DValue(j);
      
      // RT_0-normal low order shapes
      shape[0] = Tx(fswap);
      
      ArrayMem<AutoDiff<2, Tx>, 20> adpol1(p);
      ArrayMem<AutoDiff<2, Tx>, 20> adpol2(p);
      
      IntLegNoBubble::EvalScaledMult (p-1, le-ls, le+ls, ls*le, adpol1); 

    for (int k = 0; k <= p-1; k++)
      {
        JacobiPolynomialAlpha jac(2*k+3);
	Tx factor = Cross (lo, adpol1[k]).DValue(0);
        jac.EvalMult(p-1-k, 2*lo.Value()-1, factor, shape+ii);
	ii += p-k;
      }

    IntegratedJacobiPolynomialAlpha jac(3);
    jac.EvalMult(p-1, 2*lo-1, lo, adpol2);

    // Typ 2
    Tx curlned;
    curlned = 2.* (ls.DValue(0)*le.DValue(1) - ls.DValue(1)*le.DValue(0));
    for (int k = 0; k <= p-1; k++, ii++)
      shape[ii] = adpol2[k].DValue(0)*nedelec(1) - adpol2[k].DValue(1)*nedelec(0) 
        + adpol2[k].Value()*curlned;
    }
      
  };

  template <class T_ORTHOPOL = TrigExtensionMonomial>
  class HDivHighOrderNormalQuad : public T_HDivHighOrderNormalFiniteElement<HDivHighOrderNormalQuad<T_ORTHOPOL>, ET_QUAD>
  {
    typedef T_HDivHighOrderNormalFiniteElement<HDivHighOrderNormalQuad<T_ORTHOPOL>, ET_QUAD> BASE;
    using BASE::order_inner;
    using BASE::order;
    using BASE::ndof;
    using BASE::vnums;
  public:
    HDivHighOrderNormalQuad (int aorder);
    virtual void ComputeNDof();
    /*
    virtual void CalcShape (const IntegrationPoint & ip,
                            FlatVector<> shape) const;
    */
    template<typename Tx, typename TFA>  
    INLINE void T_CalcShape (TIP<2,Tx> ip, TFA & shape) const
    {
      AutoDiff<2, Tx> x (ip.x, 0);
      AutoDiff<2, Tx> y (ip.y, 1);
      
      AutoDiff<2, Tx> sigma[4] = {(1-x)+(1-y),x+(1-y),x+y,(1-x)+y};
      
      // shape = 0.0;
      
      int ii = 1;
      
      INT<2> p = order_inner;
      // int pp = max2(p[0],p[1]); 
      
      ArrayMem<AutoDiff<2, Tx>,20> pol_xi(p[0]+1), pol_eta(p[1]+1);
      
      int fmax = 0;
      for (int j = 1; j < 4; j++)
        if (vnums[j] < vnums[fmax])
          fmax = j;
      
      int f1 = (fmax+3)%4;
      int f2 = (fmax+1)%4;
      
      int fac = 1;
      if(vnums[f2] < vnums[f1])
        {
          swap(f1,f2); // fmax > f1 > f2;
          fac *= -1;
        }
      
      AutoDiff<2, Tx> xi  = sigma[fmax]-sigma[f1];
      AutoDiff<2, Tx> eta = sigma[fmax]-sigma[f2];
      
      shape[0] = fac;
      
      /*
        T_ORTHOPOL::Calc(p[0]+1, xi,pol_xi);
        T_ORTHOPOL::Calc(p[1]+1,eta,pol_eta);
      */
      IntLegNoBubble::EvalMult (p[0]-1, xi, 1-xi*xi, pol_xi);
      IntLegNoBubble::EvalMult (p[1]-1, eta, 1-eta*eta, pol_eta);
      
      // Typ 1
      for (int k = 0; k < p[0]; k++)
        for (int l = 0; l < p[1]; l++, ii++)
          shape[ii] = 2.*(pol_eta[l].DValue(0)*pol_xi[k].DValue(1)-pol_eta[l].DValue(1)*pol_xi[k].DValue(0));
      
      //Typ 2
      for (int k = 0; k < p[0]; k++)
        shape[ii++] = -eta.DValue(0)*pol_xi[k].DValue(1) + eta.DValue(1)*pol_xi[k].DValue(0); 
      for (int k = 0; k < p[1]; k++)
        shape[ii++]   = -xi.DValue(0)*pol_eta[k].DValue(1) + xi.DValue(1)*pol_eta[k].DValue(0);
    }
  };




  template <ELEMENT_TYPE ET> class HDivHighOrderFE_Shape;

  template <ELEMENT_TYPE ET> 
  class NGS_DLL_HEADER HDivHighOrderFE : 
    public T_HDivFiniteElement< HDivHighOrderFE_Shape<ET>, ET > , public ET_trait<ET>, public VertexOrientedFE<ET>
  {
  protected:
    using ET_trait<ET>::N_VERTEX;
    using ET_trait<ET>::N_FACET;
    using ET_trait<ET>::DIM;

    typedef IntegratedLegendreMonomialExt T_ORTHOPOL;  

    using HDivFiniteElement<DIM>::ndof;
    using HDivFiniteElement<DIM>::order;

    using VertexOrientedFE<ET>::vnums;
    

    INT<DIM> order_inner;
    INT<N_FACET,INT<DIM-1>> order_facet;  

    bool ho_div_free;
    bool only_ho_div;

  public:
    using VertexOrientedFE<ET>::SetVertexNumbers;
    /// minimal constructor, orders will be set later
    HDivHighOrderFE () 
      : ho_div_free(false), only_ho_div(false)
    { ; }
  
    /// builds a functional element of order aorder.
    HDivHighOrderFE (int aorder)
      : ho_div_free(false), only_ho_div(false)
    { 
      for (int i = 0; i < N_VERTEX; i++) vnums[i] = i;

      order_inner = aorder;
      order_facet = aorder;

      ComputeNDof();
    }


    void SetOrderInner (INT<DIM> oi)
    { 
      order_inner = oi; 
    }

    template <typename TA>
    void SetOrderFacet (const TA & oe)
    { 
      for (int i = 0; i < N_FACET; i++) 
        order_facet[i] = oe[i]; 
    }

    void SetHODivFree (bool aho_div_free) 
    { 
      ho_div_free = aho_div_free; 
      only_ho_div = only_ho_div && !ho_div_free;
    };  

    void SetOnlyHODiv (bool aonly_ho_div) 
    { 
      only_ho_div = aonly_ho_div; 
      ho_div_free = ho_div_free && !only_ho_div;
    };  

    virtual void ComputeNDof();
    virtual ELEMENT_TYPE ElementType() const { return ET; }
    virtual void GetFacetDofs(int i, Array<int> & dnums) const;

    /// calc normal components of facet shapes, ip has facet-nr
    virtual void CalcNormalShape (const IntegrationPoint & ip, 
                                  SliceVector<> nshape) const;

  };


  
  // still to be changed ....

#ifdef HDIVHEX
  template<> 
  class HDivHighOrderFE<ET_HEX> : 
    public HDivHighOrderFiniteElement<3>
  {
  public:
    HDivHighOrderFE () { ; }
    HDivHighOrderFE (int aorder);



    virtual void ComputeNDof();
    virtual ELEMENT_TYPE ElementType() const { return ET_HEX; }

    // virtual void GetInternalDofs (Array<int> & idofs) const;
  

    /// compute shape
    virtual void CalcShape (const IntegrationPoint & ip,
                            SliceMatrix<> shape) const;

    /// compute Div of shape
    virtual void CalcDivShape (const IntegrationPoint & ip,
                               SliceVector<> shape) const;
    /// compute Div numerical diff
    //void CalcNumDivShape( const IntegrationPoint & ip,
    //			FlatVector<> divshape) const;
    virtual void GetFacetDofs(int i, Array<int> & dnums) const; 

  };
#endif

}



#ifdef FILE_HDIVHOFE_CPP

#define HDIVHOFE_EXTERN
#include <thdivfe_impl.hpp>
#include <hdivhofe_impl.hpp>
#include <hdivhofefo.hpp>

#else

#define HDIVHOFE_EXTERN extern

#endif


namespace ngfem
{
  HDIVHOFE_EXTERN template class HDivHighOrderFE<ET_TRIG>;
  HDIVHOFE_EXTERN template class HDivHighOrderFE<ET_QUAD>;
  HDIVHOFE_EXTERN template class HDivHighOrderFE<ET_TET>;
  HDIVHOFE_EXTERN template class HDivHighOrderFE<ET_PRISM>;
  HDIVHOFE_EXTERN template class HDivHighOrderFE<ET_HEX>;

  HDIVHOFE_EXTERN template class T_HDivFiniteElement<HDivHighOrderFE_Shape<ET_TRIG>, ET_TRIG>;
  HDIVHOFE_EXTERN template class T_HDivFiniteElement<HDivHighOrderFE_Shape<ET_QUAD>, ET_QUAD>;
  HDIVHOFE_EXTERN template class T_HDivFiniteElement<HDivHighOrderFE_Shape<ET_TET>, ET_TET>;
  HDIVHOFE_EXTERN template class T_HDivFiniteElement<HDivHighOrderFE_Shape<ET_PRISM>, ET_PRISM>;
  HDIVHOFE_EXTERN template class T_HDivFiniteElement<HDivHighOrderFE_Shape<ET_HEX>, ET_HEX>;
}

#endif



