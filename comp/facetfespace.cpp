#include <comp.hpp>
#include <fem.hpp> 

#include "../fem/l2hofe.hpp"
#include "../fem/diffop_impl.hpp"
#include "../fem/facethofe.hpp"

namespace ngcomp
{ 

  /// Identity
  template <int D>
  class DiffOpIdFacet : public DiffOp<DiffOpIdFacet<D> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = D };
    enum { DIM_ELEMENT = D };
    enum { DIM_DMAT = 1 };
    enum { DIFFORDER = 0 };

    template <typename FEL, typename MIP, typename MAT>
    static void GenerateMatrix (const FEL & bfel, const MIP & mip,
                                MAT & mat, LocalHeap & lh)
    {
      int facetnr = mip.IP().FacetNr();
      if (facetnr >= 0)
        {
          mat = 0.0;
          const FacetVolumeFiniteElement<D> & fel_facet = static_cast<const FacetVolumeFiniteElement<D>&> (bfel);
          fel_facet.Facet(facetnr).CalcShape(mip.IP(), 
                                             mat.Row(0).Range(fel_facet.GetFacetDofs(facetnr)));
        }
      else
        {
          if (mip.BaseMappedIntegrationPoint::VB() == BND) 
            {
              const BaseScalarFiniteElement & fel = static_cast<const BaseScalarFiniteElement&> (bfel);
              fel.CalcShape (mip.IP(), mat.Row(0));
            }
          else
            throw Exception("cannot evaluate facet-fe inside element");
        }
    }

    /*
      // not ready yet ...
    static void GenerateMatrixSIMDIR (const FiniteElement & fel,
                                      const SIMD_BaseMappedIntegrationRule & mir,
                                      BareSliceMatrix<SIMD<double>> mat)
    {
      int facetnr = mip.IP().FacetNr();
      if (facetnr >= 0)
        {
          mat = 0.0;
          const FacetVolumeFiniteElement<D> & fel_facet = static_cast<const FacetVolumeFiniteElement<D>&> (bfel);
          fel_facet.Facet(facetnr).CalcShape(mip.IP(), 
                                             mat.Row(0).Range(fel_facet.GetFacetDofs(facetnr)));
        }
      else
        {
          if (mip.BaseMappedIntegrationPoint::VB() == BND) 
            {
              const BaseScalarFiniteElement & fel = static_cast<const BaseScalarFiniteElement&> (bfel);
              fel.CalcShape (mip.IP(), mat.Row(0));
            }
          else
            throw Exception("cannot evaluate facet-fe inside element");
        }
      // Cast(fel).CalcMappedDShape (mir, mat);      
    }
    */

    
    using DiffOp<DiffOpIdFacet<D>>::ApplySIMDIR;          
    static void ApplySIMDIR (const FiniteElement & bfel, const SIMD_BaseMappedIntegrationRule & mir,
                             BareSliceVector<double> x, ABareSliceMatrix<double> y)
    {
      // Cast(fel).Evaluate (mir.IR(), x, y.Row(0));

      const FacetVolumeFiniteElement<D> & fel_facet = static_cast<const FacetVolumeFiniteElement<D>&> (bfel);

      int facetnr = mir.IR()[0].FacetNr();
      if (facetnr < 0)
        throw Exception("cannot evaluate facet-fe inside element, apply simd");
      else
        fel_facet.Facet(facetnr).Evaluate(mir.IR(),
                                          x.Range(fel_facet.GetFacetDofs(facetnr)),
                                          y.Row(0));
    }

    using DiffOp<DiffOpIdFacet<D>>::AddTransSIMDIR;          
    static void AddTransSIMDIR (const FiniteElement & bfel, const SIMD_BaseMappedIntegrationRule & mir,
                                ABareSliceMatrix<double> y, BareSliceVector<double> x)
    {
      // Cast(fel).AddTrans (mir.IR(), y.Row(0), x);
      const FacetVolumeFiniteElement<D> & fel_facet = static_cast<const FacetVolumeFiniteElement<D>&> (bfel);

      int facetnr = mir.IR()[0].FacetNr();
      if (facetnr < 0)
        throw Exception("cannot evaluate facet-fe inside element, add trans simd");
      else
        fel_facet.Facet(facetnr).AddTrans(mir.IR(),
                                          y.Row(0),
                                          x.Range(fel_facet.GetFacetDofs(facetnr)));
    }
    
    
  }; 




  FacetFESpace ::  FacetFESpace (shared_ptr<MeshAccess> ama, const Flags & flags, bool checkflags)
    : FESpace(ama, flags)
  {
    name="FacetFESpace(facet)";
    // defined flags
    DefineNumFlag("relorder");
    DefineDefineFlag("variableorder"); 


    if(checkflags) CheckFlags(flags);
    
    
    ndlevel.SetSize(0);
    Flags loflags;
    loflags.SetFlag("order",0.0);
    if ( this->IsComplex() )
      loflags.SetFlag("complex");

    low_order_space = 0; // never used?
    all_dofs_together = flags.GetDefineFlag ("all_dofs_together");
		cout << "setting all_dofs_together " << all_dofs_together << endl;
    // Variable order space: 
    //      in case of (var_order && order) or (relorder) 
    var_order = flags.GetDefineFlag("variableorder");  
    order =  int (flags.GetNumFlag ("order",0)); 

    if(flags.NumFlagDefined("relorder") && !flags.NumFlagDefined("order")) 
      var_order = 1; 
    
    rel_order=int(flags.GetNumFlag("relorder",order-1)); 


    if(flags.NumFlagDefined("order") && flags.NumFlagDefined("relorder")) 
      {
	if(var_order)
	  cerr << " WARNING: FacetFESpace: inconsistent flags: variableorder, order and relorder "
	       << "-> variable order space with rel_order " << rel_order << "is used, but order is ignored " << endl; 
	else 
	  cerr << " WARNING: FacetFESpace: inconsistent flags: order and rel_order "
	       << "-> uniform order space with order " << order << " is used " << endl; 
      }

    if (flags.NumFlagDefined("order")) 
      { 
	if(var_order) 
	  { 
	    rel_order = int(flags.GetNumFlag("relorder",order-1)); 
	    order = rel_order + 1;
	  }
	else 
	  order =  int (flags.GetNumFlag ("order",0));
      }
    else if(flags.NumFlagDefined("relorder"))
      {
	var_order=1; 
	rel_order = int (flags.GetNumFlag ("relorder",-1));
	order=1+rel_order; 
      }
    else // neither rel_order nor order is given  
      {
	rel_order = -1;  
	order = 0;  
      }

    highest_order_dc = flags.GetDefineFlag("highest_order_dc");
    if (order == 0)
      highest_order_dc = false;
    
    nowirebasket = flags.GetDefineFlag ("nowirebasket");
    
    auto one = make_shared<ConstantCoefficientFunction>(1);
    if (ma->GetDimension() == 2)
      {
        evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpIdFacet<2>>>();
        evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<2>>>();
        integrator[BND] = make_shared<RobinIntegrator<2>> (one);
      }
    else
      {
        evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpIdFacet<3>>>();
	evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<3>>>();
        integrator[BND] = make_shared<RobinIntegrator<3>> (one);
      }

    if (dimension > 1)
      {
        integrator[VOL] = make_shared<BlockBilinearFormIntegrator> (integrator[VOL], dimension);
        integrator[BND] = make_shared<BlockBilinearFormIntegrator> (integrator[BND], dimension);
      }
  }
  

  FacetFESpace :: ~FacetFESpace ()
  { ; }


  void FacetFESpace :: Update(LocalHeap & lh)
  {
    FESpace :: Update (lh);
    
    if(print) 
      *testout << " FacetFEspace with order " << order << " rel_order " << rel_order << " var_order " << var_order << endl; 

    if (low_order_space)
      low_order_space -> Update(lh);

    nel = ma->GetNE();
    nfa = ma->GetNFacets(); 
    
    int p = 0; 
    if(!var_order) p = order; 
    
    order_facet.SetSize(nfa);
    order_facet = p;

    fine_facet.SetSize(nfa);
    fine_facet = 0; 
    
    Array<int> fanums;
        
    for (int i = 0; i < nel; i++)
      {
        ElementId ei(VOL, i);
	ELEMENT_TYPE eltype=ma->GetElType(ei); 
	const POINT3D * points = ElementTopology :: GetVertices (eltype);	
	
	if (ma->GetDimension() == 2)
	  {
            /*
	    ma->GetElEdges (ei, fanums);
	    for (int j=0;j<fanums.Size();j++) 
	      fine_facet[fanums[j]] = 1; 
            */
            auto fanums = ma->GetElEdges(ei);
            for (auto f : fanums)
              fine_facet[f] = true;
            
	    if(var_order)
	      {
                INT<3> el_orders = ma->GetElOrders(i); 

                const EDGE * edges = ElementTopology::GetEdges (eltype);
                for(int j=0; j<fanums.Size(); j++)
		  for(int k=0;k<2;k++)
		    if(points[edges[j][0]][k] != points[edges[j][1]][k])
		      { 
			order_facet[fanums[j]] = INT<2>(max2(el_orders[k]+rel_order, order_facet[fanums[j]][0]),0);
			break; 
		      }
	      }
	  }
	else
	  {
	    // Array<int> elfaces,vnums;
	    // ma->GetElFaces(ei,elfaces);
            auto elfaces = ma->GetElFaces(ei); 
	    for (int j=0;j<elfaces.Size();j++) fine_facet[elfaces[j]] = 1; 
	    
	    if(var_order) 
	      {
                INT<3> el_orders = ma->GetElOrders(i); 

		// ma->GetElVertices (i, vnums);
                auto vnums = ma->GetElVertices(ei);
		const FACE * faces = ElementTopology::GetFaces (eltype);
		for(int j=0;j<elfaces.Size();j++)
		  {
		    if(faces[j][3]==-1) // trig  
		      {
			order_facet[elfaces[j]][0] = max2(order_facet[elfaces[j]][0],el_orders[0]+rel_order);
			order_facet[elfaces[j]][1] = order_facet[elfaces[j]][0]; 
		      }
		    else //quad_face
		      {
			int fmax = 0;
			for(int k = 1; k < 4; k++) 
			  if(vnums[faces[j][k]] > vnums[faces[j][fmax]]) fmax = k;   
					
			INT<2> f((fmax+3)%4,(fmax+1)%4); 
			if(vnums[faces[j][f[1]]] > vnums[faces[j][f[0]]]) swap(f[0],f[1]);
			
			// fmax > f[0] > f[1]
			// p[0] for direction fmax,f[0] 
			// p[1] for direction fmax,f[1] 
			for(int l=0;l<2;l++)
			  for(int k=0;k<3;k++)
			    if(points[faces[j][fmax]][k] != points[faces[j][f[l] ]][k])
			      {
				order_facet[elfaces[j]][l] = max2(order_facet[elfaces[j]][l], rel_order + el_orders[k]);
				break; 
			      } 
			
		      }
		  }
	      }
	    
	  }
	
      }
    

    for (int i = 0; i < nfa; i++)
      if (!fine_facet[i]) order_facet[i] = 0;

    // distribute dofs
    ncfa = 0; 
    //ndof = nfa; // low_order space
    ndof = all_dofs_together ? 0 : nfa;
    
    first_facet_dof.SetSize(nfa+1); 
    first_facet_dof = nfa;


    if (ma->GetDimension() == 1)
      {
        for (int i = 0; i < nfa; i++)
          {
            first_facet_dof[i] = ndof;
            ndof ++;
          }
        first_facet_dof[nfa] = ndof;
      }
    else if(ma->GetDimension() == 2)
      {
        for(int i = 0; i < nfa; i++)
          {
            first_facet_dof[i] = ndof;
            if (!fine_facet[i]) continue;
            ndof += order_facet[i][0];
            if (all_dofs_together) ndof++;  // include lowest-order dof
            if(highest_order_dc && order_facet[i][0] > 0) ndof--;
          }
        first_facet_dof[nfa] = ndof;
        
        if(highest_order_dc)
          {
            int ne = ma->GetNE();
            first_inner_dof.SetSize(ne+1);
            for(int i = 0; i < ne; i++)
              {
                ElementId ei(VOL, i);
                first_inner_dof[i] = ndof;
                ELEMENT_TYPE eltype = ma->GetElType(ei);
                if(eltype == ET_TRIG)
                  ndof += 3;
                else
                  ndof += 4;
              }
            first_inner_dof[ne] = ndof;
          }
        
      } // 2D
    else  // 3D
      {
        int inci = 0;
        Array<int> pnums;
        for(int i=0; i< nfa; i++)
          {
            first_facet_dof[i] = ndof;
            if (!fine_facet[i]) continue;
            int p = order_facet[i][0];
            if(highest_order_dc  && order_facet[i][0] > 0) p--;
            ma->GetFacePNums(i,pnums);
            
            int n_lowest_order_dofs = all_dofs_together ? 0 : 1;
            switch(pnums.Size())
              {
              case 3: inci = ((p+1)*(p+2))/2 - n_lowest_order_dofs; break;
              case 4: inci= (p+1)*(p+1) - n_lowest_order_dofs; break;
              }
            ndof+= inci;
          }
        first_facet_dof[nfa] = ndof;
        
        if(highest_order_dc)
	  {
	    int ne = ma->GetNE();
	    first_inner_dof.SetSize(ne+1);
	    for (int i = 0; i < ne; i++)
	      {
                ElementId ei(VOL, i);
		first_inner_dof[i] = ndof;
		
		ELEMENT_TYPE eltype = ma->GetElType(ei);
		for (int k = 0; k < ElementTopology::GetNFacets(eltype); k++)
		  if (ElementTopology::GetFacetType(eltype, k) == ET_TRIG)
		    ndof += order+1;
		  else
		    ndof += 2*order+1;
		
		// ndof += 4*(order+1);
	      }
	    first_inner_dof[ne] = ndof;
	  }
      } // 3D
    

    while (ma->GetNLevels() > ndlevel.Size())
      ndlevel.Append (ndof);
    ndlevel.Last() = ndof;
      
    if(print)
      {
	*testout << "*** Update FAcetFESpace: General Information" << endl;
	*testout << " order facet (facet) " << order_facet << endl; 
	*testout << " first_facet_dof (facet)  " << first_facet_dof << endl; 
      } 

    UpdateCouplingDofArray();

    if (timing) Timing();
  }


  void FacetFESpace :: UpdateCouplingDofArray()
  {
    ctofdof.SetSize(ndof);
    ctofdof = INTERFACE_DOF;
    
    Array<bool> fine_facet(ma->GetNFacets());
    fine_facet = false;
    
    if(ma->GetDimension() == 3)
      for(ElementId ei : ma->Elements<VOL>())
				fine_facet[(*ma)[ei].Faces()] = true;
    else
      for(ElementId ei : ma->Elements<VOL>())
        fine_facet[(*ma)[ei].Edges()] = true;
    
    if (!AllDofsTogether())
      {
        for(int facet = 0; facet < ma->GetNFacets(); facet++)
          {
            if(fine_facet[facet])
              ctofdof[facet] = nowirebasket ? INTERFACE_DOF : WIREBASKET_DOF;
            else
              ctofdof[facet] = UNUSED_DOF;
            ctofdof[GetFacetDofs(facet)] = INTERFACE_DOF;
          }
      }
    else // AllDofsTogether
      {
        for(int facet = 0; facet < ma->GetNFacets(); facet++)
          {
            ctofdof[GetFacetDofs(facet)] = INTERFACE_DOF;
            if(fine_facet[facet]) // for first dof, overwrite INTERFACE_DOF with  WIREBASKET_DOF!
              ctofdof[first_facet_dof[facet]] = nowirebasket ? INTERFACE_DOF : WIREBASKET_DOF;
          }
        
      }
    
    if(highest_order_dc)
      ctofdof.Range(first_inner_dof[0],ndof) = LOCAL_DOF;
    
    if(print)
      *testout << "FacetFESpace, ctofdof = " << endl << ctofdof << endl;
  }


  template <ELEMENT_TYPE ET>
  FiniteElement & FacetFESpace :: T_GetFE (int elnr, Allocator & alloc) const
  {
    Ngs_Element ngel = ma->GetElement<ET_trait<ET>::DIM,VOL> (elnr);

    FacetFE<ET> * fe =  new (alloc) FacetFE<ET> ();
    fe -> SetVertexNumbers (ngel.Vertices());
    if (ET_trait<ET>::DIM >= 2)
      for (int i = 0; i < ET_trait<ET>::N_FACET; i++)
        fe -> SetOrder (i, order_facet[ngel.Facets()[i]][0]);
    fe -> ComputeNDof();
    
    return *fe;
  }

  
  // ------------------------------------------------------------------------
  FiniteElement & FacetFESpace :: GetFE (ElementId ei, Allocator  & lh) const
  {
    switch(ei.VB())
      {
      case VOL:
        {
          switch (ma->GetElType(ei))
            {
            case ET_POINT:   break;
            case ET_SEGM:    return T_GetFE<ET_SEGM>(ei.Nr(), lh);              
            case ET_TRIG:    return T_GetFE<ET_TRIG>(ei.Nr(), lh);
            case ET_QUAD:    return T_GetFE<ET_QUAD>(ei.Nr(), lh);
            case ET_TET:     return T_GetFE<ET_TET>(ei.Nr(), lh);
            case ET_PYRAMID: return T_GetFE<ET_PYRAMID>(ei.Nr(), lh);
            case ET_PRISM:   return T_GetFE<ET_PRISM>(ei.Nr(), lh);
            case ET_HEX:     return T_GetFE<ET_HEX>(ei.Nr(), lh);
            }

          throw Exception (string("FacetFESpace ") + GetClassName() 
                           + ", undefined eltype " 
                           + ElementTopology::GetElementName(ma->GetElType(ei)));
        } 
            
          /*
        FacetVolumeFiniteElement<1> * fe1d = NULL;
        FacetVolumeFiniteElement<2> * fe2d = NULL;
        FacetVolumeFiniteElement<3> * fe3d = NULL;;

        switch (ma->GetElType(ei))
          {
          case ET_POINT:   break;
          case ET_SEGM:    fe1d = new (lh) FacetFE<ET_SEGM> (); break;
          case ET_TRIG:    fe2d = new (lh) FacetFE<ET_TRIG> (); break;
          case ET_QUAD:    fe2d = new (lh) FacetFE<ET_QUAD> (); break;
          case ET_TET:     fe3d = new (lh) FacetFE<ET_TET> (); break;
          case ET_PYRAMID: fe3d = new (lh) FacetFE<ET_PYRAMID> (); break;
          case ET_PRISM:   fe3d = new (lh) FacetFE<ET_PRISM> (); break;
          case ET_HEX:     fe3d = new (lh) FacetFE<ET_HEX> (); break;
          }

        if (!fe2d && !fe3d && !fe1d)
          {
            stringstream str;
            str << "FacetFESpace " << GetClassName() 
                << ", undefined eltype " 
                << ElementTopology::GetElementName(ma->GetElType(ei))
                << ", order = " << order << endl;
            throw Exception (str.str());
          }

        // ArrayMem<int, 12> vnums;
        ArrayMem<int, 6> fanums, order_fa;
    
        // ma->GetElVertices(ei, vnums);
        ma->GetElFacets (ei, fanums);

        auto vnums = ma->GetElVertices(ei);
        
        order_fa.SetSize(fanums.Size());
        for (int j = 0; j < fanums.Size(); j++)
          order_fa[j] = order_facet[fanums[j]][0]; //SZ not yet anisotropric
    

        if (ma->GetDimension() == 1)
          {
            fe1d -> SetVertexNumbers (vnums);
            fe1d -> SetOrder (order_fa);
            fe1d -> ComputeNDof();
            return *fe1d;
          }
        else if (ma->GetDimension() == 2)
          {
            fe2d -> SetVertexNumbers (vnums);
            fe2d -> SetOrder (order_fa);
            fe2d -> ComputeNDof();
            return *fe2d;
          }
        else
          {
            fe3d -> SetVertexNumbers (vnums);
            fe3d -> SetOrder (order_fa);
            fe3d -> ComputeNDof();
            return *fe3d;
          }
        }
          */
      case BND:
        {
        DGFiniteElement<1> * fe1d = 0;
        DGFiniteElement<2> * fe2d = 0;

        switch (ma->GetElType(ei))
          {
          case ET_SEGM: fe1d = new (lh) L2HighOrderFE<ET_SEGM> (); break;
          case ET_TRIG: fe2d = new (lh) L2HighOrderFE<ET_TRIG> (); break;
          case ET_QUAD: fe2d = new (lh) L2HighOrderFE<ET_QUAD> (); break;
          default:
            throw Exception (string("FacetFESpace::GetSFE: unsupported element ")+
                             ElementTopology::GetElementName(ma->GetElType(ei)));
          }
     
        ArrayMem<int,4> ednums;
    
        auto vnums = ma->GetElVertices(ei);
        switch (ma->GetElType(ei))
          {
          case ET_SEGM:
            {
              fe1d -> SetVertexNumbers (vnums);
              ednums = ma->GetElEdges(ei);
              int p = order_facet[ednums[0]][0];
              if (highest_order_dc) p--;
              fe1d -> SetOrder (p); 
              fe1d -> ComputeNDof();
              return *fe1d;
              break;
            }
          case ET_TRIG: 
          case ET_QUAD: 
            {
              fe2d -> SetVertexNumbers (vnums);
              int p = order_facet[ma->GetSElFace(ei.Nr())][0];
              if (highest_order_dc) p--;
              fe2d -> SetOrder (p);   // SZ not yet anisotropic order for facet fe !!! 
              fe2d -> ComputeNDof();
              return *fe2d;
              break;
            }
          default:
            break;
          }
        return *fe2d;
        }
      case BBND:
        throw Exception("No BBND GetFE implemented for FacetFESpace");
      case BBBND:
        throw Exception("No BBBND GetFE implemented for FacetFESpace");
      }
  }

  // // ------------------------------------------------------------------------
  // const FiniteElement & FacetFESpace :: GetSFE (int selnr, LocalHeap & lh) const
  // {
  //   DGFiniteElement<1> * fe1d = 0;
  //   DGFiniteElement<2> * fe2d = 0;

  //   switch (ma->GetSElType(selnr))
  //     {
  //     case ET_SEGM: fe1d = new (lh) L2HighOrderFE<ET_SEGM> (); break;
  //     case ET_TRIG: fe2d = new (lh) L2HighOrderFE<ET_TRIG> (); break;
  //     case ET_QUAD: fe2d = new (lh) L2HighOrderFE<ET_QUAD> (); break;
  //     default:
  //       throw Exception (string("FacetFESpace::GetSFE: unsupported element ")+
  //                        ElementTopology::GetElementName(ma->GetSElType(selnr)));
  //     }
     
  //   ArrayMem<int,4> vnums;
  //   ArrayMem<int,4> ednums;
    
  //   ma->GetSElVertices(selnr, vnums);
  //   switch (ma->GetSElType(selnr))
  //     {
  //     case ET_SEGM:
  //       {
  //         fe1d -> SetVertexNumbers (vnums);
  //         ma->GetSElEdges(selnr, ednums);
  //         int p = order_facet[ednums[0]][0];
  //         if (highest_order_dc) p--;
  //         fe1d -> SetOrder (p); 
  //         fe1d -> ComputeNDof();
  //         return *fe1d;
  //         break;
  //       }
  //     case ET_TRIG: 
  //     case ET_QUAD: 
  //       {
  //         fe2d -> SetVertexNumbers (vnums);
  //         int p = order_facet[ma->GetSElFace(selnr)][0];
  //         if (highest_order_dc) p--;
  //         fe2d -> SetOrder (p);   // SZ not yet anisotropic order for facet fe !!! 
  //         fe2d -> ComputeNDof();
  //         return *fe2d;
  //         break;
  //       }
  //     default:
  //       break;
  //     }
  //   return *fe2d;
  // }



  // ------------------------------------------------------------------------
  size_t FacetFESpace :: GetNDof () const throw()
  {
    return ndof;
  }

  // ------------------------------------------------------------------------
  size_t FacetFESpace :: GetNDofLevel (int level) const
  {
    return ndlevel[level];
  }


  void FacetFESpace :: GetDofRanges (ElementId ei, Array<IntRange> & dranges) const
  {
    if (highest_order_dc)
      {
        // not optimal, needs reordering of dofs ...
        Array<int> dnums;
        GetDofNrs (ei, dnums);
        dranges.SetSize (0);
        for (int j = 0; j < dnums.Size(); j++)
          dranges.Append (IntRange (dnums[j], dnums[j+1]));
        return;
      }


    dranges.SetSize(0);

    if (!DefinedOn (ei)) return;
    Ngs_Element ngel = ma->GetElement(ei);
    
    
    if (ma->GetDimension() == 2)
      for (int e : ngel.Edges())
        {
          if (!all_dofs_together)  
            {
              dranges.Append (IntRange (e,e+1));
            }
          dranges.Append (GetFacetDofs(e));
        }
    else
      for (int f : ngel.Faces())
        {
           if (!all_dofs_together)  
             {
               dranges.Append (IntRange (f,f+1));
             }
          dranges.Append (GetFacetDofs(f));
        }

  }

  // ------------------------------------------------------------------------
  void FacetFESpace :: GetDofNrs (ElementId ei, Array<int> & dnums) const
  {
    // TODO: remove switch...
    switch (ei.VB())
      {
      case VOL:
	{
	  // ArrayMem<int,6> fanums;
	  // ma->GetElFacets (ei.Nr(),fanums);
	  auto fanums = ma->GetElFacets(ei);
	  dnums.SetSize0();
	  
	  if(!highest_order_dc)
	    {
	      for (auto f : fanums)
		{
		  if (!all_dofs_together)
		    dnums.Append(f); // low_order
		  dnums += GetFacetDofs (f);
		}
	    }
	  else
	    {
	      int innerdof = first_inner_dof[ei.Nr()];
	      ELEMENT_TYPE eltype = ma->GetElType (ei);
	      
	      for(int i=0; i<fanums.Size(); i++)
		{
		  int facetdof = first_facet_dof[fanums[i]];
		  
		  if(ma->GetDimension()==2)
		    {
		      for(int j = 0; j <= order; j++)
			{
			  if(j == 0 && !all_dofs_together) dnums.Append (fanums[i]);
			  else if(j == order) dnums.Append (innerdof++);
			  else dnums.Append (facetdof++);
			}
		    }
		  else
		    {
		      if(ElementTopology::GetFacetType(eltype,i) == ET_TRIG)
			for(int j = 0; j <= order; j++)
			  for(int k = 0; k <= order-j; k++)
			    {
			      if(j + k == 0 && !all_dofs_together) dnums.Append (fanums[i]);
			      else if(j + k == order) dnums.Append (innerdof++);
			      else dnums.Append (facetdof++);
			    } else
			for(int j = 0; j <= order; j++)
			  for(int k = 0; k <= order; k++)
			    {
			      if(j + k == 0 && !all_dofs_together) dnums.Append (fanums[i]);
			      else if(j == order || k == order) dnums.Append (innerdof++);
			      else dnums.Append (facetdof++);
			    }
		    }
		}		
	    }
	break;
      }
      case BND:
	{
	  dnums.SetSize0();
	  
	  int fnum = 0;
	  if (ma->GetDimension() == 2)
	    {
	      // ArrayMem<int, 4> fanums;
	      auto fanums = ma->GetElEdges (ei);
	      fnum = fanums[0];
	    }
	  else
	    fnum = ma->GetSElFace(ei.Nr());
	  
	  if (!all_dofs_together) dnums.Append (fnum);
	  dnums += GetFacetDofs(fnum);
	}
	break;
      case BBND: case BBBND:
	dnums.SetSize(0);
      }
  }
  
  // ------------------------------------------------------------------------
  shared_ptr<Table<int>> FacetFESpace :: CreateSmoothingBlocks (const Flags & precflags) const
  {
    if (all_dofs_together)
      throw Exception("FacetFESpace ::CreateSmoothingBlocks not implemented for case all_dofs_together!");
    
    int ncnt;

    // 1 x low order + faces/edges
    ncnt = nfa-ncfa;
      
    Array<int> cnt(ncnt);
    cnt = 0;

    // setup table
    for (int i = ncfa; i < nfa; i++)
      cnt[i-ncfa] = 1 + first_facet_dof[i+1]-first_facet_dof[i];


    Table<int> table(cnt);
    
    // face/edges
    int ii;
    for (int i = ncfa; i < nfa; i++)
      {
        table[i-ncfa][0] = i-ncfa;
        ii=1;
        for (int j = first_facet_dof[i]; j < first_facet_dof[i+1]; j++, ii++)
          table[i][ii] = j;
      }
      
    // cout << "smoothingblocks = " << endl << table << endl;
    return make_shared<Table<int>> (table);
  }


  
  shared_ptr<Array<int>> FacetFESpace :: CreateDirectSolverClusters (const Flags & precflags) const
  {
    if (all_dofs_together)
      throw Exception("FacetFESpace ::CreateDirectSolverClusters not implemented for case all_dofs_together!");

    auto spclusters = make_shared<Array<int>>(GetNDof());
    Array<int> & clusters = *spclusters;

    clusters.SetSize(ndof);
    clusters = 0;
    
    for (int i = 0; i < nfa; i++)
      clusters[i] = 1;
    
    return spclusters;
    
    //
  
    for (int i=0; i<nfa-ncfa; i++)
      clusters[i]  = 1;
  
    // cout << "direct solver cluster = " << clusters << endl;
    return spclusters;
  }





  /// Identity
  template <int D>
  class DiffOpIdHDG : public DiffOp<DiffOpIdHDG<D> >
  {
  public:
    enum { DIM = 1 };
    enum { DIM_SPACE = D };
    enum { DIM_ELEMENT = D };
    enum { DIM_DMAT = 1 };
    enum { DIFFORDER = 0 };

    template <typename FEL, typename MIP, typename MAT>
    static void GenerateMatrix (const FEL & bfel, const MIP & mip,
                                MAT & mat, LocalHeap & lh)
    {
      const CompoundFiniteElement & fel = static_cast<const CompoundFiniteElement&> (bfel);
      const ScalarFiniteElement<D> & fel_vol = static_cast<const ScalarFiniteElement<D>&> (fel[0]);
      const FacetVolumeFiniteElement<D> & fel_facet = static_cast<const FacetVolumeFiniteElement<D>&> (fel[1]);

      int facetnr = mip.IP().FacetNr();
      mat = 0.0;
      if (facetnr < 0)
        fel_vol.CalcShape(mip.IP(), mat.Row(0).Range(fel.GetRange(0)));
      else
        fel_facet.Facet(facetnr).CalcShape(mip.IP(), 
                                           mat.Row(0).Range(fel.GetRange(1)).Range(fel_facet.GetFacetDofs(facetnr)));
    }
  }; 


  template <int D>
  class NGS_DLL_HEADER HDG_MassIntegrator 
    : public T_BDBIntegrator<DiffOpIdHDG<D>, DiagDMat<1> >
  {
    typedef  T_BDBIntegrator<DiffOpIdHDG<D>, DiagDMat<1> > BASE;
  public:
    using  T_BDBIntegrator<DiffOpIdHDG<D>, DiagDMat<1> >::T_BDBIntegrator;
    virtual string Name () const { return "Mass-HDG"; }
  };

  template class HDG_MassIntegrator<1>;
  template class HDG_MassIntegrator<2>;
  template class HDG_MassIntegrator<3>;
  static RegisterBilinearFormIntegrator<HDG_MassIntegrator<1> > initlap1 ("HDG_mass", 1, 1);
  static RegisterBilinearFormIntegrator<HDG_MassIntegrator<2> > initlap2 ("HDG_mass", 2, 1);
  static RegisterBilinearFormIntegrator<HDG_MassIntegrator<3> > initlap3 ("HDG_mass", 3, 1);


  HybridDGFESpace :: HybridDGFESpace (shared_ptr<MeshAccess> ama, 
                                      const Flags & flags)
    : CompoundFESpace (ama, flags)
  { 
    Flags l2flags(flags), facetflags(flags);

    int order = int (flags.GetNumFlag ("order", 1));
    
    if (flags.GetDefineFlag("l2_dofs_together")){
      l2flags.SetFlag ("all_dofs_together");
      cout << "l2_dofs_together active" << endl; 
    }

    facetflags.SetFlag("orderfacet", order);
    if (flags.NumListFlagDefined ("dirichlet"))
      facetflags.SetFlag ("dirichlet", flags.GetNumListFlag ("dirichlet"));

    if (flags.NumFlagDefined ("relorder")) facetflags.SetFlag("variableorder");
    
    // const FESpaceClasses::FESpaceInfo * info;
    auto info = GetFESpaceClasses().GetFESpace("DGhotp");
    if (!info) info = GetFESpaceClasses().GetFESpace("l2hotp");
    if (!info) info = GetFESpaceClasses().GetFESpace("l2ho");
    
    AddSpace (info->creator(ma, l2flags));
    AddSpace (make_shared<FacetFESpace> (ma, facetflags));        

    if (flags.GetDefineFlag ("edges"))
      throw Exception ("HDG fespace with edges not supported");

    static ConstantCoefficientFunction one(1);
    integrator[VOL] = GetIntegrators().CreateBFI("HDG_mass", ma->GetDimension(), &one);

    if (ma->GetDimension() == 2)
      {
	// integrator = new HDG_MassIntegrator<2> (&one);
        integrator[BND].reset (new RobinIntegrator<2> (&one));
	evaluator[VOL] = make_shared<T_DifferentialOperator<ngcomp::DiffOpIdHDG<2>>>();
      }
    else
      {
        // integrator = new HDG_MassIntegrator<3> (&one);
        integrator[BND] = make_shared<RobinIntegrator<3>> (&one);
	evaluator[VOL] = make_shared<T_DifferentialOperator<ngcomp::DiffOpIdHDG<3>>>();
      }
    integrator[BND] = make_shared<CompoundBilinearFormIntegrator> (integrator[BND], 1);
  }

  HybridDGFESpace :: ~HybridDGFESpace () { ; }


  shared_ptr<Array<int>> HybridDGFESpace :: CreateDirectSolverClusters (const Flags & flags) const
  {
    if (flags.GetDefineFlag("subassembled"))
      {
	cout << IM(3) << "creating bddc-coarse grid(vertices)" << endl;
        auto spclusters = make_shared<Array<int>> (GetNDof());
	Array<int> & clusters = *spclusters;
	clusters = 0;
	/*
          int nv = ma->GetNV();
          int ned = ma->GetNEdges();
          // int nfa = ma->GetNFaces();
          int basefac = spaces[0]->GetNDof();;
          int baseh1 = basefac + spaces[1]->GetNDof();
	  
          Array<int> dnums;
          //low order: 2D: vertices
          if (ma->GetDimension() == 2 && withedges)
	  for (int i = 0; i < nv; i++)
          if (!IsDirichletVertex(i) && spaces.Size()>2){
          spaces[2]->GetVertexDofNrs(i,dnums);
          clusters[dnums[0]+baseh1]=1;
          }
	    
          //low order: l.o. edges (2D: from facet-space, 3D: from edge-space)
          if (ma->GetDimension() == 2 || ((ma->GetDimension() == 3) && withedges))
	  for (int i = 0; i < ned; i++)
          if (!IsDirichletEdge(i))
          {
          dnums.SetSize(0);
          if (ma->GetDimension() == 2){
          // 		spaces[1]->GetEdgeDofNrs(i,dnums);
          // 		clusters[dnums[0]+basefac]=1;
          }else{
          spaces[2]->GetEdgeDofNrs(i,dnums);
          clusters[dnums[0]+baseh1]=1;
          }
          }
	*/


	/*
	//low order: 3D: l.o. face
	if (ma->GetDimension() == 3)
        for (int i = 0; i < nfa; i++)
        if (!IsDirichletFace(i))
        {
        dnums.SetSize(0);
        spaces[1]->GetFaceDofNrs(i,dnums);
        // 		for (int l=0; l<dnums.Size(); l++)
        // 		  clusters[dnums[l]+basefac]=1;		
        clusters[dnums[0]+basefac]=1;
        //end-if isdirichletvertex
        }
	*/

	return spclusters;
      }
    else
      {
        auto spclusters = make_shared<Array<int>> (GetNDof());
	Array<int> & clusters = *spclusters;
	clusters = 0;

	Array<DofId> dnums;
	int nfa = ma->GetNFacets();

	for (int i = 0; i < nfa; i++)
	  {
	    if (ma->GetDimension() == 2)
	      GetDofNrs (NodeId(NT_EDGE,i), dnums);
	    else
	      GetFaceDofNrs (i, dnums);

	    clusters[dnums[0]] = 1;
	  }

	const BitArray & freedofs = *GetFreeDofs();
	for (int i = 0; i < freedofs.Size(); i++)
	  if (!freedofs.Test(i)) clusters[i] = 0;
	*testout << "Hybrid-FESpace, dsc = " << endl << clusters << endl;
	return spclusters;
      }
  }

  shared_ptr<Table<int>> HybridDGFESpace :: CreateSmoothingBlocks (const Flags & precflags) const
  {
    bool eliminate_internal = precflags.GetDefineFlag("eliminate_internal");
    bool subassembled = precflags.GetDefineFlag("subassembled");
    int smoothing_type = int(precflags.GetNumFlag("blocktype",1)); 
    COUPLING_TYPE dof_mode = eliminate_internal? (subassembled? WIREBASKET_DOF : EXTERNAL_DOF) : ANY_DOF;
    BitArray filter;
    GetFilteredDofs(dof_mode, filter, true);
    
    int nv = ma->GetNV();
    int ned = ma->GetNEdges();
    cout << " dof_mode " << dof_mode << endl; 
    cout << " blocktype " << smoothing_type << endl; 
    cout << " Use HDG-Block Smoother:  "; 
    Array<int> dnums;

    FilteredTableCreator creator(&filter);
    for ( ; !creator.Done(); creator++)
      {
	switch (smoothing_type)
	  {
	  case 1: 
            //for BDDC: we have only the condensated (after subassembling) dofs, 
            //and build patches around each vertex Vertices + Edges
		
	    if (creator.GetMode() == 1)
	      cout << "BDDC-Edges-around-Vertex-Block" << endl;
		
            // 	    int ds_order = precflags.GetNumFlag ("ds_order", 1);
            // 	    cout << "ds_order = " << ds_order << endl;
            // 		
	    if (ma->GetDimension() == 2)
	      {  
		for (int i = 0; i < nv; i++)
                  {
                    dnums.SetSize(0);
                    GetDofNrs(NodeId(NT_VERTEX,i),dnums);
                    if (dnums.Size()>0)
                      creator.Add (i, dnums[0]);
                  }
	      }
	    for (int i = 0; i < ned; i++)
	      {
		Ng_Node<1> edge = ma->GetNode<1> (i);
		for (int k = 0; k < 2; k++){
		  dnums.SetSize(0);
		  if (ma->GetDimension() == 2){
		    GetDofNrs(NodeId(NT_EDGE,i),dnums);
		    creator.Add (edge.vertices[k], dnums[0]);
		  }
		}
	      }
	    break; 	    
	  case 2: 
	    //for BDDC: we have only the condensated (after subassembling) dofs, 
	    //and build patches around each edge: [Faces?!] + Edges
		
	    if (creator.GetMode() == 1)
	      cout << "BDDC-Faces-around-Edges" << endl;
		
	    if (ma->GetDimension() == 2)
	      {
		;
	      }

	    else
	      
	      {
		Array<int> dnums, ednums;

		for (int i = 0; i < ned; i++)
		  if (!IsDirichletEdge(i))
		    {
		      GetDofNrs(NodeId(NT_EDGE,i),dnums);
		      for (int l=0;l<dnums.Size();l++)
			creator.Add (i, dnums[l]);
		    }
	      }
            break; 	    


	  case 3: 
	    // facet-by-facet
		
	    if (creator.GetMode() == 1)
	      cout << "Facet-by-facet blocks" << endl;
		
	    
	    Array<DofId> dnums;
	    size_t nfa = ma->GetNFacets();
	    for (size_t i = 0; i < nfa; i++)
	      {
		if (ma->GetDimension() == 2)
		  {
		    if (IsDirichletEdge (i)) continue;
		    GetDofNrs (NodeId(NT_EDGE,i), dnums);
		  }
		else
		  {
		    if (IsDirichletFace (i)) continue;
		    GetFaceDofNrs (i, dnums);
		  }

		for (auto d : dnums)
		  creator.Add (i, d);
	      }
	    break; 	    
	  }
      }
    // return shared_ptr<Table<int>> (creator.GetTable());
    return make_shared<Table<int>> (creator.MoveTable());
  }


  
  // ------------------------------------------------------------------------

  
  static RegisterFESpace<FacetFESpace> init_facet ("facet");
  static RegisterFESpace<HybridDGFESpace> init_hde ("HDG");
}


