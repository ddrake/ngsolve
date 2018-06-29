#ifndef VECTOR_FACET_FESPACE_HPP
#define VECTOR_FACET_FESPACE_HPP

/*********************************************************************/
/* File:   vectorfacetfespace.hpp                                    */
/* Author: A. Sinwel, Joachim Schoeberl                              */
/* Date:   2008                                                      */
/*********************************************************************/



namespace ngcomp
{

  class NGS_DLL_HEADER VectorFacetFESpace : public FESpace
  {
  protected:
    /// Level
    int level;
    /// Number of Facets
    // int nfacets;
    /// 
    // int ncfacets;
    ///
    // int nel;

    Array<int> first_facet_dof;
    Array<int> first_inner_dof;  // for highest_order_dc
    // int ndof_lo;

    int rel_order;

    Array<INT<2> > order_facet;
    Array<bool> fine_facet;

    int ndof;
    Array<int> ndlevel;
    bool var_order;
    bool print;

    bool highest_order_dc;

  public:
    ///
    VectorFacetFESpace (shared_ptr<MeshAccess> ama, const Flags & flags, 
			bool parseflags = false );

    virtual ~VectorFacetFESpace () { ; }

    virtual string GetClassName () const override
    {
      return "VectorFacetFESpace";
    }

    virtual void Update (LocalHeap& lh) override;
    virtual void UpdateCouplingDofArray() override;
    virtual size_t GetNDof() const throw() override { return ndof; }

    virtual size_t GetNDofLevel ( int i ) const override { return ndlevel[i]; }

    // virtual int GetNDofLowOrder () const
    // { return ndof_lo; }

    virtual FiniteElement & GetFE(ElementId ei, Allocator & lh) const override;
    
    // virtual const FiniteElement & GetFE ( int elnr, LocalHeap & lh ) const;
    // virtual const FiniteElement & GetSFE ( int selnr, LocalHeap & lh ) const;

    virtual void GetFacetDofNrs (int felnr, Array<DofId> & dnums) const;

    virtual int GetNFacetDofs (int felnr) const;

    virtual void GetDofNrs (ElementId ei, Array<DofId> & dnums) const override;

    virtual shared_ptr<Table<int>> CreateSmoothingBlocks (const Flags & precflags) const override;
    ///
    virtual shared_ptr<Array<int>> CreateDirectSolverClusters (const Flags & precflags) const override;
  
    // some utility functions for convenience
    ///
    virtual void GetVertexNumbers(int elnr, Array<int>& vnums) const;
    ///
    virtual INT<2> GetFacetOrder(int fnr) const;

    virtual int GetFirstFacetDof(int fanr) const;

    virtual bool UsesHighestOrderDiscontinuous() const {return highest_order_dc;};

    virtual void GetVertexDofNrs (int elnum, Array<DofId> & dnums) const override;
    virtual void GetEdgeDofNrs (int elnum, Array<DofId> & dnums) const override;
    virtual void GetFaceDofNrs (int felnr, Array<DofId> & dnums) const override;
    virtual void GetInnerDofNrs (int felnr, Array<DofId> & dnums) const override;
  };

}

#endif

