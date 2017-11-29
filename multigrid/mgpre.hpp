#ifndef FILE_MGPRE
#define FILE_MGPRE

/*********************************************************************/
/* File:   mgpre.hh                                                  */
/* Author: Joachim Schoeberl                                         */
/* Date:   20. Apr. 2000                                             */
/*********************************************************************/


namespace ngmg
{

  /** 
      Multigrid preconditioner
  */

  class Smoother;
  ///
  class Prolongation;


  ///
  class NGS_DLL_HEADER MultigridPreconditioner : public BaseMatrix
  {

  public:
    ///
    enum COARSETYPE { EXACT_COARSE, CG_COARSE, SMOOTHING_COARSE, USER_COARSE };

  private:
    ///
    const MeshAccess & ma;
    ///
    const FESpace & fespace;
    ///
    const BilinearForm & biform;
  
    ///
    shared_ptr<Smoother> smoother;
    ///
    shared_ptr<Prolongation> prolongation;
    ///
    shared_ptr<BaseMatrix> coarsegridpre;
    ///
    double checksumcgpre;
    ///
    COARSETYPE coarsetype;
    ///
    int cycle, incsmooth, smoothingsteps;
    ///
    int coarsesmoothingsteps;
    ///
    int updateall;
    /// creates a new smoother for each update
    bool update_always; 
    /// for robust prolongation
    // Array<BaseMatrix*> prol_projection;
  public:
    ///
    MultigridPreconditioner (const MeshAccess & ama,
			     const FESpace & afespace,
			     const BilinearForm & abiform,
			     shared_ptr<Smoother> asmoother,
			     shared_ptr<Prolongation> aprolongation);
    ///
    ~MultigridPreconditioner ();
    ///
    virtual bool IsComplex() const override { return fespace.IsComplex(); }

    ///
    void SetSmoothingSteps (int sstep);
    ///
    void SetCycle (int c);
    ///
    void SetIncreaseSmoothingSteps (int incsm);
    ///
    void SetCoarseType (COARSETYPE ctyp);
    ///
    void SetCoarseGridPreconditioner (shared_ptr<BaseMatrix> acoarsegridpre);
    ///
    void SetCoarseSmoothingSteps (int cstep);

    void SetUpdateAll (int ua = 1);
    ///
    void SetUpdateAlways (bool ua = 1) { update_always = ua; }
    ///
    virtual void Update () override;

    ///
    virtual void Mult (const BaseVector & x, BaseVector & y) const override;

    ///
    void MGM (int level, BaseVector & u, 
	      const BaseVector & f, int incsm = 1) const;
    ///
    virtual AutoVector CreateVector () const override
    { return biform.GetMatrix().CreateVector(); }
  
    ///
    const Smoother & GetSmoother() const
    { return *smoother; }
    ///
    Smoother & GetSmoother()
    { return *smoother; }
    ///
    const Prolongation & GetProlongation() const
    { return *prolongation; }


    virtual int VHeight() const override
    {
      return biform.GetMatrix().Height();
    }

    virtual int VWidth() const override
    {
      return biform.GetMatrix().VWidth();
    }


    virtual void MemoryUsage (Array<MemoryUsageStruct*> & mu) const override;
  };







  ///
  class NGS_DLL_HEADER TwoLevelMatrix : public BaseMatrix
  {
    ///
    const BaseMatrix * mat;
    ///
    const BaseMatrix * cpre;
    ///
    shared_ptr<Smoother> smoother;
    ///
    int level;
    ///
    int smoothingsteps;
  public:
    ///
    TwoLevelMatrix (const BaseMatrix * amat, 
		    const BaseMatrix * acpre, 
		    shared_ptr<Smoother> asmoother, int alevel);
    ///
    ~TwoLevelMatrix ();

    virtual bool IsComplex() const override { return mat->IsComplex(); }

    ///

    virtual void Mult (const BaseVector & x, BaseVector & y) const override;
    ///
    virtual AutoVector CreateVector () const override;
    ///
    virtual ostream & Print (ostream & s) const override;
    ///
    void SetSmoothingSteps(int ass) { smoothingsteps = ass; }
    ///
    const Smoother & GetSmoother() const
    { return *smoother; }
    ///
    Smoother & GetSmoother()
    { return *smoother; }
    ///
    virtual void Update() override;

    virtual int VHeight() const override
    {
      return mat->Height();
    }

    virtual int VWidth() const override
    {
      return mat->VWidth();
    }
  
    virtual void MemoryUsage (Array<MemoryUsageStruct*> & mu) const override;
  };

}

#endif
