/**********************************************************************/
/* File:   fespace.cpp                                                */
/* Author: Joachim Schoeberl                                          */
/* Date:   25. Mar. 2000                                              */
/**********************************************************************/

/* 
   Finite Element Space
*/


#include <comp.hpp>
#include <multigrid.hpp>

#include "../fem/h1lofe.hpp"
// #include <tscalarfe_impl.hpp>
#include <parallelngs.hpp>


using namespace ngmg;

namespace ngcomp
{

  FESpace :: FESpace (shared_ptr<MeshAccess> ama, const Flags & flags, bool checkflags)
    : NGS_Object (ama, flags, "FESpace")
  {
    // register flags
    DefineStringFlag("type");
    DefineNumFlag("order");
    DefineNumFlag("dim");
    DefineDefineFlag("vec");
    DefineDefineFlag("complex");
    DefineDefineFlag("timing");
    DefineDefineFlag("print");
    DefineNumListFlag("directsolverdomains");
    DefineNumListFlag("dirichlet");
    DefineNumListFlag("definedon");
    DefineNumFlag ("definedon");
    DefineStringListFlag ("definedon");
    DefineNumListFlag("definedonbound");
    DefineNumFlag ("definedonbound");
    DefineStringListFlag ("definedonbound");
    DefineDefineFlag("dgjumps");

    order = int (flags.GetNumFlag ("order", 1));

    if (flags.NumFlagDefined("order_left"))
      {
        auto order_left = int(flags.GetNumFlag("order_left", 1));
        order = max(order, order_left);
        for (auto et : element_types)
          SetOrderLeft (et, order_left);
      }
    if (flags.NumFlagDefined("order_right"))
      {
        auto order_right = int(flags.GetNumFlag("order_right", 1));
        order = max(order, order_right);    
        for (auto et : element_types)
          SetOrderRight (et, order_right);
      }
    
    
    dimension = int (flags.GetNumFlag ("dim", 1));

    if (flags.GetDefineFlag ("vec"))
      dimension = ma->GetDimension();
    if (flags.GetDefineFlag ("tensor")) 
      dimension = sqr (ma->GetDimension());
    if (flags.GetDefineFlag ("symtensor")) 
      dimension = ma->GetDimension()*(ma->GetDimension()+1) / 2;
    
    iscomplex = flags.GetDefineFlag ("complex");
//     eliminate_internal = flags.GetDefineFlag("eliminate_internal");
    timing = flags.GetDefineFlag("timing");
    print = flags.GetDefineFlag("print");
    dgjumps = flags.GetDefineFlag("dgjumps");
    no_low_order_space = flags.GetDefineFlag("no_low_order_space");
    if (dgjumps) 
      *testout << "ATTENTION: flag dgjumps is used!\n This leads to a \
lot of new non-zero entries in the matrix!\n" << endl;
    // else *testout << "\n (" << order << ") flag dgjumps is not used!" << endl;
    
    if(flags.NumListFlagDefined("directsolverdomains"))
      {
	directsolverclustered.SetSize(ama->GetNDomains());
	directsolverclustered = false;
	Array<double> clusters(flags.GetNumListFlag("directsolverdomains"));
	for(int i=0; i<clusters.Size(); i++) 
	  directsolverclustered[static_cast<int>(clusters[i])-1] = true; // 1-based!!
      }
    
    if(flags.NumListFlagDefined("dirichlet"))
      {
	dirichlet_boundaries.SetSize (ma->GetNBoundaries());
	dirichlet_boundaries.Clear();
        for (double dbi : flags.GetNumListFlag("dirichlet"))
          {
	    int bnd = int(dbi-1);
	    if (bnd >= 0 && bnd < dirichlet_boundaries.Size())
	      dirichlet_boundaries.Set (bnd);
	    // else
            //   cerr << "Illegal Dirichlet boundary index " << bnd+1 << endl;
          }
	if (print)
	  *testout << "dirichlet_boundaries:" << endl << dirichlet_boundaries << endl;
      }
    
    if (flags.NumListFlagDefined("definedon") || 
        flags.NumFlagDefined("definedon") ||
        flags.StringListFlagDefined("definedon"))
      {
	definedon[VOL].SetSize (ma->GetNDomains());
	definedon[VOL] = false;
	Array<double> defon;
	if (flags.NumListFlagDefined("definedon")) 
	  defon = flags.GetNumListFlag("definedon");
	else if (flags.NumFlagDefined("definedon"))
	  {
	    defon.SetSize(1);
	    defon[0] = flags.GetNumFlag("definedon",0);
	  }
        /*
	for(int i = 0; i< defon.Size(); i++)
	  if (defon[i] <= ma->GetNDomains() && defon[i] > 0)
	    definedon[int(defon[i])-1] = true;
        */
        for (int di : defon)
          if (di > 0 && di <= ma->GetNDomains())
            definedon[VOL][di-1] = true;
          
	if(flags.StringListFlagDefined("definedon"))
	  {
	    Array<string> dmaterials(flags.GetStringListFlag ("definedon").Size());
	    for(int i=0; i<dmaterials.Size(); i++)
	      dmaterials[i] = flags.GetStringListFlag ("definedon")[i];
	    for(int i = 0; i < ma->GetNDomains(); i++)
	      {
		for(int j = 0; j < dmaterials.Size(); j++)
		  if(StringFitsPattern(ma->GetMaterial(VOL,i),dmaterials[j]))
		    {
		      definedon[VOL][i] = true;
		      break;
		    }
	      }
	  }

	// default:
	// fespace only defined on boundaries matching definedon-domains
	definedon[BND].SetSize (ma->GetNBoundaries());
	definedon[BND] = false;
	for (int sel = 0; sel < ma->GetNSE(); sel++)
	  {
            ElementId sei(BND, sel);
	    int index = ma->GetElIndex(sei);
	    int dom1, dom2;
	    ma->GetSElNeighbouringDomains(sel, dom1, dom2);
	    dom1--; dom2--;
	    if ( dom1 >= 0 )
	      if ( definedon[VOL][dom1] )
		definedon[BND][index] = true;

	    if ( dom2 >= 0 )
	      if ( definedon[VOL][dom2] )
		definedon[BND][index] = true;
	  }
      }

    // additional boundaries
    if(flags.NumListFlagDefined("definedonbound")|| flags.NumFlagDefined("definedonbound") )
      {
	if ( definedon[BND].Size() == 0 )
	  {
	    definedon[BND].SetSize (ma->GetNBoundaries());
	    definedon[BND] = false;
	  }
	Array<double> defon;
	if ( flags.NumListFlagDefined("definedonbound") )
	  defon = (flags.GetNumListFlag("definedonbound"));
	else
	  {
	    defon.SetSize(1);
	    defon[0] = flags.GetNumFlag("definedonbound",0);
	  }

	for(int i=0; i< defon.Size(); i++) 
	  if(defon[i] <= ma->GetNBoundaries() && defon[i] > 0)
	    definedon[BND][int(defon[i])-1] = true;
      }
    

    else if(flags.StringListFlagDefined("definedonbound") || flags.StringFlagDefined("definedonbound"))
      {
	if ( definedon[BND].Size() == 0 )
	  {
	    definedon[BND].SetSize (ma->GetNBoundaries());
	    definedon[BND] = false;
	  }

	Array<string*> defon;

	if(flags.StringFlagDefined("definedonbound"))
	  defon.Append(new string(flags.GetStringFlag("definedonbound","")));
	else
	  for(int i=0; i<flags.GetStringListFlag ("definedonbound").Size(); i++)
	    defon.Append(new string(flags.GetStringListFlag("definedonbound")[i]));
	
	for(int selnum = 0; selnum < ma->GetNSE(); selnum++)
	  {
            ElementId sei(BND, selnum);
	    if(definedon[BND][ma->GetElIndex(sei)] == false)
	      {
		for(int i=0; i<defon.Size(); i++)
		  {
		    // if(StringFitsPattern(ma->GetSElBCName(selnum),*(defon[i])))
                    if(StringFitsPattern(ma->GetMaterial(ElementId(BND, selnum)),*(defon[i])))	
		      {		
		 	definedon[BND][ma->GetElIndex(sei)] = true;
			continue;
		      }
		  }
	      }
	  }
	for(int i=0; i<defon.Size(); i++)
	  delete defon[i];
      }
    
    level_updated = -1;


    point = NULL;
    segm = NULL;
    trig = NULL;
    quad = NULL;
    tet = NULL;
    prism = NULL;
    pyramid = NULL;
    hex = NULL;
    
    dummy_tet = new DummyFE<ET_TET>();
    dummy_pyramid = new DummyFE<ET_PYRAMID>();
    dummy_prism = new DummyFE<ET_PRISM>();
    dummy_hex = new DummyFE<ET_HEX>();
    dummy_trig = new DummyFE<ET_TRIG>();
    dummy_quad = new DummyFE<ET_QUAD>();
    dummy_segm = new DummyFE<ET_SEGM>();
    dummy_point = new DummyFE<ET_POINT>();

    for(auto vb : {VOL,BND,BBND})
      {
	evaluator[vb] = nullptr;
	flux_evaluator[vb] = nullptr;
	integrator[vb] = nullptr;
      }
    low_order_space = NULL;
    prol = NULL;


    // element_coloring = NULL;
    // selement_coloring = NULL;
    paralleldofs = NULL;

    ctofdof.SetSize(0);

    for (auto & et : et_bonus_order) et = 0;
    et_bonus_order[ET_QUAD] = int (flags.GetNumFlag("quadbonus",0));
  }

  
  FESpace :: ~FESpace ()
  {
    delete tet;
    delete pyramid;
    delete prism;
    delete hex;
    delete trig;
    delete quad;
    delete segm;
    delete point;

    delete dummy_tet;
    delete dummy_pyramid;
    delete dummy_prism;
    delete dummy_hex;
    delete dummy_trig;
    delete dummy_quad;
    delete dummy_segm;
    delete dummy_point;

    delete paralleldofs;
  }
  
  void FESpace :: SetNDof (size_t _ndof)
  {
    ndof = _ndof;
    while (ma->GetNLevels() > ndof_level.Size())
      ndof_level.Append (ndof);
    ndof_level.Last() = ndof;
  }
  
  void FESpace :: Update(LocalHeap & lh)
  {
    if (print)
      {
 	*testout << "Update FESpace, type = " << typeid(*this).name() << endl;
	*testout << "name = " << name << endl;
      }

    for (int i = 0; i < specialelements.Size(); i++)
      delete specialelements[i]; 
    specialelements.SetSize(0);


    int dim = ma->GetDimension();
    
    dirichlet_vertex.SetSize (ma->GetNV());
    dirichlet_edge.SetSize (ma->GetNEdges());
    if (dim == 3)
      dirichlet_face.SetSize (ma->GetNFaces());
    
    dirichlet_vertex = false;
    dirichlet_edge = false;
    dirichlet_face = false;

    // for clang compatibility ... 
    // #pragma omp parallel    
    {
      if (dirichlet_boundaries.Size())
        for (Ngs_Element ngel : ma->Elements(BND))  // .OmpSplit())
          if (dirichlet_boundaries[ngel.GetIndex()])
            {
              dirichlet_vertex[ngel.Vertices()] = true;
              if (dim >= 2)
                dirichlet_edge[ngel.Edges()] = true;
              if (dim == 3)
                dirichlet_face[ngel.Faces()[0]] = true;
            }
    }

    if (print)
      {
	(*testout) << "Dirichlet_vertex,1 = " << endl << dirichlet_vertex << endl;
	(*testout) << "Dirichlet_edge,1 = " << endl << dirichlet_edge << endl;
	(*testout) << "Dirichlet_face,1 = " << endl << dirichlet_face << endl;
      }


    ma->AllReduceNodalData (NT_VERTEX, dirichlet_vertex, MPI_LOR);
    ma->AllReduceNodalData (NT_EDGE, dirichlet_edge, MPI_LOR);
    ma->AllReduceNodalData (NT_FACE, dirichlet_face, MPI_LOR);
    
    if (print)
      {
	(*testout) << "Dirichlet_vertex = " << endl << dirichlet_vertex << endl;
	(*testout) << "Dirichlet_edge = " << endl << dirichlet_edge << endl;
	(*testout) << "Dirichlet_face = " << endl << dirichlet_face << endl;
      }
  }

  class MyMutex
  {
    atomic<bool> m;
  public:
    MyMutex() { m.store(false, memory_order_relaxed); }
    void lock()
    {
      bool should = false;
      while (!m.compare_exchange_weak(should, true))
        {
          should = false;
          _mm_pause();
        }
    }
    void unlock()
    {
      m = false;
    }
  };
  
  void FESpace :: FinalizeUpdate(LocalHeap & lh)
  {
    static Timer timer ("FESpace::FinalizeUpdate");
    static Timer tcol ("FESpace::FinalizeUpdate - coloring");
    static Timer tcolbits ("FESpace::FinalizeUpdate - bitarrays");
    static Timer tcolmutex ("FESpace::FinalizeUpdate - coloring, init mutex");
    
    if (low_order_space) low_order_space -> FinalizeUpdate(lh);

    RegionTimer reg (timer);

    dirichlet_dofs.SetSize (GetNDof());
    dirichlet_dofs.Clear();

    /*
    if (dirichlet_boundaries.Size())
      for (ElementId ei : ma->Elements<BND>())
	if (dirichlet_boundaries[ma->GetElIndex(ei)])
	  {
	    GetDofNrs (ei, dnums);
	    for (int d : dnums)
	      if (d != -1) dirichlet_dofs.Set (d);
	  }
    */

    if (dirichlet_boundaries.Size())
      for (FESpace::Element el : Elements(BND))
        if (dirichlet_boundaries[el.GetIndex()])
          for (int d : el.GetDofs())
            if (d != -1) dirichlet_dofs.Set (d);

    /*
    Array<DofId> dnums;
    for (auto i : Range(dirichlet_vertex))
      if (dirichlet_vertex[i])
	{
	  GetDofNrs (NodeId(NT_VERTEX,i), dnums);
	  for (DofId d : dnums)
	    if (d != -1) dirichlet_dofs.Set (d);
	}
    */
    ParallelForRange
      (dirichlet_vertex.Size(),
       [&] (IntRange r)
       {
         Array<DofId> dnums;
         for (auto i : r)
           if (dirichlet_vertex[i])
             {
               GetDofNrs (NodeId(NT_VERTEX,i), dnums);
               for (DofId d : dnums)
                 if (d != -1) dirichlet_dofs.Set (d);
             }
       });

    /*
    for (auto i : Range(dirichlet_edge))
      if (dirichlet_edge[i])
	{
	  GetDofNrs (NodeId(NT_EDGE,i), dnums);
	  for (DofId d : dnums)
	    if (d != -1) dirichlet_dofs.Set (d);
	}
    */
    ParallelForRange
      (dirichlet_edge.Size(),
       [&] (IntRange r)
       {
         Array<DofId> dnums;         
         for (auto i : r)
           if (dirichlet_edge[i])
             {
               GetDofNrs (NodeId(NT_EDGE,i), dnums);
               for (DofId d : dnums)
                 if (d != -1) dirichlet_dofs.Set (d);
             }
       });

    Array<DofId> dnums;             
    for (int i : Range(dirichlet_face))
      if (dirichlet_face[i])
	{
	  GetFaceDofNrs (i, dnums);
	  for (DofId d : dnums)
	    if (d != -1) dirichlet_dofs.Set (d);
	}
    
    tcolbits.Start();
    free_dofs = make_shared<BitArray>(GetNDof());
    *free_dofs = dirichlet_dofs;
    free_dofs->Invert();
    
    for (auto i : Range(ctofdof))
      if (ctofdof[i] == UNUSED_DOF)
	free_dofs->Clear(i);

    external_free_dofs = make_shared<BitArray>(GetNDof());
    *external_free_dofs = *free_dofs;
    for (auto i : Range(ctofdof))
      if (ctofdof[i] & LOCAL_DOF)
	external_free_dofs->Clear(i);

    if (print)
      *testout << "freedofs = " << endl << *free_dofs << endl;
    tcolbits.Stop();
    
    UpdateParallelDofs();

    if (print)
      *testout << "coloring ... " << flush;

    if (low_order_space)
      {
	for(auto vb : {VOL, BND, BBND, BBBND})
	  element_coloring[vb] = Table<int>(low_order_space->element_coloring[vb]);
      }
    else
      {
        tcolmutex.Start();
      Array<MyMutex> locks(GetNDof());
      tcolmutex.Stop();
      
      for (auto vb : { VOL, BND, BBND, BBBND })
      {
        /*
        tcol.Start();
        Array<int> col(ma->GetNE(vb));
        col = -1;

        int maxcolor = 0;
        
        int basecol = 0;
        Array<unsigned int> mask(GetNDof());

        size_t cnt = 0, found = 0;
        for (ElementId el : Elements(vb)) { cnt++; (void)el; } // no warning 

        do
          {
            mask = 0;

            Array<DofId> dofs;
            // for (auto el : Elements(vb))
            for (auto el : ma->Elements(vb))
              {
                if (!DefinedOn(el)) continue;
                if (col[el.Nr()] >= 0) continue;

                unsigned check = 0;
                GetDofNrs(el, dofs);
                for (auto d : dofs) // el.GetDofs())
                  if (d != -1) check |= mask[d];

                if (check != UINT_MAX) // 0xFFFFFFFF)
                  {
                    found++;
                    unsigned checkbit = 1;
                    int color = basecol;
                    while (check & checkbit)
                      {
                        color++;
                        checkbit *= 2;
                      }

                    col[el.Nr()] = color;
                    if (color > maxcolor) maxcolor = color;

                    if (HasAtomicDofs())
                      {
                        for (auto d : dofs) // el.GetDofs())
                          if (d != -1 && !IsAtomicDof(d)) mask[d] |= checkbit;
                      }
                    else
                      {
                        for (auto d : dofs) // el.GetDofs())
                          if (d != -1) mask[d] |= checkbit;
                      }
                  }
              }
            
            basecol += 8*sizeof(unsigned int); // 32;
          }
        while (found < cnt);

        tcol.Stop();

        Array<int> cntcol(maxcolor+1);
        cntcol = 0;

        for (ElementId el : Elements(vb))
          cntcol[col[el.Nr()]]++;
        
        Table<int> & coloring = element_coloring[vb];
        coloring = Table<int> (cntcol);

	cntcol = 0;
        for (ElementId el : Elements(vb))
          coloring[col[el.Nr()]][cntcol[col[el.Nr()]]++] = el.Nr();
        */



        tcol.Start();
        Array<int> col(ma->GetNE(vb));
        col = -1;

        int maxcolor = 0;
        
        int basecol = 0;
        Array<unsigned int> mask(GetNDof());

        atomic<int> found(0);
        size_t cnt = 0;
        for (ElementId el : Elements(vb)) { cnt++; (void)el; } // no warning 

        while (found < cnt)
          {
            // mask = 0   | tasks;

            ParallelForRange
              (mask.Size(),
               [&] (IntRange myrange) { mask[myrange] = 0; });

            size_t ne = ma->GetNE(vb);

            ParallelForRange
              (ne, [&] (IntRange myrange)
               {
                 Array<DofId> dofs;
                 size_t myfound = 0;
                 
                 for (size_t nr : myrange)
                   {
                     ElementId el = { vb, nr };
                     if (!DefinedOn(el)) continue;
                     if (col[el.Nr()] >= 0) continue;
                     
                     unsigned check = 0;
                     GetDofNrs(el, dofs);
                     
                     if (HasAtomicDofs())
                       {
                         for (int i = dofs.Size()-1; i >= 0; i--)
                           if (dofs[i] == -1 || IsAtomicDof(dofs[i])) dofs.DeleteElement(i);
                       }
                     else
                       for (int i = dofs.Size()-1; i >= 0; i--)
                         if (dofs[i] == -1) dofs.DeleteElement(i);
                     QuickSort (dofs);   // sort to avoid dead-locks
                     
                     for (auto d : dofs) 
                       locks[d].lock();
                     
                     for (auto d : dofs) 
                       check |= mask[d];
                     
                     if (check != UINT_MAX) // 0xFFFFFFFF)
                       {
                         myfound++;
                         unsigned checkbit = 1;
                         int color = basecol;
                         while (check & checkbit)
                           {
                             color++;
                             checkbit *= 2;
                           }
                         
                         col[el.Nr()] = color;
                         if (color > maxcolor) maxcolor = color;
                         
                         for (auto d : dofs) // el.GetDofs())
                           mask[d] |= checkbit;
                       }
                     
                     for (auto d : dofs) 
                       locks[d].unlock();
                   }
                 found += myfound;
               });
                 
            basecol += 8*sizeof(unsigned int); // 32;
          }

        tcol.Stop();

        Array<int> cntcol(maxcolor+1);
        cntcol = 0;

        for (ElementId el : Elements(vb))
          cntcol[col[el.Nr()]]++;
        
        Table<int> & coloring = element_coloring[vb];
        coloring = Table<int> (cntcol);

	cntcol = 0;
        for (ElementId el : Elements(vb))
          coloring[col[el.Nr()]][cntcol[col[el.Nr()]]++] = el.Nr();
        
        if (print)
          *testout << "needed " << maxcolor+1 << " colors" 
                   << " for " << ((vb == VOL) ? "vol" : "bnd") << endl;
      }
      }
    
    // invalidate facet_coloring
    facet_coloring = Table<int>();
       
    level_updated = ma->GetNLevels();
    if (timing) Timing();
    // CheckCouplingTypes();
  }


  const Table<int> & FESpace :: FacetColoring() const
  {
    if (facet_coloring.Size()) return facet_coloring;

    size_t nf = ma->GetNFacets();
    Array<int> col(nf);
    col = -1;
    
    int maxcolor = 0;
    int basecol = 0;
    Array<unsigned int> mask(GetNDof());

    int cnt = nf, found = 0;
    Array<int> dofs, dofs1, elnums; 
    do
      {
        mask = 0;
        for (size_t f = 0; f < nf; f++)
          {
            if (col[f] >= 0) continue;
            
            ma->GetFacetElements(f,elnums);
            dofs.SetSize0();
            for (int el : elnums)
              {
                GetDofNrs(ElementId(VOL, el), dofs1);
                dofs += dofs1;
              }
            
            unsigned check = 0;
            for (auto d : dofs)
              if (d != -1) check |= mask[d];
            
            if (check != UINT_MAX) // 0xFFFFFFFF)
              {
                found++;
                unsigned checkbit = 1;
                int color = basecol;
                while (check & checkbit)
                  {
                    color++;
                    checkbit *= 2;
                  }
                
                col[f] = color;
                if (color > maxcolor) maxcolor = color;
		
                for (auto d : dofs)
                  if (d != -1) mask[d] |= checkbit;
              }
          }
        
        basecol += 8*sizeof(unsigned int); // 32;
      }
    while (found < cnt);

    Array<int> cntcol(maxcolor+1);
    cntcol = 0;
    
    for (auto f : Range(nf))
      cntcol[col[f]]++;
    
    const_cast<Table<int>&> (facet_coloring) = Table<int> (cntcol);
    
    cntcol = 0;
    for (auto f : Range(nf))          
      facet_coloring[col[f]][cntcol[col[f]]++] = f;

    if (print)
      *testout << "needed " << maxcolor+1 << " colors for facet-coloring" << endl;

    return facet_coloring;
  }
  

  // FiniteElement & FESpace :: GetFE (ElementId ei, Allocator & alloc) const
  // {
  //   LocalHeap & lh = dynamic_cast<LocalHeap&> (alloc);
  //   switch(ei.VB())
  //     {
  //     case VOL:
  //       {
  //       FiniteElement * fe = NULL;
    
  //       if (DefinedOn (ei))
  //         {
  //           switch (ma->GetElType(ei))
  //             {
  //             case ET_TET: fe = tet; break;
  //             case ET_PYRAMID: fe = pyramid; break;
  //             case ET_PRISM: fe = prism; break;
  //             case ET_HEX: fe = hex; break;
  //             case ET_TRIG: fe = trig; break;
  //             case ET_QUAD: fe = quad; break;
  //             case ET_SEGM: fe = segm; break;
  //             case ET_POINT: fe = point; break;
  //             }
  //         }
  //       else
  //         {
  //           switch (ma->GetElType(ei))
  //             {
  //             case ET_TET: fe = dummy_tet; break;
  //             case ET_PYRAMID: fe = dummy_pyramid; break;
  //             case ET_PRISM: fe = dummy_prism; break;
  //             case ET_HEX: fe = dummy_hex; break;
  //             case ET_TRIG: fe = dummy_trig; break;
  //             case ET_QUAD: fe = dummy_quad; break;
  //             case ET_SEGM: fe = dummy_segm; break;
  //             case ET_POINT: fe = dummy_point; break;
  //             }
  //         }

  //       if (!fe)
  //         {
  //           /*
  //             Exception ex;
  //             ex << "FESpace" << GetClassName() << ", undefined eltype " 
  //             << ElementTopology::GetElementName(ma->GetElType(elnr))
  //             << ", order = " << ToString (order) << "\n";
  //             throw ex;
  //           */
  //           stringstream str;
  //           str << "FESpace" << GetClassName() << ", undefined eltype " 
  //               << ElementTopology::GetElementName(ma->GetElType(ei.Nr()))
  //               << ", order = " << ToString (order) << "\n";
  //           throw Exception (str.str());
  //         }
    
  //       return *fe;
  //       }
  //     case BND:
  //       switch (ma->GetElement(ei).GetType())
  //         {
  //         case ET_TRIG:  return *trig; 
  //         case ET_QUAD:  return *quad; 
  //         case ET_SEGM:  return *segm; 
  //         case ET_POINT: return *point; 
  //         case ET_TET: case ET_PYRAMID:
  //         case ET_PRISM: case ET_HEX: 
  //           ;
  //         }
  //       throw Exception ("GetFE BND: unknown type");
  //     case BBND:
  //       switch (ma->GetElement(ei).GetType())
  //         {
  //         case ET_SEGM: return *segm;
  //         case ET_POINT: return *point;
  //         default: ;
  //         }
  //       throw Exception("GetFE BBND: unknown type");
  //     default:
  //       __assume(false);
  //     }
  // }
  

  const FiniteElement & FESpace :: GetFE (int elnr, LocalHeap & lh) const
  {
    return const_cast<FiniteElement&>(GetFE(ElementId(VOL,elnr),lh));
  }

  Table<int> FESpace :: CreateDofTable (VorB vorb) const
  {
    TableCreator<int> creator;
    
    for ( ; !creator.Done(); creator++)
      for (FESpace::Element el : Elements(vorb))
        creator.Add(el.Nr(), el.GetDofs());

    return creator.MoveTable();
  }

  /// get coupling type of dof
  COUPLING_TYPE FESpace :: GetDofCouplingType (DofId dof) const 
  {
    if (ctofdof.Size()==0) //this is the standard case if the FESpace does not specify anything.
      return WIREBASKET_DOF;
    if (dof >= ctofdof.Size()) throw Exception("FESpace::GetDofCouplingType out of range. Did you set up ctofdof?");
    return ctofdof[dof];
  }

  void FESpace :: SetDofCouplingType (DofId dof, COUPLING_TYPE ct) const
  {
    if (dof >= ctofdof.Size()) throw Exception("FESpace::SetDofCouplingType out of range");
    ctofdof[dof] = ct;
  }
  
  
  /// get coupling types of dofs
  void  FESpace :: GetDofCouplingTypes (int elnr, Array<COUPLING_TYPE> & ctypes) const
  { 
    ArrayMem<int,100> dnums;
    GetDofNrs(ElementId(VOL,elnr), dnums);
    ctypes.SetSize(dnums.Size());

    if (ctofdof.Size()==0)
      ctypes = INTERFACE_DOF;
    else
      {
        for (int i = 0; i < dnums.Size(); i++)
          if (dnums[i] != -1)
            ctypes[i] = ctofdof[dnums[i]];
          else
            ctypes[i] = UNUSED_DOF;
      }
  }

  void FESpace :: CheckCouplingTypes() const
  {
    cout << "checking coupling-types, type = " << typeid(*this).name() << endl;
    int ndof = GetNDof();
    if (ctofdof.Size() != ndof) 
      cout << "ndof = " << ndof
           << ", but couplingtype.size = " << ctofdof.Size() << endl;

    Array<int> cnt(ndof);
    cnt = 0;

    Array<int> dnums;
    for (ElementId id : ma->Elements<VOL>())
      {
        GetDofNrs(id, dnums);
        for (auto d : dnums) cnt[d]++;
      }
    for (int i : IntRange(0,ndof))
      {
        if (cnt[i] == 0 && ctofdof[i] != UNUSED_DOF)
          cout << "dof " << i << " not used, but coupling-type = " << ctofdof[i] << endl;
      }


    cout << "check dofs" << endl;
    for(auto vb : {VOL,BND,BBND})
      for (ElementId id : ma->Elements(vb))
	{
	  GetDofNrs (id, dnums);
	  for (auto d : dnums)
	    if (d < 0 || d >= ndof)
	      cout << "dof out of range: " << d << endl;
	}
  }


  void FESpace :: GetDofNrs (int elnr, Array<int> & dnums, COUPLING_TYPE ctype) const
  {
    ArrayMem<int,100> alldnums; 
    GetDofNrs(ElementId(VOL,elnr), alldnums);

    dnums.SetSize(0);
    if (ctofdof.Size() == 0)
      {
	if ( (INTERFACE_DOF & ctype) != 0)
          dnums = alldnums;
      }
    else
      {
        /*
	for (int i = 0; i < alldnums.Size(); i++)
	  if ( (ctofdof[alldnums[i]] & ctype) != 0)
	    dnums.Append(alldnums[i]);
        */
        for (auto d : alldnums)
	  if ( (d != -1) && ((ctofdof[d] & ctype) != 0) )
            dnums.Append(d);
      }
  }

  void FESpace :: GetNodeDofNrs (NODE_TYPE nt, int nr, Array<int> & dnums) const
  {
    GetDofNrs(NodeId(nt,nr),dnums);
  }

  void FESpace :: GetDofNrs (NodeId ni, Array<int> & dnums) const
  {
    switch (ni.GetType())
      {
      case NT_VERTEX: GetVertexDofNrs(ni.GetNr(), dnums); break;
      case NT_EDGE:   GetEdgeDofNrs(ni.GetNr(), dnums); break;
      case NT_FACE:   
        if (ma->GetDimension() == 3)
          GetFaceDofNrs(ni.GetNr(), dnums); 
        else
          {
            // have to convert from face# to element# on refined meshes
            auto surfel = ma->GetNode<2>(ni.GetNr()).surface_el;
            if (surfel >= 0)
              GetInnerDofNrs(surfel, dnums);
            else
              dnums.SetSize0();
          }
        break;
      case NT_CELL:   GetInnerDofNrs(ni.GetNr(), dnums); break;
      case NT_ELEMENT: case NT_FACET:
        GetDofNrs (NodeId(StdNodeType(ni.GetType(), ma->GetDimension()), ni.GetNr()), dnums);
        break;
      }
  }
  
  void FESpace :: GetVertexDofNrs (int vnr, Array<int> & dnums) const
  {
    dnums.SetSize0 ();
  }

  void FESpace :: GetEdgeDofNrs (int ednr, Array<int> & dnums) const
  {
    dnums.SetSize0 ();
  }

  void FESpace :: GetFaceDofNrs (int fanr, Array<int> & dnums) const
  {
    dnums.SetSize0 ();
  }

  void FESpace :: GetInnerDofNrs (int elnr, Array<int> & dnums) const
  {
    dnums.SetSize0 ();
  }



  shared_ptr<BilinearFormIntegrator> FESpace :: GetIntegrator (VorB vb) const
  {
    if (integrator[vb])
      return integrator[vb];

    /*
        auto single_evaluator =  flux_evaluator;
        if (dynamic_pointer_cast<BlockDifferentialOperator>(single_evaluator))
          single_evaluator = dynamic_pointer_cast<BlockDifferentialOperator>(single_evaluator)->BaseDiffOp();
        
        auto trial = make_shared<ProxyFunction>(false, false, single_evaluator,
                                                nullptr, nullptr, nullptr, nullptr, nullptr);
        auto test  = make_shared<ProxyFunction>(true, false, single_evaluator,
                                                nullptr, nullptr, nullptr, nullptr, nullptr);
        fluxbli = make_shared<SymbolicBilinearFormIntegrator> (InnerProduct(trial,test), vb, false);
        single_fluxbli = fluxbli;
    */
    auto evaluator = GetEvaluator(vb);
    if (!evaluator) return nullptr;
    
    bool is_block = false;
    int block_dim;
    auto block_evaluator = dynamic_pointer_cast<BlockDifferentialOperator> (evaluator);
    if (block_evaluator)
      {
        is_block = true;
        block_dim = block_evaluator->BlockDim();
        evaluator = block_evaluator->BaseDiffOp();
      }
    auto trial = make_shared<ProxyFunction>(false, false, evaluator,
                                            nullptr, nullptr, nullptr, nullptr, nullptr);
    auto test  = make_shared<ProxyFunction>(true, false, evaluator,
                                            nullptr, nullptr, nullptr, nullptr, nullptr);
    shared_ptr<BilinearFormIntegrator> bli =
      make_shared<SymbolicBilinearFormIntegrator> (InnerProduct(trial,test), vb, false);
    
    if (is_block)
      bli = make_shared<BlockBilinearFormIntegrator> (bli, block_dim);
    const_cast<shared_ptr<BilinearFormIntegrator>&> (integrator[vb]) = bli;
    return bli;
  }

  const FiniteElement & FESpace :: GetSFE (int selnr, LocalHeap & lh) const
  {
    return const_cast<FiniteElement&>(GetFE(ElementId(BND,selnr),lh));
  }

  const FiniteElement & FESpace :: GetCD2FE (int cd2elnr, LocalHeap & lh) const
  {
    return const_cast<FiniteElement&>(GetFE(ElementId(BBND,cd2elnr),lh));
  }

  const FiniteElement & FESpace :: GetFE (ELEMENT_TYPE type) const
  {
    switch (type)
      {
      case ET_SEGM:  return *segm;
      case ET_TRIG:  return *trig;
      case ET_QUAD:  return *quad;
      case ET_TET:   return *tet;
      case ET_PYRAMID: return *pyramid;
      case ET_PRISM: return *prism;
      case ET_HEX:   return *hex;
      case ET_POINT: return *point;
      }
    throw Exception ("GetFE: unknown type");
  }

  
  void FESpace :: PrintReport (ostream & ost) const
  {
    ost << "type  = " << GetClassName() << endl
	<< "order = " << order << endl
	<< "dim   = " << dimension << endl
	<< "dgjmps= " << dgjumps << endl
	<< "complex = " << iscomplex << endl;
    ost << "definedon = " << definedon[VOL] << endl;
    ost << "definedon boundary = " << definedon[BND] << endl;
    ost << "definedon codim 2 = " << definedon[BBND] << endl;
    if (!free_dofs) return;

    ost << "ndof = " << GetNDof() << endl;
    int ntype[8] = { 0 };
    // for (int i = 0; i < ctofdof.Size(); i++)
    // ntype[ctofdof[i]]++;
    for (auto ct : ctofdof) ntype[ct]++;
    if (ntype[UNUSED_DOF]) ost << "unused = " << ntype[UNUSED_DOF] << endl;
    if (ntype[LOCAL_DOF])  ost << "local  = " << ntype[LOCAL_DOF] << endl;

    int nfree = 0;
    for (int i = 0; i < free_dofs->Size(); i++)
      if ((*free_dofs)[i])
	nfree++;
  }
  
  void FESpace :: DoArchive (Archive & archive)
  {
    archive & order & dimension & iscomplex & dgjumps & print & level_updated;
    archive & definedon[VOL] & definedon[BND] & definedon[BBND];
    archive & dirichlet_boundaries & dirichlet_dofs & free_dofs & external_free_dofs;
    archive & dirichlet_vertex & dirichlet_edge & dirichlet_face;
  }

  /*
  size_t FESpace :: GetNDofLevel (int level) const
  {
    return GetNDof();
  } 
  */
  
  std::list<std::tuple<std::string,double>> FESpace :: Timing () const
  {
    double starttime;
    double time;
    std::list<std::tuple<std::string,double>> results;
    LocalHeap lh (100000, "FESpace - Timing");

    // cout << endl << "timing fespace " << GetName() 
    //      << (low_order_space ? "" : " low-order")
    //      << " ..." << endl;

    time = RunTiming([&]() {
        ParallelForRange( IntRange(ma->GetNE()), [&] ( IntRange r )
        {
	  LocalHeap &clh = lh, lh = clh.Split();
          Array<int> dnums;
	  for (int i : r)
            GetDofNrs (ElementId(VOL,i), dnums);
	});
    });
    results.push_back(std::make_tuple<std::string,double>("GetDofNrs",1e9*time / (ma->GetNE())));

    time = RunTiming([&]() {
        ParallelForRange( IntRange(ma->GetNE()), [&] ( IntRange r )
        {
	  LocalHeap &clh = lh, lh = clh.Split();

	  for (int i : r)
	    {
	      HeapReset hr(lh);
	      GetFE (ElementId(VOL,i), lh);
	    }
	});
    });
    results.push_back(std::make_tuple<std::string,double>("GetFE",1e9 * time / (ma->GetNE()))); 

    time = RunTiming([&]() {
        ParallelFor( IntRange(ma->GetNE()), [&] (size_t i)
          {
	    /* Ng_Element ngel = */ ma->GetElement(ElementId(VOL,i));
          });
    });
    results.push_back(std::make_tuple<std::string,double>("Get Ng_Element", 1e9 * time / (ma->GetNE())));


    time = RunTiming([&]() {
        ParallelForRange( IntRange(ma->GetNE()), [&] ( IntRange r )
        {
	  LocalHeap &clh = lh, lh = clh.Split();
          for (int i : r)
            {
              HeapReset hr(lh);
              /* ElementTransformation & trafo = */ ma->GetTrafo(ElementId(VOL, i), lh);
            }
        });
    });
    results.push_back(std::make_tuple<std::string,double>("GetTrafo", 1e9 * time / (ma->GetNE())));


    Array<int> global(GetNDof());
    global = 0;
    time = RunTiming([&]() {
        ParallelForRange( IntRange(ma->GetNE()), [&] ( IntRange r )
        {
          Array<DofId> dnums;
          for (size_t i : r)
            {
              GetDofNrs( { VOL, i }, dnums);
              for (auto d : dnums)
                {
                // global[d]++;
                  AsAtomic (global[d])++;
                  AsAtomic (global[d])++;
                  // int oldval = 0;
                  // AsAtomic (global[d]).compare_exchange_strong (oldval, 1);
                  // AsAtomic (global[d]).compare_exchange_strong (oldval, 0);
                }
                                                                
            }
        });
    });
    results.push_back(std::make_tuple<std::string,double>("Count els of dof", 1e9 * time / (ma->GetNE())));


    
#ifdef TIMINGSEQUENTIAL


    starttime = WallTime();
    int steps = 0;
    do
      {
        for (int i = 0; i < ma->GetNE(); i++)
	  {
	    ArrayMem<int,100> dnums;
	    GetDofNrs (i, dnums);
	  }
        steps++;
        time = WallTime()-starttime;
      }
    while (time < 2.0);
    
    cout << 1e9*time / (ma->GetNE()*steps) << " ns per GetDofNrs" << endl;

    // ***************************

    starttime = WallTime();
    steps = 0;
    do
      {
        for (int i = 0; i < ma->GetNE(); i++)
	  {
            HeapReset hr(lh);
	    /* FlatArray<int> dnums = */ GetDofNrs (ElementId(VOL, i), lh);
	  }
        steps++;
        time = WallTime()-starttime;
      }
    while (time < 2.0);
    
    cout << 1e9*time / (ma->GetNE()*steps) << " ns per GetDofNrs(lh)" << endl;

    // ***************************


    starttime = WallTime();
    steps = 0;
    do
      {
        for (int i = 0; i < ma->GetNE(); i++)
          {
            HeapReset hr(lh);
            GetFE (i, lh);
          }
        steps++;
        time = WallTime()-starttime;
      }
    while (time < 2.0);
    
    cout << 1e9 * time / (ma->GetNE()*steps) << " ns per GetFE" << endl;


    starttime = WallTime();
    steps = 0;
    do
      {
        for (int i = 0; i < ma->GetNE(); i++)
          {
	    /* Ng_Element ngel = */ ma->GetElement(i);
          }
        steps++;
        time = WallTime()-starttime;
      }
    while (time < 2.0);
    
    cout << 1e9 * time / (ma->GetNE()*steps) << " ns per Get - Ng_Element" << endl;


    starttime = WallTime();
    steps = 0;
    do
      {
        for (int i = 0; i < ma->GetNE(); i++)
          {
            HeapReset hr(lh);
            /* ElementTransformation & trafo = */ ma->GetTrafo(i, VOL, lh);
          }
        steps++;
        time = WallTime()-starttime;
      }
    while (time < 2.0);
    
    cout << 1e9 * time / (ma->GetNE()*steps) << " ns per GetTrafo" << endl;

#endif

    return results;

  }



  void FESpace :: GetFilteredDofs (COUPLING_TYPE doffilter, BitArray & output, 
                                   bool freedofsonly) const
  {
    int ndof = GetNDof();
    output.SetSize(ndof);
    output.Clear();
    if (ctofdof.Size()>0)
      for (int i = 0; i < ndof; i++)
	if ((ctofdof[i] & doffilter) != 0)
	  output.Set(i);
    if (freedofsonly && free_dofs->Size()) {
      output.And(*free_dofs);
    }
  }



  shared_ptr<Table<int>> FESpace :: CreateSmoothingBlocks (const Flags & flags) const
  {
    size_t nd = GetNDof();
    TableCreator<int> creator;

    for ( ; !creator.Done(); creator++)
      {
	for (size_t i = 0; i < nd; i++)
	  if (!IsDirichletDof(i))
	    creator.Add (i, i);
      }
    // return shared_ptr<Table<int>> (creator.GetTable());
    return make_shared<Table<int>> (creator.MoveTable());
  }

    
  void FESpace :: SetDefinedOn (VorB vb, const BitArray & defon)
  {
    definedon[vb].SetSize(defon.Size());
    for (int i = 0; i < defon.Size(); i++)
      definedon[vb][i] = defon.Test(i);

    if (low_order_space)
      low_order_space -> SetDefinedOn (vb, defon);
  }

  void FESpace :: SetDirichletBoundaries (const BitArray & dirbnds)
  {
    dirichlet_boundaries = dirbnds;
    if (low_order_space)
      low_order_space -> SetDirichletBoundaries (dirbnds);
  }


  shared_ptr<BitArray> FESpace :: GetFreeDofs (bool external) const
  {
    if (external)
      return external_free_dofs;
    else
      return free_dofs;
  }

  void IterateElements (const FESpace & fes, 
			VorB vb, 
			LocalHeap & clh, 
			const function<void(FESpace::Element,LocalHeap&)> & func)
  {
    static mutex copyex_mutex;
    const Table<int> & element_coloring = fes.ElementColoring(vb);
    
    if (task_manager)
      {
        for (FlatArray<int> els_of_col : element_coloring)
          {
            SharedLoop2 sl(els_of_col.Range());

            task_manager -> CreateJob
              ( [&] (const TaskInfo & ti) 
                {
                  LocalHeap lh = clh.Split(ti.thread_nr, ti.nthreads);
                  ArrayMem<int,100> temp_dnums;

                  for (int mynr : sl)
                    {
                      HeapReset hr(lh);
                      FESpace::Element el(fes, 
                                          ElementId (vb, els_of_col[mynr]), 
                                          temp_dnums, lh);
                      
                      func (move(el), lh);
                    }

                  ProgressOutput::SumUpLocal();
                } );
          }
        return;
      }


    Exception * ex = nullptr;

    for (FlatArray<int> els_of_col : element_coloring)
      ParallelForRange( IntRange(els_of_col.Size()), [&] ( IntRange r )
      {
        LocalHeap lh = clh.Split();
        Array<int> temp_dnums;

        // lh.ClearValues();
        
        for (int i : r)
          {
            try
              {
                HeapReset hr(lh);
                FESpace::Element el(fes, ElementId (vb, els_of_col[i]), temp_dnums, lh);
                
                func (move(el), lh);
              }
	    
            catch (const Exception & e)
              {
                {
                  lock_guard<mutex> guard(copyex_mutex);
                  if (!ex)
		    ex = new Exception (e);
                }
              }
	    catch (...)
	      { ; }
          }
      // cout << "lh, used size = " << lh.UsedSize() << endl;
    });
    
    if (ex)
      {
        throw Exception (*ex);
      }
  }
  

  // Aendern, Bremse!!!
  template < int S, class T >
  void FESpace :: TransformVec (int elnr, VorB vb,
				const FlatVector< Vec<S,T> >& vec, TRANSFORM_TYPE type) const
  {
    //cout << "Achtung, Bremse" << endl;

    Vector<T> partvec(vec.Size());

    for(int i=0; i<S; i++)
      {
	for(int j=0;j<vec.Size(); j++)
	  partvec(j) = vec[j](i);

	TransformVec(ElementId(vb, elnr),partvec,type);

	for(int j=0;j<vec.Size(); j++)
	  vec[j](i) = partvec(j);
      }
  }

  template void FESpace::TransformVec(int elnr, VorB vb,
				      const FlatVector< Vec<2,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
				      const FlatVector< Vec<3,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
				      const FlatVector< Vec<4,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
				      const FlatVector< Vec<5,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
				      const FlatVector< Vec<6,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
				      const FlatVector< Vec<7,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<8,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<9,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<10,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<11,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<12,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<13,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<14,double> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<15,double> >& vec, TRANSFORM_TYPE type) const;

  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<2,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<3,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<4,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<5,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<6,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<7,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<8,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<9,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<10,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<11,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<12,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<13,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<14,Complex> >& vec, TRANSFORM_TYPE type) const;
  template void FESpace::TransformVec(int elnr, VorB vb,
					const FlatVector< Vec<15,Complex> >& vec, TRANSFORM_TYPE type) const;


  ostream & operator<< (ostream & ost, COUPLING_TYPE ct)
  {
    switch (ct)
      {
      case UNUSED_DOF: ost << "unused"; break;
      case LOCAL_DOF:  ost << "local"; break;
      case INTERFACE_DOF: ost << "interface"; break;
      case NONWIREBASKET_DOF: ost << "non-wirebasket"; break;
      case WIREBASKET_DOF: ost << "wirebasket"; break;
      case EXTERNAL_DOF: ost << "external"; break;
      case ANY_DOF: ost << "any"; break;
      };
    return ost;
  }

  void FESpace :: SolveM (CoefficientFunction & rho, BaseVector & vec,
                          LocalHeap & lh) const
  {
    cout << "SolveM is only available for L2-space, not for " << typeid(*this).name() << endl;
  }

  void FESpace :: UpdateParallelDofs ( )
  {
    if (MyMPI_GetNTasks() == 1) return;

    Array<NodeId> dofnodes (GetNDof());
    dofnodes = NodeId (NT_VERTEX, -1);
    Array<int> dnums;

    // for (NODE_TYPE nt = NT_VERTEX; nt <= NT_CELL; nt++)
    for (NODE_TYPE nt : { NT_VERTEX, NT_EDGE, NT_FACE, NT_CELL })
      for ( int nr = 0; nr < ma->GetNNodes (nt); nr++ )
	{
	  GetDofNrs (NodeId(nt, nr), dnums);
	  for (auto d : dnums)
	    dofnodes[d] = NodeId (nt, nr);
	} 

    paralleldofs = new ParallelMeshDofs (ma, dofnodes, dimension, iscomplex);

    if (MyMPI_AllReduce (ctofdof.Size(), MPI_SUM))
      AllReduceDofData (ctofdof, MPI_MAX, GetParallelDofs());
  }


  bool FESpace :: IsParallel() const
  { 
    return paralleldofs != NULL; 
  }

  size_t FESpace :: GetNDofGlobal() const 
  { 
    return paralleldofs ?
      paralleldofs -> GetNDofGlobal() : GetNDof(); 
  }





  NodalFESpace :: NodalFESpace (shared_ptr<MeshAccess> ama,
				const Flags & flags,
                                bool parseflags)
    : FESpace (ama, flags)
  {
    name="NodalFESpace";
    
    prol = make_shared<LinearProlongation> (*this);

    if (order >= 2)
      {
	Flags loflags;
	loflags.SetFlag ("order", 1);
	loflags.SetFlag ("dim", dimension);
	if (dgjumps) loflags.SetFlag ("dgjumps");
	if (iscomplex) loflags.SetFlag ("complex");
	low_order_space = make_shared<NodalFESpace> (ma, loflags);
      }
    hb_defined = flags.GetDefineFlag("hb");

    SetDummyFE<ScalarDummyFE> ();

    auto one = make_shared<ConstantCoefficientFunction> (1);
    if (ma->GetDimension() == 2)
      {
	integrator[VOL] = make_shared<MassIntegrator<2>> (one);
        integrator[BND] = make_shared<RobinIntegrator<2>> (one);
      }
    else
      {
	integrator[VOL] = make_shared<MassIntegrator<3>> (one);
	integrator[BND] = make_shared<RobinIntegrator<3>> (one);
      }
    
    if (dimension > 1)
      {
	integrator[VOL] = make_shared<BlockBilinearFormIntegrator> (integrator[VOL], dimension);
	integrator[BND] = make_shared<BlockBilinearFormIntegrator> (integrator[BND], dimension);  
      }


    switch (ma->GetDimension())
      {
      case 1:
        {
          evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<1>>>();
	  evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<1>>>();
	  flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpGradient<1>>>();
          break;
        }
      case 2:
        {
          evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<2>>>();
	  evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<2>>>();
          flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpGradient<2>>>();
	  flux_evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpGradientBoundary<2>>>();
          break;
        }
      case 3:
        {
          evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<3>>>();
	  evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<3>>>();
          flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpGradient<3>>>();
	  flux_evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpGradientBoundary<3>>>();
          break;
        }
      }
    if (dimension > 1)
      {
	evaluator[VOL] = make_shared<BlockDifferentialOperator> (evaluator[VOL], dimension);
	evaluator[BND] = make_shared<BlockDifferentialOperator> (evaluator[BND], dimension);
	flux_evaluator[VOL] = make_shared<BlockDifferentialOperator> (flux_evaluator[VOL], dimension);
	flux_evaluator[BND] = make_shared<BlockDifferentialOperator> (flux_evaluator[BND], dimension);
      }

    
  }

  NodalFESpace :: ~NodalFESpace ()
  {
    ;
  }

  FiniteElement & NodalFESpace :: GetFE(ElementId ei, Allocator & lh) const
  {
    if(order == 1)
      {
        switch (ma->GetElType(ei))
          {
          case ET_SEGM:    return *(new (lh) FE_Segm1);
          case ET_TRIG:    return *(new (lh) ScalarFE<ET_TRIG,1>);
          case ET_QUAD:    return *(new (lh) ScalarFE<ET_QUAD,1>);
          case ET_TET:     return *(new (lh) ScalarFE<ET_TET,1>);
          case ET_PRISM:   return *(new (lh) FE_Prism1);
          case ET_PYRAMID: return *(new (lh) FE_Pyramid1);
          case ET_HEX:     return *(new (lh) FE_Hex1);
          case ET_POINT:   return *(new (lh) FE_Point);
            throw Exception ("Inconsistent element type in NodalFESpace::GetFE");
          }
      }
    else
      {
        if(hb_defined)
          {
            switch (ma->GetElType(ei))
              {
              case ET_TET:     return *(new (lh) FE_Tet2HB);
              case ET_PRISM:   return *(new (lh) FE_Prism1);
              case ET_PYRAMID: return *(new (lh) FE_Pyramid1);
              case ET_TRIG:    return *(new (lh) FE_Trig2HB);
              case ET_QUAD:    return *(new (lh) ScalarFE<ET_QUAD,1>);
              case ET_SEGM:    return *(new (lh) FE_Segm2);
              case ET_POINT:   return *(new (lh) FE_Point);
              default:
                throw Exception ("Inconsistent element type in NodalFESpace::GetFE, hb defined");
              }
          }
        else
          {
            switch (ma->GetElType(ei))
              {
              case ET_TET:     return *(new (lh) FE_Tet2);
              case ET_PRISM:   return *(new (lh) FE_Prism1);
              case ET_PYRAMID: return *(new (lh) FE_Pyramid1);
              case ET_TRIG:    return *(new (lh) FE_Trig2);
              case ET_QUAD:    return *(new (lh) ScalarFE<ET_QUAD,1>);
              case ET_SEGM:    return *(new (lh) FE_Segm2);
              case ET_POINT:   return *(new (lh) FE_Point);
              default:
                throw Exception ("Inconsistent element type in NodalFESpace::GetFE, no hb defined");
              }
          }
      }
  }

  /*
  size_t NodalFESpace :: GetNDof () const throw()
  {
    return ndlevel.Last();
  }
  */
  
  void NodalFESpace :: Update(LocalHeap & lh)
  {
    FESpace :: Update (lh);
    if (low_order_space) low_order_space -> Update(lh);

    // if (ma->GetNLevels() > ndlevel.Size())
      {
	int ndof = ma->GetNV();

        for (auto el : Elements(VOL))
          for (DofId d : el.GetDofs()) ndof = max2(ndof, d+1);              

        for (auto el : Elements(BND))
          for (DofId d : el.GetDofs()) ndof = max2(ndof, d+1);           

	// ndlevel.Append (ndof);
        SetNDof(ndof);
      }

    prol->Update();

    if (dirichlet_boundaries.Size())
      {
	dirichlet_dofs.SetSize (GetNDof());
	dirichlet_dofs.Clear();
	for (auto el : Elements(BND))
          if (dirichlet_boundaries[el.GetIndex()])
            for (int d : el.GetDofs())
              if (d != -1) dirichlet_dofs.Set (d);
      }
  }

  void NodalFESpace :: DoArchive (Archive & archive)
  {
    ;
    /*
    if (archive.Input())
      {
	ndlevel.SetSize(1);
	ndlevel[0] = ma->GetNV();
      }
    */
  }

  /*
  size_t NodalFESpace :: GetNDofLevel (int level) const
  {
    return ndlevel[level];
  }
  */


 
  void NodalFESpace :: GetDofNrs (ElementId ei, Array<DofId> & dnums) const
  {
    dnums = ma->GetElement(ei).Vertices();

    if (!DefinedOn (ei)) dnums = -1;
  }



  void NodalFESpace :: GetVertexDofNrs (int vnr, Array<int> & dnums) const
  {
    dnums.SetSize(1);
    dnums[0] = vnr;
  }

  void NodalFESpace :: GetEdgeDofNrs (int ednr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
  }

  void NodalFESpace :: GetFaceDofNrs (int fanr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
  }

  void NodalFESpace :: GetInnerDofNrs (int elnr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
  }

  
  Array<int> * 
  NodalFESpace :: CreateDirectSolverClusters (const Flags & flags) const
  {
    Array<int> & clusters = *new Array<int> (GetNDof());
    clusters = 0;

    const int stdoffset = 1;

    /*
    (*testout) << "directvertexclusters" << directvertexclusters << endl;
    (*testout) << "directedgeclusters" << directedgeclusters << endl;
    (*testout) << "directfaceclusters" << directfaceclusters << endl;
    (*testout) << "directelementclusters" << directelementclusters << endl;
    */

    int i;

    for(i=0; i<directvertexclusters.Size(); i++)
      if(directvertexclusters[i] >= 0)
	clusters[i] = directvertexclusters[i] + stdoffset;


    bool nonzero = false;
    for (i = 0; !nonzero && i < clusters.Size(); i++)
      if (clusters[i]) nonzero = true;
    if (!nonzero)
      {
	delete &clusters;
	return 0;
      }

    return &clusters;
  }








  NonconformingFESpace :: 
  NonconformingFESpace (shared_ptr<MeshAccess> ama, const Flags & flags, bool parseflags)
    : FESpace (ama, flags)
  {
    name="NonconformingFESpace(nonconforming)";
    // defined flags
    DefineDefineFlag("nonconforming");
    if (parseflags) CheckFlags(flags);
    
    // prol = new LinearProlongation(*this);
    

    // trig = new FE_NcTrig1;
    // segm = new FE_Segm0;
      
    auto one = make_shared<ConstantCoefficientFunction> (1);
    if (ma->GetDimension() == 2)
      {
	integrator[VOL] = make_shared<MassIntegrator<2>> (one);
        integrator[BND] = make_shared<RobinIntegrator<2>> (one);
        evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<2>>>();
        flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpGradient<2>>>();
        evaluator[BND] = make_shared<T_DifferentialOperator<DiffOpIdBoundary<2>>>();
      }
    else
      {
	integrator[VOL].reset (new MassIntegrator<3> (new ConstantCoefficientFunction(1)));
	integrator[BND].reset (new RobinIntegrator<3> (new ConstantCoefficientFunction(1)));
      }

    if (dimension > 1)
      {
	integrator[VOL] = make_shared<BlockBilinearFormIntegrator> (integrator[VOL], dimension);
	integrator[BND] = make_shared<BlockBilinearFormIntegrator> (integrator[BND], dimension);
      }
  }

  NonconformingFESpace :: ~NonconformingFESpace ()
  {
    ;
  }

  FiniteElement & NonconformingFESpace :: GetFE (ElementId ei, Allocator & lh) const
  {
    switch (ma->GetElType(ei))
      {
      case ET_TRIG: return *(new (lh)FE_NcTrig1);
      case ET_SEGM: return *(new (lh) FE_Segm0);
      default: throw Exception ("Element type not available in NonconformingFESpace::GetFE");
      }
  }
  size_t NonconformingFESpace :: GetNDof () const throw()
  {
    return ma->GetNEdges();
  }


  void NonconformingFESpace :: Update(LocalHeap & lh)
  {
    ctofdof.SetSize (ma->GetNFacets());
    ctofdof = UNUSED_DOF;
    for (auto el : ma->Elements(VOL))
      ctofdof[el.Facets()] = WIREBASKET_DOF;
    
    /*
    if (ma->GetNLevels() > ndlevel.Size())
      {
	Array<int> dnums;
	int i, j;
	int ne = ma->GetNE();
	int nse = ma->GetNSE();
	int ndof = 0;
	for (i = 0; i < ne; i++)
	  {
	    GetDofNrs (i, dnums);
	    for (j = 0; j < dnums.Size(); j++)
	      if (dnums[j] > ndof)
		ndof = dnums[j];
	  }
	for (i = 0; i < nse; i++)
	  {
	    GetSDofNrs (i, dnums);
	    for (j = 0; j < dnums.Size(); j++)
	      if (dnums[j] > ndof)
		ndof = dnums[j];
	  }

	ndlevel.Append (ndof+1);
      }

    prol->Update();

    if (dirichlet_boundaries.Size())
      {
	dirichlet_dofs.SetSize (GetNDof());
	dirichlet_dofs.Clear();
	Array<int> dnums;
	for (int i = 0; i < ma->GetNSE(); i++)
	  {
	    if (dirichlet_boundaries[ma->GetSElIndex(i)])
	      {
		GetSDofNrs (i, dnums);
		for (int j = 0; j < dnums.Size(); j++)
		  if (dnums[j] != -1)
		    dirichlet_dofs.Set (dnums[j]);
	      }
	  }
      }
    */
  }

  void NonconformingFESpace :: GetDofNrs (ElementId ei, Array<int> & dnums) const
  {
    dnums = ma->GetElEdges(ei);
    if (!DefinedOn(ei))
      dnums = -1;
  }







  ElementFESpace :: ElementFESpace (shared_ptr<MeshAccess> ama, const Flags& flags, 
                                    bool parseflags)
    : FESpace (ama, flags)
  {
    name="ElementFESpace(l2)";
    if (parseflags) CheckFlags(flags);
    
    order = int(flags.GetNumFlag ("order", 0));

    prol = make_shared<ElementProlongation> (*this);

    if (order == 0)
    {
      // tet     = new ScalarFE<ET_TET,0>;
      // prism   = new FE_Prism0;
      // pyramid = new FE_Pyramid0;
      // hex     = new FE_Hex0;
      // trig    = new ScalarFE<ET_TRIG,0>;
      // quad    = new ScalarFE<ET_QUAD,0>;
      // segm    = new FE_Segm0;

      n_el_dofs = 1;
    }
    else
    {
      // tet     = new ScalarFE<ET_TET,1>;
      // prism   = new FE_Prism1;
      // pyramid = new FE_Pyramid1;
      // trig    = new ScalarFE<ET_TRIG,1>;
      // quad    = new ScalarFE<ET_QUAD,1>;
      // segm    = new FE_Segm1;

      if (ma->GetDimension() == 2)
        n_el_dofs = 4;
      else
        n_el_dofs = 6;
    }

    SetDummyFE<ScalarDummyFE> ();
    static ConstantCoefficientFunction one(1);

    if (ma->GetDimension() == 2)
      {
        integrator[VOL].reset (new MassIntegrator<2> (&one));
        integrator[BND] = 0;
        evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<2>>>();
      }
    else
      {
        integrator[VOL].reset (new MassIntegrator<3> (&one));
	integrator[BND] = 0;
        evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpId<3>>>();
      }
    
    if (dimension > 1)
      integrator[VOL] = make_shared<BlockBilinearFormIntegrator> (integrator[VOL], dimension);
  }
  
  ElementFESpace :: ~ElementFESpace ()
  {
    ;
  }

  void ElementFESpace :: Update(LocalHeap & lh)
  {
    /*
    while (ma->GetNLevels() > ndlevel.Size())
      ndlevel.Append (n_el_dofs * ma->GetNE());
    */
    SetNDof (n_el_dofs * ma->GetNE());
  }

  void ElementFESpace :: DoArchive (Archive & archive)
  {
    FESpace :: DoArchive (archive);
    archive /* & ndlevel */ & n_el_dofs;
  }

  FiniteElement & ElementFESpace :: GetFE (ElementId ei, Allocator & lh) const
  {
    
    if (order == 0)
      {
        switch (ma->GetElType(ei))
          {
          case ET_TET:     return * new (lh) ScalarFE<ET_TET,0>;
          case ET_PRISM:   return * new (lh) FE_Prism0;
          case ET_PYRAMID: return * new (lh) FE_Pyramid0;
          case ET_HEX:     return * new (lh) FE_Hex0;
          case ET_TRIG:    return * new (lh) ScalarFE<ET_TRIG,0>;
          case ET_QUAD:    return * new (lh) ScalarFE<ET_QUAD,0>;
          case ET_SEGM:    return * new (lh) FE_Segm0;
          case ET_POINT:   return * new (lh) FE_Point;
          }
      }
    else
      {
        switch (ma->GetElType(ei))
          {
          case ET_TET:     return *(new (lh) ScalarFE<ET_TET,1>);
          case ET_PRISM:   return *(new (lh) FE_Prism1);
          case ET_PYRAMID: return *(new (lh) FE_Pyramid1);
          case ET_HEX:     return *(new (lh) FE_Hex1);
          case ET_TRIG:    return *(new (lh) ScalarFE<ET_TRIG,1>);
          case ET_QUAD:    return *(new (lh) ScalarFE<ET_QUAD,1>);
          case ET_SEGM:    return *(new (lh) FE_Segm1);
          case ET_POINT:   return * new (lh) FE_Point;            
          }
      }
  }
  
  void ElementFESpace :: GetDofNrs (ElementId ei, Array<int> & dnums) const
  {
    if(ei.VB()!=VOL) { dnums.SetSize(0); return; }
    if (order == 0)
      {
	dnums.SetSize(1);
	dnums[0] = ei.Nr();
      }
    else if (order == 1)
      {
	switch (ma->GetElType(ei))
	  {
	  case ET_TRIG:
	    dnums.SetSize(3);
	    break;
	  case ET_QUAD:
	    dnums.SetSize(4);
	    break;
	  case ET_TET:
	    dnums.SetSize(4);
	    break;
	  case ET_PRISM:
	    dnums.SetSize(6);
	    break;
	  case ET_PYRAMID:
	    dnums.SetSize(5);
	    break;
	  default:
	    throw Exception ("ElementFESpace::GetDofNrs, unknown element type");
	    break;
	  }

	for (int i = 0; i < dnums.Size(); i++)
	  dnums[i] = n_el_dofs*ei.Nr()+i;
      }
  }

  /*
  size_t ElementFESpace :: GetNDofLevel (int level) const
  {
    return ndlevel[level];
  }
  */
  
  /*
  const FiniteElement & ElementFESpace :: GetSFE (int selnr) const
  {
    throw Exception ("ElementFESpace::GetSFE not available");
  }
  */


 
  SurfaceElementFESpace :: 
      SurfaceElementFESpace (shared_ptr<MeshAccess> ama, const Flags& flags, bool parseflags)
  : FESpace (ama, flags)
  {
    name="SurfaceElementFESpace(surfl2)";
    if(parseflags) CheckFlags(flags);
    
    // prol = new SurfaceElementProlongation (GetMeshAccess(), *this);

    if (order == 0)
    {
      trig    = new ScalarFE<ET_TRIG,0>;
      quad    = new ScalarFE<ET_QUAD,0>;
      segm    = new FE_Segm0;

      n_el_dofs = 1;
    }

    else if (order == 1)
    {
      trig    = new ScalarFE<ET_TRIG,1>;
      quad    = new ScalarFE<ET_QUAD,1>;
      segm    = new FE_Segm1;
	
      if (ma->GetDimension() == 2)
        n_el_dofs = 2;
      else
        n_el_dofs = 4;
    }

    else if (order == 2)
    {
      trig    = new FE_Trig2HB;
      quad    = new ScalarFE<ET_QUAD,1>;
      segm    = new FE_Segm2;

      if (ma->GetDimension() == 2)
        n_el_dofs = 3;
      else
        n_el_dofs = 9;
    }

    integrator[BND].reset (new RobinIntegrator<3> (new ConstantCoefficientFunction(1)));

    if (dimension > 1)
      integrator[BND] = make_shared<BlockBilinearFormIntegrator> (integrator[BND], dimension);
  }

  
  SurfaceElementFESpace :: ~SurfaceElementFESpace ()
  {
    ;
  }

  void SurfaceElementFESpace :: Update(LocalHeap & lh)
  {
    // const MeshAccess & ma = GetMeshAccess();

    while (ma->GetNLevels() > ndlevel.Size())
      ndlevel.Append (n_el_dofs * ma->GetNSE());
  }

  const FiniteElement & SurfaceElementFESpace :: GetFE (ElementId ei, LocalHeap & lh) const
  {
    throw Exception ("SurfaceElementFESpace::GetFE not available");
  }

  
  void SurfaceElementFESpace :: GetDofNrs (ElementId ei, Array<int> & dnums) const
  {
    if(ei.VB()!=BND) {dnums.SetSize (0); return; }
    if (order == 0)
      {
	dnums.SetSize(1);
	dnums[0] = ei.Nr();
      }
    else if (order == 1)
      {
	switch (ma->GetElType(ei))
	  {
	  case ET_SEGM:
	    dnums.SetSize(2);
	    break;
	  case ET_TRIG:
	    dnums.SetSize(3);
	    break;
	  case ET_QUAD:
	    dnums.SetSize(4);
	    break;
	  default:
	    dnums.SetSize(4);
	    break;
	  }
	for (int i = 0; i < dnums.Size(); i++)
	  dnums[i] = n_el_dofs*ei.Nr()+i;
      }
    else if (order == 2)
      {
	switch (ma->GetElType(ei))
	  {
	  case ET_SEGM:
	    dnums.SetSize(3);
	    break;
	  case ET_TRIG:
	    dnums.SetSize(6);
	    break;
	  case ET_QUAD:
	    dnums.SetSize(4);
	    break;
	  default:
	    dnums.SetSize(4);
	    break;
	  }
	for (int i = 0; i < dnums.Size(); i++)
	  dnums[i] = n_el_dofs*ei.Nr()+i;
      }
    
  }
  
  size_t SurfaceElementFESpace :: GetNDofLevel (int level) const
  {
    return ndlevel[level];
  }








  CompoundFESpace :: CompoundFESpace (shared_ptr<MeshAccess> ama,
				      const Flags & flags, bool parseflags)
    : FESpace (ama, flags)
  {
    name="CompoundFESpaces";
    DefineDefineFlag("compound");
    DefineStringListFlag("spaces");
    if (parseflags) CheckFlags(flags);
    
    prol = make_shared<CompoundProlongation> (this);
  }



  CompoundFESpace :: CompoundFESpace (shared_ptr<MeshAccess> ama,
				      const Array<shared_ptr<FESpace>> & aspaces,
				      const Flags & flags, bool parseflags)
    : FESpace (ama, flags), spaces(aspaces)
  {
    name="CompoundFESpaces";
    DefineDefineFlag("compound");
    DefineStringListFlag("spaces");
    if(parseflags) CheckFlags(flags);
    
    auto hprol = make_shared<CompoundProlongation> (this);
    for (auto space : spaces)
      hprol -> AddProlongation (space->GetProlongation());      
    prol = hprol;
  }


  void CompoundFESpace :: AddSpace (shared_ptr<FESpace> fes)
  {
    spaces.Append (fes);
    dynamic_pointer_cast<CompoundProlongation> (prol) -> AddProlongation (fes->GetProlongation());
  }

  CompoundFESpace :: ~CompoundFESpace ()
  {
    ;
  }

  void CompoundFESpace :: Update(LocalHeap & lh)
  {
    FESpace :: Update (lh);

    cummulative_nd.SetSize (spaces.Size()+1);
    cummulative_nd[0] = 0;
    for (int i = 0; i < spaces.Size(); i++)
      {
	spaces[i] -> Update(lh);
	cummulative_nd[i+1] = cummulative_nd[i] + spaces[i]->GetNDof();
      }

    SetNDof (cummulative_nd.Last());
    // while (ma->GetNLevels() > ndlevel.Size())
    // ndlevel.Append (cummulative_nd.Last());


    /*
    free_dofs = make_shared<BitArray> (GetNDof());
    free_dofs->Clear();
    for (int i = 0; i < spaces.Size(); i++)
      {
	shared_ptr<BitArray> freedofsi = spaces[i]->GetFreeDofs(false);
	for (int j = 0; j < freedofsi->Size();j++)
	  if (freedofsi->Test(j)) 
	    free_dofs->Set(cummulative_nd[i]+j);
      }
    external_free_dofs = make_shared<BitArray> (GetNDof());
    external_free_dofs->Clear();
    for (int i = 0; i < spaces.Size(); i++)
      {
	shared_ptr<BitArray> freedofsi = spaces[i]->GetFreeDofs(true);
	for (int j = 0; j < freedofsi->Size();j++)
	  if (freedofsi->Test(j)) 
	    external_free_dofs->Set(cummulative_nd[i]+j);
      }
    */
    
    
    bool has_atomic = false;
    for (auto & space : spaces)
      if (space->HasAtomicDofs())
        has_atomic = true;
    if (has_atomic)
      {
        is_atomic_dof = BitArray(GetNDof());
        is_atomic_dof = false;
        for (int i = 0; i < spaces.Size(); i++)
          {
            FESpace & spacei = *spaces[i];
            IntRange r(cummulative_nd[i], cummulative_nd[i+1]);
            if (spacei.HasAtomicDofs())
              {
                for (size_t j = 0; j < r.Size(); j++)
                  if (spacei.IsAtomicDof(j))
                    is_atomic_dof.Set(r.begin()+j);
              }
          }       
      }
    // cout << "AtomicDofs = " << endl << is_atomic_dof << endl;

    prol -> Update();

    UpdateCouplingDofArray();

    if (print)
      {
	(*testout) << "Update compound fespace" << endl;
	(*testout) << "cummulative dofs start at " << cummulative_nd << endl;
      }
  }

  void CompoundFESpace :: FinalizeUpdate(LocalHeap & lh)
  {
    for (int i = 0; i < spaces.Size(); i++)
      spaces[i] -> FinalizeUpdate(lh);

    FESpace::FinalizeUpdate (lh);


    // dirichlet-dofs from sub-spaces
    // ist das umsonst ? (JS)
    // haben jetzt ja immer dirichlet-dofs
    bool has_dirichlet_dofs = false;
    for (int i = 0; i < spaces.Size(); i++)
      if (spaces[i]->GetFreeDofs()) 
	has_dirichlet_dofs = true;

    has_dirichlet_dofs = MyMPI_AllReduce (has_dirichlet_dofs, MPI_LOR);

    if (has_dirichlet_dofs)
      {
	free_dofs = make_shared<BitArray> (GetNDof());
	free_dofs->Set();

	for (int i = 0; i < spaces.Size(); i++)
	  {
	    shared_ptr<BitArray> free_dofs_sub = spaces[i]->GetFreeDofs();
	    if (free_dofs_sub)
	      {
		int base = cummulative_nd[i];
		int nd = cummulative_nd[i+1] - base;
		for (int i = 0; i < nd; i++)
		  if (!free_dofs_sub->Test(i))
		    free_dofs->Clear (base+i);
	      }
	  }

        for (int i = 0; i < ctofdof.Size(); i++)
          if (ctofdof[i] == UNUSED_DOF)
            free_dofs->Clear(i);

	dirichlet_dofs = *free_dofs;
	dirichlet_dofs.Invert();

        external_free_dofs = make_shared<BitArray>(GetNDof());
        *external_free_dofs = *free_dofs;
        for (int i = 0; i < ctofdof.Size(); i++)
          if (ctofdof[i] & LOCAL_DOF)
            external_free_dofs->Clear(i);


        if (print)
          (*testout) << "compound fespace freedofs:" << endl
                     << free_dofs << endl;

      }
  }



  void CompoundFESpace :: UpdateCouplingDofArray()
  {
    ctofdof.SetSize(this->GetNDof());

    for (int i = 0; i < spaces.Size(); i++)
      for (int j=0; j< spaces[i]->GetNDof();j++)
	ctofdof[cummulative_nd[i]+j] = spaces[i]->GetDofCouplingType(j);	

    // *testout << "CompoundFESpace :: UpdateCouplingDofArray() presents \n ctofdof = \n" << ctofdof << endl;
  }



  FiniteElement & CompoundFESpace :: GetFE (ElementId ei, Allocator & alloc) const
  {
    FlatArray<const FiniteElement*> fea(spaces.Size(), alloc);
    for (int i = 0; i < fea.Size(); i++)
      fea[i] = &spaces[i]->GetFE(ei, alloc);
    return *new (alloc) CompoundFiniteElement (fea);
  }


  void CompoundFESpace :: GetDofNrs (ElementId ei, Array<int> & dnums) const
  {
    ArrayMem<int,500> hdnums;
    dnums.SetSize0();
    for (int i = 0; i < spaces.Size(); i++)
      {
	spaces[i]->GetDofNrs (ei, hdnums);
        int base = dnums.Size();
        int base_cum = cummulative_nd[i];
        dnums.SetSize(base+hdnums.Size());

        for (auto j : Range(hdnums))
          {
            int val = hdnums[j];
            if (val != -1)
              val += base_cum;
            dnums[base+j] = val;
          }
      }
  }

  void CompoundFESpace :: GetDofNrs (NodeId ni, Array<DofId> & dnums) const
  {
    ArrayMem<int,500> hdnums;
    dnums.SetSize0();
    for (int i = 0; i < spaces.Size(); i++)
      {
	spaces[i]->GetDofNrs (ni, hdnums);
        int base = dnums.Size();
        int base_cum = cummulative_nd[i];
        dnums.SetSize(base+hdnums.Size());

        for (auto j : Range(hdnums))
          {
            int val = hdnums[j];
            if (val != -1)
              val += base_cum;
            dnums[base+j] = val;
          }
      }
  }
  

  void CompoundFESpace :: GetVertexDofNrs (int vnr, Array<int> & dnums) const
  {
    ArrayMem<int,500> hdnums;
    dnums.SetSize(0);
    for (int i = 0; i < spaces.Size(); i++)
      {
	spaces[i]->GetVertexDofNrs (vnr, hdnums);
	for (int j = 0; j < hdnums.Size(); j++)
	  if (hdnums[j] != -1)
	    dnums.Append (hdnums[j]+cummulative_nd[i]);
	  else
	    dnums.Append (-1);
      }
  }

  void CompoundFESpace :: GetEdgeDofNrs (int ednr, Array<int> & dnums) const
  {
    ArrayMem<int,500> hdnums;
    dnums.SetSize(0);
    for (int i = 0; i < spaces.Size(); i++)
      {
	spaces[i]->GetEdgeDofNrs (ednr, hdnums);
	for (int j = 0; j < hdnums.Size(); j++)
	  if (hdnums[j] != -1)
	    dnums.Append (hdnums[j]+cummulative_nd[i]);
	  else
	    dnums.Append (-1);
      }
  }

  void CompoundFESpace :: GetFaceDofNrs (int fanr, Array<int> & dnums) const
  {
    ArrayMem<int,500> hdnums;
    dnums.SetSize(0);
    for (int i = 0; i < spaces.Size(); i++)
      {
	spaces[i]->GetFaceDofNrs (fanr, hdnums);
	for (int j = 0; j < hdnums.Size(); j++)
	  if (hdnums[j] != -1)
	    dnums.Append (hdnums[j]+cummulative_nd[i]);
	  else
	    dnums.Append (-1);
      }
  }

  void CompoundFESpace :: GetInnerDofNrs (int elnr, Array<int> & dnums) const
  {
    ArrayMem<int,500> hdnums;
    dnums.SetSize(0);

    for (int i = 0; i < spaces.Size(); i++)
      {
	spaces[i]->GetInnerDofNrs (elnr, hdnums);
	for (int j = 0; j < hdnums.Size(); j++)
	  if (hdnums[j] != -1)
	    dnums.Append (hdnums[j]+cummulative_nd[i]);
	  else
	    dnums.Append (-1);
      }
  }
  




  template <class T>
  void CompoundFESpace::T_TransformMat (ElementId ei, 
                                        SliceMatrix<T> mat, TRANSFORM_TYPE tt) const
  {
    size_t base = 0;
    LocalHeapMem<100005> lh("CompoundFESpace - transformmat");
    for (int i = 0; i < spaces.Size(); i++)
      {
        HeapReset hr(lh);
        size_t nd = spaces[i]->GetFE(ei, lh).GetNDof();

	spaces[i]->TransformMat (ei, mat.Rows(base, base+nd), TRANSFORM_MAT_LEFT);
	spaces[i]->TransformMat (ei, mat.Cols(base, base+nd), TRANSFORM_MAT_RIGHT);

	base += nd;
      }
  }

  template <class T>
  void CompoundFESpace::T_TransformVec (ElementId ei, 
                                        SliceVector<T> vec, TRANSFORM_TYPE tt) const
  {
    LocalHeapMem<100006> lh("CompoundFESpace - transformvec");
    for (int i = 0, base = 0; i < spaces.Size(); i++)
      {
        HeapReset hr(lh);
        int nd = spaces[i]->GetFE(ei, lh).GetNDof();

	spaces[i]->TransformVec (ei, vec.Range(base, base+nd), tt);
	base += nd;
      }
  }



  template NGS_DLL_HEADER
  void CompoundFESpace::T_TransformVec<double> 
  (ElementId ei, SliceVector<double> vec, TRANSFORM_TYPE tt) const;
  template NGS_DLL_HEADER
  void CompoundFESpace::T_TransformVec<Complex>
  (ElementId ei, SliceVector<Complex> vec, TRANSFORM_TYPE tt) const;
  
  template
  void CompoundFESpace::T_TransformMat<double> 
  (ElementId ei, SliceMatrix<double> mat, TRANSFORM_TYPE tt) const;
  template
  void CompoundFESpace::T_TransformMat<Complex> 
  (ElementId ei, SliceMatrix<Complex> mat, TRANSFORM_TYPE tt) const;
  
  
  void CompoundFESpace::VTransformMR (ElementId ei, 
				      SliceMatrix<double> mat, TRANSFORM_TYPE tt) const 
  {
    T_TransformMat (ei, mat, tt);
  }
  
  void CompoundFESpace::VTransformMC (ElementId ei, 
				      SliceMatrix<Complex> mat, TRANSFORM_TYPE tt) const
  {
    T_TransformMat (ei, mat, tt);
  }
  

  void CompoundFESpace::VTransformVR (ElementId ei, 
				      SliceVector<double> vec, TRANSFORM_TYPE tt) const 
  {
    T_TransformVec (ei, vec, tt);
  }
  
  void CompoundFESpace::VTransformVC (ElementId ei, 
				      SliceVector<Complex> vec, TRANSFORM_TYPE tt) const 
  {
    T_TransformVec (ei, vec, tt);
  }






  Table<int> * Nodes2Table (const MeshAccess & ma,
			    const Array<NodeId> & dofnodes)
  {
    int ndof = dofnodes.Size();

    Array<int> distprocs;
    Array<int> ndistprocs(ndof);
    ndistprocs = 0;
    for (int i = 0; i < ndof; i++)
      {
	if (dofnodes[i].GetNr() == -1) continue;
	ma.GetDistantProcs (dofnodes[i], distprocs);
	ndistprocs[i] = distprocs.Size();
      }

    Table<int> * dist_procs = new Table<int> (ndistprocs);

    for (int i = 0; i < ndof; i++)
      {
	if (dofnodes[i].GetNr() == -1) continue;
	ma.GetDistantProcs (dofnodes[i], distprocs);
	(*dist_procs)[i] = distprocs;
      }

    return dist_procs;
  }


#ifdef PARALLEL
  ParallelMeshDofs :: ParallelMeshDofs (shared_ptr<MeshAccess> ama, 
					const Array<Node> & adofnodes, 
					int dim, bool iscomplex)
    : ParallelDofs (ama->GetCommunicator(),
		    Nodes2Table (*ama, adofnodes), dim, iscomplex),		    
      ma(ama), dofnodes(adofnodes)
  { ; }
#endif







  FESpaceClasses :: ~FESpaceClasses() { ; }
  
  void FESpaceClasses :: 
  AddFESpace (const string & aname,
	      shared_ptr<FESpace> (*acreator)(shared_ptr<MeshAccess> ma, const Flags & flags))
  {
    fesa.Append (make_shared<FESpaceInfo> (aname, acreator));
  }

  const shared_ptr<FESpaceClasses::FESpaceInfo> 
  FESpaceClasses::GetFESpace(const string & name)
  {
    for (auto & fes : fesa)
      if (name == fes->name) return fes;
    return NULL;
  }

  void FESpaceClasses :: Print (ostream & ost) const
  {
    ost << endl << "FESpaces:" << endl;
    ost <<         "---------" << endl;
    ost << setw(20) << "Name" << endl;
    for (auto & fes : fesa)
      ost << setw(20) << fes->name << endl;
  }

 
  FESpaceClasses & GetFESpaceClasses ()
  {
    static FESpaceClasses fecl;
    return fecl;
  }

  extern NGS_DLL_HEADER shared_ptr<FESpace> CreateFESpace (const string & type,
                                                           shared_ptr<MeshAccess> ma,
                                                           const Flags & flags)
  {
    shared_ptr<FESpace> space;
    for (int i = 0; i < GetFESpaceClasses().GetFESpaces().Size(); i++)
      if (type == GetFESpaceClasses().GetFESpaces()[i]->name ||
	  flags.GetDefineFlag (GetFESpaceClasses().GetFESpaces()[i]->name) )
	{
	  space = GetFESpaceClasses().GetFESpaces()[i]->creator (ma, flags);
          space -> type = type;
	}
    if (!space)
      throw Exception (string ("undefined fespace '") + type + '\'');
    return space;
  }



  // standard fespaces:

  static RegisterFESpace<NodalFESpace> initnodalfes ("nodal");
  static RegisterFESpace<NonconformingFESpace> initncfes ("nonconforming");

}



