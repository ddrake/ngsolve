/*********************************************************************/
/* File:   l2hofespace.cpp                                         */
/* Author: Start                                                     */
/* Date:   24. Feb. 2003                                             */
/*********************************************************************/

/**
   High Order Finite Element Space for L2
*/

/* ***********************************************
To do: *Internal External Dofs (eliminate internal) 
       *Flag for low-order dofs eliminated ...   
************************* */ 

#include <comp.hpp>
#include <multigrid.hpp>

using namespace ngmg;

namespace ngcomp
{

  class BlockDifferentialOperatorId : public BlockDifferentialOperator
  {
  public:
    using BlockDifferentialOperator::BlockDifferentialOperator;

    virtual void Apply (const FiniteElement & fel,
                        const SIMD_BaseMappedIntegrationRule & mir,
                        BareSliceVector<double> x, 
                        BareSliceMatrix<SIMD<double>> flux) const
    {
      if (comp == -1)
        static_cast<const BaseScalarFiniteElement&> (fel).
          Evaluate(mir.IR(), SliceMatrix<double> (fel.GetNDof(), dim, dim, &x(0)), flux);
      else
        diffop->Apply(fel, mir, x.Slice(comp, dim), flux.RowSlice(comp,dim));
    }
    virtual void
    AddTrans (const FiniteElement & fel,
              const SIMD_BaseMappedIntegrationRule & mir,
              BareSliceMatrix<SIMD<double>> flux,
              BareSliceVector<double> x) const
    {
    if (comp == -1)
      static_cast<const BaseScalarFiniteElement&> (fel).      
        AddTrans(mir.IR(), flux, SliceMatrix<double> (fel.GetNDof(), dim, dim, &x(0)));
    else
      diffop->AddTrans(fel, mir, flux.RowSlice(comp,dim), x.Slice(comp,dim));
    }
    
  };


  
  L2HighOrderFESpace ::  
  L2HighOrderFESpace (shared_ptr<MeshAccess> ama, const Flags & flags, bool parseflags)
    : FESpace (ama, flags)
  {
    name="L2HighOrderFESpace(l2ho)";
    type = "l2ho";
   
    // defined flags
    DefineNumFlag("relorder");
    DefineDefineFlag("l2ho");
    DefineDefineFlag("all_dofs_together");

    if (parseflags) CheckFlags(flags);

    var_order = 0; 
    
    if (flags.NumFlagDefined("order"))
      order =  int (flags.GetNumFlag("order",0));
    else 
      {
	if(flags.NumFlagDefined("relorder"))
	  {
	    order=0; 
	    var_order = 1; 
	    rel_order = int (flags.GetNumFlag("relorder",0));
	  }
	else 
	  order = 0; 
      }
    
    if(flags.GetDefineFlag("variableorder") )
      {
	throw Exception ("Flag 'variableorder' for l2ho is obsolete. \n  Either choose uniform order by -order= .. \n -relorder=.. for relative mesh order "); 
      }

    /*
    integrator[VOL] = CreateBFI("mass", ma->GetDimension(),
                           make_shared<ConstantCoefficientFunction>(1));
    if (dimension > 1)
      integrator[VOL] = make_shared<BlockBilinearFormIntegrator> (integrator[VOL], dimension);
    */

    SetDefinedOn(BND, BitArray(ma->GetNRegions(BND)).Clear());

    switch (ma->GetDimension())
      {
      case 1:
        {
          evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<1>>>();
          flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpGradient<1>>>();
          // evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<1>>>();
          break;
        }
      case 2:
        {
          evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<2>>>();
          flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpGradient<2>>>();
          // evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<2>>>();
          break;
        }
      case 3:
        {
          evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<3>>>();
          flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpGradient<3>>>();
          // evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<3>>>();
          break;
        }
      }
    if (dimension > 1) 
      {
        evaluator[VOL] = make_shared<BlockDifferentialOperatorId> (evaluator[VOL], dimension);
        // evaluator[VOL] = make_shared<BlockDifferentialOperator> (evaluator[VOL], dimension);
	flux_evaluator[VOL] = make_shared<BlockDifferentialOperator> (flux_evaluator[VOL], dimension);
	// evaluator[BND] = make_shared<BlockDifferentialOperator> (evaluator[BND], dimension);
        /*
	boundary_flux_evaluator = 
	  make_shared<BlockDifferentialOperator> (boundary_flux_evaluator, dimension);
        */
      }





    all_dofs_together = flags.GetDefineFlag ("all_dofs_together");

    Flags loflags;
    loflags.SetFlag ("order", 0.0);
    loflags.SetFlag ("dim", dimension);
    if (dgjumps){ *testout << "(L2HOFES:)setting loflag dgjumps " << endl; loflags.SetFlag ("dgjumps");}
    if (iscomplex) loflags.SetFlag ("complex");


    if(all_dofs_together)
      prol = make_shared<L2HoProlongation>(ma,first_element_dof);
    else
      {
        low_order_space = make_shared<ElementFESpace> (ma, loflags);
        prol = make_shared<ElementProlongation> (*static_cast<ElementFESpace*> (low_order_space.get()));
      }
  }

  L2HighOrderFESpace :: ~L2HighOrderFESpace ()
  { ; }

  shared_ptr<FESpace> L2HighOrderFESpace :: 
  Create (shared_ptr<MeshAccess> ma, const Flags & flags)
  {
    int order = int(flags.GetNumFlag ("order", 0));
    if (order == 0)
      return make_shared<ElementFESpace> (ma, flags);
    else
      return make_shared<L2HighOrderFESpace> (ma, flags, true);
  }  

  
  void L2HighOrderFESpace :: Update(LocalHeap & lh)
  {
    FESpace::Update(lh);
    if(low_order_space) low_order_space -> Update(lh);

    nel = ma->GetNE();
    order_inner.SetSize(nel); 
 
    order_inner = INT<3>(order);

    if(var_order) 
      for(int i = 0; i < nel; i++) 
        order_inner[i] = ma->GetElOrders(i)+INT<3>(rel_order);
    
    for(int i = 0; i < nel; i++) 
      {
        ElementId ei(VOL,i);
        order_inner[i] = order_inner[i] + INT<3> (et_bonus_order[ma->GetElType(ei)]);
        order_inner[i] = Max(order_inner[i], INT<3>(0));
        if (!DefinedOn (VOL, ma->GetElIndex (ei)))
          order_inner[i] = 0;
      }
    if(print) 
      *testout << " order_inner (l2ho) " << order_inner << endl; 

    UpdateDofTables();
    while (ma->GetNLevels() > ndlevel.Size())
      ndlevel.Append (ndof);
    ndlevel.Last() = ndof;

    if(low_order_space) prol->Update();

    UpdateCouplingDofArray();
  } 
  
  void L2HighOrderFESpace :: UpdateCouplingDofArray()
  {
    ctofdof.SetSize(ndof);
    ctofdof = WIREBASKET_DOF;

    if (!all_dofs_together)
      for (int i=0; i<ma->GetNE(); i++)
	{
          ElementId ei(VOL, i);
          if (!DefinedOn (VOL, ma->GetElIndex (ei)))
            {
              ctofdof[i] = UNUSED_DOF;
              continue;
            }

	  ctofdof[i] = LOCAL_DOF; //lowest order (constants)
	  int first = first_element_dof[i];
	  int next = first_element_dof[i+1];
	  for (int j = first; j < next; j++)
	    ctofdof[j] = LOCAL_DOF; //higher order
	}
    else
      for (int i=0; i<ma->GetNE(); i++)
	{
	  int first = first_element_dof[i];
	  int next = first_element_dof[i+1];
	  if (next > first)
	    ctofdof[first] = LOCAL_DOF;  //lowest order (constants)
	  for (int j = first+1; j < next; j++)
	    ctofdof[j] = LOCAL_DOF;
	}
  }

  void L2HighOrderFESpace :: UpdateDofTables()
  {
    ndof = all_dofs_together ? 0 : nel;
    first_element_dof.SetSize(nel+1);
    for (int i = 0; i < nel; i++)
      {
        ElementId ei(VOL, i);
	first_element_dof[i] = ndof;
	INT<3> pi = order_inner[i]; 
	switch (ma->GetElType(ei))
	  {
	  case ET_SEGM:
	    ndof += pi[0]+1;
	    break;
	  case ET_TRIG:
	    ndof += (pi[0]+1)*(pi[0]+2)/2 ;
	    break;
	  case ET_QUAD:
	    ndof += (pi[0]+1)*(pi[1]+1);
	    break;
	  case ET_TET:
	    ndof += (pi[0]+1)*(pi[0]+2)*(pi[0]+3)/6;
	    break;
	  case ET_PRISM:
	    ndof += (pi[0]+1)*(pi[0]+2)*(pi[2]+1)/2;
	    break;
	  case ET_PYRAMID:
	    ndof += 5 + 8*(pi[0]-1) + 2*(pi[0]-1)*(pi[0]-2) + (pi[0]-1)*(pi[0]-1) 
	      + (pi[0]-1)*(pi[0]-2)*(2*pi[0]-3)/6;
	    break;
	  case ET_HEX:
	    ndof += (pi[0]+1)*(pi[1]+1)*(pi[2]+1);
	    break;
          default:  // for the compiler
            break;
	  }
	if (!all_dofs_together)
	  ndof--; // subtract constant 
      }
    first_element_dof[nel] = ndof;
    
    if(print) 
      *testout << " first_element dof (l2hofe) " << first_element_dof << endl;  

    while (ma->GetNLevels() > ndlevel.Size())
      ndlevel.Append (ndof);
    ndlevel.Last() = ndof;

    prol->Update();
  }

  FiniteElement & L2HighOrderFESpace :: GetFE (ElementId ei, Allocator & alloc) const
  {
    Ngs_Element ngel = ma->GetElement(ei);
    ELEMENT_TYPE eltype = ngel.GetType();

    if (ei.IsVolume())
      {
        int elnr = ei.Nr();
        
        if (!DefinedOn (ngel))
          {
            /*
            switch (eltype)
              {
              case ET_POINT:   return * new (alloc) ScalarDummyFE<ET_POINT> (); break;
              case ET_SEGM:    return * new (alloc) ScalarDummyFE<ET_SEGM> (); break;
              case ET_TRIG:    return * new (alloc) ScalarDummyFE<ET_TRIG> (); break;
              case ET_QUAD:    return * new (alloc) ScalarDummyFE<ET_QUAD> (); break;
              case ET_TET:     return * new (alloc) ScalarDummyFE<ET_TET> (); break;
              case ET_PYRAMID: return * new (alloc) ScalarDummyFE<ET_PYRAMID> (); break;
              case ET_PRISM:   return * new (alloc) ScalarDummyFE<ET_PRISM> (); break;
              case ET_HEX:     return * new (alloc) ScalarDummyFE<ET_HEX> (); break;
              }
            */
            return SwitchET(eltype,
                            [&alloc] (auto et) -> FiniteElement&
                            { return *new(alloc) ScalarDummyFE<et.ElementType()>(); });
          }

	if (eltype == ET_TRIG) 
          return *CreateL2HighOrderFE<ET_TRIG> (order, INT<3>(ngel.Vertices()), alloc);

        if (eltype == ET_TET)         
          return *CreateL2HighOrderFE<ET_TET> (order, INT<4>(ngel.Vertices()), alloc);

        /*
        return SwitchET(eltype,
                        [this,elnr,&alloc] (auto et) -> FiniteElement&
                        {
                        // return T_GetFE<et.ElementType()>(elnr, alloc);
                          // Ngs_Element ngel = ma->GetElement<ET_trait<ET>::DIM,VOL>(elnr);
                          auto * hofe =  new (alloc) L2HighOrderFE<et.ElementType()> ();
                          hofe -> SetVertexNumbers (ngel.vertices);
                          hofe -> L2HighOrderFE<et.ElementType()>::SetOrder (order_inner[ngel.Nr()]);
                          hofe -> L2HighOrderFE<et.ElementType()>::ComputeNDof();
                          return *hofe;
                        });
        */

        switch (eltype)
          {
          case ET_SEGM:    return T_GetFE<ET_SEGM> (elnr, alloc);

          case ET_TRIG:    return T_GetFE<ET_TRIG> (elnr, alloc);
          case ET_QUAD:    return T_GetFE<ET_QUAD> (elnr, alloc);
            
          case ET_TET:     return T_GetFE<ET_TET> (elnr, alloc);
          case ET_PRISM:   return T_GetFE<ET_PRISM> (elnr, alloc);
          case ET_PYRAMID: return T_GetFE<ET_PYRAMID> (elnr, alloc);
          case ET_HEX:     return T_GetFE<ET_HEX> (elnr, alloc);
            
          default:
            throw Exception ("illegal element in L2HoFeSpace::GetFE");
          }
      }
    else
      {
        try
          {
            return SwitchET<ET_POINT,ET_SEGM,ET_TRIG,ET_QUAD>
              (eltype,
               [&alloc] (auto et) -> FiniteElement&
               { return * new (alloc) DummyFE<et.ElementType()>; });
          }
        catch (Exception e)
          {
            throw Exception("illegal element type in L2::GetSurfaceFE");
          }
        /*
        switch (eltype)
          {
          case ET_POINT: return *new (alloc) DummyFE<ET_POINT>; 
          case ET_SEGM:  return *new (alloc) DummyFE<ET_SEGM>;
          case ET_TRIG:  return *new (alloc) DummyFE<ET_TRIG>;
          case ET_QUAD:  return *new (alloc) DummyFE<ET_QUAD>;
            
          default:
            stringstream str;
            str << "FESpace " << GetClassName() 
                << ", undefined surface eltype " << ma->GetElType(ei) 
                << ", order = " << order << endl;
            throw Exception (str.str());
          }
        */
      }
  }

  // const FiniteElement & L2HighOrderFESpace :: GetFE (int elnr, LocalHeap & lh) const
  // {
  //   try
  //     { 
  //       Ngs_Element ngel = ma->GetElement(elnr);
  //       ELEMENT_TYPE eltype = ngel.GetType();
        
  //       if (!DefinedOn (ngel.GetIndex()))
  //         {
  //           switch (eltype)
  //             {
  //             case ET_POINT:   return * new (lh) ScalarDummyFE<ET_POINT> (); break;
  //             case ET_SEGM:    return * new (lh) ScalarDummyFE<ET_SEGM> (); break;
  //             case ET_TRIG:    return * new (lh) ScalarDummyFE<ET_TRIG> (); break;
  //             case ET_QUAD:    return * new (lh) ScalarDummyFE<ET_QUAD> (); break;
  //             case ET_TET:     return * new (lh) ScalarDummyFE<ET_TET> (); break;
  //             case ET_PYRAMID: return * new (lh) ScalarDummyFE<ET_PYRAMID> (); break;
  //             case ET_PRISM:   return * new (lh) ScalarDummyFE<ET_PRISM> (); break;
  //             case ET_HEX:     return * new (lh) ScalarDummyFE<ET_HEX> (); break;
  //             }
  //         }

  //       if (ngel.GetType() == ET_TRIG) 
  //         {
  //           int ia[3];
  //           FlatArray<int> vnums(3, &ia[0]);
  //           vnums = ngel.Vertices();
  //           return *CreateL2HighOrderFE<ET_TRIG> (order, vnums, lh);
  //         }

  //       if (eltype == ET_TET)         
  //         return *CreateL2HighOrderFE<ET_TET> (order, INT<4>(ngel.Vertices()), lh);

  //       switch (eltype)
  //         {
  //         case ET_SEGM:    return T_GetFE<ET_SEGM> (elnr, lh);

  //         case ET_TRIG:    return T_GetFE<ET_TRIG> (elnr, lh);
  //         case ET_QUAD:    return T_GetFE<ET_QUAD> (elnr, lh);
            
  //         case ET_TET:     return T_GetFE<ET_TET> (elnr, lh);
  //         case ET_PRISM:   return T_GetFE<ET_PRISM> (elnr, lh);
  //         case ET_PYRAMID: return T_GetFE<ET_PYRAMID> (elnr, lh);
  //         case ET_HEX:     return T_GetFE<ET_HEX> (elnr, lh);
            
  //         default:
  //           throw Exception ("illegal element in L2HoFeSpace::GetFE");
  //         }
  //     } 
  //   catch (Exception & e)
  //     {
  //       e.Append ("in L2HoFESpace::GetElement");
  //       e.Append ("\n");
  //       throw; 
  //     }
  // }

  
  template <ELEMENT_TYPE ET>
  FiniteElement & L2HighOrderFESpace :: T_GetFE (int elnr, Allocator & lh) const
  {
    Ngs_Element ngel = ma->GetElement<ET_trait<ET>::DIM,VOL>(elnr);
    L2HighOrderFE<ET> * hofe =  new (lh) L2HighOrderFE<ET> ();

    hofe -> SetVertexNumbers (ngel.vertices);
    hofe -> L2HighOrderFE<ET>::SetOrder (order_inner[elnr]);
    hofe -> L2HighOrderFE<ET>::ComputeNDof();

    return *hofe;
  }




  const FiniteElement & L2HighOrderFESpace :: GetFacetFE (int fnr, LocalHeap & lh) const
  {
    DGFiniteElement<2> * fe2d = NULL;

    ArrayMem<int,4> vnums;
    ma->GetFacetPNums (fnr, vnums);

    switch (vnums.Size())
      {
      case 1: return *new (lh) L2HighOrderFE<ET_POINT> (0); 
      case 2: return *CreateL2HighOrderFE<ET_SEGM> (order, vnums, lh);
      case 3: fe2d = new (lh) L2HighOrderFE<ET_TRIG> (); break;
      case 4: fe2d = new (lh) L2HighOrderFE<ET_QUAD> (); break;
      default:
	{
	  stringstream str;
	  str << "L2HighOrderFESpace " << GetClassName() 
	      << ", undefined facet-eltype" << endl;
	  throw Exception (str.str());
	}
      }
    
    fe2d-> SetVertexNumbers (vnums); 
    fe2d-> SetOrder(order);
    fe2d-> ComputeNDof(); 
    return *fe2d;
  } 







 
  // const FiniteElement & L2HighOrderFESpace :: GetSFE (int elnr, LocalHeap & lh) const
  // {
  //   switch (ma->GetSElType(elnr))
  //     {
  //     case ET_POINT: return *new (lh) DummyFE<ET_POINT>; 
  //     case ET_SEGM:  return *new (lh) DummyFE<ET_SEGM>; break;
  //     case ET_TRIG:  return *new (lh) DummyFE<ET_TRIG>; break;
  //     case ET_QUAD:  return *new (lh) DummyFE<ET_QUAD>; break;

  //     default:
  //       stringstream str;
  //       str << "FESpace " << GetClassName() 
  //           << ", undefined surface eltype " << ma->GetSElType(elnr) 
  //           << ", order = " << order << endl;
  //       throw Exception (str.str());
  //     }
  // }

  size_t L2HighOrderFESpace :: GetNDof () const throw()
  {
    return ndof;
  }

  size_t L2HighOrderFESpace :: GetNDofLevel (int level) const
  {
    return ndlevel[level];
  }


  void L2HighOrderFESpace :: GetDofRanges (ElementId ei, Array<IntRange> & dranges) const
  {
    dranges.SetSize(0);

    if (!ei.IsVolume()) return;
    if (!DefinedOn (VOL, ma->GetElIndex (ei))) return;
    
    if (!all_dofs_together)
      dranges.Append (IntRange (ei.Nr(), ei.Nr()+1));
    dranges.Append (GetElementDofs(ei.Nr()));
  }


  void L2HighOrderFESpace :: GetDofNrs (ElementId ei, Array<int> & dnums) const
  {
    dnums.SetSize0();
    if (!DefinedOn (ei) || ei.VB() != VOL) return;

    auto eldofs = GetElementDofs(ei.Nr());
    size_t size = eldofs.Size();
    size_t base = all_dofs_together ? 0 : 1;
    size += base;
    dnums.SetSize(size);
    if (!all_dofs_together) dnums[0] = ei.Nr();
    dnums.Range(base, size) = eldofs;
    /*
    if (!all_dofs_together)
      dnums.Append (elnr); // lowest_order 
    dnums += GetElementDofs(elnr);
    */
  }
  
  shared_ptr<Table<int>> L2HighOrderFESpace :: 
  CreateSmoothingBlocks (const Flags & precflags) const
  {
    int i, j, first;
    Array<int> cnt(nel);
    cnt = 0;
    for (i = 0; i < nel; i++)
      cnt[i] = first_element_dof[i+1]-first_element_dof[i];
	
    Table<int> table(cnt);
    
    for (i = 0; i < nel; i++)
      {
	first = first_element_dof[i];
	for (j = 0; j < cnt[i]; j++)
	  table[i][j] = first+j;
      }
    return make_shared<Table<int>> (table);
  }

  void  L2HighOrderFESpace :: GetVertexDofNrs (int vnr, Array<int> & dnums) const
  { 
    dnums.SetSize0();
  }
  
  void  L2HighOrderFESpace ::GetEdgeDofNrs (int ednr, Array<int> & dnums) const
  { 
    dnums.SetSize0(); 
  }
  
  void  L2HighOrderFESpace ::GetFaceDofNrs (int fanr, Array<int> & dnums) const
  { 
    dnums.SetSize0(); 
  }
  
  void  L2HighOrderFESpace ::GetInnerDofNrs (int elnr, Array<int> & dnums) const
  { 
    GetDofNrs (elnr, dnums); 
  }

  void L2HighOrderFESpace :: SolveM (CoefficientFunction * rho, BaseVector & vec,
                                     LocalHeap & lh) const
  {
    static Timer t("SolveM"); RegionTimer reg(t);
    
    IterateElements (*this, VOL, lh,
                     [&rho, &vec,this] (FESpace::Element el, LocalHeap & lh)
                     {
                       auto & fel = static_cast<const BaseScalarFiniteElement&>(el.GetFE());
                       const ElementTransformation & trafo = el.GetTrafo();
                       
                       Array<int> dnums(fel.GetNDof(), lh);
                       GetDofNrs (el.Nr(), dnums);
                       
                       FlatVector<double> elx(fel.GetNDof()*dimension, lh);
                       vec.GetIndirect(dnums, elx);
		       auto melx = elx.AsMatrix(fel.GetNDof(),dimension);

                       FlatVector<double> diag_mass(fel.GetNDof(), lh);
                       fel.GetDiagMassMatrix (diag_mass);

                       bool curved = trafo.IsCurvedElement();
                       if (rho && !rho->ElementwiseConstant()) curved = true;
                       
                       if (!curved)
                         {
                           IntegrationRule ir(fel.ElementType(), 0);
                           BaseMappedIntegrationRule & mir = trafo(ir, lh);
                           double jac = mir[0].GetMeasure();
                           if (rho) jac *= rho->Evaluate(mir[0]);
                           diag_mass *= jac;
                           for (int i = 0; i < melx.Height(); i++)
                             melx.Row(i) /= diag_mass(i);
                         }
                       else
                         {
                           SIMD_IntegrationRule ir(fel.ElementType(), 2*fel.Order());
                           auto & mir = trafo(ir, lh);
                           FlatVector<SIMD<double>> pntvals(ir.Size(), lh);
                           FlatMatrix<SIMD<double>> rhovals(1, ir.Size(), lh);
                           if (rho) rho->Evaluate (mir, rhovals);
                           
                           for (int i = 0; i < melx.Height(); i++)
                             melx.Row(i) /= diag_mass(i);
                           for (int comp = 0; comp < dimension; comp++)
                             {
                               fel.Evaluate (ir, melx.Col(comp), pntvals);
                               if (rho)
                                 for (size_t i = 0; i < ir.Size(); i++)
                                   pntvals(i) *= ir[i].Weight() / (mir[i].GetMeasure() * rhovals(0,i));
                               else
                                 for (size_t i = 0; i < ir.Size(); i++)
                                   pntvals(i) *= ir[i].Weight() / mir[i].GetMeasure();
                               
                               melx.Col(comp) = 0.0;
                               fel.AddTrans (ir, pntvals, melx.Col(comp));
                             }
                           for (int i = 0; i < melx.Height(); i++)
                             melx.Row(i) /= diag_mass(i);
                         }
                       vec.SetIndirect(dnums, elx);
                     });
  }
  





  template <int D, typename FEL = ScalarFiniteElement<D-1> >
  class DiffOpSurfaceGradient : public DiffOp<DiffOpSurfaceGradient<D, FEL> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = D };
    enum { DIM_ELEMENT = D-1 };
    enum { DIM_DMAT = D };
    enum { DIFFORDER = 1 };

    ///
    template <typename AFEL, typename MIP, typename MAT>
    static void GenerateMatrix (const AFEL & fel, const MIP & mip,
				MAT & mat, LocalHeap & lh)
    {
      mat = Trans (mip.GetJacobianInverse ()) * 
	Trans (static_cast<const FEL&>(fel).GetDShape(mip.IP(),lh));
    }

    static void GenerateMatrixSIMDIR (const FiniteElement & fel,
                                      const SIMD_BaseMappedIntegrationRule & mir,
                                      BareSliceMatrix<SIMD<double>> mat)
    {
      static_cast<const FEL&>(fel).CalcMappedDShape (mir, mat);      
    }
  };


  L2SurfaceHighOrderFESpace ::  
  L2SurfaceHighOrderFESpace (shared_ptr<MeshAccess> ama, const Flags & flags, bool parseflags)
    : FESpace (ama, flags)
  {
    type = "l2surf";
    name="L2SurfaceHighOrderFESpace(l2surf)";
    // defined flags 
    DefineDefineFlag("l2surf");

    if(parseflags) CheckFlags(flags);
    
    
    if(flags.NumFlagDefined("relorder"))
      throw Exception("Variable order not implemented for L2SurfaceHighOrderFESpace"); 
    
    segm = new L2HighOrderFE<ET_SEGM> (order);
    trig = new L2HighOrderFE<ET_TRIG> (order);
    quad = new L2HighOrderFE<ET_QUAD> (order);

    if (ma->GetDimension() == 2)
      {
        integrator[BND] = 
          make_shared<RobinIntegrator<2>>(make_shared<ConstantCoefficientFunction>(1));
        evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<2>>>();
        evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<2>>>(); // for dimension
      }
    else
      {
	integrator[BND] = 
          make_shared<RobinIntegrator<3>> (make_shared<ConstantCoefficientFunction>(1));
        evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<3>>>();
        evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<3>>>(); // for dimension
	flux_evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpSurfaceGradient<3>>>();
          
      }

    if (dimension > 1)
    {
      integrator[BND] = make_shared<BlockBilinearFormIntegrator> (integrator[BND], dimension);
      for (auto vb : { VOL,BND, BBND, BBBND })
      {
        if (evaluator[vb])
          evaluator[vb] = make_shared<BlockDifferentialOperator> (evaluator[vb], dimension);
        if (flux_evaluator[vb])
          flux_evaluator[vb] = make_shared<BlockDifferentialOperator> (flux_evaluator[vb], dimension);            
      }
    }
  }

  L2SurfaceHighOrderFESpace :: ~L2SurfaceHighOrderFESpace ()
  {
    ;
  }

  shared_ptr<FESpace> L2SurfaceHighOrderFESpace :: 
  Create (shared_ptr<MeshAccess> ma, const Flags & flags)
  {
    return make_shared<L2SurfaceHighOrderFESpace> (ma, flags, true);
  }

  void L2SurfaceHighOrderFESpace :: Update(LocalHeap & lh)
  {
    nel = ma->GetNSE();

    ndof = 0;
    first_element_dof.SetSize(nel+1);
    for (int i = 0; i < nel; i++)
      {
        ElementId sei(BND, i);
	first_element_dof[i] = ndof;
	switch (ma->GetElType(sei))
	  {
	  case ET_SEGM:
	    ndof += order+1;
	    break;
	  case ET_TRIG:
	    ndof += (order+1)*(order+2)/2;
	    break;
	  case ET_QUAD:
	    ndof += (order+1)*(order+1);
	    break;
	  default:
	    ;
	  }
      }
    first_element_dof[nel] = ndof;
  }

  // const FiniteElement & L2SurfaceHighOrderFESpace :: GetSFE (int elnr, LocalHeap & lh) const
  // {
  //   if (ma->GetDimension() == 2)
  //     {
  //       DGFiniteElement<1> * fe1d = 0;
	
  //       Ngs_Element ngel = ma->GetElement<1,BND> (elnr);

  //       switch (ngel.GetType())
  //         {
  //         case ET_SEGM: fe1d = new (lh) L2HighOrderFE<ET_SEGM> (); break;
  //         default:
  //           ;
  //         }

  //       fe1d -> SetVertexNumbers (ngel.vertices);
  //       fe1d -> SetOrder (INT<1> (order));
  //       fe1d -> ComputeNDof(); 
  //       return *fe1d;
  //     }
  //   else
  //     {
  //       DGFiniteElement<2> * fe2d = 0;
	
  //       Ngs_Element ngel = ma->GetElement<2,BND> (elnr);
	
  //       switch (ngel.GetType())
  //         {
  //         case ET_TRIG: fe2d = new (lh) L2HighOrderFE<ET_TRIG> (); break;
  //         case ET_QUAD: fe2d = new (lh) L2HighOrderFE<ET_QUAD> (); break;
  //         default:
  //           ;
  //         }
	
  //       fe2d -> SetVertexNumbers (ngel.vertices);
  //       fe2d -> SetOrder (INT<2> (order));
  //       fe2d -> ComputeNDof(); 
  //       return *fe2d;
  //     }
  //       /*

  //   FiniteElement * fe = 0;
    
  //   switch (ma->GetSElType(elnr))
  //     {
  //     case ET_SEGM:
  //       fe = segm; break;
  //     case ET_TRIG:
  //       fe = trig; break;
  //     case ET_QUAD:
  //       fe = quad; break;
  //     default:
  //       fe = 0;
  //     }
    
  //   ArrayMem<int,12> vnums; // calls GetElPNums -> max 12 for PRISM12
  //   ma->GetSElVertices(elnr, vnums);

  //   if (!fe)
  //     {
  //       stringstream str;
  //       str << "L2SurfaceHighOrderFESpace " << GetClassName() 
  //           << ", undefined eltype " 
  //           << ElementTopology::GetElementName(ma->GetSElType(elnr))
  //           << ", order = " << order << endl;
  //       throw Exception (str.str());
  //     }

  //   dynamic_cast<L2HighOrderFiniteElement<2>*> (fe) -> SetVertexNumbers (vnums);

  //   return *fe;
  //       */
  // }

  FiniteElement & L2SurfaceHighOrderFESpace :: GetFE (ElementId ei, Allocator & lh) const
  {
    if (ei.VB() == BND)
      {
        if (ma->GetDimension() == 2)
          {
            DGFiniteElement<1> * fe1d = 0;
	
            Ngs_Element ngel = ma->GetElement<1,BND> (ei.Nr());

            switch (ngel.GetType())
              {
              case ET_SEGM: fe1d = new (lh) L2HighOrderFE<ET_SEGM> (); break;
              default:
                ;
              }

            fe1d -> SetVertexNumbers (ngel.vertices);
            fe1d -> SetOrder (INT<1> (order));
            fe1d -> ComputeNDof(); 
            return *fe1d;
          }
        else
          {
            DGFiniteElement<2> * fe2d = 0;
	
            Ngs_Element ngel = ma->GetElement<2,BND> (ei.Nr());
	
            switch (ngel.GetType())
              {
              case ET_TRIG: fe2d = new (lh) L2HighOrderFE<ET_TRIG> (); break;
              case ET_QUAD: fe2d = new (lh) L2HighOrderFE<ET_QUAD> (); break;
              default:
                ;
              }
	
            fe2d -> SetVertexNumbers (ngel.vertices);
            fe2d -> SetOrder (INT<2> (order));
            fe2d -> ComputeNDof(); 
            return *fe2d;
          }
      }
    
    else

      return * SwitchET (ma->GetElement(ei).GetType(),
                         [&lh] (auto et) -> FiniteElement*
                         {
                           return new (lh) ScalarDummyFE<et.ElementType()>();
                         });
    
  }
  // const FiniteElement & L2SurfaceHighOrderFESpace :: GetFE (int elnr, LocalHeap & lh) const
  // {
  //   throw Exception ("Volume elements not available for L2SurfaceHighOrderFESpace");
  // }
 
  size_t L2SurfaceHighOrderFESpace :: GetNDof () const throw()
  {
    return ndof;
  }

  void L2SurfaceHighOrderFESpace :: 
  GetDofNrs (ElementId ei, Array<int> & dnums) const
  {
    dnums.SetSize (0);
    if(ei.VB()!=BND) return;
    int first = first_element_dof[ei.Nr()];
    int neldofs = first_element_dof[ei.Nr()+1] - first;
    for (int j = 0; j < neldofs; j++)
      dnums.Append (first+j);

    if (!DefinedOn (ei))
      dnums = -1;
    
  }
  
  shared_ptr<Table<int>> L2SurfaceHighOrderFESpace :: 
  // CreateSmoothingBlocks ( int type) const
  CreateSmoothingBlocks (const Flags & precflags) const    
  {
    int i, j, first;
    Array<int> cnt(nel);
    cnt = 0;
    for (i = 0; i < nel; i++)
      cnt[i] = first_element_dof[i+1]-first_element_dof[i];
	
    Table<int> table(cnt);
    
    for (i = 0; i < nel; i++)
      {
	first = first_element_dof[i];
	for (j = 0; j < cnt[i]; j++)
	  table[i][j] = first+j;
      }
    return make_shared<Table<int>> (table);
  }


  void  L2SurfaceHighOrderFESpace :: GetVertexDofNrs (int vnr, Array<int> & dnums) const
  { dnums.SetSize(0); return; }
  
  void  L2SurfaceHighOrderFESpace ::GetEdgeDofNrs (int ednr, Array<int> & dnums) const
  { dnums.SetSize(0); return; }
  
  void  L2SurfaceHighOrderFESpace ::GetFaceDofNrs (int fanr, Array<int> & dnums) const
  { GetDofNrs ( fanr, dnums ); return; }
  
  void  L2SurfaceHighOrderFESpace ::GetInnerDofNrs (int elnr, Array<int> & dnums) const
  { GetDofNrs ( elnr, dnums ); return; }
    




  

  template <int DIM_SPC, VorB VB = VOL>
  class DiffOpIdVectorL2Piola : public DiffOp<DiffOpIdVectorL2Piola<DIM_SPC> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = DIM_SPC };
    enum { DIM_ELEMENT = DIM_SPC-VB };
    enum { DIM_DMAT = DIM_SPC };
    enum { DIFFORDER = 0 };

    template <typename FEL, typename MIP, typename MAT>
    static void GenerateMatrix (const FEL & bfel, const MIP & mip,
                                MAT & mat, LocalHeap & lh)
    {
      auto & fel = static_cast<const CompoundFiniteElement&> (bfel);
      mat = 0.0;
      auto & feli = static_cast<const BaseScalarFiniteElement&> (fel[0]);
      for (int i = 0; i < DIM_SPC; i++)
        feli.CalcShape (mip.IP(), mat.Row(i).Range(fel.GetRange(i)));
      Mat<DIM_SPACE> trafo = (1.0/mip.GetJacobiDet()) * mip.GetJacobian();
      for (int i = 0; i < DIM_SPACE*feli.GetNDof(); i++)
        {
          Vec<DIM_SPACE> hv = mat.Col(i);
          mat.Col(i) = trafo * hv;
        } 
    }
    /*
    static void GenerateMatrixSIMDIR (const FiniteElement & bfel,
                                      const SIMD_BaseMappedIntegrationRule & mir,
                                      BareSliceMatrix<SIMD<double>> mat)
    {
      auto & fel = static_cast<const CompoundFiniteElement&> (bfel);
      mat.AddSize(DIM_SPC*bfel.GetNDof(), mir.Size()) = 0.0;
      for (int i = 0; i < DIM_SPC; i++)
        {
          auto & feli = static_cast<const BaseScalarFiniteElement&> (fel[i]);
          feli.CalcShape (mir.IR(), mat.Rows(DIM_SPC*fel.GetRange(i)).RowSlice(i, DIM_SPC));
        }
    }

    using DiffOp<DiffOpIdVectorH1<DIM_SPC>>::ApplySIMDIR;    
    static void ApplySIMDIR (const FiniteElement & bfel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<double> x, BareSliceMatrix<SIMD<double>> y)
    {
      auto & fel = static_cast<const CompoundFiniteElement&> (bfel);
      for (int i = 0; i < DIM_SPC; i++)
        {
          auto & feli = static_cast<const BaseScalarFiniteElement&> (fel[i]);
          feli.Evaluate (mir.IR(), x.Range(fel.GetRange(i)), y.Row(i));
        }
    }

    using DiffOp<DiffOpIdVectorH1<DIM_SPC>>::AddTransSIMDIR;        
    static void AddTransSIMDIR (const FiniteElement & bfel, const SIMD_BaseMappedIntegrationRule & mir,
                                BareSliceMatrix<SIMD<double>> y, BareSliceVector<double> x)
    {
      auto & fel = static_cast<const CompoundFiniteElement&> (bfel);
      for (int i = 0; i < DIM_SPC; i++)
        {
          auto & feli = static_cast<const BaseScalarFiniteElement&> (fel[i]);
          feli.AddTrans (mir.IR(), y.Row(i), x.Range(fel.GetRange(i)));
        }
    }    
    */
  };




  VectorL2FESpace::VectorL2FESpace (shared_ptr<MeshAccess> ama, const Flags & flags, 
                     bool checkflags)
      : CompoundFESpace(ama, flags)
    {
      type = "VectorL2";
      Flags compflags = flags;
      compflags.SetFlag("all_dofs_together");
      for (int i = 0; i <  ma->GetDimension(); i++)
        AddSpace (make_shared<L2HighOrderFESpace> (ama, compflags));

      piola = flags.GetDefineFlag ("piola");
      covariant = flags.GetDefineFlag ("covariant");

      if (piola)
        {
          switch (ma->GetDimension())
            {
            case 2:
              evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpIdVectorL2Piola<2>>>();
              break;
            case 3:
              evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpIdVectorL2Piola<3>>>();
              break;
            }
        }
      else
        switch (ma->GetDimension())
          {
          case 2:
            evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpIdVectorH1<2>>>();
            flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpGradVectorH1<2>>>();
            additional_evaluators.Set ("div", make_shared<T_DifferentialOperator<DiffOpDivVectorH1<2>>> ());
            break;
          case 3:
            evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpIdVectorH1<3>>>();
            flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpGradVectorH1<3>>>();
            additional_evaluators.Set ("div", make_shared<T_DifferentialOperator<DiffOpDivVectorH1<3>>> ());
            break;
          }
    }


  void VectorL2FESpace :: GetDofNrs (ElementId ei, Array<int> & dnums) const
  {
    dnums.SetSize0();
    if (!DefinedOn (ei) || ei.VB() != VOL) return;
    auto & fes1 = static_cast<const L2HighOrderFESpace&> (*spaces[0]);
    auto eldofs = fes1.GetElementDofs(ei.Nr());
    size_t ndof1 = fes1.GetNDof();
    size_t locndof1 = eldofs.Size();
    dnums.SetSize (locndof1 * spaces.Size());
    for (size_t j = 0; j < spaces.Size(); j++)
      dnums.Range(j*locndof1, (j+1)*locndof1) = eldofs + j*ndof1;
    /*
    size_t size = eldofs.Size();
    size_t base = all_dofs_together ? 0 : 1;
    size += base;
    dnums.SetSize(size);
    if (!all_dofs_together) dnums[0] = ei.Nr();
    dnums.Range(base, size) = eldofs;
    */
  }



  
  void VectorL2FESpace :: SolveM (CoefficientFunction * rho, BaseVector & vec,
                                  LocalHeap & lh) const
  {
    if (piola)
      {
        switch (ma->GetDimension())
          {
          case 1: SolveMPiola<1>(rho, vec, lh); break;
          case 2: SolveMPiola<2>(rho, vec, lh); break;
          case 3: SolveMPiola<3>(rho, vec, lh); break;
          default: throw Exception("VectorL2FESpace::SolveM: illegal dimension");
          }
        return;
      }
    
    for (size_t i = 0; i < spaces.Size(); i++)
      {
        auto veci = vec.Range (GetRange(i));
        spaces[i] -> SolveM (rho, veci, lh);
      }
  }





  template <int DIM>
  void VectorL2FESpace ::
  SolveMPiola (CoefficientFunction * rho, BaseVector & vec,
               LocalHeap & lh) const
  {
    static Timer t("SolveM - Piola"); RegionTimer reg(t);
        
    IterateElements
      (*this, VOL, lh,
       [&rho, &vec,this] (FESpace::Element el, LocalHeap & lh)
       {
         auto & fel = static_cast<const CompoundFiniteElement&>(el.GetFE());
         auto & feli = static_cast<const BaseScalarFiniteElement&>(fel[0]);
         const ElementTransformation & trafo = el.GetTrafo();
         
         Array<int> dnums(fel.GetNDof(), lh);
         GetDofNrs (el.Nr(), dnums);
         
         FlatVector<double> elx(feli.GetNDof()*DIM, lh);
         vec.GetIndirect(dnums, elx);
         auto melx = elx.AsMatrix(DIM, feli.GetNDof());
         
         FlatVector<double> diag_mass(feli.GetNDof(), lh);
         feli.GetDiagMassMatrix (diag_mass);
         
         bool curved = trafo.IsCurvedElement();
         if (rho && !rho->ElementwiseConstant()) curved = true;
         curved = false;  // curved not implemented
         
         if (!curved)
           {
             IntegrationRule ir(fel.ElementType(), 0);
             BaseMappedIntegrationRule & mir = trafo(ir, lh);
             Mat<DIM> trans = (1/mir[0].GetMeasure()) * Trans(mir[0].GetJacobian()) * mir[0].GetJacobian();
             Mat<DIM> invtrans = Inv(trans);
             
             // double jac = mir[0].GetMeasure();
             // if (rho) jac *= rho->Evaluate(mir[0]);
             // diag_mass *= jac;
             
             for (int i = 0; i < melx.Width(); i++)
               {
                 Vec<DIM> hv = melx.Col(i);
                 hv /=  diag_mass(i);
                 melx.Col(i) = invtrans * hv;
               }
           }
         /*
           else
           {
           SIMD_IntegrationRule ir(fel.ElementType(), 2*fel.Order());
           auto & mir = trafo(ir, lh);
           FlatVector<SIMD<double>> pntvals(ir.Size(), lh);
           FlatMatrix<SIMD<double>> rhovals(1, ir.Size(), lh);
           if (rho) rho->Evaluate (mir, rhovals);
                 
           for (int i = 0; i < melx.Height(); i++)
           melx.Row(i) /= diag_mass(i);
           for (int comp = 0; comp < dimension; comp++)
           {
           fel.Evaluate (ir, melx.Col(comp), pntvals);
           if (rho)
           for (size_t i = 0; i < ir.Size(); i++)
           pntvals(i) *= ir[i].Weight() / (mir[i].GetMeasure() * rhovals(0,i));
           else
           for (size_t i = 0; i < ir.Size(); i++)
           pntvals(i) *= ir[i].Weight() / mir[i].GetMeasure();
           
           melx.Col(comp) = 0.0;
           fel.AddTrans (ir, pntvals, melx.Col(comp));
           }
           for (int i = 0; i < melx.Height(); i++)
           melx.Row(i) /= diag_mass(i);
           }
         */
         vec.SetIndirect(dnums, elx);
       });
  }


  
  static RegisterFESpace<VectorL2FESpace> initvecl2 ("VectorL2");
  


  // register FESpaces
  namespace l2hofespace_cpp
  {
    class Init
    { 
    public: 
      Init ();
    };
    
    Init::Init()
    {
      GetFESpaceClasses().AddFESpace ("l2", L2HighOrderFESpace::Create);
      GetFESpaceClasses().AddFESpace ("l2ho", L2HighOrderFESpace::CreateHO);
      GetFESpaceClasses().AddFESpace ("l2surf", L2SurfaceHighOrderFESpace::Create);
    }
    
    Init init;
  }
}
