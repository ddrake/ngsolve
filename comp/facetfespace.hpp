/**
   High Order Finite Element Space for Facets
*/
/* facetfespace.*pp
 * An fespace for functions living on the facets (=edges in 2D, faces in 3D)
 *   - the functions on different facets are independent (no continuity across
 *     vertices ( or edges in 3D))
 *   - the functions on the facets are accessed via the elements
 *     (=FacetVolumeElements), which then can access their facets;
 *     see facetfe.hpp for details
 *   - the ordering of dofs is facet'wise
 *   - no low-order space
 *   - no prolongation so far (will probably never be needed)
 *   - some additional utility functions 
 */
#ifndef FACET_FESPACE_HPP
#define FACET_FESPACE_HPP

namespace ngcomp
{

  class NGS_DLL_HEADER FacetFESpace : public FESpace 
  {
  protected:  
    // Level
    int level;
    // Number of Facets
    int nfa;
    // Number of coarse facets, number of fine facets;
    int ncfa;
    // Number of Elements
    int nel;
  
    Array<int> first_facet_dof;
    Array<int> first_inner_dof;  // for highest_order_dc
  
    // relative order to mesh-order
    int rel_order; 
  
    Array<INT<2> > order_facet;
    Array<bool> fine_facet;
  
    // int ndof;
    // Array<int> ndlevel;
    bool var_order; 
    bool highest_order_dc;
    bool nowirebasket;
    bool all_dofs_together;
  public:
    ///
    FacetFESpace (shared_ptr<MeshAccess> ama, const Flags & flags, bool parseflags=false);
    ///
    virtual ~FacetFESpace ();
    ///
    virtual string GetClassName () const override
    {
      return "FacetFESpace";
    }
  
    ///
    virtual void Update(LocalHeap & lh) override;
  
    //  virtual void UpdateDofTables();
    virtual void UpdateCouplingDofArray() override;    
    ///
    // virtual size_t GetNDof () const throw() override;
    // virtual size_t GetNDofLevel (int level) const override;

    virtual FiniteElement & GetFE (ElementId ei, Allocator & lh) const override;

    template <ELEMENT_TYPE ET>
    FiniteElement & T_GetFE (int elnr, Allocator & alloc) const;

    using FESpace::GetDofNrs;
    virtual void GetDofNrs (ElementId ei, Array<DofId> & dnums) const override;

    IntRange GetFacetDofs (int nr) const
    { 
      return IntRange (first_facet_dof[nr], first_facet_dof[nr+1]); 
    }
  
    virtual void GetFacetDofNrs (int nr, Array<DofId> & dnums) const
    {
      dnums.SetSize(0);
      dnums += nr;
      dnums += GetFacetDofs(nr);
    }

    ///
    virtual int GetNFacetDofs (int felnr) const 
    { return (first_facet_dof[felnr+1]-first_facet_dof[felnr] + 1); }
    ///
    virtual shared_ptr<Table<int>> CreateSmoothingBlocks (const Flags & precflags) const override;
    ///
    virtual shared_ptr<Array<int>> CreateDirectSolverClusters (const Flags & precflags) const override;


    virtual INT<2> GetFacetOrder(int fnr) 
    { return order_facet[fnr]; };
  

    virtual int GetFirstFacetDof(int fanr) const 
    {
      return first_facet_dof[fanr];
    }

    virtual void GetVertexDofNrs ( int nr, Array<DofId> & dnums ) const override
    {
      dnums.SetSize0();
    }

    virtual void GetEdgeDofNrs ( int nr, Array<DofId> & dnums ) const override
    {
      dnums.SetSize0();
      if (ma->GetDimension() == 3) return;

      /*
      dnums += nr;
      dnums += GetFacetDofs(nr);
      */
      dnums = MakeTuple (nr, GetFacetDofs(nr));
    }

    virtual void GetFaceDofNrs (int nr, Array<DofId> & dnums) const override
    {
      dnums.SetSize(0);
      if (ma->GetDimension() == 2) return;

      dnums += nr;
      dnums += GetFacetDofs(nr);
    }
  
    virtual void GetInnerDofNrs (int elnr, Array<DofId> & dnums) const override
    {
      dnums.SetSize(0);
    }
    
    bool AllDofsTogether(){return all_dofs_together;};
    
  };





  class NGS_DLL_HEADER HybridDGFESpace : public CompoundFESpace
  {
  public:
    HybridDGFESpace (shared_ptr<MeshAccess> ama, 
                     const Flags & flags);

    virtual ~HybridDGFESpace ();
    virtual string GetClassName () const override { return "HybridDGFESpace"; }

    virtual shared_ptr<Array<int>> CreateDirectSolverClusters (const Flags & flags) const override;
    virtual shared_ptr<Table<int>> CreateSmoothingBlocks (const Flags & precflags) const override;
  };

}



#endif
