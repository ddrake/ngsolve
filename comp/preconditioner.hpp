#ifndef FILE_PRECONDITIONER
#define FILE_PRECONDITIONER

/*********************************************************************/
/* File:   preconditioner.hh                                         */
/* Author: Joachim Schoeberl                                         */
/* Date:   10. Jul. 2000                                             */
/*********************************************************************/



namespace ngcomp
{
  class PDE;

  /**
     Base class for preconditioners.
  */
  class NGS_DLL_HEADER Preconditioner : public NGS_Object, public BaseMatrix
  {
  protected:
    bool test;
    bool timing;
    bool print;

    /// if true, the update in SolveBVP() is ignored, Update() has to be called explicitly.
    bool laterupdate;

    double * testresult_ok;
    double * testresult_min;
    double * testresult_max;
  
    Flags flags; 

    // for calculation of eigenvalues
    bool uselapack;

    int on_proc;

  public:
    Preconditioner (const PDE * const apde, const Flags & aflags,
		    const string aname = "precond");
    Preconditioner (shared_ptr<BilinearForm> bfa, const Flags & aflags,
		    const string aname = "precond");
    ///
    virtual ~Preconditioner ();
  
    ///
    virtual bool LaterUpdate (void) { return laterupdate; }
    ///
    virtual void Update () = 0;
    ///
    virtual void CleanUpLevel () { ; }
    ///
    virtual const BaseMatrix & GetMatrix() const
    {
      return *this; 
    }
    
    virtual shared_ptr<BaseMatrix> GetMatrixPtr() 
    {
      return BaseMatrix::SharedFromThis<BaseMatrix>();
    }

    virtual bool IsComplex() const { return GetMatrix().IsComplex(); }
        
    ///
    virtual void Mult (const BaseVector & x, BaseVector & y) const
    {
      GetMatrix().Mult(x, y);
    }

    virtual void InitLevel (shared_ptr<BitArray> freedofs = NULL) { ; }
    virtual void FinalizeLevel (const ngla::BaseMatrix * mat = NULL) { ; }
    virtual void AddElementMatrix (FlatArray<int> dnums,
				   const FlatMatrix<double> & elmat,
				   ElementId ei, 
				   LocalHeap & lh) { ; }

    virtual void AddElementMatrix (FlatArray<int> dnums,
				   const FlatMatrix<Complex> & elmat,
				   ElementId ei, 
				   LocalHeap & lh) { ; }



    virtual const BaseMatrix & GetAMatrix() const
    { throw Exception ("Preconditioner, A-Matrix not available"); }

    ///
    virtual const char * ClassName() const
    { return "base-class Preconditioner"; }


    virtual void PrintReport (ostream & ost) const
    {
      ost << "type = " << ClassName() << endl;
    }

    virtual void MemoryUsage (Array<MemoryUsageStruct*> & mu) const
    {
      cout << "MemoryUsage not implemented for preconditioner " << ClassName() << endl;
    }

    virtual int VHeight() const { return GetMatrix().VHeight();}
    virtual int VWidth() const { return GetMatrix().VWidth();}

    void Test () const;
    void Timing () const;
    void ThrowPreconditionerNotReady() const;
    const Flags & GetFlags() const { return flags; }

    using BaseMatrix::shared_from_this;
  };



  ///




  ///
  class TwoLevelPreconditioner : public Preconditioner
  {
    ///
    PDE * pde;
    ///
    shared_ptr<BilinearForm> bfa;
    ///
    shared_ptr<Preconditioner> cpre;
    ///
    ngmg::TwoLevelMatrix * premat;
    ///
    int smoothingsteps;
  public:
    ///
    TwoLevelPreconditioner (PDE * apde, const Flags & aflags,
			    const string aname = "twolevelprecond");
    ///
    virtual ~TwoLevelPreconditioner();

    ///
    virtual void Update ();
    ///
    virtual const BaseMatrix & GetMatrix() const;
    // { return *new SparseMatrix<double> (1,1); } // *premat; }
    // { return *premat; }
    ///
    virtual const char * ClassName() const
    { return "TwoLevel Preconditioner"; }
  };








  ///
  class NGS_DLL_HEADER ComplexPreconditioner : public Preconditioner
  {
  protected:
    ///
    shared_ptr<Preconditioner> creal;
    ///
    // Real2ComplexMatrix<double,Complex> cm;
    int dim;
    BaseMatrix * cm;
  public:
    ///
    ComplexPreconditioner (PDE * apde, const Flags & aflags,
			   const string aname = "complexprecond");
    ///
    virtual ~ComplexPreconditioner();
    ///
    virtual void Update ();
    ///
    virtual const BaseMatrix & GetMatrix() const
    { 
      return *cm; 
    }
    ///
    virtual const char * ClassName() const
    { return "Complex Preconditioner"; }
  };




  ///
  class ChebychevPreconditioner : public Preconditioner
  {
  protected:
    ///
    shared_ptr<Preconditioner> csimple;
    /// 
    ChebyshevIteration * cm;
    /// 
    shared_ptr<BilinearForm> bfa;
    ///
    int steps; 
  public:
    ///
    ChebychevPreconditioner (PDE * apde, const Flags & aflags,
			     const string aname = "chebychevprecond");
    ///
    virtual ~ChebychevPreconditioner();
    ///
    virtual void Update ();
    ///
    virtual const BaseMatrix & GetMatrix() const
    { 
      return *cm; 
    }
    virtual const BaseMatrix & GetAMatrix() const
    {
      return bfa->GetMatrix(); 
    }

    ///
    virtual const char * ClassName() const
    { return "Chebychev Preconditioner"; }
  };



  /**
     Multigrid preconditioner.
     High level objects, contains a \Ref{MultigridPreconditioner}
  */
  class NGS_DLL_HEADER MGPreconditioner : public Preconditioner
  {
    ///
    shared_ptr<ngmg::MultigridPreconditioner> mgp;
    ///
    shared_ptr<ngmg::TwoLevelMatrix> tlp;
    ///
    shared_ptr<BilinearForm> bfa;
    ///
    // MGPreconditioner * low_order_preconditioner;
    ///
    shared_ptr<Preconditioner> coarse_pre;
    ///
    int finesmoothingsteps;
    ///
    string smoothertype;
    ///
    bool mgtest;
    string mgfile;
    int mgnumber;

    string inversetype;

  public:
    ///
    MGPreconditioner (const PDE & pde, const Flags & aflags,
		      const string aname = "mgprecond");
    MGPreconditioner (shared_ptr<BilinearForm> bfa, const Flags & aflags,
		      const string aname = "mgprecond");
    ///
    virtual ~MGPreconditioner() { ; }

    void FreeSmootherMem(void);

    virtual void FinalizeLevel (const BaseMatrix * mat)
    {
      Update();
    }

    ///
    virtual void Update ();
    ///
    virtual void CleanUpLevel ();
    ///
    virtual const BaseMatrix & GetMatrix() const;
    ///
    virtual const BaseMatrix & GetAMatrix() const
    {
      return bfa->GetMatrix();
    }
    ///
    virtual const char * ClassName() const
    { return "Multigrid Preconditioner"; }

    virtual void PrintReport (ostream & ost) const;

    virtual void MemoryUsage (Array<MemoryUsageStruct*> & mu) const;

    void MgTest () const;
  };

  class CommutingAMGPreconditioner : public Preconditioner
  {
  protected:
    PDE * pde;
    shared_ptr<BilinearForm> bfa;
    // CommutingAMG * amg;
    BaseMatrix * amg;
    shared_ptr<CoefficientFunction> coefe, coeff, coefse;
    bool hcurl;
    bool coarsegrid;
    int levels;
  public:
    CommutingAMGPreconditioner (PDE * apde, const Flags & aflags,
				const string aname = "commutingamgprecond");

    virtual ~CommutingAMGPreconditioner ();

    virtual void Update ();
    ///

    virtual const BaseMatrix & GetAMatrix() const
    {
      return bfa->GetMatrix(); 
    }

    virtual const BaseMatrix & GetMatrix() const
    { 
      return *amg; 
    }
    ///
    virtual void CleanUpLevel ();

    ///
    virtual const char * ClassName() const
    { return "CommutingAMG Preconditioner"; }
  };






  // added 08/19/2003:

  ////////////////////////////////////////////////////////////////////////////////
  //
  // special preconditioner for system
  //   (  A   M  )
  //   ( -M   A  )
  //
  // 
  // C = (  1  1  ) (  A+M       )
  //     ( -1  1  ) (       A+M  )
  //
  ////////////////////////////////////////////////////////////////////////////////
  class NonsymmetricPreconditioner : public Preconditioner
  {
  protected:
    ///
    shared_ptr<Preconditioner> cbase;
    ///
    int dim;
    BaseMatrix * cm;
  public:
    ///
    NonsymmetricPreconditioner (PDE * apde, const Flags & aflags,
				const string aname = "nonsymmetricprecond");
    ///
    virtual ~NonsymmetricPreconditioner();
    ///
    virtual bool IsComplex() const { return cm->IsComplex(); }

    virtual void Update ();
    ///
    virtual const BaseMatrix & GetMatrix() const
    { 
      return *cm; 
    }
    ///
    virtual const char * ClassName() const
    { return "Nonsymmetric Preconditioner"; }
  };







  /// Registered Preconditioner classes
  class NGS_DLL_HEADER PreconditionerClasses
  {
  public:
    struct PreconditionerInfo
    {
      string name;
      shared_ptr<Preconditioner> (*creator)(const PDE & pde, const Flags & aflags, const string & name);
      shared_ptr<Preconditioner> (*creatorbf)(shared_ptr<BilinearForm> bfa, const Flags & aflags, const string & name);
      PreconditionerInfo (const string & aname,
			  shared_ptr<Preconditioner> (*acreator)
                          (const PDE & pde, const Flags & aflags, const string & name),
			  shared_ptr<Preconditioner> (*acreatorbf)
                          (shared_ptr<BilinearForm> bfa, const Flags & aflags, const string & name));
    };
  
    Array<PreconditionerInfo*> prea;
  public:
    PreconditionerClasses();
    ~PreconditionerClasses();  
    void AddPreconditioner (const string & aname, 
			    shared_ptr<Preconditioner> (*acreator)
                            (const PDE & pde, const Flags & aflags, const string & name),
			    shared_ptr<Preconditioner> (*acreatorbf)
                            (shared_ptr<BilinearForm> bfa, const Flags & aflags, const string & name));
  
    const Array<PreconditionerInfo*> & GetPreconditioners() { return prea; }
    const PreconditionerInfo * GetPreconditioner(const string & name);

    void Print (ostream & ost) const;
  };
 
  extern NGS_DLL_HEADER PreconditionerClasses & GetPreconditionerClasses ();

  template <typename PRECOND>
  class RegisterPreconditioner
  {
  public:
    RegisterPreconditioner (string label, bool isparallel = true)
    {
      GetPreconditionerClasses().AddPreconditioner (label, Create, CreateBF);
      // cout << "register preconditioner '" << label << "'" << endl;
    }
    
    static shared_ptr<Preconditioner> Create (const PDE & pde, const Flags & flags, const string & name)
    {
      return make_shared<PRECOND> (pde, flags, name);
    }

    static shared_ptr<Preconditioner> CreateBF (shared_ptr<BilinearForm> bfa, const Flags & flags, const string & name)
    {
      return make_shared<PRECOND> (bfa, flags, name);
    }
  };

}

#endif

