#ifndef FILE_PARALLELDOFS
#define FILE_PARALLELDOFS

/**************************************************************************/
/* File:   paralleldofs.hpp                                               */
/* Author: Joachim Schoeberl                                              */
/* Date:   July 2011                                                      */
/**************************************************************************/



namespace ngla
{

#ifdef PARALLEL

  /**
     Handles the distribution of degrees of freedom for vectors and matrices
   */
  class ParallelDofs
  {
  protected:
    /// the communicator 
    MPI_Comm comm;
    
    /// local ndof
    size_t ndof;

    /// global ndof
    size_t global_ndof;

    /// proc 2 dofs
    Table<int> exchangedofs;
    
    /// dof 2 procs
    Table<int> dist_procs;

    /// all procs with connected dofs
    Array<int> all_dist_procs;
    
    /// mpi-datatype to send exchange dofs
    Array<MPI_Datatype> mpi_t;
    
    /// am I the master process ?
    BitArray ismasterdof;
    
  public:
    /**
       setup parallel dofs.
       Table adist_procs must provide the distant processes for each dof.
       table 
     */
    ParallelDofs (MPI_Comm acomm, Table<int> && adist_procs, 
		  int dim = 1, bool iscomplex = false);

    
    virtual ~ParallelDofs()  { ; }

    int GetNTasks() const { return exchangedofs.Size(); }

    FlatArray<int> GetExchangeDofs (int proc) const
    { return exchangedofs[proc]; }

    FlatArray<int> GetDistantProcs (int dof) const
    { return dist_procs[dof]; }

    FlatArray<int> GetDistantProcs () const
    { return all_dist_procs; }

    bool IsMasterDof (size_t localdof) const
    { return ismasterdof.Test(localdof); }

    size_t GetNDofLocal () const { return ndof; }

    size_t GetNDofGlobal () const { return global_ndof; }

    bool IsExchangeProc (int proc) const
    { return exchangedofs[proc].Size() != 0; }

    MPI_Datatype MyGetMPI_Type (int dest) const
    { return mpi_t[dest]; }

    MPI_Comm GetCommunicator () const { return comm; }

    int GetMasterProc (int dof) const
    {
      int m = MyMPI_GetId(comm);
      for (int p : GetDistantProcs(dof))
	m = min2(p, m);
      return m;
    }

    void EnumerateGlobally (shared_ptr<BitArray> freedofs, Array<int> & globnum, int & num_glob_dofs) const;


    template <typename T>
    void ReduceDofData (FlatArray<T> data, MPI_Op op) const;

    template <typename T>
    void ScatterDofData (FlatArray<T> data) const;
    
    template <typename T>
    void AllReduceDofData (FlatArray<T> data, MPI_Op op) const
    {
      if (this == NULL)  // illformed C++, shall get rid of this
        throw Exception("AllReduceDofData for null-object");
      ReduceDofData (data, op);
      ScatterDofData (data);
    }
    
  };

#else
  class ParallelDofs 
  {
  protected:
    int ndof;
    
  public:
    
    int GetNDofLocal () const { return ndof; }
    int GetNDofGlobal () const { return ndof; }

    FlatArray<int> GetExchangeDofs (int proc) const
    { return FlatArray<int> (0, nullptr); }

    FlatArray<int> GetDistantProcs (int dof) const
    { return FlatArray<int> (0, nullptr); }

    FlatArray<int> GetDistantProcs () const
    { return FlatArray<int> (0, nullptr); }      
    
    template <typename T>
    void ReduceDofData (FlatArray<T> data, MPI_Op op) const { ; }

    template <typename T>
    void ScatterDofData (FlatArray<T> data) const { ; } 
    
    template <typename T>
    void AllReduceDofData (FlatArray<T> data, MPI_Op op) const { ; }
  };
  
#endif



  template <typename T>
  void ReduceDofData (FlatArray<T> data, MPI_Op op, const shared_ptr<ParallelDofs> & pardofs)
  {
    if (pardofs)
      pardofs->ReduceDofData(data, op);
  }

  template <typename T>
  void ScatterDofData (FlatArray<T> data, const shared_ptr<ParallelDofs> & pardofs)
  {
    if (pardofs)
      pardofs->ScatterDofData (data);
  }

  template <typename T>
  void AllReduceDofData (FlatArray<T> data, MPI_Op op, 
			 const shared_ptr<ParallelDofs> & pardofs)
  {
    if (pardofs)
      pardofs->AllReduceDofData (data, op);
  }




#ifdef PARALLEL

  template <typename T>
  void ParallelDofs::ReduceDofData (FlatArray<T> data, MPI_Op op) const
  {
    if (this == NULL)  // illformed C++, shall get rid of this
      throw Exception("ReduceDofData for null-object");
    static Timer t("ParallelDofs::ReduceDofData"); RegionTimer reg(t);

    int ntasks = GetNTasks ();
    int rank = MyMPI_GetId(comm);
    if (ntasks <= 1) return;

    static Timer t0("ParallelDofs :: ReduceDofData");
    RegionTimer rt(t0);

    Array<int> nsend(ntasks), nrecv(ntasks);
    nsend = 0;
    nrecv = 0;

    /** Count send/recv size **/
    for (int i = 0; i < GetNDofLocal(); i++) {
      auto dps = GetDistantProcs(i);
      if(!dps.Size()) continue;
      int master = min2(rank, dps[0]);
      if(rank==master)
	for(auto p:dps)
	  nrecv[p]++;
      else
	nsend[master]++;
    }

    Table<T> send_data(nsend);
    Table<T> recv_data(nrecv);

    /** Fill send_data **/
    nsend = 0;
    for (int i = 0; i < GetNDofLocal(); i++) {
      auto dps = GetDistantProcs(i);
      if(!dps.Size()) continue;
      int master = min2(rank, dps[0]);
      if(master!=rank)
	send_data[master][nsend[master]++] = data[i];
    }

    // if (!IsMasterDof(i))
    //   {
    // 	FlatArray<int> distprocs = GetDistantProcs (i);
    // 	int master = ntasks;
    // 	for (int j = 0; j < distprocs.Size(); j++)
    // 	  master = min (master, distprocs[j]);
    // 	dist_data.Add (master, data[i]);
    //   }
    
    // DynamicTable<T> dist_data(ntasks);

    // for (int i = 0; i < GetNDofLocal(); i++)
    //   if (!IsMasterDof(i))
    // 	{
    // 	  FlatArray<int> distprocs = GetDistantProcs (i);
    // 	  int master = ntasks;
    // 	  for (int j = 0; j < distprocs.Size(); j++)
    // 	    master = min (master, distprocs[j]);
    // 	  dist_data.Add (master, data[i]);
    // 	}
    
    // Array<int> nsend(ntasks), nrecv(ntasks);
    // for (int i = 0; i < ntasks; i++)
    //   nsend[i] = dist_data[i].Size();

    // MyMPI_AllToAll (nsend, nrecv, comm);
    // Table<T> recv_data(nrecv);

    Array<MPI_Request> requests; 
    for (int i = 0; i < ntasks; i++)
      {
	if (nsend[i])
	  requests.Append (MyMPI_ISend (send_data[i], i, MPI_TAG_SOLVE, comm));
	if (nrecv[i])
	  requests.Append (MyMPI_IRecv (recv_data[i], i, MPI_TAG_SOLVE, comm));
      }

    MyMPI_WaitAll (requests);

    Array<int> cnt(ntasks);
    cnt = 0;
    
    MPI_Datatype type = MyGetMPIType<T>();
    for (int i = 0; i < GetNDofLocal(); i++)
      if (IsMasterDof(i))
	{
	  FlatArray<int> distprocs = GetDistantProcs (i);
	  for (int j = 0; j < distprocs.Size(); j++)
	    MPI_Reduce_local (&recv_data[distprocs[j]][cnt[distprocs[j]]++], 
			      &data[i], 1, type, op);
	}
  }    





  template <typename T>
  void ParallelDofs :: ScatterDofData (FlatArray<T> data) const
  {
    if (this == NULL)   // illformed C++, shall get rid of this
      throw Exception("ScatterDofData for null-object");

    MPI_Comm comm = GetCommunicator();

    int ntasks = MyMPI_GetNTasks (comm);
    int rank = MyMPI_GetId(comm);
    if (ntasks <= 1) return;

    static Timer t0("ParallelDofs :: ScatterDofData");
    RegionTimer rt(t0);

    Array<int> nsend(ntasks), nrecv(ntasks);
    nsend = 0;
    nrecv = 0;

    /** Count send/recv size **/
    for (int i = 0; i < GetNDofLocal(); i++) {
      auto dps = GetDistantProcs(i);
      if(!dps.Size()) continue;
      int master = min2(rank, dps[0]);
      if(rank==master)
	for(auto p:dps)
	  nsend[p]++;
      else
	nrecv[master]++;
    }

    Table<T> send_data(nsend);
    Table<T> recv_data(nrecv);

    /** Fill send_data **/
    nsend = 0;
    for (int i = 0; i < GetNDofLocal(); i++) {
      auto dps = GetDistantProcs(i);
      if(!dps.Size()) continue;
      int master = min2(rank, dps[0]);
      if(rank==master)
	for(auto p:dps)
	  send_data[p][nsend[p]++] = data[i];
    }

    // DynamicTable<T> dist_data(ntasks);
    // for (int i = 0; i < GetNDofLocal(); i++)
    //   if (IsMasterDof(i))
    // 	{
    // 	  FlatArray<int> distprocs = GetDistantProcs (i);
    // 	  for (int j = 0; j < distprocs.Size(); j++)
    // 	    dist_data.Add (distprocs[j], data[i]);
    // 	}

    // Array<int> nsend(ntasks), nrecv(ntasks);
    // for (int i = 0; i < ntasks; i++)
    //   nsend[i] = dist_data[i].Size();

    // MyMPI_AllToAll (nsend, nrecv, comm);

    // Table<T> recv_data(nrecv);

    Array<MPI_Request> requests;
    for (int i = 0; i < ntasks; i++)
      {
	if (nsend[i])
	  requests.Append (MyMPI_ISend (send_data[i], i, MPI_TAG_SOLVE, comm));
	if (nrecv[i])
	  requests.Append (MyMPI_IRecv (recv_data[i], i, MPI_TAG_SOLVE, comm));
      }

    MyMPI_WaitAll (requests);

    Array<int> cnt(ntasks);
    cnt = 0;
    
    for (int i = 0; i < GetNDofLocal(); i++)
      if (!IsMasterDof(i))
	{
	  FlatArray<int> distprocs = GetDistantProcs (i);
	  
	  int master = ntasks;
	  for (int j = 0; j < distprocs.Size(); j++)
	    master = min (master, distprocs[j]);
	  data[i] = recv_data[master][cnt[master]++];
	}
  }    

#endif //PARALLEL


}



#endif
