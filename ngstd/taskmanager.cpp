/********************************************************************/
/* File:   taskmanager.cpp                                          */
/* Author: M. Hochsterger, J. Schoeberl                             */
/* Date:   10. Mar. 2015                                            */
/********************************************************************/

#include <ngstd.hpp>
#include "taskmanager.hpp"


#ifdef USE_NUMA
#include <numa.h>
#include <sched.h>
#endif


namespace ngstd
{
  TaskManager * task_manager = nullptr;

 

  void RunWithTaskManager (function<void()> alg)
  {
    if (task_manager)
      {
        // cout << "task-manager already active, using it" << endl;
        alg();
        return;
      }


    task_manager = new TaskManager();

#pragma omp parallel
    {
#pragma omp single nowait
      {
        cout << "new task-based parallelization" << endl;



#ifdef USE_NUMA
        int num_nodes = numa_max_node() + 1;
        int thd = omp_get_thread_num();
        int thds = omp_get_num_threads();
        int mynode = num_nodes * thd/thds;
        numa_run_on_node (mynode);
#endif


        
        // master has maximal priority !
        int policy;
        struct sched_param param;
        pthread_getschedparam(pthread_self(), &policy, &param);
        param.sched_priority = sched_get_priority_max(policy);
        pthread_setschedparam(pthread_self(), policy, &param);

        
        alg();
        
        task_manager->Done();
      }
      

      task_manager->Loop();
    }

    
    delete task_manager;
    task_manager = nullptr;
  }






  TaskManager :: TaskManager()
    {
#ifdef USE_NUMA
      numa_available();
      num_nodes = numa_max_node() + 1;

      for (int j = 0; j < num_nodes; j++)
        {
          void * mem = numa_alloc_onnode (sizeof(NodeData), j);
          nodedata[j] = new (mem) NodeData;
        }
#else
      num_nodes = 1;
      nodedata[0] = new NodeData;
      nodedata[0]->participate = 0;
#endif

      jobnr = 0;
      done = 0;
    }



  void TaskManager :: CreateJob (function<void(TaskInfo&)> afunc,
                                 int antasks)
  {

    for (int j = 0; j < num_nodes; j++)
      {
        while (nodedata[j]->participate > 0);
        int oldval = 0;
        while (!nodedata[j]->participate.compare_exchange_weak (oldval, -1))
          oldval = 0;
      }

    func = afunc;
    ntasks = antasks;
    ex = nullptr;

    // atomic_thread_fence (memory_order_release);

    for (int j = 0; j < num_nodes; j++)
      {
        nodedata[j]->start_cnt = 0;
        nodedata[j]->complete_cnt = 0;

        complete[j] = 0;
      }

    // complete_cnt = 0;
    jobnr++;
      
    for (int j = 0; j < num_nodes; j++)
      nodedata[j]->participate = 0;


    int thd = omp_get_thread_num();
    int thds = omp_get_num_threads();

    // int tasks_per_node = thds / num_nodes;
    int mynode = num_nodes * thd/thds;

    IntRange mytasks = Range(int(ntasks)).Split (mynode, num_nodes);
      
    // #ifdef USE_NUMA
    // numa_run_on_node (mynode);
    // #endif

    NodeData & mynode_data = *(nodedata[mynode]);


    TaskInfo ti;
    ti.nthreads = thds;
    ti.thread_nr = thd;
    ti.nnodes = num_nodes;
    ti.node_nr = mynode;

    try
      {
        
        
        while (1)
          {
            int mytask = mynode_data.start_cnt++;
            if (mytask >= mytasks.Size()) break;
            
            ti.task_nr = mytasks.First()+mytask;
            ti.ntasks = ntasks;
            func(ti); 
            if (++mynode_data.complete_cnt == mytasks.Size())
              complete[mynode] = true;
          }

      }
    catch (Exception e)
      {
#pragma omp critical(copyex)
        {
          delete ex;
          ex = new Exception (e);
          mynode_data.start_cnt = mytasks.Size();
          complete[mynode] = true;
        }
      }

    
    for (int j = 0; j < num_nodes; j++)
      while (!complete[j])
        ;

    if (ex)
      throw Exception (*ex);

    // atomic_thread_fence (memory_order_acquire);
  }
    
  void TaskManager :: Loop()
  {
    int thd = omp_get_thread_num();
    int thds = omp_get_num_threads();

    int mynode = num_nodes * thd/thds;

    NodeData & mynode_data = *(nodedata[mynode]);



    TaskInfo ti;
    ti.nthreads = thds;
    ti.thread_nr = thd;
    ti.nnodes = num_nodes;
    ti.node_nr = mynode;

      
#ifdef USE_NUMA
    numa_run_on_node (mynode);
#endif
      
    int jobdone = 0;
      
    while (!done)
      {
        if (jobnr == jobdone)
          {
// #pragma omp taskyield
            sched_yield();
            continue;
          }
          
        while (mynode_data.participate == -1);
          
        int oldpart = 0;
        while (! mynode_data.participate.compare_exchange_weak (oldpart, oldpart+1))
          if (oldpart == -1) oldpart = 0;

        // atomic_thread_fence (memory_order_acquire);

        IntRange mytasks = Range(int(ntasks)).Split (mynode, num_nodes);
          
        try
          {
            
            while (1)
              {
                int mytask = mynode_data.start_cnt++;
                if (mytask >= mytasks.Size()) break;
                
                ti.task_nr = mytasks.First()+mytask;
                ti.ntasks = ntasks;
                
                func(ti); 
                
                if (++mynode_data.complete_cnt == mytasks.Size())
                  complete[mynode] = true;
              }

          }
        catch (Exception e)
          {
#pragma omp critical(copyex)
            {
              delete ex;
              ex = new Exception (e);
              mynode_data.start_cnt = mytasks.Size();
              complete[mynode] = true;
            }
          }

        atomic_thread_fence (memory_order_release);     

        jobdone = jobnr;
        mynode_data.participate--;
      }
  }

}