#ifndef FILE_HDIVDIVFE
#define FILE_HDIVDIVFE

/*********************************************************************/
/* File:   hdivdivfe.hpp                                             */
/* Author: Astrid Pechstein, Joachim Schoeberl                       */
/* Date:   orig 2006, redesign Dec 2016                              */
/*********************************************************************/


namespace ngfem
{

  template <int DIM>
  class HDivDivFiniteElement : public FiniteElement
  {
  public:
    using FiniteElement::FiniteElement;
    using FiniteElement::ndof;
    using FiniteElement::order;

    // old style
    virtual void CalcShape (const IntegrationPoint & ip, 
                            BareSliceMatrix<double> shape) const = 0;

    virtual void CalcDivShape (const IntegrationPoint & ip, 
                               BareSliceMatrix<double> divshape) const = 0;

    // new implementation
    virtual void CalcMappedShape_Matrix (const MappedIntegrationPoint<DIM,DIM> & mip,
      BareSliceMatrix<double> shape) const = 0;

    virtual void CalcMappedShape_Vector (const MappedIntegrationPoint<DIM,DIM> & mip,
      BareSliceMatrix<double> shape) const = 0;

    virtual void CalcMappedDivShape (const MappedIntegrationPoint<DIM,DIM> & mip,
      BareSliceMatrix<double> shape) const = 0;

  };

  template <int D,typename VEC,typename MAT>
  void VecToSymMat(const VEC & vec,MAT & mat)
  {
    switch(D)
    {
    case 2:
      mat(0) = vec(0);
      mat(3) = vec(1);
      mat(1) = mat(2) = vec(2);
      break;
    case 3:
      mat(0) = vec(0);
      mat(4) = vec(1);
      mat(8) = vec(2);
      mat(1) = mat(3) = vec(5);
      mat(2) = mat(6) = vec(4);
      mat(5) = mat(7) = vec(3);
      break;
    }

  }

  template <ELEMENT_TYPE ET> class HDivDivFE;

  
  template <ELEMENT_TYPE ET>
  class T_HDivDivFE : public HDivDivFiniteElement<ET_trait<ET>::DIM>,
    public VertexOrientedFE<ET>
  {
  protected:
    enum { DIM = ET_trait<ET>::DIM };
    enum { DIM_STRESS = (DIM*(DIM+1))/2 };
    
    using VertexOrientedFE<ET>::vnums;
    using HDivDivFiniteElement<ET_trait<ET>::DIM>::ndof;
    using HDivDivFiniteElement<ET_trait<ET>::DIM>::order;

    //enum { N_VERTEX = ET_trait<ET>::N_VERTEX };
    //enum { N_FACET   = ET_trait<ET>::N_FACET };    
    //
    //size_t vnums[N_VERTEX];
    INT<DIM-1> order_facet[ET_trait<ET>::N_FACET];
    INT<DIM> order_inner;

    // additional div-div free bubbles
    bool plus;

  public:
    using VertexOrientedFE<ET>::SetVertexNumbers;
    
    T_HDivDivFE (int aorder, bool _plus = false)
      : plus(_plus)
    {
      order = aorder;
      for (auto & of : order_facet) of = aorder;
      order_inner = aorder;
      //ndof = DIM*(DIM+1)/2 * ET_trait<ET>::PolDimension(aorder);

    }
    
    virtual ELEMENT_TYPE ElementType() const { return ET; }
    const HDivDivFE<ET> * Cast() const { return static_cast<const HDivDivFE<ET>*> (this); } 
    
    INLINE void SetOrderFacet (int nr, INT<DIM-1,int> order) { order_facet[nr] = order; }
    INLINE void SetOrderInner (INT<DIM,int> order) { order_inner = order; }

    virtual void ComputeNDof()
    {
      cout << "Error, T_HDivDivFE<ET>:: ComputeNDof not available, only for ET == TRIG" << endl;
    }

    // old style
    virtual void CalcShape (const IntegrationPoint & ip, 
                            BareSliceMatrix<double> shape) const
    {
      Vec<DIM, AutoDiffDiff<DIM>> adp;
      for ( int i=0; i<DIM; i++)
      {
        adp(i) = AutoDiffDiff<DIM>(ip(i),i);
      }

      Cast() -> T_CalcShape (TIP<DIM, AutoDiffDiff<DIM>> (adp), SBLambda([&] (int nr, auto val)
                                          {
                                            shape.Row(nr).AddSize(DIM_STRESS) = val.Shape();
                                          }));
    }

    virtual void CalcDivShape (const IntegrationPoint & ip,
                               BareSliceMatrix<double> shape) const
    {
      Vec<DIM, AutoDiffDiff<DIM>> adp;
      for ( int i=0; i<DIM; i++)
      {
        adp[i] = AutoDiffDiff<DIM>(ip(i),i);
      }
      
      Cast() -> T_CalcShape (TIP<DIM, AutoDiffDiff<DIM>> (adp), SBLambda([&] (int nr, auto val)
                                          {
                                            shape.Row(nr).AddSize(DIM) = val.DivShape();
                                          }));
    }

    // new style
    virtual void CalcMappedShape_Vector (const MappedIntegrationPoint<DIM,DIM> & mip,
                            BareSliceMatrix<double> shape) const
    {
      Vec<DIM, AutoDiff<DIM>> adp = mip;
      Vec<DIM, AutoDiffDiff<DIM>> addp;
      for (int i=0; i<DIM; i++)
      {
        addp[i].Value() = adp[i].Value();
        addp[i].LoadGradient(&adp[i].DValue(0));
      }
      Cast() -> T_CalcShape (TIP<DIM, AutoDiffDiff<DIM>> (addp), SBLambda([&] (int nr, auto val)
                                          {
                                            shape.Row(nr).AddSize(DIM_STRESS) = val.Shape();
                                          }));
    }


    virtual void CalcMappedShape_Matrix (const MappedIntegrationPoint<DIM,DIM> & mip,
                            BareSliceMatrix<double> shape) const
    {
      Vec<DIM, AutoDiff<DIM>> adp = mip;
      Vec<DIM, AutoDiffDiff<DIM>> addp;
      for (int i=0; i<DIM; i++)
      {
        addp[i].Value() = adp[i].Value();
        addp[i].LoadGradient(&adp[i].DValue(0));
      }
      Cast() -> T_CalcShape (TIP<DIM,AutoDiffDiff<DIM>> (addp),SBLambda([&](int nr,auto val)
      {
        Vec<DIM_STRESS> vecshape = val.Shape();
        BareVector<double> matshape = shape.Row(nr);
        VecToSymMat<DIM> (vecshape, matshape);
      }));
    }


    virtual void CalcMappedDivShape (const MappedIntegrationPoint<DIM,DIM> & mip,
                            BareSliceMatrix<double> shape) const
    {
      Vec<DIM, AutoDiff<DIM>> adp = mip;
      Vec<DIM, AutoDiffDiff<DIM>> addp;
      for (int i=0; i<DIM; i++)
      {
        addp[i].Value() = adp[i].Value();
        addp[i].LoadGradient(&adp[i].DValue(0));
      }

      if(!mip.GetTransformation().IsCurvedElement()) // non-curved element
      {
        Cast() -> T_CalcShape (TIP<DIM,AutoDiffDiff<DIM>> (addp),SBLambda([&](int nr,auto val)
        {
          shape.Row(nr).AddSize(DIM) = val.DivShape();
        }));
      }
      else // curved element
      {

        Mat<DIM> jac = mip.GetJacobian();
        Mat<DIM> inv_jac = mip.GetJacobianInverse();
        Mat<DIM> hesse[3],finvT_h_tilde_finv[3];
        mip.CalcHesse (hesse[0],hesse[1],hesse[2]);

        Mat<DIM,DIM,AutoDiff<DIM> > f_tilde;
        for(int i = 0; i < DIM; i++)
        {
          for(int j = 0; j < DIM; j++)
          {
            f_tilde(i,j).Value() = jac(i,j);
            for(int k = 0; k < DIM; k++)
              f_tilde(i,j).DValue(k) = hesse[i](j,k);
          }
        }

        AutoDiff<DIM> ad_det = Det (f_tilde);
        AutoDiff<DIM> iad_det = 1.0 / ad_det;
        f_tilde *= iad_det;

        for(int i=0; i<DIM; i++)
        {
          finvT_h_tilde_finv[i] = 0;
          for(int alpha=0; alpha<DIM; alpha++)
            for(int beta=0; beta<DIM; beta++)
              for(int gamma=0; gamma<DIM; gamma++)
                for(int delta=0; delta<DIM; delta++)
                  finvT_h_tilde_finv[i](alpha,beta) += inv_jac(gamma,alpha)*f_tilde(i,gamma).DValue(delta)*inv_jac(delta,beta);
        }

        Cast() -> T_CalcShape (TIP<DIM,AutoDiffDiff<DIM>> (addp),SBLambda([&](int nr,auto val)
                                  {
                                    shape.Row(nr).AddSize(DIM) = val.DivShape();
                                    BareVector<double> divshape = shape.Row(nr);
                                    Vec<DIM_STRESS> vecshape = val.Shape();
                                    Vec<DIM*DIM> matshape;
                                    VecToSymMat<DIM> (vecshape, matshape);

                                    for(int k=0; k<DIM; k++)
                                    {
                                      for(int j=0; j<DIM*DIM; j++)
                                      {
                                        divshape(k) += mip.GetJacobiDet() * finvT_h_tilde_finv[k](j) * matshape(j);
                                      }
                                    }
                                    
                                  }));
      }
    }

  };


  
  // ***************** SigmaGrad ****************************** */
  // sigma (nabla u)
  
  template <int D> class T_SigmaGrad;
  template <> class T_SigmaGrad<2>
  {
    AutoDiffDiff<2> u;
  public:
    T_SigmaGrad  (AutoDiffDiff<2> au) : u(au) { ; }
    Vec<3> Shape() { return Vec<3> (u.DDValue(1,1), u.DDValue(0,0), -u.DDValue(1,0)); }
    Vec<2> DivShape() { return Vec<2> (0.0, 0.0); }
  };
  
  template <int D>
  auto SigmaGrad (AutoDiffDiff<D> au) { return T_SigmaGrad<D>(au); }
  
  
  // ***************** Sigma_u_Gradv ****************************** */
  // sigma (u nabla v)
  
  template <int D> class T_Sigma_u_Gradv;
  template <> class T_Sigma_u_Gradv<2>
  {
    AutoDiffDiff<2> u, v;
  public:
    T_Sigma_u_Gradv  (AutoDiffDiff<2> au, AutoDiffDiff<2> av) : u(au), v(av) { ; }
    Vec<3> Shape() { return Vec<3> (u.Value()*v.DDValue(1,1) + u.DValue(1)*v.DValue(1),
                                    u.Value()*v.DDValue(0,0) + u.DValue(0)*v.DValue(0),
                                    -u.Value()*v.DDValue(1,0) - 0.5 * (u.DValue(0)*v.DValue(1)+u.DValue(1)*v.DValue(0))); }
    Vec<2> DivShape()
    {
      double uxx = u.DDValue(0,0), uyy = u.DDValue(1,1), uxy = u.DDValue(0,1);
      double ux = u.DValue(0), uy = u.DValue(1);
      double vxx = v.DDValue(0,0), vyy = v.DDValue(1,1), vxy = v.DDValue(0,1);
      double vx = v.DValue(0), vy = v.DValue(1);
      
      return -0.5 * Vec<2> (uyy*vx - uxy*vy + uy*vxy - ux*vyy,
                            -uxy*vx + uxx*vy - uy*vxx + ux*vxy);
    }
  };
  
  template <int D>
  auto Sigma_u_Gradv (AutoDiffDiff<D> au, AutoDiffDiff<D> av) { return T_Sigma_u_Gradv<D>(au, av); }
  
  
  // ***************** Type2 ****************************** */
  // ????
  
  template <int D> class T_Type2;
  template <> class T_Type2<2>
  {
    AutoDiffDiff<2> u,v;
  public:
    T_Type2  (AutoDiffDiff<2> au, AutoDiffDiff<2> av) : u(au), v(av) { ; }
    Vec<3> Shape() { return Vec<3> (u.DDValue(1,1)*v.Value() - 2*u.DValue(1)*v.DValue(1) + u.Value()*v.DDValue(1,1),
                                    u.DDValue(0,0)*v.Value() - 2*u.DValue(0)*v.DValue(0) + u.Value()*v.DDValue(0,0),
                                    -(u.DDValue(0,1)*v.Value() - u.DValue(0)*v.DValue(1) -
                                      u.DValue(1)*v.DValue(0) + u.Value()*v.DDValue(1,0))); }

    Vec<2> DivShape()
    {
      double uxx = u.DDValue(0,0), uyy = u.DDValue(1,1), uxy = u.DDValue(0,1);
      double ux = u.DValue(0), uy = u.DValue(1);
      double vxx = v.DDValue(0,0), vyy = v.DDValue(1,1), vxy = v.DDValue(0,1);
      double vx = v.DValue(0), vy = v.DValue(1);
      
      return Vec<2> (2*uyy*vx + 2*ux*vyy - 2*uxy*vy - 2*uy*vxy, 2*uxx*vy + 2*uy*vxx - 2*uxy*vx - 2*ux*vxy);
    }
  };
  
  template <int D>
  auto Type2 (AutoDiffDiff<D> au, AutoDiffDiff<D> av) { return T_Type2<D>(au, av); }
  
  // ***************** Type3 ****************************** */
  // ????
  
  template <int D> class T_Type3;
  template <> class T_Type3<2>
  {
    AutoDiffDiff<2> u,v;
  public:
    T_Type3  (AutoDiffDiff<2> au, AutoDiffDiff<2> av) : u(au), v(av) { ; }
    Vec<3> Shape() { return Vec<3> (u.DDValue(1,1)*v.Value() - u.Value()*v.DDValue(1,1),
                                    u.DDValue(0,0)*v.Value() - u.Value()*v.DDValue(0,0),
                                    -(u.DDValue(0,1)*v.Value() - u.Value()*v.DDValue(1,0))); }
    Vec<2> DivShape()
    {
      double uxx = u.DDValue(0,0), uyy = u.DDValue(1,1), uxy = u.DDValue(0,1);
      double ux = u.DValue(0), uy = u.DValue(1);
      double vxx = v.DDValue(0,0), vyy = v.DDValue(1,1), vxy = v.DDValue(0,1);
      double vx = v.DValue(0), vy = v.DValue(1);

      return Vec<2> (uyy*vx - uxy*vy - ux*vyy + uy*vxy, uxx*vy - uxy*vx - uy*vxx + ux*vxy);
    }
  };
  
  template <int D>
  auto Type3 (AutoDiffDiff<D> au, AutoDiffDiff<D> av) { return T_Type3<D>(au, av); }

  
  // ***************** Sigma ((vDu - uDv) w) ****************************** */
  // where u, v are NOW POSSIBLY NON-linear hat basis functions (i.e. vDu - uDv is Nedelec0 edge basis function)
  template <int D> class T_Sigma_Duv_minus_uDv_w;
  template <> class T_Sigma_Duv_minus_uDv_w<2>
  {
    AutoDiffDiff<2> u,v,w;
  public:
    T_Sigma_Duv_minus_uDv_w  (AutoDiffDiff<2> au, AutoDiffDiff<2> av, AutoDiffDiff<2> aw) : u(au), v(av), w(aw) { ; }
    Vec<3> Shape() { return Vec<3> (w.DValue(1)*(v.DValue(1)*u.Value()-u.DValue(1)*v.Value()), 
      w.DValue(0)*(v.DValue(0)*u.Value()-u.DValue(0)*v.Value()),
      -0.5*( w.DValue(0)*(v.DValue(1)*u.Value()-u.DValue(1)*v.Value()) +
        w.DValue(1)*(v.DValue(0)*u.Value()-u.DValue(0)*v.Value()) )
      ); }

    Vec<2> DivShape()
    {
      double uxx = u.DDValue(0,0), uyy = u.DDValue(1,1), uxy = u.DDValue(0,1);
      double ux = u.DValue(0), uy = u.DValue(1);
      double vxx = v.DDValue(0,0), vyy = v.DDValue(1,1), vxy = v.DDValue(0,1);
      double vx = v.DValue(0), vy = v.DValue(1);
      double wxx = w.DDValue(0,0), wyy = w.DDValue(1,1), wxy = w.DDValue(0,1);
      double wx = w.DValue(0), wy = w.DValue(1);

      return Vec<2> (0.5*wxy*(vy*u.Value() - uy*v.Value()) 
        -0.5*wyy*(vx*u.Value() - ux*v.Value())
        +1.5*wy*(vy*ux - uy*vx) + 0.5*wy*(vxy*u.Value()-uxy*v.Value()) 
        -0.5*wx*(vyy*u.Value()-uyy*v.Value()),
        0.5*wxy*(vx*u.Value() - ux*v.Value()) 
        -0.5*wxx*(vy*u.Value() - uy*v.Value())
        +1.5*wx*(vx*uy - ux*vy) + 0.5*wx*(vxy*u.Value()-uxy*v.Value()) 
        -0.5*wy*(vxx*u.Value()-uxx*v.Value())
        ); 
    }

  };
  

  

  class T_SymRotRot_Dl2xDl1_v
  {
    AutoDiff<2> l1,l2,v;
  public:
    T_SymRotRot_Dl2xDl1_v  (AutoDiff<2> lam1, AutoDiff<2> lam2, AutoDiff<2> av) : l1(lam1), l2(lam2), v(av) { ; }
    Vec<3> Shape() { return Vec<3> (v.Value()*(l1.DValue(1)*l2.DValue(1)),
      v.Value()*(l1.DValue(0)*l2.DValue(0)),
      -0.5*v.Value()*(l1.DValue(1)*l2.DValue(0) + l1.DValue(0)*l2.DValue(1))
      ); }

    Vec<2> DivShape()
    {
      // todo
      double lam1 = l1.Value();
      double lam1x = l1.DValue(0);
      double lam1y = l1.DValue(1);
      double lam2 = l2.Value();
      double lam2x = l2.DValue(0);
      double lam2y = l2.DValue(1);
      return Vec<2> (
        v.DValue(0)*(lam1y*lam2y) - 0.5*v.DValue(1)*(lam1x*lam2y+lam1y*lam2x),
        -0.5*v.DValue(0)*(lam1x*lam2y+lam1y*lam2x) + v.DValue(1)*(lam1x*lam2x)
        ); 
    }

  };

  template <int D>
  auto Sigma_Duv_minus_uDv_w (AutoDiffDiff<D> au, AutoDiffDiff<D> av, AutoDiffDiff<D> aw)
  { return T_Sigma_Duv_minus_uDv_w<D>(au, av, aw); }
  
  
  template <ELEMENT_TYPE ET> class HDivDivFE : public T_HDivDivFE<ET> 
  {
  protected:
    using T_HDivDivFE<ET> :: order;
    using T_HDivDivFE<ET> :: ndof;
  public:
    template <typename Tx, typename TFA> 
    void T_CalcShape (TIP<ET_trait<ET>::DIM,Tx> ip, TFA & shape) const
    {
      throw Exception ("Hdivdivfe not implementend for element type");
    }
  };

  
  template <> class HDivDivFE<ET_TRIG> : public T_HDivDivFE<ET_TRIG> 
  {
    
  public:
    using T_HDivDivFE<ET_TRIG> :: T_HDivDivFE;

    virtual void ComputeNDof()
    {
      order = 0;
      ndof = 0;
      for (int i=0; i<3; i++)
      {
        ndof += order_facet[i][0]+1;
        order = max2(order, order_facet[i][0]);
      }
      int ninner = 3*order_inner[0]*(order_inner[0]+1)/2 ;
      order = max2(order, order_inner[0]);
      if (plus)
      { 
        order ++;
        ninner += 2*order_inner[0]; 
      }
      ndof += ninner;

    }
   template <typename Tx, typename TFA> 
    void T_CalcShape (TIP<2,Tx> ip/*AutoDiffDiff<2> hx[2]*/, TFA & shape) const
    {
      auto x = ip.x, y = ip.y;
      AutoDiffDiff<2> ddlami[3] ={ x, y, 1-x-y };
      
      int ii = 0;
      
      int maxorder_facet =
        max2(order_facet[0][0],max2(order_facet[1][0],order_facet[2][0]));

      const EDGE * edges = ElementTopology::GetEdges(ET_TRIG);

      ArrayMem<AutoDiffDiff<2>,20> ha(maxorder_facet+1);
      ArrayMem<AutoDiffDiff<2>,20> u(order_inner[0]+2), v(order_inner[0]+2);
      
      for (int i = 0; i < 3; i++)
        {
          int es = edges[i][0], ee = edges[i][1];
          if (vnums[es] > vnums[ee]) swap (es,ee);
          
          AutoDiffDiff<2> ls = ddlami[es], le = ddlami[ee];
          
          // edge functions are all div-free!
          IntegratedLegendreMonomialExt::CalcTrigExt(maxorder_facet+2,
                                                     le-ls, 1-le-ls, ha);

          //ScaledLegendrePolynomial(maxorder_facet,le-ls, 1-le-ls,ha);

          
          for (int l = 0; l <= order_facet[i][0]; l++)
            shape[ii++] = SigmaGrad (ha[l]);
            //shape[ii++] = SymRotRot_Dl2xDl1_v_diffdiff(le, ls, ha[l]);
        }
      
      int es = 0; int ee = 1; int et = 2;
      AutoDiffDiff<2> ls = ddlami[es];
      AutoDiffDiff<2> le = ddlami[ee];
      AutoDiffDiff<2> lt = ddlami[et];
      
      int oi=order_inner[0];
      int oi_plus = oi; //plus ? oi+1 : oi;


      IntegratedLegendreMonomialExt::CalcTrigExt(oi_plus+3,le-ls,1-le-ls,u);
      LegendrePolynomial::EvalMult(oi_plus+1, 2*lt-1, lt, v);
      
      
      for(int i = 0; i <= oi-1; i++)
      {
        for(int j = 0; j+i <= oi-1; j++)
        {
          shape[ii++] = SigmaGrad(u[i]*v[j]);
          shape[ii++] = Type2(u[i],v[j]);
        }
      }
      for(int i = 0; i <= oi_plus-1; i++)
      {
        for(int j = 0; j+i <= oi_plus-1; j++)
        {
          if(j > 0)
            shape[ii++] = Type3(u[i],v[j]);
        }
      }
      
      for (int i = 0; i < oi_plus; i++)
        shape[ii++] = Sigma_Duv_minus_uDv_w (le, -ls, v[i]);
      
      //// element bubbles for Sigma+ space
      if (plus)
        for (int i = 0; i <= oi-1; i++)
          {
            AutoDiffDiff<2> bubble = u[i]*v[oi-1-i];
            shape[ii++] = Sigma_u_Gradv(bubble, x);
            shape[ii++] = Sigma_u_Gradv(bubble, y);
          }
    };
  };
  

  // ***************** S_zz(uvw) ****************************** */
  // write uvw into zz component
  template <int D> class T_S_zz;
  template <> class T_S_zz<3>
  {
    AutoDiff<2> u, v;
    AutoDiff<1> w;
  public:
    T_S_zz ( AutoDiff<2> au, AutoDiff<2> av, AutoDiff<1> aw) : u(au), v(av), w(aw) { ; }
    Vec<6> Shape() 
    { 
      Vec<6> sigma(0.);
      sigma[2] = u.Value()*v.Value()*w.Value();
      return sigma;
    }

    Vec<3> DivShape()
    {
      return Vec<3> (0., 0., u.Value()*v.Value()*w.DValue(0));
    }

  };
  
  template <int D>
  auto S_zz (AutoDiff<D> au, AutoDiff<D> av, AutoDiff<1> aw)
  { return T_S_zz<D+1>(au, av, aw); }
    
  // ***************** S_xz ****************************** */
  template <int D> class T_S_xz;
  template <> class T_S_xz<3>
  {
    AutoDiff<2> uv;
    AutoDiff<1> w;

    int comp;
  public:
    T_S_xz ( int acomp, AutoDiff<2> auv, AutoDiff<1> aw) : comp(acomp), uv(auv), w(aw) { ; }
    Vec<6> Shape() 
    { 
      Vec<6> sigma;
      sigma = 0.;
      if (comp==0)
        sigma[4] = uv.Value()*w.Value();
      else
        sigma[3] = uv.Value()*w.Value();
      return sigma;
    }

    Vec<3> DivShape()
    {
      if (comp == 0)
        return Vec<3> (uv.Value()*w.DValue(0), 0, uv.DValue(0)*w.Value() );
      else
        return Vec<3> (0, uv.Value()*w.DValue(0), uv.DValue(1)*w.Value() );
    }

  };
  
  template <int D>
  auto S_xz (int comp, AutoDiff<D> auv, AutoDiff<1> aw)
  { return T_S_xz<D+1>(comp,auv, aw); }





  class Prism_wSigmaGradu
  {
    AutoDiffDiff<2> u;
    AutoDiff<1> w;
  public:
    Prism_wSigmaGradu ( AutoDiffDiff<2> au, AutoDiff<1> aw) : u(au), w(aw) { ; }
    Vec<6> Shape() 
    { 
      Vec<3> sigma2d = T_SigmaGrad<2>(u).Shape();
      Vec<6> sigma(0.);
      sigma[0] = w.Value()*sigma2d[0];
      sigma[1] = w.Value()*sigma2d[1];
      sigma[5] = w.Value()*sigma2d[2];
      return sigma;
    }

    Vec<3> DivShape()
    {
      return Vec<3> (0., 0., 0);
    }

  };

  class Prism_wType2
  {
    AutoDiffDiff<2> u, v;
    AutoDiff<1> w;
  public:
    Prism_wType2 ( AutoDiffDiff<2> au, AutoDiffDiff<2> av, AutoDiff<1> aw) : u(au), v(av),  w(aw) { ; }
    Vec<6> Shape() 
    { 
      Vec<3> sigma2d = T_Type2<2>(u,v).Shape();
      Vec<6> sigma(0.);
      sigma[0] = w.Value()*sigma2d[0];
      sigma[1] = w.Value()*sigma2d[1];
      sigma[5] = w.Value()*sigma2d[2];
      return sigma;
    }

    Vec<3> DivShape()
    {
      Vec<2> divsigma2d = w.Value()*T_Type2<2>(u,v).DivShape();
      return Vec<3> (divsigma2d[0], divsigma2d[1], 0);
    }

  };
  
  class Prism_wType3
  {
    AutoDiffDiff<2> u, v;
    AutoDiff<1> w;
  public:
    Prism_wType3 ( AutoDiffDiff<2> au, AutoDiffDiff<2> av, AutoDiff<1> aw) : u(au), v(av),  w(aw) { ; }
    Vec<6> Shape() 
    { 
      Vec<3> sigma2d = T_Type3<2>(u,v).Shape();
      Vec<6> sigma(0.);
      sigma[0] = w.Value()*sigma2d[0];
      sigma[1] = w.Value()*sigma2d[1];
      sigma[5] = w.Value()*sigma2d[2];
      return sigma;
    }

    Vec<3> DivShape()
    {
      Vec<2> divsigma2d = w.Value()*T_Type3<2>(u,v).DivShape();
      return Vec<3> (divsigma2d[0], divsigma2d[1], 0);
    }

  };

  class Prism_wType4
  {
    AutoDiffDiff<2> u, v, w;
    AutoDiff<1> wz;
  public:
    Prism_wType4 ( AutoDiffDiff<2> au, AutoDiffDiff<2> av, AutoDiffDiff<2> aw, AutoDiff<1> awz) : u(au), v(av),  w(aw), wz(awz) { ; }
    Vec<6> Shape() 
    { 
      Vec<3> sigma2d = wz.Value()*T_Sigma_Duv_minus_uDv_w<2>(u,v,w).Shape();
      Vec<6> sigma(0.);
      sigma[0] = sigma2d[0];
      sigma[1] = sigma2d[1];
      sigma[5] = sigma2d[2];
      return sigma;
    }

    Vec<3> DivShape()
    {
      Vec<2> divsigma2d = wz.Value()*T_Sigma_Duv_minus_uDv_w<2>(u,v,w).DivShape();
      return Vec<3> (divsigma2d[0], divsigma2d[1], 0);
    }

  };

  class Prism_SymRotRot_Dl2xDl1_vw
  {
    AutoDiff<2> l1,l2,v;
    AutoDiff<1> wz;
  public:
    Prism_SymRotRot_Dl2xDl1_vw ( AutoDiff<2> lam1, AutoDiff<2> lam2, AutoDiff<2> av, AutoDiff<1> awz) : l1(lam1), l2(lam2), v(av), wz(awz) { ; }
    Vec<6> Shape() 
    { 
      Vec<3> sigma2d = wz.Value()*T_SymRotRot_Dl2xDl1_v(l1,l2,v).Shape();
      Vec<6> sigma(0.);
      sigma[0] = sigma2d[0];
      sigma[1] = sigma2d[1];
      sigma[5] = sigma2d[2];
      return sigma;
    }

    Vec<3> DivShape()
    {
      Vec<2> divsigma2d = wz.Value()*T_SymRotRot_Dl2xDl1_v(l1,l2,v).DivShape();
      return Vec<3> (divsigma2d[0], divsigma2d[1], 0);
    }

  };

  class Prism_Dl1xDl3_symtensor_Dl2xDl4_u
  {
    AutoDiff<3> l1,l2,l3, l4;
    AutoDiff<3> u;
  public:
    Prism_Dl1xDl3_symtensor_Dl2xDl4_u ( AutoDiff<3> lam1, AutoDiff<3> lam2, AutoDiff<3> alz1, AutoDiff<3> alz2, AutoDiff<3> av) 
      : l1(lam1), l2(lam2), l3(alz1), l4(alz2), u(av) { ; }
    Vec<6> Shape() 
    { 
      auto rotlam1 = Cross(l1, l3);
      auto rotlam2 = Cross(l2, l4);

      Vec<6> sigma(0.);
      sigma[0] = u.Value()*rotlam1.DValue(0)*rotlam2.DValue(0);
      sigma[1] = u.Value()*rotlam1.DValue(1)*rotlam2.DValue(1);
      sigma[2] = u.Value()*rotlam1.DValue(2)*rotlam2.DValue(2);
      sigma[3] = 0.5*u.Value()*(rotlam1.DValue(2)*rotlam2.DValue(1) + rotlam2.DValue(2)*rotlam1.DValue(1));
      sigma[4] = 0.5*u.Value()*(rotlam1.DValue(2)*rotlam2.DValue(0) + rotlam2.DValue(2)*rotlam1.DValue(0));
      sigma[5] = 0.5*u.Value()*(rotlam1.DValue(0)*rotlam2.DValue(1) + rotlam2.DValue(0)*rotlam1.DValue(1));
      return sigma;
    }

    INLINE AutoDiff<3> Cross (const AutoDiff<3> & x,
      const AutoDiff<3> & y)
    {
      double hv[3];
      hv[0] = x.DValue(1)*y.DValue(2)-x.DValue(2)*y.DValue(1);
      hv[1] = x.DValue(2)*y.DValue(0)-x.DValue(0)*y.DValue(2);
      hv[2] = x.DValue(0)*y.DValue(1)-x.DValue(1)*y.DValue(0);
      return AutoDiff<3> (0,hv);
    }
    Vec<3> DivShape()
    {
      auto lam1 = Cross(l1, l3);
      auto lam2 = Cross(l2, l4);
      return Vec<3> (u.DValue(0)*lam1.DValue(0)*lam2.DValue(0) + 
        0.5*(u.DValue(1)*(lam1.DValue(0)*lam2.DValue(1)+lam2.DValue(0)*lam1.DValue(1)) + u.DValue(2)*(lam1.DValue(0)*lam2.DValue(2)+lam2.DValue(0)*lam1.DValue(2))),
        u.DValue(1)*lam1.DValue(1)*lam2.DValue(1) + 
        0.5*(u.DValue(0)*(lam1.DValue(0)*lam2.DValue(1)+lam2.DValue(0)*lam1.DValue(1)) + u.DValue(2)*(lam1.DValue(1)*lam2.DValue(2)+lam2.DValue(1)*lam1.DValue(2))),
        u.DValue(2)*lam1.DValue(2)*lam2.DValue(2) + 
        0.5*(u.DValue(0)*(lam1.DValue(0)*lam2.DValue(2)+lam2.DValue(0)*lam1.DValue(2)) + u.DValue(1)*(lam1.DValue(1)*lam2.DValue(2)+lam2.DValue(1)*lam1.DValue(2)))
        );
    }

  };


  template <> class HDivDivFE<ET_PRISM> : public T_HDivDivFE<ET_PRISM> 
  {
  public:
    // order k+1 for certain components, for inner and boundary shapes
    // analysis from TDNNS paper for case xx1=0, zz1=xx2=zz2=1 for inner and boundary shapes
    // however, works also when boundary order is not increased.. check
    enum { incrorder_xx1 = 0};
    enum { incrorder_zz1 = 1};
    enum { incrorder_xx2 = 1};
    enum { incrorder_zz2 = 1};
    enum { incrorder_xx1_bd = 0};
    enum { incrorder_zz1_bd = 0};
    enum { incrorder_xx2_bd = 0};
    enum { incrorder_zz2_bd = 0};
    using T_HDivDivFE<ET_PRISM> :: T_HDivDivFE;

    virtual void ComputeNDof()
    {
      order = 0;
      ndof = 0;
      for (int i=0; i<2; i++)
      {
        ndof += (order_facet[i][0]+1+incrorder_zz1_bd)*(order_facet[i][0]+2+incrorder_zz1_bd)/2;
        order = max2(order, order_facet[i][0]+incrorder_zz1_bd);
      }
      for (int i=2; i<5; i++)
      {
        ndof += (order_facet[i][0]+1+incrorder_xx1_bd)*(order_facet[i][1]+1+incrorder_xx2_bd);
        order = max2(order, order_facet[i][0]+incrorder_xx2_bd);
      }
      int oi0 = order_inner[0];
      int oi2 = order_inner[2];
      int ninner = 3*((oi0+1+incrorder_xx1)*(oi0+incrorder_xx1))/2 *(oi2+1+incrorder_xx2) 
        + (oi0+1)*(oi0+2)*(oi2+1) 
        + (oi0+1+incrorder_zz1)*(oi0+2+incrorder_zz1)*(oi2-1+incrorder_zz2)/2;
      ndof += ninner; 

      order = max3(order, oi0+incrorder_zz1, oi2+incrorder_zz2);

    }

    // works only with old-style Transformation
    // does not work with CalcMappedShape
   template <typename Tx, typename TFA> 
    void T_CalcShape_Complex (TIP<3,Tx> ip, TFA & shape) const
    {
      AutoDiffDiff<2> x(ip.x.Value(),0);
      AutoDiffDiff<2> y(ip.y.Value(),1);
      AutoDiff<2> xd(ip.x.Value(),0);
      AutoDiff<2> yd(ip.y.Value(),1);
      AutoDiff<1> z(ip.z.Value(), 0);
      AutoDiffDiff<2> lami[6] ={ x,y,1-x-y,x,y,1-x-y };
      AutoDiff<2> lamid[6] ={ xd,yd,1-xd-yd,xd,yd,1-xd-yd };
      AutoDiff<1> lamiz[6] ={ 1-z,1-z,1-z,z,z,z };

      int ii = 0;
      
      int maxorder_facet =
        max2(order_facet[0][0],max2(order_facet[1][0],order_facet[2][0]));

      const FACE * faces = ElementTopology::GetFaces(ET_PRISM);

      ArrayMem<AutoDiffDiff<2>,20> ha(maxorder_facet+2);
      ArrayMem<AutoDiffDiff<2>,20> u(order+2), v(order+3);
      ArrayMem<AutoDiff<2>,20> leg_u(order+2), leg_v(order+3);
      ArrayMem<AutoDiff<1>,20> leg_w(order+2);

      
      // Trig faces, (p+1)(p+2)/2
      for (int fa=0; fa<2; fa++)
      {
        int fav[3] ={faces[fa][0],faces[fa][1],faces[fa][2]};

        //Sort vertices  first edge op minimal vertex
        if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]);
        if(vnums[fav[1]] > vnums[fav[2]]) swap(fav[1],fav[2]);
        if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]);
        
        leg_u.SetSize(order_facet[fa][0]+incrorder_zz1_bd+1);
        leg_v.SetSize(order_facet[fa][0]+incrorder_zz1_bd+1);
        ScaledLegendrePolynomial(order_facet[fa][0]+incrorder_zz1_bd,lamid[fav[0]]-lamid[fav[1]],1-lamid[fav[0]]-lamid[fav[1]],leg_u);
        LegendrePolynomial::Eval(order_facet[fa][0]+incrorder_zz1_bd,2 * lamid[fav[2]] - 1,leg_v);

        for(int j = 0; j <= order_facet[fa][0]+incrorder_zz1_bd; j++)
          for(int k = 0; k <= order_facet[fa][0]+incrorder_zz1_bd-j; k++)
            shape[ii++] = S_zz(leg_u[j],leg_v[k],lamiz[fav[0]]);
      }
      // quad faces -- use face bubbles of trig multiplied by leg_w
      // (px+1)(pz+1)
      for(int fa = 2; fa < 5; fa++)
      {
        int fmax = 0;
        for(int j = 1; j < 4; j++)
          if(vnums[faces[fa][j]] > vnums[faces[fa][fmax]]) fmax = j;

        int fz,ftrig;

        fz = 3 - fmax;
        ftrig = fmax^1;

        fmax = faces[fa][fmax];
        fz = faces[fa][fz];
        ftrig = faces[fa][ftrig];


        int orderz = order_facet[fa][1];

        bool rotate = false;
        if(vnums[fz] > vnums[ftrig]) rotate = true;
        leg_w.SetSize(order_facet[fa][1]+incrorder_xx2_bd+1);
        ha.SetSize(order_facet[fa][0]+incrorder_xx1_bd+1);
        LegendrePolynomial::Eval(order_facet[fa][1]+incrorder_xx2_bd,lamiz[fmax]*2-1,leg_w);


        // edge functions are all div-free!
        IntegratedLegendreMonomialExt::CalcTrigExt(order_facet[fa][0]+incrorder_xx1_bd+2,
          lami[fmax]-lami[ftrig],1-lami[fmax]-lami[ftrig],ha);

        if(rotate)
          for(int k = 0; k <= order_facet[fa][1]+incrorder_xx2_bd; k++)
            for(int l = 0; l <= order_facet[fa][0]+incrorder_xx1_bd; l++)
            {
              shape[ii++] = Prism_wSigmaGradu(ha[l],leg_w[k]);
            }

        else
          for(int l = 0; l <= order_facet[fa][0]+incrorder_xx1_bd; l++)
            for(int k = 0; k <= order_facet[fa][1]+incrorder_xx2_bd; k++)
            {
              shape[ii++] = Prism_wSigmaGradu(ha[l],leg_w[k]);
            }


      }



      int oi = order_inner[0];
      leg_u.SetSize(oi+incrorder_zz1+1);
      leg_v.SetSize(oi+incrorder_zz1+1);
      leg_w.SetSize(oi+incrorder_xx2+1);
      u.SetSize(oi-1+incrorder_xx1+1);
      v.SetSize(oi-1+incrorder_xx1+1);

      ScaledLegendrePolynomial(oi+incrorder_zz1, lamid[0]-lamid[1], 1-lamid[0]-lamid[1], leg_u);
      LegendrePolynomial::Eval(oi+incrorder_zz1, 2*lamid[2]-1, leg_v);
      LegendrePolynomial::Eval(oi+incrorder_xx2, 2*lamiz[0]-1, leg_w);

      // ------------------------------------
      // based on elasticity-complex-based triangle shapes
      IntegratedLegendreMonomialExt::CalcTrigExt(oi-1+incrorder_xx1+2,lami[0]-lami[1],1-lami[0]-lami[1],u);
      LegendrePolynomial::EvalMult(oi-1+incrorder_xx1,2*lami[2]-1, lami[2], v);
      for(int k=0; k<=oi+incrorder_xx2; k++)
      {
        for(int i = 0; i <= oi-1+incrorder_xx1; i++)
        {
          for(int j = 0; j+i <= oi-1+incrorder_xx1; j++)
          {
            shape[ii++] = Prism_wSigmaGradu(u[i]*v[j],leg_w[k]);
            shape[ii++] = Prism_wType2(u[i],v[j],leg_w[k]);
          }
        }
        for(int i = 0; i <= oi-1+incrorder_xx1; i++)
        {
          for(int j = 0; j+i <= oi-1+incrorder_xx1; j++)
          {
            if(j > 0)
            {
              shape[ii++] = Prism_wType3(u[i],v[j],leg_w[k]);
            }
          }
        }
        for (int i = 0; i < oi+incrorder_xx1; i++)
        {
          shape[ii++] = Prism_wType4 (lami[0], -lami[1], v[i],leg_w[k]);
        }

      }

      // S_xz
      for (int i=0; i<=oi; i++)
      {
        for (int j=0; j+i<=oi; j++)
        {
          AutoDiff<2> uv = leg_u[i]*leg_v[j];
          for (int k=0; k<=oi; k++)
          {
            shape[ii++] = S_xz(0,uv, leg_w[k]);
            shape[ii++] = S_xz(1,uv, leg_w[k]);
          }
        }
      }

      // S_zz
      for(int k=0; k<=oi-2+incrorder_zz2; k++)
      {
        AutoDiff<1> bubw = leg_w[k]*lamiz[0]*(1-lamiz[0]);
        for(int i=0; i<=oi+incrorder_zz1; i++)
        {
          for(int j=0; j<=oi+incrorder_zz1-i; j++)
          {
            shape[ii++] = S_zz(leg_u[i],leg_v[j],bubw);
          }
        }
      }


    };




    // alternative to T_CalcShape, with "simpler" shape functions,
    // that are described in anisotropic paper
    // works with CalcMappedShape etc. routines, also for curved elments
    template <typename Tx, typename TFA> 
    void T_CalcShape/*_nocomplex*/ (TIP<3,Tx> ip, TFA & shape) const
    {
      AutoDiffDiff<3> x = ip.x, y = ip.y, z = ip.z;
      AutoDiff<3> xx(x.Value(), &x.DValue(0));
      AutoDiff<3> yy(y.Value(), &y.DValue(0));
      AutoDiff<3> zz(z.Value(), &z.DValue(0));
      AutoDiff<3> lx[6] ={ xx, yy, 1-xx-yy, xx, yy, 1-xx-yy };
      AutoDiff<3> lz[6] ={ 1-zz,1-zz,1-zz,zz,zz,zz };
      int ii = 0;
      
      int maxorder_facet =
        max2(order_facet[0][0],max2(order_facet[1][0],order_facet[2][0]));

      const FACE * faces = ElementTopology::GetFaces(ET_PRISM);

      ArrayMem<AutoDiff<3>,20> leg_u(order+2), leg_v(order+3);
      ArrayMem<AutoDiff<3>,20> leg_w(order+2);

      
      // Trig faces, (p+1)(p+2)/2
      for (int fa=0; fa<2; fa++)
      {
        int fav[3] ={faces[fa][0],faces[fa][1],faces[fa][2]};

        //Sort vertices  first edge op minimal vertex
        if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]);
        if(vnums[fav[1]] > vnums[fav[2]]) swap(fav[1],fav[2]);
        if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]);
        
        leg_u.SetSize(order_facet[fa][0]+incrorder_zz1_bd+1);
        leg_v.SetSize(order_facet[fa][0]+incrorder_zz1_bd+1);
        ScaledLegendrePolynomial(order_facet[fa][0]+incrorder_zz1_bd,lx[fav[0]]-lx[fav[1]],1-lx[fav[0]]-lx[fav[1]],leg_u);
        LegendrePolynomial::Eval(order_facet[fa][0]+incrorder_zz1_bd,2 * lx[fav[2]] - 1,leg_v);

        for(int j = 0; j <= order_facet[fa][0]+incrorder_zz1_bd; j++)
          for(int k = 0; k <= order_facet[fa][0]+incrorder_zz1_bd-j; k++)
            shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lx[fav[0]], lx[fav[1]], lx[fav[2]], lx[fav[2]], leg_u[j]*leg_v[k]*lz[fav[0]]);
      }
      // quad faces -- use face bubbles of trig multiplied by leg_w
      // (px+1)(pz+1)
      for(int fa = 2; fa < 5; fa++)
      {
        int fmax = 0;
        for(int j = 1; j < 4; j++)
          if(vnums[faces[fa][j]] > vnums[faces[fa][fmax]]) fmax = j;

        int fz,ftrig;

        fz = 3 - fmax;
        ftrig = fmax^1;

        fmax = faces[fa][fmax];
        fz = faces[fa][fz];
        ftrig = faces[fa][ftrig];


        int orderz = order_facet[fa][1];

        bool rotate = false;
        if(vnums[fz] > vnums[ftrig]) rotate = true;
        leg_w.SetSize(order_facet[fa][1]+incrorder_xx2_bd+1);
        LegendrePolynomial::Eval(order_facet[fa][1]+incrorder_xx2_bd,lz[fmax]*2-1,leg_w);


        ScaledLegendrePolynomial(order_facet[fa][0]+incrorder_xx1_bd, lx[fmax]-lx[ftrig], lx[fmax]+lx[ftrig], leg_u);      

        if(rotate)
          for(int k = 0; k <= order_facet[fa][1]+incrorder_xx2_bd; k++)
            for(int l = 0; l <= order_facet[fa][0]+incrorder_xx1_bd; l++)
            {
              shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lz[fmax], lz[fz], lx[fmax], lx[ftrig], leg_u[l]* leg_w[k]);
            }

        else
          for(int l = 0; l <= order_facet[fa][0]+incrorder_xx1_bd; l++)
            for(int k = 0; k <= order_facet[fa][1]+incrorder_xx2_bd; k++)
            {
              shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lx[fmax], lx[ftrig], lz[fmax], lz[fz], leg_u[l]* leg_w[k]);
            }


      }



      int oi = order_inner[0];
      leg_u.SetSize(oi+incrorder_zz1+1);
      leg_v.SetSize(oi+incrorder_zz1+1);
      leg_w.SetSize(oi+incrorder_xx2+1);

      ScaledLegendrePolynomial(oi+incrorder_zz1, lx[0]-lx[1], 1-lx[0]-lx[1], leg_u);
      LegendrePolynomial::Eval(oi+incrorder_zz1, 2*lx[2]-1, leg_v);
      LegendrePolynomial::Eval(oi+incrorder_xx2, 2*lz[0]-1, leg_w);

      for(int k=0; k<=oi+incrorder_xx2; k++)
      {
        for(int i = 0; i <= oi-1+incrorder_xx1; i++)
        {
          for(int j = 0; j+i <= oi-1+incrorder_xx1; j++)
          {
            shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lx[0], lx[1], lz[0], lz[0], lx[2]*leg_u[i]*leg_v[j]* leg_w[k]);
            shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lx[2], lx[0], lz[0], lz[0], lx[1]*leg_u[i]*leg_v[j]* leg_w[k]);
            shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lx[1], lx[2], lz[0], lz[0], lx[0]*leg_u[i]*leg_v[j]* leg_w[k]);
          }
        }
      }


      // S_xz
      for (int i=0; i<=oi; i++)
      {
        for (int j=0; j+i<=oi; j++)
        {
          for (int k=0; k<=oi; k++)
          {
            shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lz[0], lx[1], lx[0], lx[0], leg_u[i]*leg_v[j]*leg_w[k]);
            shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lz[0], lx[0], lx[1], lx[1], leg_u[i]*leg_v[j]*leg_w[k]);
          }
        }
      }

      // S_zz
      for(int k=0; k<=oi-2+incrorder_zz2; k++)
      {
        AutoDiff<3> bubw = leg_w[k]*lz[0]*(1-lz[0]);
        for(int i=0; i<=oi+incrorder_zz1; i++)
        {
          for(int j=0; j<=oi+incrorder_zz1-i; j++)
          {
            shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lx[0], lx[2], lx[1], lx[1], leg_u[i]*leg_v[j]*bubw);
          }
        }
      }


    };

  };




  ////////////////////// SURFACE ////////////////////////////
    template <int DIM>
  class HDivDivSurfaceFiniteElement : public FiniteElement
  {
  public:
    using FiniteElement::FiniteElement;
    using FiniteElement::ndof;
    using FiniteElement::order;

    virtual void CalcMappedShape_Matrix (const MappedIntegrationPoint<DIM,DIM+1> & mip,
      BareSliceMatrix<double> shape) const = 0;

    virtual void CalcMappedShape_Vector (const MappedIntegrationPoint<DIM,DIM+1> & mip,
      BareSliceMatrix<double> shape) const = 0;

  };


  template <ELEMENT_TYPE ET> class HDivDivSurfaceFE;

  
  template <ELEMENT_TYPE ET>
  class T_HDivDivSurfaceFE : public HDivDivSurfaceFiniteElement<ET_trait<ET>::DIM>,
    public VertexOrientedFE<ET>
  {
  protected:
    enum { DIM = ET_trait<ET>::DIM };
    enum { DIM_STRESS = ((DIM+2)*(DIM+1))/2 };
    
    using VertexOrientedFE<ET>::vnums;
    using HDivDivSurfaceFiniteElement<ET_trait<ET>::DIM>::ndof;
    using HDivDivSurfaceFiniteElement<ET_trait<ET>::DIM>::order;

    INT<DIM> order_inner;


  public:
    using VertexOrientedFE<ET>::SetVertexNumbers;
    
    T_HDivDivSurfaceFE (int aorder)
    {
      order = aorder;
      order_inner = aorder;
    }
    
    virtual ELEMENT_TYPE ElementType() const { return ET; }
    const HDivDivSurfaceFE<ET> * Cast() const { return static_cast<const HDivDivSurfaceFE<ET>*> (this); } 
    
    INLINE void SetOrderInner (INT<DIM,int> order) { order_inner = order; }

    virtual void ComputeNDof()
    {
      cout << "Error, T_HDivDivSurfaceFE<ET>:: ComputeNDof not available for base class" << endl;
    }

    virtual void CalcMappedShape_Vector (const MappedIntegrationPoint<DIM,DIM+1> & mip,
                            BareSliceMatrix<double> shape) const
    {
      Vec<DIM, AutoDiff<DIM+1>> adp = mip;
      Vec<DIM, AutoDiffDiff<DIM+1>> addp;
      for (int i=0; i<DIM+1; i++)
      {
        addp[i].Value() = adp[i].Value();
        addp[i].LoadGradient(&adp[i].DValue(0));
      }
      Cast() -> T_CalcShape (TIP<DIM, AutoDiffDiff<DIM+1>> (addp), SBLambda([&] (int nr, auto val)
                                          {
                                            shape.Row(nr).AddSize(DIM_STRESS) = val.Shape();
                                          }));
    }


    virtual void CalcMappedShape_Matrix (const MappedIntegrationPoint<DIM,DIM+1> & mip,
                            BareSliceMatrix<double> shape) const
    {
      Vec<DIM, AutoDiff<DIM+1>> adp = mip;
      Vec<DIM, AutoDiffDiff<DIM+1>> addp;
      for (int i=0; i<DIM+1; i++)
      {
        addp[i].Value() = adp[i].Value();
        addp[i].LoadGradient(&adp[i].DValue(0));
      }
      Cast() -> T_CalcShape (TIP<DIM,AutoDiffDiff<DIM+1>> (addp),SBLambda([&](int nr,auto val)
      {
        Vec<DIM_STRESS> vecshape = val.Shape();
        BareVector<double> matshape = shape.Row(nr);
        VecToSymMat<DIM+1> (vecshape, matshape);
      }));
    }


  };

  template <> class HDivDivSurfaceFE<ET_SEGM> : public T_HDivDivSurfaceFE<ET_SEGM> 
  {
    
  public:
    using T_HDivDivSurfaceFE<ET_SEGM> :: T_HDivDivSurfaceFE;

    virtual void ComputeNDof()
    {
      order = 0;
      ndof = 0;
      ndof += order_inner[0]+1;
      order = max2(order,order_inner[0]);

    }
   template <typename Tx, typename TFA> 
    void T_CalcShape (TIP<1,Tx> ip/*AutoDiffDiff<2> hx[2]*/, TFA & shape) const
    {
      auto x = ip.x;
      AutoDiffDiff<2> ddlami[2] ={ x, 1-x };
      
      int ii = 0;
      
      ArrayMem<AutoDiffDiff<2>,20> u(order_inner[0]+2);
      
      int es = 0,ee = 1;
      if(vnums[es] > vnums[ee]) swap (es,ee);

      AutoDiffDiff<2> ls = ddlami[es],le = ddlami[ee];

      IntegratedLegendreMonomialExt::Calc(order_inner[0]+2, le-ls,u);

      for(int l = 0; l <= order_inner[0]; l++)
        shape[ii++] = SigmaGrad (u[l]);

      
    };
  };


  template <> class HDivDivSurfaceFE<ET_TRIG> : public T_HDivDivSurfaceFE<ET_TRIG> 
  {
    
  public:
    using T_HDivDivSurfaceFE<ET_TRIG> :: T_HDivDivSurfaceFE;

    virtual void ComputeNDof()
    {
      order = 0;
      ndof = 0;
      ndof += (order_inner[0]+1+HDivDivFE<ET_PRISM>::incrorder_zz1_bd)*(order_inner[0]+2+HDivDivFE<ET_PRISM>::incrorder_zz1_bd)/2;
      order = max2(order, order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_zz1_bd);
    }


    template <typename Tx, typename TFA> 
    void T_CalcShape (TIP<2,Tx> ip, TFA & shape) const
    {
      AutoDiffDiff<3> x = ip.x, y = ip.y;
      AutoDiff<3> xx(x.Value(), &x.DValue(0));
      AutoDiff<3> yy(y.Value(), &y.DValue(0));
      AutoDiff<3> lx[6] ={ xx, yy, 1-xx-yy};
      int ii = 0;

      ArrayMem<AutoDiff<3>,20> leg_u(order_inner[0]+2), leg_v(order_inner[0]+3);

      
        int fav[3] ={0,1,2};

        //Sort vertices  first edge op minimal vertex
        if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]);
        if(vnums[fav[1]] > vnums[fav[2]]) swap(fav[1],fav[2]);
        if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]);
        
        leg_u.SetSize(order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_zz1_bd+1);
        leg_v.SetSize(order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_zz1_bd+1);
        ScaledLegendrePolynomial(order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_zz1_bd,lx[fav[0]]-lx[fav[1]],1-lx[fav[0]]-lx[fav[1]],leg_u);
        LegendrePolynomial::Eval(order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_zz1_bd,2 * lx[fav[2]] - 1,leg_v);

        for(int j = 0; j <= order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_zz1_bd; j++)
          for(int k = 0; k <= order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_zz1_bd-j; k++)
            shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(lx[fav[0]], lx[fav[1]], lx[fav[2]], lx[fav[2]], leg_u[j]*leg_v[k]);
      }
  };


    template <> class HDivDivSurfaceFE<ET_QUAD> : public T_HDivDivSurfaceFE<ET_QUAD> 
  {
    
  public:
    using T_HDivDivSurfaceFE<ET_QUAD> :: T_HDivDivSurfaceFE;

    virtual void ComputeNDof()
    {
      order = 0;
      ndof = 0;
      ndof += (order_inner[0]+1+HDivDivFE<ET_PRISM>::incrorder_xx1_bd)*(order_inner[1]+1+HDivDivFE<ET_PRISM>::incrorder_xx2_bd);
      order = max2(order, order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_xx1_bd);
      order = max2(order, order_inner[1]+HDivDivFE<ET_PRISM>::incrorder_xx2_bd);
    }


    template <typename Tx, typename TFA> 
    void T_CalcShape (TIP<2,Tx> ip, TFA & shape) const
    {
      AutoDiffDiff<3> x = ip.x, z = ip.y;
      AutoDiff<3> xx(x.Value(), &x.DValue(0));
      AutoDiff<3> zz(z.Value(), &z.DValue(0));
      AutoDiff<3> sigma[4] = {1-xx+1-zz, xx+1-zz, xx+zz, 1-xx+zz};
      int ii = 0;
      

      ArrayMem<AutoDiff<3>,20> leg_u(order_inner[0]+2);
      ArrayMem<AutoDiff<3>,20> leg_w(order_inner[1]+2);

      
      int fmax = 0;
      for(int j = 1; j < 4; j++)
        if(vnums[j] > vnums[fmax]) fmax = j;

      int f1, f2;
      f1 = (fmax+1)%4;
      f2 = (fmax+3)%4;


      if(vnums[f1] > vnums[f2])
      {
        swap(f1,f2);
      }

      LegendrePolynomial::Eval(order_inner[0],sigma[fmax] - sigma[f1],leg_u);
      LegendrePolynomial::Eval(order_inner[0],sigma[fmax] - sigma[f2],leg_w);

      for(int k = 0; k <= order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_xx2_bd; k++)
        for(int l = 0; l <= order_inner[0]+HDivDivFE<ET_PRISM>::incrorder_xx1_bd; l++)
        {
          shape[ii++] = Prism_Dl1xDl3_symtensor_Dl2xDl4_u(0.5*(sigma[fmax]-sigma[f2]),-0.5*(sigma[fmax]-sigma[f2]),
            0.5*(sigma[fmax]-sigma[f1]),-0.5*(sigma[fmax]-sigma[f1]),leg_u[l]* leg_w[k]);
        }
                

    }
  };




}


#endif
  
