/*********************************************************************/
/* File:   symbolicintegrator.cpp                                    */
/* Author: Joachim Schoeberl                                         */
/* Date:   August 2015                                               */
/*********************************************************************/
/* 
   Symbolic integrators
*/

#include <fem.hpp>

namespace ngfem
{
  
  ProxyFunction ::
  ProxyFunction (shared_ptr<ngcomp::FESpace> afes,
                 bool atestfunction, bool ais_complex,
                 shared_ptr<DifferentialOperator> aevaluator, 
                 shared_ptr<DifferentialOperator> aderiv_evaluator,
                 shared_ptr<DifferentialOperator> atrace_evaluator,
                 shared_ptr<DifferentialOperator> atrace_deriv_evaluator,
		 shared_ptr<DifferentialOperator> attrace_evaluator,
		 shared_ptr<DifferentialOperator> attrace_deriv_evaluator)
    
    : CoefficientFunction(aevaluator ? aevaluator->Dim() : 1,  /* ais_complex */ false),
      fes(afes),
      testfunction(atestfunction), is_other(false),
      evaluator(aevaluator), 
      deriv_evaluator(aderiv_evaluator),
      trace_evaluator(atrace_evaluator), 
      trace_deriv_evaluator(atrace_deriv_evaluator),
      ttrace_evaluator(attrace_evaluator),
      ttrace_deriv_evaluator(attrace_deriv_evaluator)
  {
    /*
    if (!aevaluator)
      {
        cerr << "don't have a primal evaluator" << endl;
        if (trace_evaluator) cout << "trace-eval = " << typeid(*trace_evaluator).name() << endl;
        if (deriv_evaluator) cout << "deriv-eval = " << typeid(*deriv_evaluator).name() << endl;
        throw Exception("don't have a primal evaluator");
      }
    */
    if (deriv_evaluator || trace_deriv_evaluator)
      deriv_proxy = make_shared<ProxyFunction> (fes, testfunction, is_complex,
                                                deriv_evaluator, nullptr,
                                                trace_deriv_evaluator, nullptr,
						ttrace_deriv_evaluator, nullptr);
    if (evaluator)
      SetDimensions (evaluator->Dimensions());
    else if (trace_evaluator)
      SetDimensions (trace_evaluator->Dimensions());
    else
      SetDimensions (ttrace_evaluator->Dimensions());
  }
  
  shared_ptr<ProxyFunction> ProxyFunction :: Trace() const
  {
    if (!trace_evaluator)
      throw Exception (string("don't have a trace, primal evaluator = ")+
                       evaluator->Name());
    
    return make_shared<ProxyFunction> (fes, testfunction, is_complex,
                                       trace_evaluator, trace_deriv_evaluator,
                                       ttrace_evaluator, ttrace_deriv_evaluator,
                                       nullptr, nullptr);
  }
  
  void ProxyFunction ::
  GenerateCode(Code &code, FlatArray<int> inputs, int index) const
  {
    auto dims = Dimensions();

    string header = "\n\
    {flatmatrix} {values};\n\
    ProxyUserData * {ud} = (ProxyUserData*)mir.GetTransformation().userdata;\n\
    {\n\
      // if (!{ud})\n                                                      \
      // throw Exception (\"cannot evaluate ProxyFunction without userdata\");\n \
          ";

    if(!testfunction) {
      header+=
"      if ({ud}->fel) {\n\
     //     if ({ud}->HasMemory ({this})) {\n";
      if(code.is_simd) {
        header += "auto x = {ud}->GetAMemory ({this});\n";
        header += "{values}.AssignMemory(x.Height(), x.Width(), &x(0,0));\n";
      } else {
        header += "auto x = {ud}->GetMemory ({this});\n";
        header += "{values}.AssignMemory(x.Height(), x.Width(), &x(0,0));\n";
      }
      header+=
"      //    }\n\
       //   else\n\
       //     throw Exception(\"userdata has no memory!\");\n\
      }\n";
    }
    header += "}\n";

    TraverseDimensions( dims, [&](int ind, int i, int j) {
        header += Var("comp", index,i,j).Declare("{scal_type}", 0.0);
        if(!testfunction && code.deriv==2)
        {
          header += "if(( ({ud}->trialfunction == {this}) && ({ud}->trial_comp=="+ToLiteral(ind)+"))\n"+
          " ||  (({ud}->testfunction == {this}) && ({ud}->test_comp=="+ToLiteral(ind)+")))\n";
        }
        else
          header += "if({ud}->{comp_string}=="+ToLiteral(ind)+" && {ud}->{func_string} == {this})\n";
        header += Var("comp", index,i,j).S() + string("{get_component}") + " = 1.0;\n";
    });
    string body = "";

    TraverseDimensions( dims, [&](int ind, int i, int j) {
        body += Var(index, i,j).Declare("{scal_type}", 0.0);
        body += Var(index, i,j).Assign(CodeExpr("0.0"), false);
    });

    if(!testfunction) {
      body += "if ({ud}->fel) {\n";
      TraverseDimensions( dims, [&](int ind, int i, int j) {
          string var = Var(index, i,j);
          if(code.deriv)
            var += ".Value()";
          string values = "{values}";
          if(code.is_simd)
            values += "(" + ToLiteral(ind) + ",i)";
          else
            values += "(i," + ToLiteral(ind) + ")";

          body += var + " = " + values + ";\n";
      });
      if(code.deriv)
        body += "}\n";
      else
        body += "} else ";
    }
    body += "{\n";
    if(testfunction)
      TraverseDimensions( dims, [&](int ind, int i, int j) {
        if(code.deriv==0) body += Var(index,i,j).Assign( Var("comp", index,i,j), false );
        if(code.deriv==1) body += Var(index,i,j).Assign( Var("comp", index,i,j), false );
        if(code.deriv==2) body += Var(index,i,j).Call("DValue","0").Assign( Var("comp", index,i,j).Call("DValue","0"), false );
      });
    else
      TraverseDimensions( dims, [&](int ind, int i, int j) {
        if(code.deriv==0) body += Var(index,i,j).Assign( Var("comp", index,i,j), false );
        if(code.deriv>=1) body += Var(index,i,j).Call("DValue","0").Assign( Var("comp", index,i,j).Call("DValue","0"), false );
      });
    body += "}\n";

    string func_string = testfunction ? "testfunction" : "trialfunction";
    string comp_string = testfunction ? "test_comp" : "trial_comp";
    std::map<string,string> variables;
    variables["ud"] = "tmp_"+ToLiteral(index)+"_0";
    variables["this"] = "reinterpret_cast<ProxyFunction*>("+code.AddPointer(this)+")";
    variables["func_string"] = testfunction ? "testfunction" : "trialfunction";
    variables["comp_string"] = testfunction ? "test_comp" : "trial_comp";
    variables["testfunction"] = ToLiteral(testfunction);

    variables["flatmatrix"] = code.is_simd ? "FlatMatrix<SIMD<double>>" : "FlatMatrix<double>";

    variables["values"] = Var("values", index).S();

    variables["get_component"] = "";
    if(code.deriv==1)
      variables["get_component"] = testfunction ? ".Value()" : ".DValue(0)";
    if(code.deriv==2)
      variables["get_component"] = ".DValue(0)";

    string scal_type = "double";
    if(code.is_simd) scal_type = "SIMD<" + scal_type + ">";
    if(code.deriv==1) scal_type = "AutoDiff<1,"+scal_type+">";
    if(code.deriv==2) scal_type = "AutoDiffDiff<1,"+scal_type+">";
    variables["scal_type"] = scal_type;
    code.header += Code::Map(header, variables);
    code.body += Code::Map(body, variables);
  }
  
  void ProxyFunction ::
  Evaluate (const BaseMappedIntegrationPoint & mip,
            FlatVector<> result) const
  {
    ProxyUserData * ud = (ProxyUserData*)mip.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction without userdata");

    if (!testfunction && ud->fel)
      {
        static bool first = true;
        if (first) cerr << "ProxyFunction::Evaluate (mip) ... should not be here" << endl;
        first = false;
        // if (ud->HasMemory (this))
        // result = ud->GetMemory (this);
        // else
        // evaluator->Apply (*ud->fel, mip, *ud->elx, result, *ud->lh);
        return;
      }

    result = 0;
    if (ud->testfunction == this)
      result (ud->test_comp) = 1;
    if (ud->trialfunction == this)
      result (ud->trial_comp) = 1;
  }

  void ProxyFunction ::
  Evaluate (const BaseMappedIntegrationRule & mir,
            FlatMatrix<> result) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction without userdata");
    if (!testfunction && ud->fel)
      {
        if (ud->HasMemory (this))
          result = ud->GetMemory (this);
        else
          cerr << "Proxy evaluate, but no userdata" << endl;
          // evaluator->Apply (*ud->fel, mir, *ud->elx, result, *ud->lh);
        return;
      }

    result = 0;
    if (ud->testfunction == this)
      result.Col(ud->test_comp) = 1;
    if (ud->trialfunction == this)
      result.Col(ud->trial_comp) = 1;
  }

  void ProxyFunction ::
  Evaluate (const BaseMappedIntegrationRule & ir,
            FlatMatrix<Complex> result) const
  {
    size_t dim = Dimension();
    STACK_ARRAY(double, hmem, ir.Size()*dim);
    FlatMatrix<> temp(ir.Size(), dim, &hmem[0]);
    Evaluate (ir, temp);
    result = temp;
  }
  

  void ProxyFunction ::
  Evaluate (const SIMD_BaseMappedIntegrationRule & mir,
            BareSliceMatrix<SIMD<double>> result) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    assert (ud);
    
    if (!testfunction && ud->fel)
      {
        result.AddSize(Dimension(), mir.Size()) = BareSliceMatrix<SIMD<double>> (ud->GetAMemory (this));
        return;
      }

    result.AddSize(Dimension(), mir.Size()) = 0;
    if (ud->testfunction == this)
      result.Row(ud->test_comp).AddSize(mir.Size()) = 1;
    if (ud->trialfunction == this)
      result.Row(ud->trial_comp).AddSize(mir.Size()) = 1;
  }

  void ProxyFunction ::
  Evaluate (const SIMD_BaseMappedIntegrationRule & mir,
            BareSliceMatrix<SIMD<Complex>> result) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction without userdata");
    
    if (!testfunction && ud->fel)
      {
        if (ud->HasMemory (this))
          result.AddSize(Dimension(), mir.Size()) = BareSliceMatrix<SIMD<double>> (ud->GetAMemory (this));
        else
          {
            throw ExceptionNOSIMD ("ProxyFunction :: Evaluate(SIMD<Complex>) without precomputed trial values");
          }
        return;
      }

    result.AddSize(Dimension(), mir.Size()) = 0;
    if (ud->testfunction == this)
      result.Row(ud->test_comp).AddSize(mir.Size()) = 1;
    if (ud->trialfunction == this)
      result.Row(ud->trial_comp).AddSize(mir.Size()) = 1;
  }

  
  void ProxyFunction ::
  Evaluate (const SIMD_BaseMappedIntegrationRule & mir,
            FlatArray<AFlatMatrix<double>*> input,
            AFlatMatrix<double> result) const
  {
    ProxyFunction::Evaluate (mir, result);
  }

  
  void ProxyFunction ::
  Evaluate (const BaseMappedIntegrationPoint & ip,
            FlatVector<Complex> result) const
  {
    STACK_ARRAY(double, hmem, result.Size());
    FlatVector<> result_double(result.Size(), &hmem[0]);
    Evaluate (ip, result_double);
    result = result_double;
  }

  void ProxyFunction ::
  EvaluateDeriv (const BaseMappedIntegrationRule & mir,
                 FlatMatrix<> result,
                 FlatMatrix<> deriv) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction");

    deriv = 0;
    result = 0;

    if (!testfunction && ud->fel)
      {
        if (ud->HasMemory(this))
          result = ud->GetMemory (this);          
        else
          {
            static bool first = true;
            if (first) cerr << "ProxyFunction::EvaluateDeriv ... should not be here" << endl;
            first = false;
            // evaluator->Apply (*ud->fel, mir, *ud->elx, result, *ud->lh);
          }
      }
    
    if (ud->testfunction == this)
      result.Col(ud->test_comp) = 1;
    if (ud->trialfunction == this)
      deriv.Col(ud->trial_comp) = 1;
  }


  void ProxyFunction ::
  EvaluateDDeriv (const BaseMappedIntegrationRule & mir,
                  FlatMatrix<> result,
                  FlatMatrix<> deriv,
                  FlatMatrix<> dderiv) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction");

    result = 0;
    deriv = 0;
    dderiv = 0;

    if (!testfunction && ud->fel)
      {
        if (ud->HasMemory (this))
          result = ud->GetMemory (this);
        else
          {
            static bool first = true;
            if (first) cerr << "ProxyFunction::EvaluateDDeriv ... should not be here" << endl;
            first = false;
            // evaluator->Apply (*ud->fel, mir, *ud->elx, result, *ud->lh);
          }
      }
    if (ud->testfunction == this)
      deriv.Col(ud->test_comp) = 1;
    if (ud->trialfunction == this)
      deriv.Col(ud->trial_comp) = 1;
  }


  void ProxyFunction ::
  Evaluate (const SIMD_BaseMappedIntegrationRule & mir,
            BareSliceMatrix<AutoDiff<1,SIMD<double>>> values) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    assert (ud);
    // assert (ud->fel);

    size_t np = mir.Size();
    size_t dim = Dimension();
    
    values.AddSize(dim, np) = AutoDiff<1,SIMD<double>> (0.0);

    if (IsTrialFunction())
      {
        auto val = ud->GetAMemory(this);
        for (size_t i = 0; i < dim; i++)
          for (size_t j = 0; j < np; j++)
            values(i,j).Value() = val(i,j);
      }

    if (ud->testfunction == this)
      {
        auto row = values.Row(ud->test_comp);        
        for (size_t i = 0; i < np; i++)
          row(i).Value() = 1;
      }
    if (ud->trialfunction == this)
      {
        auto row = values.Row(ud->trial_comp);
        for (size_t i = 0; i < np; i++)
          row(i).DValue(0) = 1;
      }
  }

  

  void ProxyFunction ::
  Evaluate (const SIMD_BaseMappedIntegrationRule & mir,
            BareSliceMatrix<AutoDiffDiff<1,SIMD<double>>> values) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;

    assert (ud);
    assert (ud->fel);

    size_t np = mir.Size();
    size_t dim = Dimension();

    values.AddSize(dim, np) = AutoDiffDiff<1,SIMD<double>> (0.0);

    if (!testfunction) //  && ud->fel)
      {
        auto val = ud->GetAMemory(this);
        for (size_t i = 0; i < dim; i++)
          for (size_t j = 0; j < np; j++)
            values(i,j).Value() = val(i,j);
      }

    if (ud->testfunction == this)
      for (size_t i = 0; i < np; i++)
        values(ud->test_comp, i).DValue(0) = 1;
    if (ud->trialfunction == this)
      for (size_t i = 0; i < np; i++)
        values(ud->trial_comp, i).DValue(0) = 1;
  }

  
  
  void ProxyFunction ::
  EvaluateDeriv (const SIMD_BaseMappedIntegrationRule & mir, 
                 AFlatMatrix<double> result, AFlatMatrix<double> deriv) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction");

    result = 0;
    deriv = 0;

    if (!testfunction && ud->fel)
      {
        if (ud->HasMemory (this))
          result = ABareSliceMatrix<> (ud->GetAMemory (this));
        else
          {
            static bool first = true;
            if (first) cerr << "ProxyFunction::EvaluateDDeriv ... should not be here" << endl;
            first = false;
            // evaluator->Apply (*ud->fel, mir, *ud->elx, result);
          }
      }
    if (ud->testfunction == this)
      result.Row(ud->test_comp) = 1;
    if (ud->trialfunction == this)
      deriv.Row(ud->trial_comp) = 1;
  }
  
  void ProxyFunction ::
  EvaluateDDeriv (const SIMD_BaseMappedIntegrationRule & mir, 
                  AFlatMatrix<double> result, AFlatMatrix<double> deriv,
                  AFlatMatrix<double> dderiv) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction");

    result = 0;
    deriv = 0;
    dderiv = 0;

    if (!testfunction && ud->fel)
      {
        if (ud->HasMemory (this))
          result = ABareSliceMatrix<> (ud->GetAMemory (this));
        else
          {
            static bool first = true;
            if (first) cerr << "ProxyFunction::EvaluateDDeriv ... should not be here" << endl;
            first = false;
            // evaluator->Apply (*ud->fel, mir, *ud->elx, result);
          }
      }
    if (ud->testfunction == this)
      deriv.Row(ud->test_comp) = 1;
    if (ud->trialfunction == this)
      deriv.Row(ud->trial_comp) = 1;
  }
  
  void ProxyFunction ::  
  NonZeroPattern (const class ProxyUserData & ud, FlatVector<bool> nonzero,
                  FlatVector<bool> nonzero_deriv, FlatVector<bool> nonzero_dderiv) const
  {
    nonzero = false;
    nonzero_deriv = false;
    nonzero_dderiv = false;

    if (ud.fel)
      {
        if (!testfunction)
          nonzero = true;
        if (ud.testfunction == this)
          nonzero_deriv(ud.test_comp) = true;
        if (ud.trialfunction == this)
          nonzero_deriv(ud.trial_comp) = true;
        return;
      }

    if (ud.testfunction == this)
      nonzero(ud.test_comp) = true;
    if (ud.trialfunction == this)
      nonzero(ud.trial_comp) = true;
  }


  SymbolicLinearFormIntegrator ::
  SymbolicLinearFormIntegrator(shared_ptr<CoefficientFunction> acf, VorB avb,
                               bool aelement_boundary)
    : cf(acf), vb(avb), element_boundary(aelement_boundary)
  {
    simd_evaluate = true;
    
    if (cf->Dimension() != 1)
      throw Exception ("SymbolicLFI needs scalar-valued CoefficientFunction");
    cf->TraverseTree
      ([&] (CoefficientFunction & nodecf)
       {
         auto proxy = dynamic_cast<ProxyFunction*> (&nodecf);
         if (proxy && !proxies.Contains(proxy))
           proxies.Append (proxy);
       });
  }

  /*
  template <typename SCAL> 
  void SymbolicLinearFormIntegrator ::
  T_CalcElementVector (const FiniteElement & fel,
                       const ElementTransformation & trafo, 
                       FlatVector<SCAL> elvec,
                       LocalHeap & lh) const
  {
    // static Timer t("symbolicLFI - CalcElementVector", 2); RegionTimer reg(t);
    
    HeapReset hr(lh);
    IntegrationRule ir(trafo.GetElementType(), 2*fel.Order());
    BaseMappedIntegrationRule & mir = trafo(ir, lh);
    
    FlatVector<SCAL> elvec1(elvec.Size(), lh);
    
    FlatMatrix<SCAL> values(ir.Size(), 1, lh);
    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    
    elvec = 0;
    for (auto proxy : proxies)
      {
        // td.Start();
        FlatMatrix<SCAL> proxyvalues(ir.Size(), proxy->Dimension(), lh);
        for (int k = 0; k < proxy->Dimension(); k++)
          {
            ud.testfunction = proxy;
            ud.test_comp = k;
            
            cf -> Evaluate (mir, values);
            for (int i = 0; i < mir.Size(); i++)
              proxyvalues(i,k) = mir[i].GetWeight() * values(i,0);
          }
        // td.Stop();
        // tb.Start();
        proxy->Evaluator()->ApplyTrans(fel, mir, proxyvalues, elvec1, lh);
        // tb.Stop();
        elvec += elvec1;
      }
  }
  */

  /*
  static Timer telvec("SymbolicLFI::CalcElVec");
  static Timer telvec_mapping("SymbolicLFI::mapping");
  static Timer telvec_zero("SymbolicLFI::zero");
  static Timer telvec_applytrans("SymbolicLFI::applytrans");
  static Timer telvec_dvec("SymbolicLFI::dvec");
  */
  
  template <typename SCAL>   
  void SymbolicLinearFormIntegrator ::
  T_CalcElementVector (const FiniteElement & fel,
                       const ElementTransformation & trafo, 
                       FlatVector<SCAL> elvec,
                       LocalHeap & lh) const
  {
    if (element_boundary)
      { // not yet simded
        elvec = 0;
    
        auto eltype = trafo.GetElementType();
        int nfacet = ElementTopology::GetNFacets(eltype);
        
        Facet2ElementTrafo transform(eltype); 
        
        for (int k = 0; k < nfacet; k++)
          {
            HeapReset hr(lh);
            ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
            
            const IntegrationRule& ir_facet = GetIntegrationRule(etfacet, 2*fel.Order()+bonus_intorder);
            IntegrationRule & ir_facet_vol = transform(k, ir_facet, lh);
            BaseMappedIntegrationRule & mir = trafo(ir_facet_vol, lh);
            
            ProxyUserData ud;
            const_cast<ElementTransformation&>(trafo).userdata = &ud;

            mir.ComputeNormalsAndMeasure (eltype, k);
            
            FlatVector<SCAL> elvec1(elvec.Size(), lh);
            FlatMatrix<SCAL> val(mir.Size(), 1,lh);
            for (auto proxy : proxies)
              {
                HeapReset hr(lh);
                FlatMatrix<SCAL> proxyvalues(mir.Size(), proxy->Dimension(), lh);
                for (int k = 0; k < proxy->Dimension(); k++)
                  {
                    ud.testfunction = proxy;
                    ud.test_comp = k;
                    cf -> Evaluate (mir, val);
                    proxyvalues.Col(k) = val.Col(0);
                  }
                
                for (size_t i = 0; i < mir.Size(); i++)
                  // proxyvalues.Row(i) *= ir_facet[i].Weight() * measure(i);
                  proxyvalues.Row(i) *= ir_facet[i].Weight() * mir[i].GetMeasure();
                
                proxy->Evaluator()->ApplyTrans(fel, mir, proxyvalues, elvec1, lh);
                elvec += elvec1;
              }
          }
        return;
      }
    
    if (simd_evaluate)
      {
        try
          {
            // static Timer t("symbolicLFI - CalcElementVector (SIMD)", 2); RegionTimer reg(t);
            size_t tid = TaskManager::GetThreadId();
            // NgProfiler::StartThreadTimer(telvec, tid);
            
            HeapReset hr(lh);
            // NgProfiler::StartThreadTimer(telvec_mapping, tid);
            const SIMD_IntegrationRule& ir = GetSIMDIntegrationRule(trafo.GetElementType(), 2*fel.Order()+bonus_intorder);
            auto & mir = trafo(ir, lh);
            // NgProfiler::StopThreadTimer(telvec_mapping, tid);
            
            // NgProfiler::StartThreadTimer(telvec_zero, tid);            
            ProxyUserData ud;
            const_cast<ElementTransformation&>(trafo).userdata = &ud;
            
            elvec = 0;
            // NgProfiler::StopThreadTimer(telvec_zero, tid);                        
            for (auto proxy : proxies)
              {
                // NgProfiler::StartThreadTimer(telvec_dvec, tid);
                FlatMatrix<SIMD<SCAL>> proxyvalues(proxy->Dimension(), ir.Size(), lh);
                for (size_t k = 0; k < proxy->Dimension(); k++)
                  {
                    ud.testfunction = proxy;
                    ud.test_comp = k;
                    
                    cf -> Evaluate (mir, proxyvalues.Rows(k,k+1));
                    for (size_t i = 0; i < mir.Size(); i++)
                      proxyvalues(k,i) *= mir[i].GetWeight();
                  }
                // NgProfiler::StopThreadTimer(telvec_dvec, tid);
                
                // NgProfiler::StartThreadTimer(telvec_applytrans, tid);                                
                proxy->Evaluator()->AddTrans(fel, mir, proxyvalues, elvec);
                // NgProfiler::StopThreadTimer(telvec_applytrans, tid);
              }
            // NgProfiler::StopThreadTimer(telvec, tid);
          }
        catch (ExceptionNOSIMD e)
          {
            cout << IM(4) << e.What() << endl
                 << "switching back to standard evaluation" << endl;
            simd_evaluate = false;
            T_CalcElementVector (fel, trafo, elvec, lh);
          }
      }
    else
      {
        // static Timer t("symbolicLFI - CalcElementVector", 2); RegionTimer reg(t);
        HeapReset hr(lh);
        // IntegrationRule ir(trafo.GetElementType(), 2*fel.Order());
        const IntegrationRule& ir = fel.GetIR(2*fel.Order()+bonus_intorder);
        BaseMappedIntegrationRule & mir = trafo(ir, lh);
        
        FlatVector<SCAL> elvec1(elvec.Size(), lh);
        
        FlatMatrix<SCAL> values(ir.Size(), 1, lh);
        ProxyUserData ud;
        const_cast<ElementTransformation&>(trafo).userdata = &ud;
        
        elvec = 0;
        for (auto proxy : proxies)
          {
            FlatMatrix<SCAL> proxyvalues(ir.Size(), proxy->Dimension(), lh);
            for (int k = 0; k < proxy->Dimension(); k++)
              {
                ud.testfunction = proxy;
                ud.test_comp = k;
                
                cf -> Evaluate (mir, values);
                for (int i = 0; i < mir.Size(); i++)
                  proxyvalues(i,k) = mir[i].GetWeight() * values(i,0);
              }
            proxy->Evaluator()->ApplyTrans(fel, mir, proxyvalues, elvec1, lh);
            elvec += elvec1;
          }
      }
  }



  void 
  SymbolicLinearFormIntegrator ::
  CalcElementVector (const FiniteElement & fel,
                     const ElementTransformation & trafo, 
                     FlatVector<double> elvec,
                     LocalHeap & lh) const
  {
    T_CalcElementVector (fel, trafo, elvec, lh);
  }
  
  void 
  SymbolicLinearFormIntegrator ::
  CalcElementVector (const FiniteElement & fel,
                     const ElementTransformation & trafo, 
                     FlatVector<Complex> elvec,
                     LocalHeap & lh) const
  {
    T_CalcElementVector (fel, trafo, elvec, lh);
  }
  

  

  SymbolicBilinearFormIntegrator ::
  SymbolicBilinearFormIntegrator (shared_ptr<CoefficientFunction> acf, VorB avb,
                                  bool aelement_boundary)
    : cf(acf), vb(avb), element_boundary(aelement_boundary)
  {
    simd_evaluate = true;
    
    if (cf->Dimension() != 1)
        throw Exception ("SymblicBFI needs scalar-valued CoefficientFunction");
    trial_cum.Append(0);
    test_cum.Append(0);    
    cf->TraverseTree
      ( [&] (CoefficientFunction & nodecf)
        {
          auto proxy = dynamic_cast<ProxyFunction*> (&nodecf);
          if (proxy) 
            {
              if (proxy->IsTestFunction())
                {
                  if (!test_proxies.Contains(proxy))
                    {
                      test_proxies.Append (proxy);
                      test_cum.Append(test_cum.Last()+proxy->Dimension());
                    }
                }
              else
                {                                         
                  if (!trial_proxies.Contains(proxy))
                    {
                      trial_proxies.Append (proxy);
                      trial_cum.Append(trial_cum.Last()+proxy->Dimension());
                    }
                }
            }
          else
            if (nodecf.StoreUserData())
              gridfunction_cfs.Append (&nodecf);
        });
    cout << IM(5) << "num test_proxies " << test_proxies.Size() << endl;
    cout << IM(5) << "num trial_proxies " << trial_proxies.Size() << endl;
    cout << IM(5) << "cumulated test_proxy dims  " << test_cum << endl;
    cout << IM(5) << "cumulated trial_proxy dims " << trial_cum << endl;

    elementwise_constant = cf -> ElementwiseConstant();
    cout << IM(5) << "element-wise constant = " << elementwise_constant << endl;

    // find non-zeros
    int cnttest = 0, cnttrial = 0;
    for (auto proxy : trial_proxies)
      cnttrial += proxy->Dimension();
    for (auto proxy : test_proxies)
      cnttest += proxy->Dimension();
    nonzeros = Matrix<bool>(cnttest, cnttrial);

    ProxyUserData ud;
    Vector<bool> nzvec(1), nzdvec(1), nzddvec(1);
    int k = 0;
    for (int k1 : test_proxies.Range())
      for (int k2 : Range(0,test_proxies[k1]->Dimension()))
        {
          int l = 0;
          for (int l1 : trial_proxies.Range())
            for (int l2 : Range(0,trial_proxies[l1]->Dimension()))
              {
                ud.trialfunction = trial_proxies[l1];
                ud.trial_comp = l2;
                ud.testfunction = test_proxies[k1];
                ud.test_comp = k2;
                cf -> NonZeroPattern (ud, nzvec, nzdvec, nzddvec);
                nonzeros(k,l) = nzvec(0);
                l++;
              }
          k++;
        }
    
    nonzeros_proxies = Matrix<bool>(test_proxies.Size(), trial_proxies.Size());
    diagonal_proxies = Matrix<bool>(test_proxies.Size(), trial_proxies.Size());
    same_diffops = Matrix<bool>(test_proxies.Size(), trial_proxies.Size());
    is_symmetric = true;
    for (int k1 : Range(trial_proxies))
      for (int l1 : Range(test_proxies))
        {
          auto proxy1 = trial_proxies[k1];
          auto proxy2 = test_proxies[l1];
          
          bool is_nonzero = false;
          bool is_diagonal = proxy1->Dimension() == proxy2->Dimension();
          
          for (int k = 0; k < proxy1->Dimension(); k++)
            for (int l = 0; l < proxy2->Dimension(); l++)
              if (nonzeros(test_cum[l1]+l, trial_cum[k1]+k))
                {
                  is_nonzero = true;
                  if (k != l) is_diagonal = false;
                }
          nonzeros_proxies(l1, k1) = is_nonzero;
          diagonal_proxies(l1, k1) = is_diagonal;
          // same_diffops(l1,k1) = *(proxy2->Evaluator()) == *(proxy1->Evaluator());
          // same objects, not only equal objects, what implies also the same space
          same_diffops(l1,k1) = proxy2->Evaluator() == proxy1->Evaluator();
          if (nonzeros_proxies(l1,k1) && (!diagonal_proxies(l1,k1) || !same_diffops(l1,k1))) is_symmetric = false;
        }
    
    cout << IM(5) << "nonzeros: " << endl << nonzeros << endl;
    cout << IM(5) << "nonzeros_proxies: " << endl << nonzeros_proxies << endl;
    cout << IM(5) << "symmetric: " << endl << is_symmetric << endl;
    
    trial_difforder = 99, test_difforder = 99;
    for (auto proxy : trial_proxies)
      trial_difforder = min2(trial_difforder, proxy->Evaluator()->DiffOrder());
    for (auto proxy : test_proxies)
      test_difforder = min2(test_difforder, proxy->Evaluator()->DiffOrder());
    if (trial_proxies.Size() == 0) trial_difforder = 0;
  }


  const IntegrationRule& SymbolicBilinearFormIntegrator ::
  GetIntegrationRule (const FiniteElement & fel, LocalHeap & /* lh */) const
  {
    if (userdefined_intrules[fel.ElementType()]) return *userdefined_intrules[fel.ElementType()];
    const MixedFiniteElement * mixedfe = dynamic_cast<const MixedFiniteElement*> (&fel);
    const FiniteElement & fel_trial = mixedfe ? mixedfe->FETrial() : fel;
    const FiniteElement & fel_test = mixedfe ? mixedfe->FETest() : fel;

    /*
    int trial_difforder = 99, test_difforder = 99;
    for (auto proxy : trial_proxies)
      trial_difforder = min2(trial_difforder, proxy->Evaluator()->DiffOrder());
    for (auto proxy : test_proxies)
      test_difforder = min2(test_difforder, proxy->Evaluator()->DiffOrder());
    if (trial_proxies.Size() == 0) trial_difforder = 0;
    */
    
    int intorder = fel_trial.Order()+fel_test.Order()+bonus_intorder;
    auto et = fel.ElementType();
    if (et == ET_TRIG || et == ET_TET)
      intorder -= test_difforder+trial_difforder;
    return SelectIntegrationRule (et, intorder);
  }

  const SIMD_IntegrationRule& SymbolicBilinearFormIntegrator ::
  Get_SIMD_IntegrationRule (const FiniteElement & fel, LocalHeap & lh) const
  {
    if (userdefined_simd_intrules[fel.ElementType()] ) return *userdefined_simd_intrules[fel.ElementType()];

    bool is_mixed = typeid(fel) == typeid(const MixedFiniteElement&);
    const MixedFiniteElement * mixedfe = static_cast<const MixedFiniteElement*> (&fel);    
    const FiniteElement & fel_trial = is_mixed ? mixedfe->FETrial() : fel;
    const FiniteElement & fel_test = is_mixed ? mixedfe->FETest() : fel;

    /*
    int trial_difforder = 99, test_difforder = 99;
    for (auto proxy : trial_proxies)
      trial_difforder = min2(trial_difforder, proxy->Evaluator()->DiffOrder());
    for (auto proxy : test_proxies)
      test_difforder = min2(test_difforder, proxy->Evaluator()->DiffOrder());
    if (trial_proxies.Size() == 0) trial_difforder = 0;
    */
    
    int intorder = fel_trial.Order()+fel_test.Order()+bonus_intorder;
    auto et = fel.ElementType();
    if (et == ET_TRIG || et == ET_TET)
      intorder -= test_difforder+trial_difforder;
    return SIMD_SelectIntegrationRule (et, intorder);
  }


#define xxNOSIMD1
#ifdef NOSIMD1
  template <typename SCAL, typename SCAL_SHAPES>
  void SymbolicBilinearFormIntegrator ::
  T_CalcElementMatrixAdd (const FiniteElement & fel,
                          const ElementTransformation & trafo, 
                          FlatMatrix<SCAL> elmat,
                          LocalHeap & lh) const
    
  { 
    // static Timer t("symbolicBFI - CalcElementMatrix", 2);
    // static Timer tstart("symboliBFI - CalcElementMatrix startup", 2);
    // static Timer tstart1("symboliBFI - CalcElementMatrix startup 1", 2);
    // static Timer tmain("symboliBFI - CalcElementMatrix main", 2);

    /*
    static Timer td("symboliBFI - CalcElementMatrix dmats", 2);
    static Timer tb("symboliBFI - CalcElementMatrix diffops", 2);
    static Timer tdb("symboliBFI - CalcElementMatrix D * B", 2);
    static Timer tlapack("symboliBFI - CalcElementMatrix lapack", 2);
    */
    
    // tstart.Start();
    
    if (element_boundary)
      {
        T_CalcElementMatrixEBAdd<SCAL, SCAL_SHAPES> (fel, trafo, elmat, lh);
        return;
      }
    
    // RegionTimer reg(t);

    const MixedFiniteElement * mixedfe = dynamic_cast<const MixedFiniteElement*> (&fel);
    const FiniteElement & fel_trial = mixedfe ? mixedfe->FETrial() : fel;
    const FiniteElement & fel_test = mixedfe ? mixedfe->FETest() : fel;

    //elmat = 0;
    IntegrationRule& ir = GetIntegrationRule (fel, lh);
    // IntegrationRule ir = fel_trial.GetIR(intorder);
    BaseMappedIntegrationRule & mir = trafo(ir, lh);
    
    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    
    
    // tstart.Stop();
    // bool symmetric_so_far = true;
    int k1 = 0;
    for (auto proxy1 : trial_proxies)
      {
        int l1 = 0;
        for (auto proxy2 : test_proxies)
          {
            bool is_diagonal = proxy1->Dimension() == proxy2->Dimension();
            bool is_nonzero = false;

            for (int k = 0; k < proxy1->Dimension(); k++)
              for (int l = 0; l < proxy2->Dimension(); l++)
                if (nonzeros(l1+l, k1+k))
                  {
                    if (k != l) is_diagonal = false;
                    is_nonzero = true;
                  }

            if (is_nonzero)
              {
                HeapReset hr(lh);
                bool samediffop = (*(proxy1->Evaluator()) == *(proxy2->Evaluator())) && !mixedfe;
                // td.Start();
                FlatTensor<3,SCAL> proxyvalues(lh, mir.Size(), proxy1->Dimension(), proxy2->Dimension());
                FlatVector<SCAL> diagproxyvalues(mir.Size()*proxy1->Dimension(), lh);
                FlatMatrix<SCAL> val(mir.Size(), 1, lh);
                
                
                if (!is_diagonal)
                  for (int k = 0; k < proxy1->Dimension(); k++)
                    for (int l = 0; l < proxy2->Dimension(); l++)
                      {
                        if (nonzeros(l1+l, k1+k))
                          {
                            if (k != l) is_diagonal = false;
                            is_nonzero = true;
                            ud.trialfunction = proxy1;
                            ud.trial_comp = k;
                            ud.testfunction = proxy2;
                            ud.test_comp = l;
                            
                            cf -> Evaluate (mir, val);
                            proxyvalues(STAR,k,l) = val.Col(0);
                          }
                        else
                          proxyvalues(STAR,k,l) = 0.0;
                      }
                else
                  for (int k = 0; k < proxy1->Dimension(); k++)
                    {
                      ud.trialfunction = proxy1;
                      ud.trial_comp = k;
                      ud.testfunction = proxy2;
                      ud.test_comp = k;

                      if (!elementwise_constant)
                        {
                          cf -> Evaluate (mir, val);
                          diagproxyvalues.Slice(k, proxy1->Dimension()) = val.Col(0);
                        }
                      else
                        {
                          cf -> Evaluate (mir[0], val.Row(0));
                          diagproxyvalues.Slice(k, proxy1->Dimension()) = val(0,0);
                        }
                    }
            
                // td.Stop();

                if (!mir.IsComplex())
                  {
                    if (!is_diagonal)
                      for (int i = 0; i < mir.Size(); i++)
                        proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();
                    else
                      for (int i = 0; i < mir.Size(); i++)
                        diagproxyvalues.Range(proxy1->Dimension()*IntRange(i,i+1)) *= mir[i].GetWeight();
                  }
                else
                  { // pml
                    if (!is_diagonal)
                      for (int i = 0; i < mir.Size(); i++)
                        proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();
                    else
                      for (int i = 0; i < mir.Size(); i++)
                        diagproxyvalues.Range(proxy1->Dimension()*IntRange(i,i+1)) *=
                          static_cast<const ScalMappedIntegrationPoint<SCAL>&> (mir[i]).GetJacobiDet()*ir[i].Weight();
                  }
                IntRange r1 = proxy1->Evaluator()->UsedDofs(fel_trial);
                IntRange r2 = proxy2->Evaluator()->UsedDofs(fel_test);
                SliceMatrix<SCAL> part_elmat = elmat.Rows(r2).Cols(r1);
                
                constexpr size_t BS = 16;
                for (size_t i = 0; i < mir.Size(); i+=BS)
                  {
                    HeapReset hr(lh);
                    size_t bs = min2(size_t(BS), mir.Size()-i);
                    
                    AFlatMatrix<SCAL_SHAPES> bbmat1(elmat.Width(), bs*proxy1->Dimension(), lh);
                    AFlatMatrix<SCAL> bdbmat1(elmat.Width(), bs*proxy2->Dimension(), lh);
                    AFlatMatrix<SCAL_SHAPES> bbmat2 = samediffop ?
                      bbmat1 : AFlatMatrix<SCAL_SHAPES>(elmat.Height(), bs*proxy2->Dimension(), lh);

                    // tb.Start();
                    BaseMappedIntegrationRule & bmir = mir.Range(i, i+bs, lh);
                    proxy1->Evaluator()->CalcMatrix(fel_trial, bmir, Trans(bbmat1), lh);
                    
                    if (!samediffop)
                      proxy2->Evaluator()->CalcMatrix(fel_test, bmir, Trans(bbmat2), lh);
                    // tb.Stop();

                    // tdb.Start();
                    if (is_diagonal)
                      {
                        AFlatVector<SCAL> diagd(bs*proxy1->Dimension(), lh);
                        diagd = diagproxyvalues.Range(i*proxy1->Dimension(),
                                                      (i+bs)*proxy1->Dimension());
                        
                        /*
                        for (int i = 0; i < diagd.Size(); i++)
                          bdbmat1.Col(i) = diagd(i) * bbmat1.Col(i);
                        */
                        MultMatDiagMat(bbmat1, diagd, bdbmat1);
                        // tdb.AddFlops (bbmat1.Height()*bbmat1.Width());
                      }
                    else
                      {
                        for (int j = 0; j < bs; j++)
                          {
                            int ii = i+j;
                            IntRange r1 = proxy1->Dimension() * IntRange(j,j+1);
                            IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                            // bdbmat1.Cols(r2) = bbmat1.Cols(r1) * proxyvalues(ii,STAR,STAR);
                            MultMatMat (bbmat1.Cols(r1), proxyvalues(ii,STAR,STAR), bdbmat1.Cols(r2));
                          }
                        // tdb.AddFlops (proxy1->Dimension()*proxy2->Dimension()*bs*bbmat1.Height());
                      }
                    //  tdb.Stop();
                    
                    // tlapack.Start();
                    // elmat.Rows(r2).Cols(r1) += bbmat2.Rows(r2) * Trans(bdbmat1.Rows(r1));
                    // AddABt (bbmat2.Rows(r2), bdbmat1.Rows(r1), elmat.Rows(r2).Cols(r1));

                    // symmetric_so_far &= samediffop && is_diagonal;

                    // if (symmetric_so_far)
                    if (is_symmetric)
                      AddABtSym (bbmat2.Rows(r2), bdbmat1.Rows(r1), part_elmat);
                    else
                      AddABt (bbmat2.Rows(r2), bdbmat1.Rows(r1), part_elmat);

                    // tlapack.Stop();
                    // tlapack.AddFlops (r2.Size()*r1.Size()*bdbmat1.Width());
                  }

                // if (symmetric_so_far)
                if (is_symmetric)
                  for (int i = 0; i < part_elmat.Height(); i++)
                    for (int j = i+1; j < part_elmat.Width(); j++)
                      part_elmat(i,j) = part_elmat(j,i);
              }
            
            l1 += proxy2->Dimension();  
          }
        k1 += proxy1->Dimension();
      }
  }
#endif
  
#define SIMD_CALCMATRIX
#ifdef SIMD_CALCMATRIX



  template <typename SCAL>
  void ExtendSymmetric (SliceMatrix<SCAL> elmat)
  {
    /*
    size_t h = elmat.Height();
    for (size_t i = 0; i+1 < h; i++)
      for (size_t j = i+1; j < h; j++)
        elmat(i,j) = elmat(j,i);
    */

    size_t h = elmat.Height();
    size_t d = elmat.Dist();
    size_t i = 0, di = 0;
    for ( ; i+2 < h; i+=2, di+=2*d)
      {
        elmat(di+i+1) = elmat(di+d+i);
        size_t j = i+2, dj = di+2*d;
        for ( ; j+1 < h; j+=2, dj+=2*d)
          {
            SCAL tmp00 = elmat(dj+i);
            SCAL tmp01 = elmat(dj+i+1);
            SCAL tmp10 = elmat(dj+d+i);
            SCAL tmp11 = elmat(dj+d+i+1);
            elmat(di+j) = tmp00;
            elmat(di+d+j) = tmp01;
            elmat(di+j+1) = tmp10;
            elmat(di+d+j+1) = tmp11;
          }
        if (j < h)
          {
            SCAL tmp0 = elmat(dj+i);
            SCAL tmp1 = elmat(dj+i+1);
            elmat(di+j) = tmp0;
            elmat(di+d+j) = tmp1;
          }
      }
    /*
    for ( ; i+1 < h; i++)
      for (size_t j = i+1; j < h; j++)
        elmat(i,j) = elmat(j,i);
    */
    if (i+1 < h)
      elmat(di+i+1) = elmat(di+d+i);
  }

  void ExtendSymmetric1 (SliceMatrix<double> elmat)
  {
    ExtendSymmetric (elmat);
  }
  

  /*
  Timer timer_SymbBFI("SymbolicBFI");
  Timer timer_SymbBFIstart("SymbolicBFI start");
  Timer timer_SymbBFIscale("SymbolicBFI scale");
  Timer timer_SymbBFIbd("SymbolicBFI bd");
  Timer timer_SymbBFIbmat("SymbolicBFI bmat");
  Timer timer_SymbBFIdmat("SymbolicBFI dmat");
  Timer timer_SymbBFImult("SymbolicBFI mult");
  Timer timer_SymbBFImultsym("SymbolicBFI multsym");
  */

  template <typename SCAL, typename SCAL_SHAPES, typename SCAL_RES>
  void SymbolicBilinearFormIntegrator ::
  T_CalcElementMatrixAdd (const FiniteElement & fel,
                          const ElementTransformation & trafo, 
                          FlatMatrix<SCAL_RES> elmat,
                          LocalHeap & lh) const
    
  {
    // static Timer t("SymbolicBFI::CalcElementMatrixAdd", 2);
    // ThreadRegionTimer reg(t, TaskManager::GetThreadId());
    
    if (element_boundary)
      {
        T_CalcElementMatrixEBAdd<SCAL, SCAL_SHAPES, SCAL_RES> (fel, trafo, elmat, lh);
        return;
      }

    bool is_mixedfe = typeid(fel) == typeid(const MixedFiniteElement&);
    const MixedFiniteElement * mixedfe = static_cast<const MixedFiniteElement*> (&fel);
    const FiniteElement & fel_trial = is_mixedfe ? mixedfe->FETrial() : fel;
    const FiniteElement & fel_test = is_mixedfe ? mixedfe->FETest() : fel;
    size_t first_std_eval = 0;
    if (simd_evaluate)
      try
        {
          // ThreadRegionTimer reg(timer_SymbBFI, TaskManager::GetThreadId());
          // NgProfiler::StartThreadTimer (timer_SymbBFIstart, TaskManager::GetThreadId());

          const SIMD_IntegrationRule& ir = Get_SIMD_IntegrationRule (fel, lh);
          SIMD_BaseMappedIntegrationRule & mir = trafo(ir, lh);

          // NgProfiler::StopThreadTimer (timer_SymbBFIstart, TaskManager::GetThreadId());

          ProxyUserData ud;
          const_cast<ElementTransformation&>(trafo).userdata = &ud;

          bool symmetric_so_far = true;
          int k1 = 0;
          int k1nr = 0;
          
          for (auto proxy1 : trial_proxies)
            {
              int l1 = 0;
              int l1nr = 0;
              for (auto proxy2 : test_proxies)
                {
                  size_t dim_proxy1 = proxy1->Dimension();
                  size_t dim_proxy2 = proxy2->Dimension();
                  
                  // bool is_nonzero = nonzeros_proxies(l1nr,k1nr);
                  // bool is_diagonal = diagonal_proxies(l1nr,k1nr);
                  size_t tt_pair = l1nr*trial_proxies.Size()+k1nr;
                  first_std_eval = k1nr*test_proxies.Size()+l1nr;  // in case of SIMDException
                  bool is_nonzero = nonzeros_proxies(tt_pair);
                  bool is_diagonal = diagonal_proxies(tt_pair);

                  if (is_nonzero)
                    {
                      HeapReset hr(lh);
                      bool samediffop = same_diffops(tt_pair) && !is_mixedfe;
                      // td.Start();

                      FlatMatrix<SIMD<SCAL>> proxyvalues(dim_proxy1*dim_proxy2, ir.Size(), lh);
                      FlatMatrix<SIMD<SCAL>> diagproxyvalues(dim_proxy1, ir.Size(), lh);
                      FlatMatrix<SIMD<SCAL>> val(1, ir.Size(), lh);
                      {
                        // ThreadRegionTimer regdmat(timer_SymbBFIdmat, TaskManager::GetThreadId());                      
                      if (!is_diagonal)
                        for (size_t k = 0, kk = 0; k < dim_proxy1; k++)
                          for (size_t l = 0; l < dim_proxy2; l++, kk++)
                            {
                              if (nonzeros(l1+l, k1+k))
                                {
                                  ud.trialfunction = proxy1;
                                  ud.trial_comp = k;
                                  ud.testfunction = proxy2;
                                  ud.test_comp = l;
                                  
                                  cf -> Evaluate (mir, proxyvalues.Rows(kk,kk+1));
                                }
                              else
                                proxyvalues.Row(kk) = 0.0;
                            }
                      else
                        for (size_t k = 0; k < dim_proxy1; k++)
                          {
                            ud.trialfunction = proxy1;
                            ud.trial_comp = k;
                            ud.testfunction = proxy2;
                            ud.test_comp = k;
                            
                            cf -> Evaluate (mir, diagproxyvalues.Rows(k,k+1));
                          }
                      // td.Stop();
                      }
                      // NgProfiler::StartThreadTimer (timer_SymbBFIscale, TaskManager::GetThreadId());                      
                      if (!is_diagonal)
                        for (size_t i = 0; i < ir.Size(); i++)
                          proxyvalues.Col(i) *= mir[i].GetWeight();
                      else
                        for (size_t i = 0; i < ir.Size(); i++)
                          diagproxyvalues.Col(i) *= mir[i].GetWeight();
                      

                      IntRange r1 = proxy1->Evaluator()->UsedDofs(fel_trial);
                      IntRange r2 = proxy2->Evaluator()->UsedDofs(fel_test);
                      SliceMatrix<SCAL_RES> part_elmat = elmat.Rows(r2).Cols(r1);

                          
                      FlatMatrix<SIMD<SCAL_SHAPES>> bbmat1(elmat.Width()*dim_proxy1, ir.Size(), lh);
                      FlatMatrix<SIMD<SCAL>> bdbmat1(elmat.Width()*dim_proxy2, ir.Size(), lh);
                      FlatMatrix<SIMD<SCAL_SHAPES>> bbmat2 = samediffop ?
                        bbmat1 : FlatMatrix<SIMD<SCAL_SHAPES>>(elmat.Height()*dim_proxy2, ir.Size(), lh);
                      
                      FlatMatrix<SIMD<SCAL>> hbdbmat1(elmat.Width(), dim_proxy2*ir.Size(),
                                                      &bdbmat1(0,0));
                      FlatMatrix<SIMD<SCAL_SHAPES>> hbbmat2(elmat.Height(), dim_proxy2*ir.Size(),
                                                            &bbmat2(0,0));

                      // NgProfiler::StopThreadTimer (timer_SymbBFIscale, TaskManager::GetThreadId());
                      // bbmat1 = 0.0;
                      // bbmat2 = 0.0;
                      {
                        // ThreadRegionTimer regbmat(timer_SymbBFIbmat, TaskManager::GetThreadId());
                      proxy1->Evaluator()->CalcMatrix(fel_trial, mir, bbmat1);
                      
                      if (!samediffop)
                        proxy2->Evaluator()->CalcMatrix(fel_test, mir, bbmat2);
                      }

                      if (is_diagonal)
                        {
                          // NgProfiler::StartThreadTimer (timer_SymbBFIbd, TaskManager::GetThreadId());                      
                          
                          /*
                          size_t ii = r1.First()*dim_proxy1;
                          for (size_t i : r1)
                            for (size_t j = 0; j < dim_proxy1; j++, ii++)
                              bdbmat1.Row(ii) = pw_mult(bbmat1.Row(ii), diagproxyvalues.Row(j));
                          */
                          
                          // size_t sr1 = r1.Size();
                          for (size_t j = 0; j < dim_proxy1; j++)
                            {
                              auto hbbmat1 = bbmat1.RowSlice(j,dim_proxy1).Rows(r1);
                              auto hbdbmat1 = bdbmat1.RowSlice(j,dim_proxy1).Rows(r1);
                              
                              for (size_t k = 0; k < bdbmat1.Width(); k++)
                                {
                                  /*
                                  auto col_bdbmat = hbdbmat1.Col(k);
                                  auto col_bbmat = hbbmat1.Col(k);
                                  auto proxy = diagproxyvalues(j,k);
                                  for (size_t i1 = 0; i1 < sr1; i1++)
                                    col_bdbmat(i1) = proxy * col_bbmat(i1);
                                  */
                                  hbdbmat1.Col(k).AddSize(r1.Size()) = diagproxyvalues(j,k) * hbbmat1.Col(k);
                                }
                            }
                          
                          // AFlatVector<SCAL> diagd(bs*proxy1->Dimension(), lh);
                          // diagd = diagproxyvalues.Range(i*proxy1->Dimension(),
                          // (i+bs)*proxy1->Dimension());
                          // MultMatDiagMat(bbmat1, diagd, bdbmat1);
                          // NgProfiler::StopThreadTimer (timer_SymbBFIbd, TaskManager::GetThreadId());                                            
                          
                        }
                      else
                        {
                          bdbmat1 = 0.0; 
                          // for (size_t i = 0; i < elmat.Width(); i++)
                          for (auto i : r1)
                            for (size_t j = 0; j < dim_proxy2; j++)
                              for (size_t k = 0; k < dim_proxy1; k++)
                                {
                                  auto res = bdbmat1.Row(i*dim_proxy2+j);
                                  auto a = bbmat1.Row(i*dim_proxy1+k);
                                  auto b = proxyvalues.Row(k*dim_proxy2+j);
                                  res += pw_mult(a,b);
                                }
                        }

                      // elmat.Rows(r2).Cols(r1) += bbmat2.Rows(r2) * Trans(bdbmat1.Rows(r1));
                      // AddABt (bbmat2.Rows(r2), bdbmat1.Rows(r1), elmat.Rows(r2).Cols(r1));

                      symmetric_so_far &= samediffop && is_diagonal;
                      /*
                      if (symmetric_so_far)
                        AddABtSym (AFlatMatrix<double>(hbbmat2.Rows(r2)),
                                   AFlatMatrix<double> (hbdbmat1.Rows(r1)), part_elmat);
                      else
                        AddABt (AFlatMatrix<double> (hbbmat2.Rows(r2)),
                                AFlatMatrix<double> (hbdbmat1.Rows(r1)), part_elmat);
                      */

                      {
                      if (symmetric_so_far)
                        {
                          /*
                            ThreadRegionTimer regdmult(timer_SymbBFImultsym, TaskManager::GetThreadId());
                            NgProfiler::AddThreadFlops(timer_SymbBFImultsym, TaskManager::GetThreadId(),
                            SIMD<double>::Size()*2*r2.Size()*(r1.Size()+1)*hbbmat2.Width() / 2);
                          */
                          AddABtSym (hbbmat2.Rows(r2), hbdbmat1.Rows(r1), part_elmat);
                        }
                      else
                        {
                          /*
                          ThreadRegionTimer regdmult(timer_SymbBFImult, TaskManager::GetThreadId());
                          NgProfiler::AddThreadFlops(timer_SymbBFImult, TaskManager::GetThreadId(),
                                                     SIMD<double>::Size()*2*r2.Size()*r1.Size()*hbbmat2.Width());
                          */
                          AddABt (hbbmat2.Rows(r2), hbdbmat1.Rows(r1), part_elmat);
                        }
                      }
                      if (symmetric_so_far)
                        {
                          ExtendSymmetric (part_elmat);
                          /*
                          size_t h = part_elmat.Height();
                          for (size_t i = 0; i+1 < h; i++)
                            for (size_t j = i+1; j < h; j++)
                              part_elmat(i,j) = part_elmat(j,i);
                          */
                        }
                    }
              
                  l1 += proxy2->Dimension();
                  l1nr++;
                }
              k1 += proxy1->Dimension();
              k1nr++;
            }

          // ir.NothingToDelete();
          return;
        }
      catch (ExceptionNOSIMD e)
        {
          cout << IM(4) << e.What() << endl
               << "switching to scalar evaluation" << endl;
          simd_evaluate = false;
          // T_CalcElementMatrixAdd<SCAL, SCAL_SHAPES, SCAL_RES> (fel, trafo, elmat, lh);
          // return;
        }
    

    // IntegrationRule ir(trafo.GetElementType(), intorder);
    const IntegrationRule& ir = GetIntegrationRule (fel, lh);
    BaseMappedIntegrationRule & mir = trafo(ir, lh);
    
    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    
    
    // tstart.Stop();
    bool symmetric_so_far = true;
    int k1 = 0;
    int k1nr = 0;
    for (auto proxy1 : trial_proxies)
      {
        int l1 = 0;
        int l1nr = 0;
        for (auto proxy2 : test_proxies)
          {
            bool is_diagonal = proxy1->Dimension() == proxy2->Dimension();
            bool is_nonzero = false;

            for (int k = 0; k < proxy1->Dimension(); k++)
              for (int l = 0; l < proxy2->Dimension(); l++)
                if (nonzeros(l1+l, k1+k))
                  {
                    if (k != l) is_diagonal = false;
                    is_nonzero = true;
                  }

            if (is_nonzero  && k1nr*test_proxies.Size()+l1nr >= first_std_eval)
              {
                HeapReset hr(lh);
                bool samediffop = *(proxy1->Evaluator()) == *(proxy2->Evaluator());
                // td.Start();
                FlatTensor<3,SCAL> proxyvalues(lh, mir.Size(), proxy1->Dimension(), proxy2->Dimension());
                FlatVector<SCAL> diagproxyvalues(mir.Size()*proxy1->Dimension(), lh);
                FlatMatrix<SCAL> val(mir.Size(), 1, lh);
                
                
                if (!is_diagonal)
                  for (int k = 0; k < proxy1->Dimension(); k++)
                    for (int l = 0; l < proxy2->Dimension(); l++)
                      {
                        if (nonzeros(l1+l, k1+k))
                          {
                            if (k != l) is_diagonal = false;
                            is_nonzero = true;
                            ud.trialfunction = proxy1;
                            ud.trial_comp = k;
                            ud.testfunction = proxy2;
                            ud.test_comp = l;
                            
                            cf -> Evaluate (mir, val);
                            proxyvalues(STAR,k,l) = val.Col(0);
                          }
                        else
                          proxyvalues(STAR,k,l) = 0.0;
                      }
                else
                  for (int k = 0; k < proxy1->Dimension(); k++)
                    {
                      ud.trialfunction = proxy1;
                      ud.trial_comp = k;
                      ud.testfunction = proxy2;
                      ud.test_comp = k;

                      if (!elementwise_constant)
                        {
                          cf -> Evaluate (mir, val);
                          diagproxyvalues.Slice(k, proxy1->Dimension()) = val.Col(0);
                        }
                      else
                        {
                          cf -> Evaluate (mir[0], val.Row(0));
                          diagproxyvalues.Slice(k, proxy1->Dimension()) = val(0,0);
                        }
                    }
            
                // td.Stop();

                if (!mir.IsComplex())
                  {
                    if (!is_diagonal)
                      for (int i = 0; i < mir.Size(); i++)
                        proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();
                    else
                      for (int i = 0; i < mir.Size(); i++)
                        diagproxyvalues.Range(proxy1->Dimension()*IntRange(i,i+1)) *= mir[i].GetWeight();
                  }
                else
                  { // pml
                    if (!is_diagonal)
                      for (int i = 0; i < mir.Size(); i++)
                        proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();
                    else
                      for (int i = 0; i < mir.Size(); i++)
                        diagproxyvalues.Range(proxy1->Dimension()*IntRange(i,i+1)) *=
                          static_cast<const ScalMappedIntegrationPoint<SCAL>&> (mir[i]).GetJacobiDet()*ir[i].Weight();
                  }
                IntRange r1 = proxy1->Evaluator()->UsedDofs(fel_trial);
                IntRange r2 = proxy2->Evaluator()->UsedDofs(fel_test);
                SliceMatrix<SCAL_RES> part_elmat = elmat.Rows(r2).Cols(r1);
                FlatMatrix<SCAL_SHAPES,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
                FlatMatrix<SCAL_SHAPES,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);

                
                constexpr size_t BS = 16;
                for (size_t i = 0; i < mir.Size(); i+=BS)
                  {
                    HeapReset hr(lh);
                    int bs = min2(size_t(BS), mir.Size()-i);
                    
                    AFlatMatrix<SCAL_SHAPES> bbmat1(elmat.Width(), bs*proxy1->Dimension(), lh);
                    AFlatMatrix<SCAL> bdbmat1(elmat.Width(), bs*proxy2->Dimension(), lh);
                    AFlatMatrix<SCAL_SHAPES> bbmat2 = samediffop ?
                      bbmat1 : AFlatMatrix<SCAL_SHAPES>(elmat.Height(), bs*proxy2->Dimension(), lh);

                    // tb.Start();
                    BaseMappedIntegrationRule & bmir = mir.Range(i, i+bs, lh);
                    proxy1->Evaluator()->CalcMatrix(fel_trial, bmir, Trans(bbmat1), lh);
                    
                    if (!samediffop)
                      proxy2->Evaluator()->CalcMatrix(fel_test, bmir, Trans(bbmat2), lh);
                    // tb.Stop();

                    // tdb.Start();
                    if (is_diagonal)
                      {
                        AFlatVector<SCAL> diagd(bs*proxy1->Dimension(), lh);
                        diagd = diagproxyvalues.Range(i*proxy1->Dimension(),
                                                      (i+bs)*proxy1->Dimension());
                        /*
                        for (int i = 0; i < diagd.Size(); i++)
                          bdbmat1.Col(i) = diagd(i) * bbmat1.Col(i);
                        */
                        MultMatDiagMat(bbmat1, diagd, bdbmat1);
                        // tdb.AddFlops (bbmat1.Height()*bbmat1.Width());
                      }
                    else
                      {
                        for (int j = 0; j < bs; j++)
                          {
                            int ii = i+j;
                            IntRange r1 = proxy1->Dimension() * IntRange(j,j+1);
                            IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                            // bdbmat1.Cols(r2) = bbmat1.Cols(r1) * proxyvalues(ii,STAR,STAR);
                            MultMatMat (bbmat1.Cols(r1), proxyvalues(ii,STAR,STAR), bdbmat1.Cols(r2));
                          }
                        // tdb.AddFlops (proxy1->Dimension()*proxy2->Dimension()*bs*bbmat1.Height());
                      }
                    // tdb.Stop();
                    // tlapack.Start();
                    // elmat.Rows(r2).Cols(r1) += bbmat2.Rows(r2) * Trans(bdbmat1.Rows(r1));
                    // AddABt (bbmat2.Rows(r2), bdbmat1.Rows(r1), elmat.Rows(r2).Cols(r1));

                    symmetric_so_far &= samediffop && is_diagonal;
                    if (symmetric_so_far)
                      AddABtSym (bbmat2.Rows(r2), bdbmat1.Rows(r1), part_elmat);
                    else
                      AddABt (bbmat2.Rows(r2), bdbmat1.Rows(r1), part_elmat);
                    // tlapack.Stop();
                    // tlapack.AddFlops (r2.Size()*r1.Size()*bdbmat1.Width());
                  }

                if (symmetric_so_far)
                  for (int i = 0; i < part_elmat.Height(); i++)
                    for (int j = i+1; j < part_elmat.Width(); j++)
                      part_elmat(i,j) = part_elmat(j,i);
              }
            
            l1 += proxy2->Dimension();
            l1nr++;
          }
        k1 += proxy1->Dimension();
        k1nr++;
      }
  }
#endif  

  void 
  SymbolicBilinearFormIntegrator ::
  CalcElementMatrix (const FiniteElement & fel,
                     const ElementTransformation & trafo, 
                     FlatMatrix<double> elmat,
                     LocalHeap & lh) const
  {
    elmat = 0.0;
    T_CalcElementMatrixAdd<double,double,double> (fel, trafo, elmat, lh);
  }
  
  void 
  SymbolicBilinearFormIntegrator ::
  CalcElementMatrix (const FiniteElement & fel,
                     const ElementTransformation & trafo, 
                     FlatMatrix<Complex> elmat,
                     LocalHeap & lh) const
  {
    elmat = 0.0;
    if (fel.ComplexShapes() || trafo.IsComplex())
      T_CalcElementMatrixAdd<Complex,Complex,Complex> (fel, trafo, elmat, lh);
    else
      {
        if (cf->IsComplex())
          T_CalcElementMatrixAdd<Complex,double,Complex> (fel, trafo, elmat, lh);
        else
          T_CalcElementMatrixAdd<double,double,Complex> (fel, trafo, elmat, lh);
      }
  }

  void 
  SymbolicBilinearFormIntegrator ::
  CalcElementMatrixAdd (const FiniteElement & fel,
                        const ElementTransformation & trafo, 
                        FlatMatrix<double> elmat,
                        LocalHeap & lh) const
  {
    T_CalcElementMatrixAdd<double,double,double> (fel, trafo, elmat, lh);
  }
  
  void 
  SymbolicBilinearFormIntegrator ::
  CalcElementMatrixAdd (const FiniteElement & fel,
                        const ElementTransformation & trafo, 
                        FlatMatrix<Complex> elmat,
                        LocalHeap & lh) const
  {
    if (fel.ComplexShapes() || trafo.IsComplex())
      T_CalcElementMatrixAdd<Complex,Complex,Complex> (fel, trafo, elmat, lh);
    else
      if (cf->IsComplex())
        T_CalcElementMatrixAdd<Complex,double,Complex> (fel, trafo, elmat, lh);
      else
        T_CalcElementMatrixAdd<double,double,Complex> (fel, trafo, elmat, lh);
  }


  

  template <typename SCAL, typename SCAL_SHAPES, typename SCAL_RES>
  void SymbolicBilinearFormIntegrator ::
  T_CalcElementMatrixEBAdd (const FiniteElement & fel,
                            const ElementTransformation & trafo, 
                            FlatMatrix<SCAL_RES> elmat,
                            LocalHeap & lh) const
      
    {
      // static Timer t("symbolicBFI - CalcElementMatrix EB", 2);
      /*
      static Timer tir("symbolicBFI - CalcElementMatrix EB - intrules", 2);
      static Timer td("symbolicBFI - CalcElementMatrix EB - dmats", 2);
      static Timer tdb("symbolicBFI - CalcElementMatrix EB - b*d", 2);
      static Timer tb("symbolicBFI - CalcElementMatrix EB - bmats", 2);
      static Timer tmult("symbolicBFI - CalcElementMatrix EB - mult", 2);
      */
      // RegionTimer reg(t);

      // elmat = 0;

      const MixedFiniteElement * mixedfe = dynamic_cast<const MixedFiniteElement*> (&fel);
      const FiniteElement & fel_trial = mixedfe ? mixedfe->FETrial() : fel;
      const FiniteElement & fel_test = mixedfe ? mixedfe->FETest() : fel;
      
      auto eltype = trafo.GetElementType();
      int nfacet = ElementTopology::GetNFacets(eltype);

      Facet2ElementTrafo transform(eltype); 

      for (int k = 0; k < nfacet; k++)
        {
          // tir.Start();
          HeapReset hr(lh);
          ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
        
          const IntegrationRule& ir_facet = GetIntegrationRule(etfacet, fel_trial.Order()+fel_test.Order());
          IntegrationRule & ir_facet_vol = transform(k, ir_facet, lh);
          BaseMappedIntegrationRule & mir = trafo(ir_facet_vol, lh);
          
          ProxyUserData ud;
          const_cast<ElementTransformation&>(trafo).userdata = &ud;

          mir.ComputeNormalsAndMeasure(eltype, k);
          
          for (int k1 : Range(trial_proxies))
            for (int l1 : Range(test_proxies))
              {
                if (!nonzeros_proxies(l1, k1)) continue;
                
                auto proxy1 = trial_proxies[k1];
                auto proxy2 = test_proxies[l1];
                
                HeapReset hr(lh);
                FlatTensor<3,SCAL> proxyvalues(lh, mir.Size(), proxy1->Dimension(), proxy2->Dimension());
                FlatMatrix<SCAL> val(mir.Size(), 1, lh);
                
                // td.Start();
                for (int k = 0; k < proxy1->Dimension(); k++)
                  for (int l = 0; l < proxy2->Dimension(); l++)
                    {
                      ud.trialfunction = proxy1;
                      ud.trial_comp = k;
                      ud.testfunction = proxy2;
                      ud.test_comp = l;
                      
                      cf->Evaluate (mir, val);
                      for (int i = 0; i < mir.Size(); i++)
                        val(i) *= ir_facet[i].Weight() * mir[i].GetMeasure(); 

                      proxyvalues(STAR,k,l) = val.Col(0);
                    }
                // td.Stop();
                /*
                for (int i = 0; i < mir.Size(); i++)
                  {
                    tb.Start();
                    FlatMatrix<SCAL_SHAPES,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
                    FlatMatrix<SCAL,ColMajor> dbmat1(proxy2->Dimension(), elmat.Width(), lh);
                    FlatMatrix<SCAL_SHAPES,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);
                    
                    proxy1->Evaluator()->CalcMatrix(fel, mir[i], bmat1, lh);
                    proxy2->Evaluator()->CalcMatrix(fel, mir[i], bmat2, lh);
                    tb.Stop();
                    tmult.Start();
                    IntRange r1 = proxy1->Evaluator()->UsedDofs(fel);
                    IntRange r2 = proxy2->Evaluator()->UsedDofs(fel);
                    
                    dbmat1 = proxyvalues(i,STAR,STAR) * bmat1;                    
                    elmat.Rows(r2).Cols(r1) += Trans (bmat2.Cols(r2)) * dbmat1.Cols(r1);
                    tmult.Stop();
                  }
                */
                
                IntRange r1 = proxy1->Evaluator()->UsedDofs(fel_trial);
                IntRange r2 = proxy2->Evaluator()->UsedDofs(fel_test);
                SliceMatrix<SCAL_RES> part_elmat = elmat.Rows(r2).Cols(r1);

                constexpr size_t BS = 16;
                for (size_t i = 0; i < mir.Size(); i+=BS)
                  {
                    HeapReset hr(lh);
                    int bs = min2(BS, mir.Size()-i);
                    
                    AFlatMatrix<SCAL_SHAPES> bbmat1(elmat.Width(), bs*proxy1->Dimension(), lh);
                    AFlatMatrix<SCAL> bdbmat1(elmat.Width(), bs*proxy2->Dimension(), lh);
                    AFlatMatrix<SCAL_SHAPES> bbmat2(elmat.Height(), bs*proxy2->Dimension(), lh);

                    // tb.Start();
                    BaseMappedIntegrationRule & bmir = mir.Range(i, i+bs, lh);
                    proxy1->Evaluator()->CalcMatrix(fel_trial, bmir, Trans(bbmat1), lh);
                    proxy2->Evaluator()->CalcMatrix(fel_test, bmir, Trans(bbmat2), lh);
                    // tb.Stop();
                    bdbmat1 = 0.0;
                    // tdb.Start();

                    auto part_bbmat1 = bbmat1.Rows(r1);
                    auto part_bdbmat1 = bdbmat1.Rows(r1);
                    auto part_bbmat2 = bbmat2.Rows(r2);
                    
                    for (int j = 0; j < bs; j++)
                      {
                        IntRange rj1 = proxy1->Dimension() * IntRange(j,j+1);
                        IntRange rj2 = proxy2->Dimension() * IntRange(j,j+1);
                        MultMatMat (part_bbmat1.Cols(rj1), proxyvalues(i+j,STAR,STAR), part_bdbmat1.Cols(rj2));
                      }

                    // tdb.Stop();
                    
                    // tmult.Start();                    
                    AddABt (part_bbmat2, part_bdbmat1, part_elmat);
                    // part_elmat += part_bbmat2 * Trans(part_bdbmat1);
                    // tmult.Stop();
                    // tmult.AddFlops (r2.Size() * r1.Size() * bbmat2.Width());
                  }                
              }
        }
    }

  
  void 
  SymbolicBilinearFormIntegrator ::
  CalcLinearizedElementMatrix (const FiniteElement & fel,
                               const ElementTransformation & trafo, 
                               FlatVector<double> elveclin,
                               FlatMatrix<double> elmat,
                               LocalHeap & lh) const
  {
    /*
      CalcElementMatrix(fel, trafo, elmat, lh);
      return;
    */

      
    if (element_boundary)
      {
        T_CalcLinearizedElementMatrixEB<double,double> (fel, trafo, elveclin, elmat, lh);
        return;
      }

    
    static Timer t("symbolicbfi - calclinearized", 2);
    size_t tid = TaskManager::GetThreadId();
    ThreadRegionTimer reg(t, tid);
    
    // static Timer td("symbolicbfi - calclinearized dmats", 2);
    // RegionTimer reg(t);

    if (simd_evaluate)
      // if (false)
      try
        {
          const MixedFiniteElement * mixedfe = dynamic_cast<const MixedFiniteElement*> (&fel);
          const FiniteElement & fel_trial = mixedfe ? mixedfe->FETrial() : fel;
          const FiniteElement & fel_test = mixedfe ? mixedfe->FETest() : fel;

          const SIMD_IntegrationRule& ir = Get_SIMD_IntegrationRule (fel, lh);
          SIMD_BaseMappedIntegrationRule & mir = trafo(ir, lh);

          ProxyUserData ud(trial_proxies.Size(), gridfunction_cfs.Size(), lh);
          const_cast<ElementTransformation&>(trafo).userdata = &ud;
          ud.fel = &fel;
          // ud.elx = &elveclin;
          // ud.lh = &lh;
          for (ProxyFunction * proxy : trial_proxies)
            {
              ud.AssignMemory (proxy, ir.GetNIP(), proxy->Dimension(), lh);
              proxy->Evaluator()->Apply(fel_trial, mir, elveclin, ud.GetAMemory(proxy));
            }
          for (CoefficientFunction * cf : gridfunction_cfs)
            ud.AssignMemory (cf, ir.GetNIP(), cf->Dimension(), lh);
    
          // AFlatMatrix<> val(1, mir.IR().GetNIP(), lh);
          FlatMatrix<AutoDiff<1,SIMD<double>>> val(1, mir.Size(), lh);
          elmat = 0;
    
          for (int l1 : Range(test_proxies))
            {
              HeapReset hr(lh);              
              auto proxy2 = test_proxies[l1];
              FlatMatrix<SIMD<double>> bdbmat1(elmat.Width()*proxy2->Dimension(), ir.Size(), lh);
              FlatMatrix<SIMD<double>> hbdbmat1(elmat.Width(), proxy2->Dimension()*ir.Size(),
                                                &bdbmat1(0,0));
              bdbmat1 = 0.0;
              
              for (int k1 : Range(trial_proxies))
                {
                  HeapReset hr(lh);
                  auto proxy1 = trial_proxies[k1];
                  
                  FlatMatrix<SIMD<double>> proxyvalues(proxy1->Dimension()*proxy2->Dimension(), ir.Size(), lh);
                  
                  for (size_t k = 0, kk = 0; k < proxy1->Dimension(); k++)
                    for (size_t l = 0; l < proxy2->Dimension(); l++, kk++)
                      {
                        ud.trialfunction = proxy1;
                        ud.trial_comp = k;
                        ud.testfunction = proxy2;
                        ud.test_comp = l;
                        // cf -> EvaluateDeriv (mir, val, proxyvalues.Rows(kk,kk+1));
                        cf -> Evaluate (mir, val);
                        auto row = proxyvalues.Row(kk);
                        for (auto j : Range(mir.Size()))
                          row(j) = val(j).DValue(0);
                      }
                  
                  for (size_t i = 0; i < mir.Size(); i++)
                    proxyvalues.Col(i) *= mir[i].GetWeight();

                  IntRange r1 = proxy1->Evaluator()->UsedDofs(fel_trial);
                  
                  FlatMatrix<SIMD<double>> bbmat1(elmat.Width()*proxy1->Dimension(), ir.Size(), lh);
                  
                  // bbmat1 = 0.0;
                  proxy1->Evaluator()->CalcMatrix(fel_trial, mir, bbmat1);
                  for (auto i : r1)
                    for (size_t j = 0; j < proxy2->Dimension(); j++)
                      for (size_t k = 0; k < proxy1->Dimension(); k++)
                        {
                          bdbmat1.Row(i*proxy2->Dimension()+j) +=
                            pw_mult (bbmat1.Row(i*proxy1->Dimension()+k),
                                     proxyvalues.Row(k*proxy2->Dimension()+j));
                        }
                  
                }

              IntRange r2 = proxy2->Evaluator()->UsedDofs(fel_test);
              
              FlatMatrix<SIMD<double>> bbmat2(elmat.Height()*proxy2->Dimension(), ir.Size(), lh);
              FlatMatrix<SIMD<double>> hbbmat2(elmat.Height(), proxy2->Dimension()*ir.Size(),
                                              &bbmat2(0,0));
              // bbmat2 = 0.0;
              proxy2->Evaluator()->CalcMatrix(fel_test, mir, bbmat2);
              
              AddABt (hbbmat2.Rows(r2), hbdbmat1, elmat.Rows(r2));
              // AddABt (hbbmat2.Rows(r2), hbdbmat1.Rows(r1), elmat.Rows(r2).Cols(r1));
            }
          /*
          Matrix<> helmat = elmat;
          simd_evaluate = false;
          CalcLinearizedElementMatrix (fel, trafo, elveclin, elmat, lh);
          simd_evaluate = true;          
          double err = L2Norm(helmat-elmat);
          if (err > 1e-5) cout << "err = " << err << endl;
          */
          return;
        }
      catch (ExceptionNOSIMD e)
        {
          cout << IM(4) << e.What() << endl
               << "switching to scalar evaluation in CalcLinearized" << endl;
          simd_evaluate = false;
          CalcLinearizedElementMatrix (fel, trafo, elveclin, elmat, lh);
          return;
        }
    
    const MixedFiniteElement * mixedfe = dynamic_cast<const MixedFiniteElement*> (&fel);
    const FiniteElement & fel_trial = mixedfe ? mixedfe->FETrial() : fel;
    const FiniteElement & fel_test = mixedfe ? mixedfe->FETest() : fel;

    const IntegrationRule& ir = GetIntegrationRule (fel, lh);
    BaseMappedIntegrationRule & mir = trafo(ir, lh);

    ProxyUserData ud(trial_proxies.Size(), lh);
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    ud.fel = &fel;
    // ud.elx = &elveclin;
    // ud.lh = &lh;
    for (ProxyFunction * proxy : trial_proxies)
      {
        ud.AssignMemory (proxy, ir.Size(), proxy->Dimension(), lh);
        proxy->Evaluator()->Apply(fel_trial, mir, elveclin, ud.GetMemory(proxy), lh);
      }
    
    FlatMatrix<> val(mir.Size(), 1, lh), deriv(mir.Size(), 1, lh);
    elmat = 0;
    
    for (int k1 : Range(trial_proxies))
      for (int l1 : Range(test_proxies))
        {
          HeapReset hr(lh);
          auto proxy1 = trial_proxies[k1];
          auto proxy2 = test_proxies[l1];

          FlatTensor<3> proxyvalues(lh, mir.Size(), proxy2->Dimension(), proxy1->Dimension());
          
          for (int k = 0; k < proxy1->Dimension(); k++)
            for (int l = 0; l < proxy2->Dimension(); l++)
              // if (nonzeros(test_cum[l1]+l, trial_cum[k1]+k)) // does no work for non-linear 
              if (true)
                {
                  ud.trialfunction = proxy1;
                  ud.trial_comp = k;
                  ud.testfunction = proxy2;
                  ud.test_comp = l;
                  
                  cf -> EvaluateDeriv (mir, val, deriv);
                  proxyvalues(STAR,l,k) = deriv.Col(0);
                }
              else
                proxyvalues(STAR,l,k) = 0;

          for (int i = 0; i < mir.Size(); i++)
            proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();

          FlatMatrix<double,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
          FlatMatrix<double,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);

          constexpr size_t BS = 16;
          for (size_t i = 0; i < mir.Size(); i+=BS)
            {
              int rest = min2(size_t(BS), mir.Size()-i);
              HeapReset hr(lh);
              FlatMatrix<double,ColMajor> bdbmat1(rest*proxy2->Dimension(), elmat.Width(), lh);
              FlatMatrix<double,ColMajor> bbmat2(rest*proxy2->Dimension(), elmat.Height(), lh);

              for (int j = 0; j < rest; j++)
                {
                  int ii = i+j;
                  IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                  proxy1->Evaluator()->CalcMatrix(fel_trial, mir[ii], bmat1, lh);
                  proxy2->Evaluator()->CalcMatrix(fel_test, mir[ii], bmat2, lh);
                  bdbmat1.Rows(r2) = proxyvalues(ii,STAR,STAR) * bmat1;
                  bbmat2.Rows(r2) = bmat2;
                }

              IntRange r1 = proxy1->Evaluator()->UsedDofs(fel_trial);
              IntRange r2 = proxy2->Evaluator()->UsedDofs(fel_test);
              elmat.Rows(r2).Cols(r1) += Trans (bbmat2.Cols(r2)) * bdbmat1.Cols(r1) | Lapack;
            }
        }
  }



  template <typename SCAL, typename SCAL_SHAPES>
  void SymbolicBilinearFormIntegrator ::
  T_CalcLinearizedElementMatrixEB (const FiniteElement & fel,
                                   const ElementTransformation & trafo, 
                                   FlatVector<double> elveclin,
                                   FlatMatrix<double> elmat,
                                   LocalHeap & lh) const
  {
    size_t tid = TaskManager::GetThreadId();    
    static Timer t("symbolicbfi - calclinearized EB", 2);
    static Timer td("symbolicbfi - calclinearized EB dmats", 2);
    ThreadRegionTimer reg(t, tid);
    
    elmat = 0;

    auto eltype = trafo.GetElementType();
    int nfacet = ElementTopology::GetNFacets(eltype);
    
    Facet2ElementTrafo transform(eltype); 
    
    
    if (simd_evaluate)
      try
        {
          for (int k = 0; k < nfacet; k++)
            {
              HeapReset hr(lh);
              ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
              
              const SIMD_IntegrationRule& ir_facet = GetSIMDIntegrationRule(etfacet, 2*fel.Order());
              SIMD_IntegrationRule & ir_facet_vol = transform(k, ir_facet, lh);
              SIMD_BaseMappedIntegrationRule & mir = trafo(ir_facet_vol, lh);
              mir.ComputeNormalsAndMeasure(eltype, k);
              
              ProxyUserData ud(trial_proxies.Size(), gridfunction_cfs.Size(), lh);
              const_cast<ElementTransformation&>(trafo).userdata = &ud;
              ud.fel = &fel;
              // ud.elx = &elveclin;
              // ud.lh = &lh;
              for (ProxyFunction * proxy : trial_proxies)
                {
                  ud.AssignMemory (proxy, ir_facet.GetNIP(), proxy->Dimension(), lh);
                  proxy->Evaluator()->Apply(fel, mir, elveclin, ud.GetAMemory(proxy));
                }
              for (CoefficientFunction * cf : gridfunction_cfs)
                ud.AssignMemory (cf, ir_facet.GetNIP(), cf->Dimension(), lh);

              // AFlatMatrix<> val(1, mir.IR().GetNIP(), lh);
              FlatMatrix<AutoDiff<1,SIMD<double>>> val(1, mir.Size(), lh);
              
              for (int l1 : Range(test_proxies))
                {
                  HeapReset hr(lh);              
                  auto proxy2 = test_proxies[l1];
                  FlatMatrix<SIMD<double>> bdbmat1(elmat.Width()*proxy2->Dimension(), ir_facet.Size(), lh);
                  FlatMatrix<SIMD<double>> hbdbmat1(elmat.Width(), proxy2->Dimension()*ir_facet.Size(),
                                                    &bdbmat1(0,0));
                  bdbmat1 = 0.0;
                  
                  for (int k1 : Range(trial_proxies))
                    {
                      HeapReset hr(lh);
                      auto proxy1 = trial_proxies[k1];
                      
                      FlatMatrix<SIMD<double>> proxyvalues(proxy1->Dimension()*proxy2->Dimension(), ir_facet.Size(), lh);
                      
                      for (size_t k = 0, kk = 0; k < proxy1->Dimension(); k++)
                        for (size_t l = 0; l < proxy2->Dimension(); l++, kk++)
                          {
                            ud.trialfunction = proxy1;
                            ud.trial_comp = k;
                            ud.testfunction = proxy2;
                            ud.test_comp = l;
                            
                            // cf -> EvaluateDeriv (mir, val, proxyvalues.Rows(kk,kk+1));
                            cf -> Evaluate (mir, val);
                            auto row = proxyvalues.Row(kk);
                            for (auto j : Range(mir.Size()))
                              row(j) = val(j).DValue(0);
                          }
                      
                      for (size_t i = 0; i < mir.Size(); i++)
                        proxyvalues.Col(i) *= mir[i].GetWeight();                        
                      
                      IntRange r1 = proxy1->Evaluator()->UsedDofs(fel);
                      FlatMatrix<SIMD<double>> bbmat1(elmat.Width()*proxy1->Dimension(), ir_facet.Size(), lh);
                      proxy1->Evaluator()->CalcMatrix(fel, mir, bbmat1);
                      
                      for (auto i : r1)
                        for (size_t j = 0; j < proxy2->Dimension(); j++)
                          for (size_t k = 0; k < proxy1->Dimension(); k++)
                            {
                              bdbmat1.Row(i*proxy2->Dimension()+j) +=
                                pw_mult (bbmat1.Row(i*proxy1->Dimension()+k),
                                         proxyvalues.Row(k*proxy2->Dimension()+j));
                            }
                      
                    }
                  
                  IntRange r2 = proxy2->Evaluator()->UsedDofs(fel);
                  
                  FlatMatrix<SIMD<double>> bbmat2(elmat.Height()*proxy2->Dimension(), ir_facet.Size(), lh);
                  FlatMatrix<SIMD<double>> hbbmat2(elmat.Height(), proxy2->Dimension()*ir_facet.Size(),
                                                   &bbmat2(0,0));

                  proxy2->Evaluator()->CalcMatrix(fel, mir, bbmat2);
                  AddABt (hbbmat2.Rows(r2), hbdbmat1, elmat.Rows(r2));
                }
            }
          /*
          Matrix<> helmat = elmat;
          simd_evaluate = false;
          T_CalcLinearizedElementMatrixEB<D,SCAL,SCAL_SHAPES> (fel, trafo, elveclin, elmat, lh);
          simd_evaluate = true;          
          double err = L2Norm(helmat-elmat);
          if (err > 1e-5) cout << "EB, err = " << err << endl;
          */
          return;
        }
      catch (ExceptionNOSIMD e)
        {
          cout << IM(4) << e.What() << endl
               << "switching to scalar evaluation in CalcLinearizedEB" << endl;
          simd_evaluate = false;
          T_CalcLinearizedElementMatrixEB<SCAL,SCAL_SHAPES> (fel, trafo, elveclin, elmat, lh);
          return;
        }




      
    for (int k = 0; k < nfacet; k++)
      {
          HeapReset hr(lh);
          ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
        
          const IntegrationRule & ir_facet = GetIntegrationRule(etfacet, 2*fel.Order());
          IntegrationRule & ir_facet_vol = transform(k, ir_facet, lh);
          BaseMappedIntegrationRule & mir = trafo(ir_facet_vol, lh);
          mir.ComputeNormalsAndMeasure(eltype, k);
          
          ProxyUserData ud(trial_proxies.Size(), lh);          
          const_cast<ElementTransformation&>(trafo).userdata = &ud;
          ud.fel = &fel;
          // ud.elx = &elveclin;
          // ud.lh = &lh;

          for (ProxyFunction * proxy : trial_proxies)
            {
              ud.AssignMemory (proxy, mir.Size(), proxy->Dimension(), lh);
              proxy->Evaluator()->Apply(fel, mir, elveclin, ud.GetMemory(proxy), lh);
            }
    
    
          FlatMatrix<> val(mir.Size(), 1,lh), deriv(mir.Size(), 1,lh);
          for (int k1 : Range(trial_proxies))
            for (int l1 : Range(test_proxies))
              {
                HeapReset hr(lh);
                auto proxy1 = trial_proxies[k1];
                auto proxy2 = test_proxies[l1];
                // td.Start();
                NgProfiler::StartThreadTimer(td, tid);                
                FlatTensor<3> proxyvalues(lh, mir.Size(), proxy2->Dimension(), proxy1->Dimension());
                
                for (int k = 0; k < proxy1->Dimension(); k++)
                  for (int l = 0; l < proxy2->Dimension(); l++)
                    // if (nonzeros(test_cum[l1]+l, trial_cum[k1]+k))
                    if (true)
                      {
                        ud.trialfunction = proxy1;
                        ud.trial_comp = k;
                        ud.testfunction = proxy2;
                        ud.test_comp = l;
                        
                        cf -> EvaluateDeriv (mir, val, deriv);
                        proxyvalues(STAR,l,k) = deriv.Col(0);
                      }
                    else
                      proxyvalues(STAR,l,k) = 0.0;
                        
                // td.Stop();
                NgProfiler::StopThreadTimer(td, tid);                                

                for (int i = 0; i < mir.Size(); i++)
                  proxyvalues(i,STAR,STAR) *= ir_facet[i].Weight() * mir[i].GetMeasure(); 
                
                t.AddFlops (double (mir.Size()) * proxy1->Dimension()*elmat.Width()*elmat.Height());
                
                FlatMatrix<double,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
                FlatMatrix<double,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);
                
                constexpr size_t BS = 16;
                for (size_t i = 0; i < mir.Size(); i+=BS)
                  {
                    int rest = min2(BS, mir.Size()-i);
                    HeapReset hr(lh);
                    FlatMatrix<double,ColMajor> bdbmat1(rest*proxy2->Dimension(), elmat.Width(), lh);
                    FlatMatrix<double,ColMajor> bbmat2(rest*proxy2->Dimension(), elmat.Height(), lh);
                    
                    for (int j = 0; j < rest; j++)
                      {
                        int ii = i+j;
                        IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                        proxy1->Evaluator()->CalcMatrix(fel, mir[ii], bmat1, lh);
                        proxy2->Evaluator()->CalcMatrix(fel, mir[ii], bmat2, lh);
                        bdbmat1.Rows(r2) = proxyvalues(ii,STAR,STAR) * bmat1;
                        bbmat2.Rows(r2) = bmat2;
                      }
                    
                    IntRange r1 = proxy1->Evaluator()->UsedDofs(fel);
                    IntRange r2 = proxy2->Evaluator()->UsedDofs(fel);
                    // elmat.Rows(r2).Cols(r1) += Trans (bbmat2.Cols(r2)) * bdbmat1.Cols(r1) | Lapack;
                    AddABt (Trans(bbmat2).Rows(r2), Trans(bdbmat1).Rows(r1),
                            SliceMatrix<> (elmat.Rows(r2).Cols(r1)));
                  }
              }
        }
  }









  
  
  void
  SymbolicBilinearFormIntegrator :: ApplyElementMatrix (const FiniteElement & fel, 
                                                        const ElementTransformation & trafo, 
                                                        const FlatVector<double> elx, 
                                                        FlatVector<double> ely,
                                                        void * precomputed,
                                                        LocalHeap & lh) const
  {
    /*
    if (element_boundary)
      {
        FlatMatrix<> elmat(elx.Size(), lh);
        CalcElementMatrix(fel, trafo, elmat, lh);
        ely = elmat * elx;
        return;
      }
    */

    if (element_boundary)
      {
        T_ApplyElementMatrixEB<double,double> (fel, trafo, elx, ely, precomputed, lh);
        return;
      }


    if (simd_evaluate)
      try
        {
          // static Timer tall("SymbolicBFI::Apply - all", 4); RegionTimer rall(tall);
          /*
          const MixedFiniteElement * mixedfe = dynamic_cast<const MixedFiniteElement*> (&fel);
          const FiniteElement & fel_trial = mixedfe ? mixedfe->FETrial() : fel;
          const FiniteElement & fel_test = mixedfe ? mixedfe->FETest() : fel;
          */
          bool is_mixed = typeid(fel) == typeid(const MixedFiniteElement&);
          const MixedFiniteElement * mixedfe = static_cast<const MixedFiniteElement*> (&fel);    
          const FiniteElement & fel_trial = is_mixed ? mixedfe->FETrial() : fel;
          const FiniteElement & fel_test = is_mixed ? mixedfe->FETest() : fel;

          HeapReset hr(lh);

          const SIMD_IntegrationRule& simd_ir = Get_SIMD_IntegrationRule (fel, lh);
          auto & simd_mir = trafo(simd_ir, lh);
          
          ProxyUserData ud(trial_proxies.Size(), gridfunction_cfs.Size(), lh);
          const_cast<ElementTransformation&>(trafo).userdata = &ud;
          ud.fel = &fel;
          // ud.elx = &elx;
          // ud.lh = &lh;
          for (ProxyFunction * proxy : trial_proxies)
            ud.AssignMemory (proxy, simd_ir.GetNIP(), proxy->Dimension(), lh);
          for (CoefficientFunction * cf : gridfunction_cfs)
            ud.AssignMemory (cf, simd_ir.GetNIP(), cf->Dimension(), lh);
          
          for (ProxyFunction * proxy : trial_proxies)
            proxy->Evaluator()->Apply(fel_trial, simd_mir, elx, ud.GetAMemory(proxy)); 
          
          ely = 0;
          for (auto proxy : test_proxies)
            {
              HeapReset hr(lh);

              // AFlatMatrix<double> simd_proxyvalues(proxy->Dimension(), simd_ir.GetNIP(), lh);
              FlatMatrix<SIMD<double>> simd_proxyvalues(proxy->Dimension(), simd_ir.Size(), lh);
              for (int k = 0; k < proxy->Dimension(); k++)
                {
                  ud.testfunction = proxy;
                  ud.test_comp = k;
                  cf -> Evaluate (simd_mir, simd_proxyvalues.Rows(k,k+1));
                }

              /*
              for (size_t i = 0; i < simd_proxyvalues.Height(); i++)
                {
                  auto row = simd_proxyvalues.Row(i);
                  for (size_t j = 0; j < row.VSize(); j++)
                    row.Get(j) *= simd_mir[j].GetMeasure().Data() * simd_ir[j].Weight().Data();
                }
              */

              for (size_t i = 0; i < simd_proxyvalues.Height(); i++)
                {
                  auto row = simd_proxyvalues.Row(i);
                  for (size_t j = 0; j < row.Size(); j++)
                    row(j) *= simd_mir[j].GetWeight(); //  * simd_ir[j].Weight();
                }
              
              proxy->Evaluator()->AddTrans(fel_test, simd_mir, simd_proxyvalues, ely); 
            }
          /*
          Vector<> hely = ely;
          simd_evaluate = false;
          ApplyElementMatrix (fel, trafo, elx, ely, precomputed, lh);
          simd_evaluate = true;
          double err = L2Norm(hely-ely);
          if (err > 1e-8)
            cerr << "apply, err = " << err << endl;
          */
          return;
        }
      catch (ExceptionNOSIMD e)
        {
          cout << IM(4) << e.What() << endl
               << "switching to scalar evaluation" << endl;
          simd_evaluate = false;
          ApplyElementMatrix (fel, trafo, elx, ely, precomputed, lh);
          return;
        }

    
    // no simd
    HeapReset hr(lh);
    const MixedFiniteElement * mixedfe = dynamic_cast<const MixedFiniteElement*> (&fel);
    const FiniteElement & fel_trial = mixedfe ? mixedfe->FETrial() : fel;
    const FiniteElement & fel_test = mixedfe ? mixedfe->FETest() : fel;
 
    ProxyUserData ud(trial_proxies.Size(), lh);    
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    ud.fel = &fel;
    // ud.elx = &elx;
    // ud.lh = &lh;

    // IntegrationRule ir(trafo.GetElementType(), fel_trial.Order()+fel_test.Order());
    IntegrationRule ir = GetIntegrationRule (fel, lh);

    BaseMappedIntegrationRule & mir = trafo(ir, lh);

    for (ProxyFunction * proxy : trial_proxies)
      ud.AssignMemory (proxy, ir.GetNIP(), proxy->Dimension(), lh);

    for (ProxyFunction * proxy : trial_proxies)
      proxy->Evaluator()->Apply(fel_trial, mir, elx, ud.GetMemory(proxy), lh);
    
    ely = 0;
    FlatVector<> ely1(ely.Size(), lh);

    FlatMatrix<> val(mir.Size(), 1,lh);
    for (auto proxy : test_proxies)
      {
        HeapReset hr(lh);
        FlatMatrix<> proxyvalues(mir.Size(), proxy->Dimension(), lh);
        for (int k = 0; k < proxy->Dimension(); k++)
          {
            ud.testfunction = proxy;
            ud.test_comp = k;
            cf -> Evaluate (mir, val);
            proxyvalues.Col(k) = val.Col(0);
          }
        
        for (int i = 0; i < mir.Size(); i++)
          proxyvalues.Row(i) *= mir[i].GetWeight();

        proxy->Evaluator()->ApplyTrans(fel_test, mir, proxyvalues, ely1, lh);
        ely += ely1;
      }
  }


 
  
  
  template <typename SCAL, typename SCAL_SHAPES>
  void SymbolicBilinearFormIntegrator ::
  T_ApplyElementMatrixEB (const FiniteElement & fel, 
                          const ElementTransformation & trafo, 
                          const FlatVector<double> elx, 
                          FlatVector<double> ely,
                          void * precomputed,
                          LocalHeap & lh) const
  {
    ely = 0;
    
    auto eltype = trafo.GetElementType();
    int nfacet = ElementTopology::GetNFacets(eltype);

    Facet2ElementTrafo transform(eltype); 

    for (int k = 0; k < nfacet; k++)
      {
        HeapReset hr(lh);
        ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
          
        const IntegrationRule & ir_facet = GetIntegrationRule(etfacet, 2*fel.Order());
        IntegrationRule & ir_facet_vol = transform(k, ir_facet, lh);
        BaseMappedIntegrationRule & mir = trafo(ir_facet_vol, lh);
        mir.ComputeNormalsAndMeasure (eltype, k);
        

        ProxyUserData ud(trial_proxies.Size(), lh);    
        const_cast<ElementTransformation&>(trafo).userdata = &ud;
        ud.fel = &fel;
        // ud.elx = &elx;
        // ud.lh = &lh;
    
        for (ProxyFunction * proxy : trial_proxies)
          ud.AssignMemory (proxy, ir_facet.GetNIP(), proxy->Dimension(), lh);
        
        for (ProxyFunction * proxy : trial_proxies)
          proxy->Evaluator()->Apply(fel, mir, elx, ud.GetMemory(proxy), lh);
        
        FlatVector<> ely1(ely.Size(), lh);
        FlatMatrix<> val(mir.Size(), 1,lh);
        for (auto proxy : test_proxies)
          {
            HeapReset hr(lh);
            FlatMatrix<> proxyvalues(mir.Size(), proxy->Dimension(), lh);
            for (int k = 0; k < proxy->Dimension(); k++)
              {
                ud.testfunction = proxy;
                ud.test_comp = k;
                cf -> Evaluate (mir, val);
                proxyvalues.Col(k) = val.Col(0);
              }
            
            for (size_t i = 0; i < mir.Size(); i++)
              proxyvalues.Row(i) *= ir_facet[i].Weight() * mir[i].GetMeasure();
            
            proxy->Evaluator()->ApplyTrans(fel, mir, proxyvalues, ely1, lh);
            ely += ely1;
          }
      }
  }


  SymbolicFacetLinearFormIntegrator ::
  SymbolicFacetLinearFormIntegrator (shared_ptr<CoefficientFunction> acf, VorB avb /* , bool eb */)
    : cf(acf), vb(avb) // , element_boundary(eb)
  {
    if (cf->Dimension() != 1)
      throw Exception ("SymblicLFI needs scalar-valued CoefficientFunction");
    test_cum.Append(0);    
    cf->TraverseTree
      ( [&] (CoefficientFunction & nodecf)
        {
          auto proxy = dynamic_cast<ProxyFunction*> (&nodecf);
          if (proxy) 
            if (proxy->IsTestFunction())
              if (!proxies.Contains(proxy))
                {
                  proxies.Append (proxy);
                  test_cum.Append(test_cum.Last()+proxy->Dimension());
                }
        });
  }


  void SymbolicFacetLinearFormIntegrator ::
  CalcFacetVector (const FiniteElement & fel1, int LocalFacetNr,
                   const ElementTransformation & trafo1, FlatArray<int> & ElVertices,
                   const ElementTransformation & strafo,  
                   FlatVector<double> elvec,
                   LocalHeap & lh) const
  {
    static Timer t("SymbolicFacetLFI::CalcFacetVector - boundary", 2);
    HeapReset hr(lh);


    elvec = 0;
    
    FlatVector<> elvec1(elvec.Size(), lh);

    int maxorder = fel1.Order();

    auto eltype1 = trafo1.GetElementType();
    auto etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr);

    const IntegrationRule & ir_facet = GetIntegrationRule(etfacet, 2*maxorder);
    Facet2ElementTrafo transform1(eltype1, ElVertices); 
    IntegrationRule & ir_facet_vol1 = transform1(LocalFacetNr, ir_facet, lh);
    BaseMappedIntegrationRule & mir1 = trafo1(ir_facet_vol1, lh);
    auto & smir = strafo(ir_facet, lh);

    mir1.SetOtherMIR (&smir);
    smir.SetOtherMIR (&mir1);
    
    // evaluate proxy-values
    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo1).userdata = &ud;
    const_cast<ElementTransformation&>(strafo).userdata = &ud;

    RegionTimer reg(t);
    
    mir1.ComputeNormalsAndMeasure (eltype1, LocalFacetNr);
    
    FlatMatrix<> val(ir_facet.Size(), 1,lh);
    for (auto proxy : proxies)
      {
        HeapReset hr(lh);
        FlatMatrix<> proxyvalues(ir_facet.Size(), proxy->Dimension(), lh);
        
        for (int k = 0; k < proxy->Dimension(); k++)
          {
            ud.testfunction = proxy;
            ud.test_comp = k;
            cf -> Evaluate (mir1, val);  // needed for grad(u), mesh_size, but: index = material index
            // cf -> Evaluate (smir, val);
            proxyvalues.Col(k) = val.Col(0);
          }

        for (int i = 0; i < mir1.Size(); i++)
          proxyvalues.Row(i) *= mir1[i].GetMeasure() * ir_facet[i].Weight(); // use also smir here ? 

        elvec1 = 0.0;
        proxy->Evaluator()->ApplyTrans(fel1, mir1, proxyvalues, elvec1, lh);
        elvec += elvec1;
      }
  }



  
  
  SymbolicFacetBilinearFormIntegrator ::
  SymbolicFacetBilinearFormIntegrator (shared_ptr<CoefficientFunction> acf, VorB avb, bool eb)
    : cf(acf), vb(avb), element_boundary(eb)
  {
    simd_evaluate = true;
    
    if (cf->Dimension() != 1)
        throw Exception ("SymblicBFI needs scalar-valued CoefficientFunction");
    trial_cum.Append(0);
    test_cum.Append(0);    
    cf->TraverseTree
      ( [&] (CoefficientFunction & nodecf)
        {
          auto proxy = dynamic_cast<ProxyFunction*> (&nodecf);
          if (proxy) 
            {
              if (proxy->IsTestFunction())
                {
                  if (!test_proxies.Contains(proxy))
                    {
                      test_proxies.Append (proxy);
                      test_cum.Append(test_cum.Last()+proxy->Dimension());
                    }
                }
              else
                {                                         
                  if (!trial_proxies.Contains(proxy))
                    {
                      trial_proxies.Append (proxy);
                      trial_cum.Append(trial_cum.Last()+proxy->Dimension());
                    }
                }
            }
          else
            if (nodecf.StoreUserData())
              gridfunction_cfs.Append (&nodecf);
        });

    neighbor_testfunction = false;
    for (auto proxy : test_proxies)
      if (proxy->IsOther())
        neighbor_testfunction = true;
    
    cout << IM(3) << "num test_proxies " << test_proxies.Size() << endl;
    cout << IM(3) << "num trial_proxies " << trial_proxies.Size() << endl;
    cout << IM(3) << "cumulated test_proxy dims  " << test_cum << endl;
    cout << IM(3) << "cumulated trial_proxy dims " << trial_cum << endl;
  }


  void SymbolicFacetBilinearFormIntegrator ::
  CalcFacetMatrix (const FiniteElement & fel1, int LocalFacetNr1,
                   const ElementTransformation & trafo1, FlatArray<int> & ElVertices1,
                   const FiniteElement & fel2, int LocalFacetNr2,
                   const ElementTransformation & trafo2, FlatArray<int> & ElVertices2,
                   FlatMatrix<double> elmat,
                   LocalHeap & lh) const
  {
    elmat = 0.0;

    if (LocalFacetNr2==-1) throw Exception ("SymbolicFacetBFI: LocalFacetNr2==-1");

    int maxorder = max2 (fel1.Order(), fel2.Order());

    auto eltype1 = trafo1.GetElementType();
    auto eltype2 = trafo2.GetElementType();
    auto etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr1);

    const IntegrationRule & ir_facet = GetIntegrationRule(etfacet, 2*maxorder);
    Facet2ElementTrafo transform1(eltype1, ElVertices1); 
    IntegrationRule & ir_facet_vol1 = transform1(LocalFacetNr1, ir_facet, lh);
    BaseMappedIntegrationRule & mir1 = trafo1(ir_facet_vol1, lh);
    
    Facet2ElementTrafo transform2(eltype2, ElVertices2); 
    IntegrationRule & ir_facet_vol2 = transform2(LocalFacetNr2, ir_facet, lh);
    BaseMappedIntegrationRule & mir2 = trafo2(ir_facet_vol2, lh);

    mir1.SetOtherMIR (&mir2);
    mir2.SetOtherMIR (&mir1);

    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo1).userdata = &ud;

    for (int k1 : Range(trial_proxies))
      for (int l1 : Range(test_proxies))
        {
          HeapReset hr(lh);
          FlatMatrix<> val(mir1.Size(), 1,lh);
          
          auto proxy1 = trial_proxies[k1];
          auto proxy2 = test_proxies[l1];

          FlatTensor<3> proxyvalues(lh, mir1.Size(), proxy2->Dimension(), proxy1->Dimension());

          mir1.ComputeNormalsAndMeasure (eltype1, LocalFacetNr1);
          mir2.ComputeNormalsAndMeasure (eltype2, LocalFacetNr2);
          
          for (int k = 0; k < proxy1->Dimension(); k++)
            for (int l = 0; l < proxy2->Dimension(); l++)
              {
                ud.trialfunction = proxy1;
                ud.trial_comp = k;
                ud.testfunction = proxy2;
                ud.test_comp = l;
                
                cf -> Evaluate (mir1, val);
                proxyvalues(STAR,l,k) = val.Col(0);
              }

          for (int i = 0; i < mir1.Size(); i++)
            // proxyvalues(i,STAR,STAR) *= measure(i) * ir_facet[i].Weight();
            proxyvalues(i,STAR,STAR) *= mir1[i].GetMeasure() * ir_facet[i].Weight();

          IntRange trial_range  = proxy1->IsOther() ? IntRange(proxy1->Evaluator()->BlockDim()*fel1.GetNDof(), elmat.Width()) : IntRange(0, proxy1->Evaluator()->BlockDim()*fel1.GetNDof());
          IntRange test_range  = proxy2->IsOther() ? IntRange(proxy2->Evaluator()->BlockDim()*fel1.GetNDof(), elmat.Height()) : IntRange(0, proxy2->Evaluator()->BlockDim()*fel1.GetNDof());

          auto loc_elmat = elmat.Rows(test_range).Cols(trial_range);
          FlatMatrix<double,ColMajor> bmat1(proxy1->Dimension(), loc_elmat.Width(), lh);
          FlatMatrix<double,ColMajor> bmat2(proxy2->Dimension(), loc_elmat.Height(), lh);

          constexpr size_t BS = 16;
          for (size_t i = 0; i < mir1.Size(); i+=BS)
            {
              int rest = min2(size_t(BS), mir1.Size()-i);
              HeapReset hr(lh);
              FlatMatrix<double,ColMajor> bdbmat1(rest*proxy2->Dimension(), loc_elmat.Width(), lh);
              FlatMatrix<double,ColMajor> bbmat2(rest*proxy2->Dimension(), loc_elmat.Height(), lh);

              for (int j = 0; j < rest; j++)
                {
                  int ii = i+j;
                  IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                  if (proxy1->IsOther())
                    proxy1->Evaluator()->CalcMatrix(fel2, mir2[ii], bmat1, lh);
                  else
                    proxy1->Evaluator()->CalcMatrix(fel1, mir1[ii], bmat1, lh);
                  
                  if (proxy2->IsOther())
                    proxy2->Evaluator()->CalcMatrix(fel2, mir2[ii], bmat2, lh);
                  else
                    proxy2->Evaluator()->CalcMatrix(fel1, mir1[ii], bmat2, lh);
                  
                  bdbmat1.Rows(r2) = proxyvalues(ii,STAR,STAR) * bmat1;
                  bbmat2.Rows(r2) = bmat2;
                }

              IntRange r1 = proxy1->Evaluator()->UsedDofs(proxy1->IsOther() ? fel2 : fel1);
              IntRange r2 = proxy2->Evaluator()->UsedDofs(proxy2->IsOther() ? fel2 : fel1);
              loc_elmat.Rows(r2).Cols(r1) += Trans (bbmat2.Cols(r2)) * bdbmat1.Cols(r1) | Lapack;
            }
        }
  }



  void SymbolicFacetBilinearFormIntegrator ::
  CalcFacetMatrix (const FiniteElement & fel1, int LocalFacetNr1,
                   const ElementTransformation & trafo1, FlatArray<int> & ElVertices1,
                   const ElementTransformation & strafo, FlatArray<int> & SElVertices1,
                   FlatMatrix<double> elmat,
                   LocalHeap & lh) const
  {
    elmat = 0.0;

    int maxorder = fel1.Order();

    auto etvol = trafo1.GetElementType();
    auto etfacet = ElementTopology::GetFacetType (etvol, LocalFacetNr1);

    const IntegrationRule& ir_facet = GetIntegrationRule(etfacet, 2*maxorder);
    Facet2ElementTrafo transform1(etvol, ElVertices1);
    Facet2SurfaceElementTrafo stransform(strafo.GetElementType(), SElVertices1); 
    
    IntegrationRule & ir_facet_vol1 = transform1(LocalFacetNr1, ir_facet, lh);
    // IntegrationRule & ir_facet_surf = stransform(ir_facet, lh);  // not yet used ???
    
    BaseMappedIntegrationRule & mir1 = trafo1(ir_facet_vol1, lh);
    mir1.ComputeNormalsAndMeasure (etvol, LocalFacetNr1);          
    
    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo1).userdata = &ud;

    for (int k1 : Range(trial_proxies))
      for (int l1 : Range(test_proxies))
        {
          HeapReset hr(lh);
          FlatMatrix<> val(mir1.Size(), 1, lh);
          
          auto proxy1 = trial_proxies[k1];
          auto proxy2 = test_proxies[l1];
          if (proxy1->IsOther() || proxy2->IsOther()) continue;

          FlatTensor<3> proxyvalues(lh, mir1.Size(), proxy2->Dimension(), proxy1->Dimension());
          
          for (size_t k = 0; k < proxy1->Dimension(); k++)
            for (size_t l = 0; l < proxy2->Dimension(); l++)
              {
                ud.trialfunction = proxy1;
                ud.trial_comp = k;
                ud.testfunction = proxy2;
                ud.test_comp = l;
                
                cf -> Evaluate (mir1, val);
                proxyvalues(STAR,l,k) = val.Col(0);
              }

          for (size_t i = 0; i < mir1.Size(); i++)
            proxyvalues(i,STAR,STAR) *=  mir1[i].GetMeasure() * ir_facet[i].Weight();

          FlatMatrix<double,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
          FlatMatrix<double,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);

          constexpr size_t BS = 16;          
          for (size_t i = 0; i < mir1.Size(); i+=BS)
            {
              size_t rest = min2(size_t(BS), mir1.Size()-i);
              HeapReset hr(lh);
              FlatMatrix<double,ColMajor> bdbmat1(rest*proxy2->Dimension(), elmat.Width(), lh);
              FlatMatrix<double,ColMajor> bbmat2(rest*proxy2->Dimension(), elmat.Height(), lh);

              for (size_t j = 0; j < rest; j++)
                {
                  size_t ii = i+j;
                  IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                  proxy1->Evaluator()->CalcMatrix(fel1, mir1[ii], bmat1, lh);
                  proxy2->Evaluator()->CalcMatrix(fel1, mir1[ii], bmat2, lh);
                  bdbmat1.Rows(r2) = proxyvalues(ii,STAR,STAR) * bmat1;
                  bbmat2.Rows(r2) = bmat2;
                }

              IntRange r1 = proxy1->Evaluator()->UsedDofs(fel1);
              IntRange r2 = proxy2->Evaluator()->UsedDofs(fel1);
              elmat.Rows(r2).Cols(r1) += Trans (bbmat2.Cols(r2)) * bdbmat1.Cols(r1) | Lapack;
            }
        }
  }


  void SymbolicFacetBilinearFormIntegrator ::
  CalcLinearizedFacetMatrix (const FiniteElement & fel1, int LocalFacetNr1,
                             const ElementTransformation & trafo1, FlatArray<int> & ElVertices1,
                             const ElementTransformation & strafo, FlatArray<int> & SElVertices1,  
                             FlatVector<double> elveclin, FlatMatrix<double> elmat,
                             LocalHeap & lh) const
  {
    elmat = 0.0;

    int maxorder = fel1.Order();

    auto eltype1 = trafo1.GetElementType();
    auto etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr1);

    const IntegrationRule& ir_facet = GetIntegrationRule(etfacet, 2*maxorder);
    Facet2ElementTrafo transform1(eltype1, ElVertices1);
    Facet2SurfaceElementTrafo stransform(strafo.GetElementType(), SElVertices1); 
    
    IntegrationRule & ir_facet_vol1 = transform1(LocalFacetNr1, ir_facet, lh);
    // IntegrationRule & ir_facet_surf = stransform(ir_facet, lh);  // not yet used ???
    
    BaseMappedIntegrationRule & mir1 = trafo1(ir_facet_vol1, lh);
    mir1.ComputeNormalsAndMeasure (eltype1, LocalFacetNr1);          
    
    // new 
    ProxyUserData ud(trial_proxies.Size(), lh);
    const_cast<ElementTransformation&>(trafo1).userdata = &ud;
    ud.fel = &fel1;
    // ud.elx = &elveclin;
    // ud.lh = &lh;
    for (ProxyFunction * proxy : trial_proxies)
      {
        ud.AssignMemory (proxy, ir_facet_vol1.Size(), proxy->Dimension(), lh);
        proxy->Evaluator()->Apply(fel1, mir1, elveclin, ud.GetMemory(proxy), lh);
      }
    
    FlatMatrix<> val(mir1.Size(), 1, lh), deriv(mir1.Size(), 1, lh);
    elmat = 0;
    // endnew


    
    for (int k1 : Range(trial_proxies))
      for (int l1 : Range(test_proxies))
        {
          HeapReset hr(lh);
          // FlatMatrix<> val(mir1.Size(), 1,lh);
          
          auto proxy1 = trial_proxies[k1];
          auto proxy2 = test_proxies[l1];
          if (proxy1->IsOther() || proxy2->IsOther()) continue;

          FlatTensor<3> proxyvalues(lh, mir1.Size(), proxy2->Dimension(), proxy1->Dimension());
          
          for (int k = 0; k < proxy1->Dimension(); k++)
            for (int l = 0; l < proxy2->Dimension(); l++)
              {
                ud.trialfunction = proxy1;
                ud.trial_comp = k;
                ud.testfunction = proxy2;
                ud.test_comp = l;
                
                cf -> EvaluateDeriv (mir1, val, deriv);
                proxyvalues(STAR,l,k) = deriv.Col(0);
              }

          for (int i = 0; i < mir1.Size(); i++)
            proxyvalues(i,STAR,STAR) *=  mir1[i].GetMeasure() * ir_facet[i].Weight();

          // auto loc_elmat = elmat.Rows(test_range).Cols(trial_range);
          FlatMatrix<double,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
          FlatMatrix<double,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);

          constexpr size_t BS = 16;          
          for (size_t i = 0; i < mir1.Size(); i+=BS)
            {
              int rest = min2(size_t(BS), mir1.Size()-i);
              HeapReset hr(lh);
              FlatMatrix<double,ColMajor> bdbmat1(rest*proxy2->Dimension(), elmat.Width(), lh);
              FlatMatrix<double,ColMajor> bbmat2(rest*proxy2->Dimension(), elmat.Height(), lh);

              for (int j = 0; j < rest; j++)
                {
                  int ii = i+j;
                  IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                  proxy1->Evaluator()->CalcMatrix(fel1, mir1[ii], bmat1, lh);
                  proxy2->Evaluator()->CalcMatrix(fel1, mir1[ii], bmat2, lh);
                  bdbmat1.Rows(r2) = proxyvalues(ii,STAR,STAR) * bmat1;
                  bbmat2.Rows(r2) = bmat2;
                }

              IntRange r1 = proxy1->Evaluator()->UsedDofs(fel1);
              IntRange r2 = proxy2->Evaluator()->UsedDofs(fel1);
              elmat.Rows(r2).Cols(r1) += Trans (bbmat2.Cols(r2)) * bdbmat1.Cols(r1) | Lapack;
            }
        }
  }
  


  void SymbolicFacetBilinearFormIntegrator ::
  ApplyFacetMatrix (const FiniteElement & fel1, int LocalFacetNr1,
                    const ElementTransformation & trafo1, FlatArray<int> & ElVertices1,
                    const FiniteElement & fel2, int LocalFacetNr2,
                    const ElementTransformation & trafo2, FlatArray<int> & ElVertices2,
                    FlatVector<double> elx, FlatVector<double> ely,
                    LocalHeap & lh) const
  {
    // const TPHighOrderFE * tpfel = dynamic_cast<const TPHighOrderFE *>(&fel1);
    // if(tpfel)
    // {
      // int facet_x_y = 0;
      // if(LocalFacetNr1>=10)
      // {
        // facet_x_y = 1;
        // LocalFacetNr1-=10;
      // }
      // ApplyFacetMatrixTP(fel1,LocalFacetNr1,trafo1,ElVertices1,fel2,LocalFacetNr2,trafo2,ElVertices2,elx,ely,facet_x_y,lh);
      // return;
    // }
    if (simd_evaluate)
      {
        try
          {
            static Timer tall("SymbolicFacetBFI::Apply - all", 2); // RegionTimer rall(tall);
            static Timer tstart("SymbolicFacetBFI::Apply - startup", 2);
            static Timer tapply("SymbolicFacetBFI::Apply - apply", 2);
            static Timer tcoef("SymbolicFacetBFI::Apply - coef", 2);
            static Timer tapplyt("SymbolicFacetBFI::Apply - apply-trans", 2); 

            ThreadRegionTimer reg(tall, TaskManager::GetThreadId());
            HeapReset hr(lh);
            // tall.Start();
            
            // tstart.Start();
            /*
              Matrix<> elmat(elx.Size());
              CalcFacetMatrix(fel1, LocalFacetNr1, trafo1, ElVertices1,
              fel2, LocalFacetNr2, trafo2, ElVertices2, elmat, lh);
              ely = elmat * elx;
              return;
            */
            
            ely = 0;
            
            int maxorder = max2 (fel1.Order(), fel2.Order());
            
            auto eltype1 = trafo1.GetElementType();
            auto eltype2 = trafo2.GetElementType();
            auto etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr1);
            
            Facet2ElementTrafo transform1(eltype1, ElVertices1); 
            Facet2ElementTrafo transform2(eltype2, ElVertices2); 
            
            const SIMD_IntegrationRule& simd_ir_facet = GetSIMDIntegrationRule(etfacet, 2*maxorder);
            
            auto & simd_ir_facet_vol1 = transform1(LocalFacetNr1, simd_ir_facet, lh);
            auto & simd_mir1 = trafo1(simd_ir_facet_vol1, lh);
            
            auto & simd_ir_facet_vol2 = transform2(LocalFacetNr2, simd_ir_facet, lh);
            auto & simd_mir2 = trafo2(simd_ir_facet_vol2, lh);

            simd_mir1.SetOtherMIR(&simd_mir2);
            simd_mir2.SetOtherMIR(&simd_mir1);

            
            simd_mir1.ComputeNormalsAndMeasure(eltype1, LocalFacetNr1);
            simd_mir2.ComputeNormalsAndMeasure(eltype2, LocalFacetNr2);
            
            // evaluate proxy-values
            ProxyUserData ud(trial_proxies.Size(), gridfunction_cfs.Size(), lh);
            const_cast<ElementTransformation&>(trafo1).userdata = &ud;
            ud.fel = &fel1;   // necessary to check remember-map
            // ud.elx = &elx;
            // ud.lh = &lh;
            for (ProxyFunction * proxy : trial_proxies)
              ud.AssignMemory (proxy, simd_ir_facet.GetNIP(), proxy->Dimension(), lh);
            for (CoefficientFunction * cf : gridfunction_cfs)
              ud.AssignMemory (cf, simd_ir_facet.GetNIP(), cf->Dimension(), lh);
            // tstart.Stop();
            // tapply.Start();
            
            for (ProxyFunction * proxy : trial_proxies)
              {
                IntRange trial_range  = proxy->IsOther() ? IntRange(proxy->Evaluator()->BlockDim()*fel1.GetNDof(), elx.Size()) : IntRange(0, proxy->Evaluator()->BlockDim()*fel1.GetNDof());
                
                if (proxy->IsOther())
                  proxy->Evaluator()->Apply(fel2, simd_mir2, elx.Range(trial_range), ud.GetAMemory(proxy)); // , lh);
                else
                  proxy->Evaluator()->Apply(fel1, simd_mir1, elx.Range(trial_range), ud.GetAMemory(proxy)); // , lh);
                // tapply.AddFlops (trial_range.Size() * simd_ir_facet_vol1.GetNIP());
              }
            // tapply.Stop();
            
            for (auto proxy : test_proxies)
              {
                HeapReset hr(lh);
                // tcoef.Start();
                FlatMatrix<SIMD<double>> simd_proxyvalues(proxy->Dimension(), simd_ir_facet.Size(), lh);        
                
                for (int k = 0; k < proxy->Dimension(); k++)
                  {
                    ud.testfunction = proxy;
                    ud.test_comp = k;
                    cf -> Evaluate (simd_mir1, simd_proxyvalues.Rows(k,k+1));
                  }
                
                for (int i = 0; i < simd_proxyvalues.Height(); i++)
                  {
                    auto row = simd_proxyvalues.Row(i);
                    for (int j = 0; j < row.Size(); j++)
                      row(j) *= simd_mir1[j].GetMeasure() * simd_ir_facet[j].Weight();
                  }
                // tcoef.Stop();
                // tapplyt.Start();
                IntRange test_range  = proxy->IsOther() ? IntRange(fel1.GetNDof(), elx.Size()) : IntRange(0, fel1.GetNDof());
                int blockdim = proxy->Evaluator()->BlockDim();
                test_range = blockdim * test_range;
                
                if (proxy->IsOther())
                  proxy->Evaluator()->AddTrans(fel2, simd_mir2, simd_proxyvalues, ely.Range(test_range));
                else
                  proxy->Evaluator()->AddTrans(fel1, simd_mir1, simd_proxyvalues, ely.Range(test_range));
                // tapplyt.AddFlops (test_range.Size() * simd_ir_facet_vol1.GetNIP());                
                // tapplyt.Stop();
              }
            // tall.Stop();
          }
        catch (ExceptionNOSIMD e)
          {
            cout << IM(4) << "caught in SymbolicFacetInegtrator::Apply: " << endl
                 << e.What() << endl;
            simd_evaluate = false;
            ApplyFacetMatrix (fel1, LocalFacetNr1, trafo1, ElVertices1,
                              fel2, LocalFacetNr2, trafo2, ElVertices2,
                              elx, ely, lh);
          }
        return;
      }
        
    
    static Timer tall("SymbolicFacetBFI::Apply - all", 2); RegionTimer rall(tall);
    /*
    static Timer t("SymbolicFacetBFI::Apply", 2);
    static Timer ts1("SymbolicFacetBFI::Apply start 1", 2);
    static Timer ts2("SymbolicFacetBFI::Apply start 2", 2);
    static Timer t1("SymbolicFacetBFI::Apply 1", 2);
    static Timer t2("SymbolicFacetBFI::Apply 2", 2);
    static Timer t3("SymbolicFacetBFI::Apply 3", 2);
    */
    
    HeapReset hr(lh);
    // ts1.Start();
    /*
    Matrix<> elmat(elx.Size());
    CalcFacetMatrix(fel1, LocalFacetNr1, trafo1, ElVertices1,
                    fel2, LocalFacetNr2, trafo2, ElVertices2, elmat, lh);
    ely = elmat * elx;
    return;
    */
    
    ely = 0;
    
    FlatVector<> ely1(ely.Size(), lh);

    int maxorder = max2 (fel1.Order(), fel2.Order());

    auto eltype1 = trafo1.GetElementType();
    auto eltype2 = trafo2.GetElementType();
    auto etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr1);

    const IntegrationRule& ir_facet = GetIntegrationRule(etfacet, 2*maxorder);
    
    Facet2ElementTrafo transform1(eltype1, ElVertices1); 
    IntegrationRule & ir_facet_vol1 = transform1(LocalFacetNr1, ir_facet, lh);
    BaseMappedIntegrationRule & mir1 = trafo1(ir_facet_vol1, lh);
    
    Facet2ElementTrafo transform2(eltype2, ElVertices2); 
    IntegrationRule & ir_facet_vol2 = transform2(LocalFacetNr2, ir_facet, lh);
    BaseMappedIntegrationRule & mir2 = trafo2(ir_facet_vol2, lh);

    mir1.SetOtherMIR (&mir2);
    mir2.SetOtherMIR (&mir1);

    // ts1.Stop();
    // ts2.Start();

    
    // evaluate proxy-values
    ProxyUserData ud(trial_proxies.Size(), lh);
    const_cast<ElementTransformation&>(trafo1).userdata = &ud;
    ud.fel = &fel1;   // necessary to check remember-map
    // ud.elx = &elx;
    // ud.lh = &lh;
    for (ProxyFunction * proxy : trial_proxies)
      ud.AssignMemory (proxy, ir_facet.Size(), proxy->Dimension(), lh);

    for (ProxyFunction * proxy : trial_proxies)
      {
	IntRange trial_range  = proxy->IsOther() ? IntRange(proxy->Evaluator()->BlockDim()*fel1.GetNDof(), elx.Size()) : IntRange(0, proxy->Evaluator()->BlockDim()*fel1.GetNDof());
	if (proxy->IsOther()) 
          proxy->Evaluator()->Apply(fel2, mir2, elx.Range(trial_range), ud.GetMemory(proxy), lh);
        else
          proxy->Evaluator()->Apply(fel1, mir1, elx.Range(trial_range), ud.GetMemory(proxy), lh);
      }

    // ts2.Stop();
    // RegionTimer reg(t);
    // t.Start();


    /*
        FlatVector<> measure(mir1.Size(), lh);
        switch (trafo1.SpaceDim())
          {
	  case 1:
            {
              Vec<1> normal_ref = ElementTopology::GetNormals<1>(eltype1)[LocalFacetNr1];
              for (int i = 0; i < mir1.Size(); i++)
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<1,1>&> (mir1[i]);
                  Mat<1> inv_jac = mip.GetJacobianInverse();
                  double det = mip.GetMeasure();
                  Vec<1> normal = det * Trans (inv_jac) * normal_ref;       
                  double len = L2Norm (normal);    // that's the surface measure 
                  normal /= len;                   // normal vector on physical element
                  const_cast<MappedIntegrationPoint<1,1>&> (mip).SetNV(normal);
                  measure(i) = len;
                }
	      break;
            }
          case 2:
            {
              Vec<2> normal_ref = ElementTopology::GetNormals<2>(eltype1)[LocalFacetNr1];
              for (int i = 0; i < mir1.Size(); i++)
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<2,2>&> (mir1[i]);
                  Mat<2> inv_jac = mip.GetJacobianInverse();
                  double det = mip.GetMeasure();
                  Vec<2> normal = det * Trans (inv_jac) * normal_ref;       
                  double len = L2Norm (normal);    // that's the surface measure 
                  normal /= len;                   // normal vector on physical element
                  const_cast<MappedIntegrationPoint<2,2>&> (mip).SetNV(normal);
                  measure(i) = len;
                }
              break;
            }
          default:
            cout << "Symbolic DG in " << trafo1.SpaceDim() << " not available" << endl;
          }
    */

    mir1.ComputeNormalsAndMeasure (eltype1, LocalFacetNr1);
    mir2.ComputeNormalsAndMeasure (eltype2, LocalFacetNr2);

    
    FlatMatrix<> val(ir_facet.Size(), 1,lh);

    for (auto proxy : test_proxies)
      {
        HeapReset hr(lh);
        // t1.Start();
        FlatMatrix<> proxyvalues(ir_facet.Size(), proxy->Dimension(), lh);
        
        // t1.Stop();
        // t2.Start();
        
        for (int k = 0; k < proxy->Dimension(); k++)
          {
            ud.testfunction = proxy;
            ud.test_comp = k;
            cf -> Evaluate (mir1, val);
            proxyvalues.Col(k) = val.Col(0);
          }

        // t2.Stop();
        // t3.Start();

        for (int i = 0; i < mir1.Size(); i++)
          // proxyvalues.Row(i) *= measure(i) * ir_facet[i].Weight();
          proxyvalues.Row(i) *= mir1[i].GetMeasure() * ir_facet[i].Weight();
        
        ely1 = 0.0;
	IntRange test_range  = proxy->IsOther() ? IntRange(proxy->Evaluator()->BlockDim()*fel1.GetNDof(), elx.Size()) : IntRange(0, proxy->Evaluator()->BlockDim()*fel1.GetNDof());
	if (proxy->IsOther())
	  proxy->Evaluator()->ApplyTrans(fel2, mir2, proxyvalues, ely1.Range(test_range), lh);
        else
          proxy->Evaluator()->ApplyTrans(fel1, mir1, proxyvalues, ely1.Range(test_range), lh);
        
        ely += ely1;
        // t3.Stop();
      }
    // t.Stop();
  }


  void SymbolicFacetBilinearFormIntegrator :: 
  CalcTraceValues (const FiniteElement & volumefel, int LocalFacetNr,
		   const ElementTransformation & eltrans, FlatArray<int> & ElVertices,
		   FlatVector<double> & trace, FlatVector<double> elx, LocalHeap & lh) const
  {

    if (simd_evaluate)
      {
        try
          {
	    int maxorder = volumefel.Order();
            
            auto eltype = eltrans.GetElementType();
            auto etfacet = ElementTopology::GetFacetType (eltype, LocalFacetNr);
            
            Facet2ElementTrafo transform(eltype, ElVertices); 
            
            const SIMD_IntegrationRule & simd_ir_facet = GetSIMDIntegrationRule(etfacet, 2*maxorder);
	    
            auto & simd_ir_facet_vol = transform(LocalFacetNr, simd_ir_facet, lh);
            auto & simd_mir = eltrans(simd_ir_facet_vol, lh);
            simd_mir.ComputeNormalsAndMeasure(eltype, LocalFacetNr);

            ProxyUserData ud(trial_proxies.Size(), lh);
            const_cast<ElementTransformation&>(eltrans).userdata = &ud;
            ud.fel = &volumefel;   // necessary to check remember-map
            // ud.lh = &lh;
            for (ProxyFunction * proxy : trial_proxies)
	      if(proxy->IsOther())
		{
		  ud.AssignMemory (proxy, simd_ir_facet.GetNIP(), proxy->Dimension(), lh);
		  IntRange trial_range  = IntRange(0, volumefel.GetNDof());
		  trial_range = proxy->Evaluator()->BlockDim() * trial_range;
		  proxy->Evaluator()->Apply(volumefel, simd_mir, elx.Range(trial_range), ud.GetAMemory(proxy)); // , lh);
		}
	    
	    size_t st = 0;
	    for (ProxyFunction * proxy : trial_proxies)
              if(proxy->IsOther())
		st += ud.GetAMemory(proxy).AsVector().Size()*SIMD<double>::Size();
		
	    
	    trace.AssignMemory(st, lh);
	    st = 0;
	    
	    for (ProxyFunction * proxy : trial_proxies)
              if(proxy->IsOther())
		for(auto k:Range(ud.GetAMemory(proxy).AsVector().Size()))
		  for(auto j:Range(SIMD<double>::Size()))
		    trace [st++] = ud.GetAMemory(proxy)(k)[j];

	    return;
	  }
        catch (ExceptionNOSIMD e)
          {
            cout << "caught in SymbolicFacetInegtrator::CalcTraceValues: " << endl
                 << e.What() << endl;
            simd_evaluate = false;
	    CalcTraceValues (volumefel, LocalFacetNr, eltrans, ElVertices, trace, elx, lh);
          }
	return;
      }  

    int maxorder = volumefel.Order();

    auto eltype = eltrans.GetElementType();
    auto etfacet = ElementTopology::GetFacetType (eltype, LocalFacetNr);

    const IntegrationRule& ir_facet = GetIntegrationRule(etfacet, 2*maxorder);
    
    Facet2ElementTrafo transform(eltype, ElVertices); 
    IntegrationRule & ir_facet_vol = transform(LocalFacetNr, ir_facet, lh);
    BaseMappedIntegrationRule & mir = eltrans(ir_facet_vol, lh);
    mir.ComputeNormalsAndMeasure(eltype, LocalFacetNr);
    
    ProxyUserData ud(trial_proxies.Size(), lh);
    const_cast<ElementTransformation&>(eltrans).userdata = &ud;
    ud.fel = &volumefel;   // necessary to check remember-map
    // ud.lh = &lh;

    for (ProxyFunction * proxy : trial_proxies)
      if(proxy->IsOther())
      {
	ud.AssignMemory (proxy, ir_facet.Size(), proxy->Dimension(), lh);
	IntRange trial_range  = IntRange(0, proxy->Evaluator()->BlockDim()*volumefel.GetNDof());
	proxy->Evaluator()->Apply(volumefel, mir, elx.Range(trial_range), ud.GetMemory(proxy), lh);
      }
    
    size_t st = 0;
    for (ProxyFunction * proxy : trial_proxies)
      if(proxy->IsOther())
	{
	  st += ud.GetMemory(proxy).AsVector().Size();
	}

    trace.AssignMemory(st, lh);
    st = 0;
    
    for (ProxyFunction * proxy : trial_proxies)
      if(proxy->IsOther())
	for(auto k:Range(ud.GetMemory(proxy).AsVector().Size()))
	  trace[st++] = ud.GetMemory(proxy).AsVector()[k];
    
  }//end CalcTraceValues (double)

  void SymbolicFacetBilinearFormIntegrator ::
  ApplyFromTraceValues (const FiniteElement & volumefel, int LocalFacetNr,
			const ElementTransformation & eltrans, FlatArray<int> & ElVertices,
			FlatVector<double> trace,
			FlatVector<double> elx, FlatVector<double> ely, 
			LocalHeap & lh) const
  {
    if (simd_evaluate)
      {
        try
          {
            ely = 0.0;

	    int maxorder = volumefel.Order();
	    auto eltype = eltrans.GetElementType();
            auto etfacet = ElementTopology::GetFacetType (eltype, LocalFacetNr);

            Facet2ElementTrafo transform(eltype, ElVertices); 
            const SIMD_IntegrationRule & simd_ir_facet = GetSIMDIntegrationRule(etfacet, 2*maxorder);

            auto & simd_ir_facet_vol = transform(LocalFacetNr, simd_ir_facet, lh);
            auto & simd_mir = eltrans(simd_ir_facet_vol, lh);
            simd_mir.ComputeNormalsAndMeasure(eltype, LocalFacetNr);

            ProxyUserData ud(trial_proxies.Size(), lh);
            const_cast<ElementTransformation&>(eltrans).userdata = &ud;
            ud.fel = &volumefel;   // necessary to check remember-map
            // ud.elx = &elx;
            // ud.lh = &lh;
	    
	    size_t ctrace = 0;	    
	    for (ProxyFunction * proxy : trial_proxies)
	      {
		ud.AssignMemory (proxy, simd_ir_facet.GetNIP(), proxy->Dimension(), lh);
		
		if(proxy->IsOther())
		  {
		    auto mem = ud.GetAMemory(proxy);
 		    for(auto k:Range(mem.AsVector().Size()))
		      {
			mem(k) = SIMD<double>(&trace[ctrace]);
			ctrace+=SIMD<double>::Size();
		      }
		  }
		else
		  {
		    IntRange trial_range  = IntRange(0, volumefel.GetNDof());
		    trial_range = proxy->Evaluator()->BlockDim() * trial_range;
		    proxy->Evaluator()->Apply(volumefel, simd_mir, elx.Range(trial_range), ud.GetAMemory(proxy));
		  }
		
	      }

	    for (auto proxy : test_proxies)
              if(!proxy->IsOther())
		{
		  HeapReset hr(lh);
		  FlatMatrix<SIMD<double>> simd_proxyvalues(proxy->Dimension(), simd_ir_facet.Size(), lh); 
                
		  for (int k = 0; k < proxy->Dimension(); k++)
		    {
		      ud.testfunction = proxy;
		      ud.test_comp = k;
		      cf -> Evaluate (simd_mir, simd_proxyvalues.Rows(k,k+1));
		    }
                
		  for (int i = 0; i < simd_proxyvalues.Height(); i++)
		    {
		      auto row = simd_proxyvalues.Row(i);
		      for (int j = 0; j < row.Size(); j++)
			row(j) *= simd_mir[j].GetMeasure() * simd_ir_facet[j].Weight();
		    }
		  IntRange test_range  = IntRange(0, volumefel.GetNDof());
		  int blockdim = proxy->Evaluator()->BlockDim();
		  test_range = blockdim * test_range;
                
		  proxy->Evaluator()->AddTrans(volumefel, simd_mir, simd_proxyvalues, ely.Range(test_range));
		}
	  }
        catch (ExceptionNOSIMD e)
          {
            cout << "caught in SymbolicFacetInegtrator::CalcTraceValues: " << endl
                 << e.What() << endl;
            simd_evaluate = false;
	    ApplyFromTraceValues(volumefel, LocalFacetNr, eltrans, ElVertices,
				 trace, elx, ely, lh);
          }
	return;
      }

    ely = 0.0;
    FlatVector<> ely1(ely.Size(), lh);

    int maxorder = volumefel.Order();
    auto eltype = eltrans.GetElementType();
    auto etfacet = ElementTopology::GetFacetType (eltype, LocalFacetNr);
    const IntegrationRule & ir_facet = GetIntegrationRule(etfacet, 2*maxorder);

    Facet2ElementTrafo transform(eltype, ElVertices); 
    IntegrationRule & ir_facet_vol = transform(LocalFacetNr, ir_facet, lh);
    BaseMappedIntegrationRule & mir = eltrans(ir_facet_vol, lh);
    mir.ComputeNormalsAndMeasure(eltype, LocalFacetNr);
        
    ProxyUserData ud(trial_proxies.Size(), lh);
    const_cast<ElementTransformation&>(eltrans).userdata = &ud;
    ud.fel = &volumefel;   // necessary to check remember-map
    // ud.lh = &lh;

    size_t ctrace = 0;
    //copy traces to user memory
    for (ProxyFunction * proxy : trial_proxies)
      {
	ud.AssignMemory (proxy, ir_facet.Size(), proxy->Dimension(), lh);
	if(proxy->IsOther())
	  {
	    auto mem = ud.GetMemory(proxy).AsVector();
	    for(auto k:Range(mem.Size()))
	      mem[k] = trace[ctrace++];
	  }
	else
	  {
	    IntRange trial_range  = IntRange(0, proxy->Evaluator()->BlockDim()*volumefel.GetNDof());
	    proxy->Evaluator()->Apply(volumefel, mir, elx.Range(trial_range), ud.GetMemory(proxy), lh);
	  }
      }

    FlatMatrix<> val(ir_facet.Size(), 1,lh);
    for (auto proxy : test_proxies)
      if(!proxy->IsOther())
	{
	  HeapReset hr(lh);

	  FlatMatrix<> proxyvalues(ir_facet.Size(), proxy->Dimension(), lh);

	  for (int k = 0; k < proxy->Dimension(); k++)
	    {
	      ud.testfunction = proxy;
	      ud.test_comp = k;
	      cf -> Evaluate (mir, val);
	      proxyvalues.Col(k) = val.Col(0);
	    }


	  for (int i = 0; i < mir.Size(); i++)
	    proxyvalues.Row(i) *= mir[i].GetMeasure() * ir_facet[i].Weight();

	  ely1 = 0.0;
	  IntRange test_range  = IntRange(0, proxy->Evaluator()->BlockDim()*volumefel.GetNDof());
	  
	  proxy->Evaluator()->ApplyTrans(volumefel, mir, proxyvalues, ely1.Range(test_range), lh);

	  ely += ely1;
	}
  }//end ApplyFromTraceValues

  void SymbolicFacetBilinearFormIntegrator ::
  ApplyFacetMatrix (const FiniteElement & fel1, int LocalFacetNr,
                    const ElementTransformation & trafo1, FlatArray<int> & ElVertices,
                    const ElementTransformation & strafo, FlatArray<int> & SElVertices,
                    FlatVector<double> elx, FlatVector<double> ely,
                    LocalHeap & lh) const
  {
    // const TPHighOrderFE * tpfel = dynamic_cast<const TPHighOrderFE *>(&fel1);
    // if(tpfel)
    // {
      // int facet_x_y = 0;
      // if(LocalFacetNr>=10)
      // {
        // facet_x_y = 1;
        // LocalFacetNr-=10;
      // }
      // ApplyFacetMatrixTP(fel1,LocalFacetNr,trafo1,ElVertices,strafo,elx,ely,facet_x_y,lh);
      // return;
    // }    
    if (simd_evaluate)
      {
        try
          {
            static Timer t("SymbolicFacetBFI::ApplyFacetMatrix - boundary", 2);
            
            HeapReset hr(lh);
            
            ely = 0;
            
            int maxorder = fel1.Order();
            
            auto eltype1 = trafo1.GetElementType();
            auto etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr);
            
            const SIMD_IntegrationRule & ir_facet = GetSIMDIntegrationRule(etfacet, 2*maxorder);
            Facet2ElementTrafo transform1(eltype1, ElVertices);
            Facet2SurfaceElementTrafo stransform(strafo.GetElementType(), SElVertices); 
            auto & ir_facet_vol1 = transform1(LocalFacetNr, ir_facet, lh);
            auto & ir_facet_surf = stransform(ir_facet, lh);
            auto & mir1 = trafo1(ir_facet_vol1, lh);
            auto & smir = strafo(ir_facet_surf, lh);

            mir1.SetOtherMIR(&smir);
            smir.SetOtherMIR(&mir1);

            mir1.ComputeNormalsAndMeasure(eltype1, LocalFacetNr);
            
            ProxyUserData ud(trial_proxies.Size(), lh);
            const_cast<ElementTransformation&>(trafo1).userdata = &ud;
            const_cast<ElementTransformation&>(strafo).userdata = &ud;
            ud.fel = &fel1;   // necessary to check remember-map
            // ud.elx = &elx;
            // ud.lh = &lh;
            
            
            for (ProxyFunction * proxy : trial_proxies)
              ud.AssignMemory (proxy, ir_facet.GetNIP(), proxy->Dimension(), lh);
            
            for (ProxyFunction * proxy : trial_proxies)
              if (! (proxy->IsOther() && proxy->BoundaryValues()) )
                proxy->Evaluator()->Apply(fel1, mir1, elx, ud.GetAMemory(proxy));
            
            for (ProxyFunction * proxy : trial_proxies)
              if (proxy->IsOther() && proxy->BoundaryValues())
                proxy->BoundaryValues()->Evaluate (smir, ud.GetAMemory(proxy));
            
            RegionTimer reg(t);
            
            
            for (auto proxy : test_proxies)
              {
                HeapReset hr(lh);
                FlatMatrix<SIMD<double>> proxyvalues(proxy->Dimension(), ir_facet.Size(), lh);
                
                for (int k = 0; k < proxy->Dimension(); k++)
                  {
                    ud.testfunction = proxy;
                    ud.test_comp = k;
                    cf -> Evaluate (mir1, proxyvalues.Rows(k,k+1));            
                  }
                
                for (int i = 0; i < proxyvalues.Height(); i++)
                  {
                    auto row = proxyvalues.Row(i);
                    for (int j = 0; j < row.Size(); j++)
                      row(j) *= mir1[j].GetMeasure() * ir_facet[j].Weight();
                  }
                
                if (proxy->IsOther() && proxy->BoundaryValues())
                  ; // nothing to do 
                else
                  proxy->Evaluator()->AddTrans(fel1, mir1, proxyvalues, ely);
              }
            
          }
        catch (ExceptionNOSIMD e)
          {
            cout << IM(4) << "caught in SymbolicFacetInegtrator::ApplyBnd: " << endl
                 << e.What() << endl;
            simd_evaluate = false;
            ApplyFacetMatrix (fel1, LocalFacetNr, trafo1, ElVertices,
                              strafo, SElVertices, 
                              elx, ely, lh);
          }
        return;
        
      }

    
    static Timer t("SymbolicFacetBFI::ApplyFacetMatrix - boundary", 2);
    
    HeapReset hr(lh);

    /*
    Matrix<> elmat(elx.Size());
    CalcFacetMatrix(fel1, LocalFacetNr, trafo1, ElVertices, strafo, elmat, lh);
    ely = elmat * elx;
    return;
    */

    ely = 0;
    
    FlatVector<> ely1(ely.Size(), lh);

    int maxorder = fel1.Order();

    auto eltype1 = trafo1.GetElementType();
    auto etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr);

    const IntegrationRule & ir_facet = GetIntegrationRule(etfacet, 2*maxorder);
    Facet2ElementTrafo transform1(eltype1, ElVertices);
    Facet2SurfaceElementTrafo stransform(strafo.GetElementType(), SElVertices); 
    
    IntegrationRule & ir_facet_vol1 = transform1(LocalFacetNr, ir_facet, lh);
    IntegrationRule & ir_facet_surf = stransform(ir_facet, lh);
    BaseMappedIntegrationRule & mir1 = trafo1(ir_facet_vol1, lh);
    auto & smir = strafo(ir_facet_surf, lh);

    mir1.SetOtherMIR (&smir);
    smir.SetOtherMIR (&mir1);
    
    // evaluate proxy-values
    ProxyUserData ud(trial_proxies.Size(), lh);
    const_cast<ElementTransformation&>(trafo1).userdata = &ud;
    const_cast<ElementTransformation&>(strafo).userdata = &ud;
    ud.fel = &fel1;   // necessary to check remember-map
    // ud.elx = &elx;
    // ud.lh = &lh;
    for (ProxyFunction * proxy : trial_proxies)
      ud.AssignMemory (proxy, ir_facet.Size(), proxy->Dimension(), lh);
    
    for (ProxyFunction * proxy : trial_proxies)
      if (! (proxy->IsOther() && proxy->BoundaryValues()))
        proxy->Evaluator()->Apply(fel1, mir1, elx, ud.GetMemory(proxy), lh);

    for (ProxyFunction * proxy : trial_proxies)    
      if (proxy->IsOther() && proxy->BoundaryValues())
        proxy->BoundaryValues()->Evaluate (smir, ud.GetMemory(proxy));

    RegionTimer reg(t);
    
    mir1.ComputeNormalsAndMeasure (eltype1, LocalFacetNr);
    
    FlatMatrix<> val(ir_facet.Size(), 1,lh);
    for (auto proxy : test_proxies)
      {
        HeapReset hr(lh);
        FlatMatrix<> proxyvalues(ir_facet.Size(), proxy->Dimension(), lh);
        /*
        FlatVector<> measure(mir1.Size(), lh);
        switch (trafo1.SpaceDim())
          {
          case 1:
            {
              Vec<1> normal_ref = ElementTopology::GetNormals<1>(eltype1)[LocalFacetNr];
              for (int i = 0; i < mir1.Size(); i++)
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<1,1>&> (mir1[i]);
                  Mat<1> inv_jac = mip.GetJacobianInverse();
                  double det = mip.GetMeasure();
                  Vec<1> normal = det * Trans (inv_jac) * normal_ref;       
                  double len = L2Norm (normal);    // that's the surface measure 
                  normal /= len;                   // normal vector on physical element
                  const_cast<MappedIntegrationPoint<1,1>&> (mip).SetNV(normal);
                  measure(i) = len;
                }
                break;
            }
          case 2:
            {
              Vec<2> normal_ref = ElementTopology::GetNormals<2>(eltype1)[LocalFacetNr];
              for (int i = 0; i < mir1.Size(); i++)
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<2,2>&> (mir1[i]);
                  Mat<2> inv_jac = mip.GetJacobianInverse();
                  double det = mip.GetMeasure();
                  Vec<2> normal = det * Trans (inv_jac) * normal_ref;       
                  double len = L2Norm (normal);    // that's the surface measure 
                  normal /= len;                   // normal vector on physical element
                  const_cast<MappedIntegrationPoint<2,2>&> (mip).SetNV(normal);
                  measure(i) = len;
                }
                break;
            }
          default:
            cout << "Symbolic DG in " << trafo1.SpaceDim() << " not available" << endl;
          }
        */
        
        for (int k = 0; k < proxy->Dimension(); k++)
          {
            ud.testfunction = proxy;
            ud.test_comp = k;
            cf -> Evaluate (mir1, val);
            proxyvalues.Col(k) = val.Col(0);
          }

        for (int i = 0; i < mir1.Size(); i++)
          // proxyvalues.Row(i) *= measure(i) * ir_facet[i].Weight();
          proxyvalues.Row(i) *= mir1[i].GetMeasure() * ir_facet[i].Weight();

        ely1 = 0.0;
        if (proxy->IsOther() && proxy->BoundaryValues())
          ;  // nothing to do 
        else
          proxy->Evaluator()->ApplyTrans(fel1, mir1, proxyvalues, ely1, lh);
        ely += ely1;
      }
  }
  
  
  SymbolicEnergy :: SymbolicEnergy (shared_ptr<CoefficientFunction> acf,
                                    VorB avb, bool aelement_boundary)
    : cf(acf), vb(avb), element_boundary(aelement_boundary)
  {
    simd_evaluate = true;
    // if (element_boundary) simd_evaluate = false;
    
    if (cf->Dimension() != 1)
      throw Exception ("SymblicEnergy needs scalar-valued CoefficientFunction");
    
    trial_cum.Append(0);
    cf->TraverseTree
      ( [&] (CoefficientFunction & nodecf)
        {
          auto proxy = dynamic_cast<ProxyFunction*> (&nodecf);
          if (proxy) 
            {
              if (!proxy->IsTestFunction())
                {                                         
                  if (!trial_proxies.Contains(proxy))
                    {
                      trial_proxies.Append (proxy);
                      trial_cum.Append(trial_cum.Last()+proxy->Dimension());
                    }             
                }
            }
        });

    nonzeros = Matrix<bool>(trial_cum.Last(), trial_cum.Last());
    nonzeros_proxies = Matrix<bool>(trial_proxies.Size(), trial_proxies.Size());
    nonzeros_proxies = false;
    
    ProxyUserData ud;
    DummyFE<ET_TRIG> dummyfe;
    ud.fel = &dummyfe;
    Vector<bool> nzvec(1), nzdvec(1), nzddvec(1);
    int k = 0;
    for (int k1 : trial_proxies.Range())
      for (int k2 : Range(0,trial_proxies[k1]->Dimension()))
        {
          int l = 0;
          for (int l1 : trial_proxies.Range())
            for (int l2 : Range(0,trial_proxies[l1]->Dimension()))
              {
                ud.trialfunction = trial_proxies[l1];
                ud.trial_comp = l2;
                ud.testfunction = trial_proxies[k1];
                ud.test_comp = k2;
                cf -> NonZeroPattern (ud, nzvec, nzdvec, nzddvec);
                // nzddvec(0) = true;
                nonzeros(k,l) = nzddvec(0);
                if (nzddvec(0))
                  nonzeros_proxies(k1,l1) = true;
                l++;
              }
          k++;
        }
    int cnt = 0;
    for (auto i : Range(nonzeros.Height()))
      for (auto j : Range(nonzeros.Width()))
        if (nonzeros(i,j)) cnt++;
    
    cout << IM(3) << "nonzero: " << cnt << "/" << sqr(nonzeros.Height()) << endl;
    cout << IM(3) << "nonzero-proxies: " << endl << nonzeros_proxies << endl;
  }


  const IntegrationRule& SymbolicEnergy ::
  GetIntegrationRule (const FiniteElement & fel, LocalHeap & /* lh */) const
  {
    if (userdefined_intrules[fel.ElementType()]) return *userdefined_intrules[fel.ElementType()];

    int trial_difforder = 99; 
    for (auto proxy : trial_proxies)
      trial_difforder = min2(trial_difforder, proxy->Evaluator()->DiffOrder());
    if (trial_proxies.Size() == 0) trial_difforder = 0;
    
    int intorder = 2*fel.Order()+bonus_intorder;
    auto et = fel.ElementType();
    if (et == ET_TRIG || et == ET_TET)
      intorder -= 2*trial_difforder;
    return SelectIntegrationRule (et, intorder);
  }

  const SIMD_IntegrationRule& SymbolicEnergy ::
  Get_SIMD_IntegrationRule (const FiniteElement & fel, LocalHeap & lh) const
  {
    if (userdefined_simd_intrules[fel.ElementType()] ) return *userdefined_simd_intrules[fel.ElementType()];

    // bool is_mixed = typeid(fel) == typeid(const MixedFiniteElement&);
    // const MixedFiniteElement * mixedfe = static_cast<const MixedFiniteElement*> (&fel);    
    // const FiniteElement & fel_trial = is_mixed ? mixedfe->FETrial() : fel;
    // const FiniteElement & fel_test = is_mixed ? mixedfe->FETest() : fel;

    int trial_difforder = 99; 
    for (auto proxy : trial_proxies)
      trial_difforder = min2(trial_difforder, proxy->Evaluator()->DiffOrder());
    if (trial_proxies.Size() == 0) trial_difforder = 0;
    
    int intorder = 2*fel.Order()+bonus_intorder;
    auto et = fel.ElementType();
    if (et == ET_TRIG || et == ET_TET)
      intorder -= 2*trial_difforder;
    return SIMD_SelectIntegrationRule (et, intorder);
  }

  

  void 
  SymbolicEnergy :: CalcLinearizedElementMatrix (const FiniteElement & fel,
                                                 const ElementTransformation & trafo, 
                                                 FlatVector<double> elveclin,
                                                 FlatMatrix<double> elmat,
                                                 LocalHeap & lh) const
  {
    size_t tid = TaskManager::GetThreadId();
    ThreadRegionTimer reg(const_cast<Timer&> (timer), tid);

    if (simd_evaluate)
      {
        try
          {
            if (!element_boundary)
              {
                // const SIMD_IntegrationRule& ir = GetSIMDIntegrationRule(trafo.GetElementType(), 2*fel.Order());
                const SIMD_IntegrationRule& ir = Get_SIMD_IntegrationRule(fel, lh);
                auto & mir = trafo(ir, lh);
                
                ProxyUserData ud(trial_proxies.Size(), lh);
                const_cast<ElementTransformation&>(trafo).userdata = &ud;
                ud.fel = &fel;
                for (ProxyFunction * proxy : trial_proxies)
                  {
                    ud.AssignMemory (proxy, ir.GetNIP(), proxy->Dimension(), lh);
                    proxy->Evaluator()->Apply(fel, mir, elveclin, ud.GetAMemory(proxy));
                  }
                
                elmat = 0;
                
                AddLinearizedElementMatrix (fel, ud, mir, elveclin, elmat, lh);
              }
            else
              {
                elmat = 0;
                auto eltype = trafo.GetElementType();
                int nfacet = ElementTopology::GetNFacets(eltype);
        
                Facet2ElementTrafo transform(eltype); 
                
                for (int k = 0; k < nfacet; k++)
                  {
                    HeapReset hr(lh);
                    ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
                    
                    auto & ir_facet = GetSIMDIntegrationRule(etfacet, 2*fel.Order());
                    auto & ir_facet_vol = transform(k, ir_facet, lh);
                    auto & mir = trafo(ir_facet_vol, lh);
                    mir.ComputeNormalsAndMeasure (eltype, k);
            
                    ProxyUserData ud(trial_proxies.Size(), lh);    
                    const_cast<ElementTransformation&>(trafo).userdata = &ud;
                    ud.fel = &fel;
                    for (ProxyFunction * proxy : trial_proxies)
                      {
                        ud.AssignMemory (proxy, ir_facet.GetNIP(), proxy->Dimension(), lh);
                        proxy->Evaluator()->Apply(fel, mir, elveclin, ud.GetAMemory(proxy));
                      }
                    
                    AddLinearizedElementMatrix (fel, ud, mir, elveclin, elmat, lh);            
                  }
              }
          }

        catch (ExceptionNOSIMD e)
          {
            cout << IM(4) << e.What() << endl
                 << "switching back to standard evaluation (in SymbolicEnergy::CalcLinearized)" << endl;
            simd_evaluate = false;
            CalcLinearizedElementMatrix (fel, trafo, elveclin, elmat, lh);
          }
        return;
      }



    if (!element_boundary)
      {
        // const IntegrationRule& ir = GetIntegrationRule(trafo.GetElementType(), 2*fel.Order());
        const IntegrationRule& ir = GetIntegrationRule(fel, lh);
        BaseMappedIntegrationRule & mir = trafo(ir, lh);
        
        ProxyUserData ud(trial_proxies.Size(), lh);
        const_cast<ElementTransformation&>(trafo).userdata = &ud;
        ud.fel = &fel;
        // ud.elx = &elveclin;
        // ud.lh = &lh;
        for (ProxyFunction * proxy : trial_proxies)
          {
            ud.AssignMemory (proxy, ir.Size(), proxy->Dimension(), lh);
            proxy->Evaluator()->Apply(fel, mir, elveclin, ud.GetMemory(proxy), lh);        
          }
        
        elmat = 0;
        
        AddLinearizedElementMatrix (fel, ud, mir, elveclin, elmat, lh);
      }
    else // element_boundary
      {
        elmat = 0;
        auto eltype = trafo.GetElementType();
        int nfacet = ElementTopology::GetNFacets(eltype);
        
        Facet2ElementTrafo transform(eltype); 
        
        for (int k = 0; k < nfacet; k++)
          {
            HeapReset hr(lh);
            ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
            
            const IntegrationRule & ir_facet = GetIntegrationRule(etfacet, 2*fel.Order());
            IntegrationRule & ir_facet_vol = transform(k, ir_facet, lh);
            BaseMappedIntegrationRule & mir = trafo(ir_facet_vol, lh);
            mir.ComputeNormalsAndMeasure (eltype, k);
            
            
            ProxyUserData ud(trial_proxies.Size(), lh);    
            const_cast<ElementTransformation&>(trafo).userdata = &ud;
            ud.fel = &fel;
            // ud.elx = &elveclin;
            // ud.lh = &lh;
            
            for (ProxyFunction * proxy : trial_proxies)
              {
                ud.AssignMemory (proxy, ir_facet.GetNIP(), proxy->Dimension(), lh);
                proxy->Evaluator()->Apply(fel, mir, elveclin, ud.GetMemory(proxy), lh);
              }

            AddLinearizedElementMatrix (fel, ud, mir, elveclin, elmat, lh);            
          }
      }
  }

  void SymbolicEnergy ::
  AddLinearizedElementMatrix (const FiniteElement & fel,
                              ProxyUserData & ud,
                              const BaseMappedIntegrationRule & mir, 
                              FlatVector<double> elveclin,
                              FlatMatrix<double> elmat,
                              LocalHeap & lh) const
  {
    static Timer t("SymbolicEnergy::AddLinearizedElementMatrix", 2);
    static Timer tdmat("SymbolicEnergy::CalcDMat", 2);
    // Timer & tdmat = const_cast<Timer&> (timer);
    static Timer tbmat("SymbolicEnergy::CalcBMat", 2);
    static Timer tmult("SymbolicEnergy::mult", 2);
    size_t tid = TaskManager::GetThreadId();
    ThreadRegionTimer reg(t, tid);
    
    HeapReset hr(lh);
    
    FlatMatrix<> val(mir.Size(), 1,lh), deriv(mir.Size(), 1,lh), dderiv(mir.Size(), 1,lh);
    FlatArray<FlatMatrix<>> diags(trial_proxies.Size(), lh);
    for (int k1 : Range(trial_proxies))
      {
        auto proxy = trial_proxies[k1];
        diags[k1].AssignMemory(mir.Size(), proxy->Dimension(), lh);
        for (int k = 0; k < proxy->Dimension(); k++)
          {
            ud.trialfunction = proxy;
            ud.trial_comp = k;
            ud.testfunction = proxy;
            ud.test_comp = k;
            cf -> EvaluateDDeriv (mir, val, deriv, dderiv);
            diags[k1].Col(k) = dderiv.Col(0);
          }
      }

    for (int k1 : Range(trial_proxies))
      for (int l1 : Range(trial_proxies))
        {
          HeapReset hr(lh);
          if (!nonzeros_proxies(k1,l1)) continue;
          
          auto proxy1 = trial_proxies[k1];
          auto proxy2 = trial_proxies[l1];

          FlatTensor<3> proxyvalues(lh, mir.Size(), proxy2->Dimension(), proxy1->Dimension());
          for (int k = 0; k < proxy1->Dimension(); k++)
            for (int l = 0; l < proxy2->Dimension(); l++)
              {
                ud.trialfunction = proxy1;
                ud.trial_comp = k;
                ud.testfunction = proxy2;
                ud.test_comp = l;

                {
                  ThreadRegionTimer reg(tdmat, tid);
                  NgProfiler::AddThreadFlops (tdmat, tid, 1);
                  if (nonzeros(trial_cum[k1]+k, trial_cum[l1]+l))
                    cf -> EvaluateDDeriv (mir, val, deriv, dderiv);
                  else
                    dderiv = 0.0;
                }
                proxyvalues(STAR,l,k) = dderiv.Col(0);

                /*
                cout << L2Norm(dderiv.Col(0)) << " ";
                Vector<bool> nzvec(1), nzdvec(1), nzddvec(1);
                cf -> NonZeroPattern (ud, nzvec, nzdvec, nzddvec);
                cout << nzddvec(0) << ";";
                if (nzddvec(0) == false && L2Norm(dderiv.Col(0)) != 0)
                  {
                    cout << "wrong" << endl;
                    cout << "type1 = " << proxy1->Evaluator()->Name() << endl;
                    cout << "type2 = " << proxy2->Evaluator()->Name() << endl;
                    cout << "cf = " << *cf << endl;

                    cf->TraverseTree
                      ( [&](CoefficientFunction & nodecf)
                        {
                          int dim = nodecf.Dimension();
                          Vector<bool> nzvec(dim), nzdvec(dim), nzddvec(dim);
                          nodecf.NonZeroPattern (ud, nzvec, nzdvec, nzddvec);
                          cout << nodecf.GetDescription() << "nzvec = " << nzvec << ", nzdvec = " << nzdvec << ", nzddvec = " << nzddvec << endl;
                        } );
                    
                    exit(1);
                  }
                */
                if (proxy1 != proxy2 || k != l)  // computed mixed second derivatives
                  {
                    proxyvalues(STAR,l,k) -= diags[k1].Col(k);
                    proxyvalues(STAR,l,k) -= diags[l1].Col(l);
                    proxyvalues(STAR,l,k) *= 0.5;
                  }
              }

          for (int i = 0; i < mir.Size(); i++)
            proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();

          FlatMatrix<double,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
          FlatMatrix<double,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);

          IntRange r1 = proxy1->Evaluator()->UsedDofs(fel);
          IntRange r2 = proxy2->Evaluator()->UsedDofs(fel);
          SliceMatrix<> part_elmat = elmat.Rows(r2).Cols(r1);

          constexpr size_t BS = 16;
          size_t i = 0;
          for ( ; i < mir.Size(); i+=BS)
            {
              HeapReset hr(lh);
              int bs = min2(size_t(BS), mir.IR().GetNIP()-i);
              FlatMatrix<double,ColMajor> bdbmat1(bs*proxy2->Dimension(), elmat.Width(), lh);
              FlatMatrix<double,ColMajor> bbmat2(bs*proxy2->Dimension(), elmat.Height(), lh);

              for (size_t j = 0; j < bs; j++)
                {
                  size_t ii = i+j;
                  IntRange r3 = proxy2->Dimension() * IntRange(j,j+1);

                  {
                    ThreadRegionTimer reg(tbmat, tid);                  
                    proxy1->Evaluator()->CalcMatrix(fel, mir[ii], bmat1, lh);
                    proxy2->Evaluator()->CalcMatrix(fel, mir[ii], bmat2, lh);
                  }
                  bdbmat1.Rows(r3).Cols(r1) = proxyvalues(ii,STAR,STAR) * bmat1.Cols(r1);
                  bbmat2.Rows(r3).Cols(r2) = bmat2.Cols(r2);
                }
              
              // elmat += Trans (bbmat2) * bdbmat1 | Lapack;
              // AddABt (Trans(bbmat2), Trans(bdbmat1), elmat);
              ThreadRegionTimer reg(tmult, tid);                                
              AddABt (Trans(bbmat2).Rows(r2), Trans(bdbmat1).Rows(r1), part_elmat);
            }
        }
  }
  
  void SymbolicEnergy :: AddLinearizedElementMatrix (const FiniteElement & fel,
                                                     ProxyUserData & ud, 
                                                     const SIMD_BaseMappedIntegrationRule & mir, 
                                                     FlatVector<double> elveclin,
                                                     FlatMatrix<double> elmat,
                                                     LocalHeap & lh) const
  {
        size_t tid = TaskManager::GetThreadId();        
        static Timer tdmat("SymbolicEnergy::CalcDMat - simd", 2);
        static Timer tdmat2("SymbolicEnergy::CalcDMat2 - simd", 2);
        static Timer tbmat("SymbolicEnergy::CalcBMat - simd", 2);
        static Timer tmult("SymbolicEnergy::mult - simd", 2);
        

    
            FlatMatrix<AutoDiffDiff<1,SIMD<double>>> ddval(1, mir.Size(), lh);
            FlatArray<FlatMatrix<SIMD<double>>> diags(trial_proxies.Size(), lh);
            for (int k1 : Range(trial_proxies))
              {
                auto proxy = trial_proxies[k1];
                new(&diags[k1]) FlatMatrix<SIMD<double>>(proxy->Dimension(), mir.Size(), lh);
                if (nonzeros_proxies(k1,k1))
                  for (int k = 0; k < proxy->Dimension(); k++)
                    {
                      ud.trialfunction = proxy;
                      ud.trial_comp = k;
                      ud.testfunction = proxy;
                      ud.test_comp = k;
                      // cf -> EvaluateDDeriv (mir, AFlatMatrix<>(val), AFlatMatrix<>(deriv), AFlatMatrix<> (diags[k1].Rows(k,k+1)));
                      auto diagrow = diags[k1].Row(k);
                      cf -> Evaluate (mir, ddval);
                      for (auto i : Range(diagrow))
                        diagrow(i) = ddval(i).DDValue(0);
                    }
                else
                  diags[k1] = 0.0;
              }

            for (int k1 : Range(trial_proxies))
              for (int l1 : Range(trial_proxies))
                {
                  HeapReset hr(lh);
                  if (!nonzeros_proxies(k1,l1)) continue;
                  if (k1 < l1) continue;
                  auto proxy1 = trial_proxies[k1];
                  auto proxy2 = trial_proxies[l1];

                  size_t dim_proxy1 = proxy1->Dimension();
                  size_t dim_proxy2 = proxy2->Dimension();

                  FlatMatrix<SIMD<double>> proxyvalues2(dim_proxy1*dim_proxy2, mir.Size(), lh);

                  {
                  ThreadRegionTimer reg(tdmat, tid);
                  for (int k = 0; k < dim_proxy1; k++)
                    for (int l = 0; l < dim_proxy2; l++)
                      {
                        ud.trialfunction = proxy1;
                        ud.trial_comp = k;
                        ud.testfunction = proxy2;
                        ud.test_comp = l;
                        auto proxyrow = proxyvalues2.Row(k*dim_proxy2+l);

                        if (nonzeros(trial_cum[k1]+k, trial_cum[l1]+l))
                          {
                            // cf -> EvaluateDDeriv (mir, AFlatMatrix<>(val), AFlatMatrix<>(deriv), AFlatMatrix<> (dderiv));
                            // proxyrow = dderiv.Row(0);

                            if (k1 != l1 || k <= l)
                              {
                                cf -> Evaluate (mir, ddval);
                                for (auto i : Range(proxyrow))
                                  proxyrow(i) = ddval(i).DDValue(0);
                              }
                            if (k1 == l1 && k < l) // symmetric
                              proxyvalues2.Row(l*dim_proxy2+k) = proxyrow;
                          }
                        else
                          {
                            // dderiv = 0.0;
                            proxyrow = 0.0;
                          }
                        if (proxy1 != proxy2 || k != l)  // computed mixed second derivatives
                          {
                            proxyrow -= diags[k1].Row(k);
                            proxyrow -= diags[l1].Row(l);
                            proxyrow *= 0.5;
                          }
                      }

                  for (size_t i = 0; i < mir.Size(); i++)
                    proxyvalues2.Col(i) *= mir[i].GetWeight();
                  }
                  

                  IntRange r1 = proxy1->Evaluator()->UsedDofs(fel);
                  IntRange r2 = proxy2->Evaluator()->UsedDofs(fel);
                  SliceMatrix<> part_elmat = elmat.Rows(r2).Cols(r1);

                  FlatMatrix<SIMD<double>> bmat1(elmat.Width()*dim_proxy1, mir.Size(), lh);
                  FlatMatrix<SIMD<double>> dbmat1(elmat.Width()*dim_proxy2, mir.Size(), lh);
                  FlatMatrix<SIMD<double>> bmat2(elmat.Height()*dim_proxy2, mir.Size(), lh);

                  FlatMatrix<SIMD<double>> hdbmat1(elmat.Width(), dim_proxy2*mir.Size(), &dbmat1(0,0));
                  FlatMatrix<SIMD<double>> hbmat2(elmat.Height(), dim_proxy2*mir.Size(), &bmat2(0,0));

                  {
                    ThreadRegionTimer reg(tbmat, tid);
                    proxy1->Evaluator()->CalcMatrix(fel, mir, bmat1);
                    proxy2->Evaluator()->CalcMatrix(fel, mir, bmat2);
                  }

                  hdbmat1.Rows(r1) = 0.0; 
                  for (auto i : r1)
                    for (size_t j = 0; j < dim_proxy2; j++)
                      for (size_t k = 0; k < dim_proxy1; k++)
                        {
                          auto res = dbmat1.Row(i*dim_proxy2+j);
                          auto a = bmat1.Row(i*dim_proxy1+k);
                          auto b = proxyvalues2.Row(k*dim_proxy2+j);
                          res += pw_mult(a,b);
                        }

                  {
                  ThreadRegionTimer reg(tmult, tid);                  
                  AddABt (hbmat2.Rows(r2), hdbmat1.Rows(r1), part_elmat);
                  if (k1 > l1)
                    elmat.Rows(r1).Cols(r2) = Trans(part_elmat);
                  }
                }
  }

  
  double SymbolicEnergy :: Energy (const FiniteElement & fel, 
                                   const ElementTransformation & trafo, 
                                   FlatVector<double> elx, 
                                   LocalHeap & lh) const
  {
    if (simd_evaluate && !element_boundary)
      {
        try
          {
            // auto & ir = GetSIMDIntegrationRule(trafo.GetElementType(), 2*fel.Order());
            const SIMD_IntegrationRule& ir = Get_SIMD_IntegrationRule(fel, lh);            
            auto & mir = trafo(ir, lh);
        
            ProxyUserData ud(trial_proxies.Size(), lh);
            const_cast<ElementTransformation&>(trafo).userdata = &ud;
            ud.fel = &fel;
            // ud.elx = &elx;
            // ud.lh = &lh;
            
            for (ProxyFunction * proxy : trial_proxies)
              {
                ud.AssignMemory (proxy, ir.GetNIP(), proxy->Dimension(), lh);
                proxy->Evaluator()->Apply(fel, mir, elx, ud.GetAMemory(proxy));
              }
            
            FlatMatrix<SIMD<double>> values(1, mir.Size(), lh);
            cf -> Evaluate(mir, values);

            SIMD<double> sum = 0;
            for (int i = 0; i < mir.Size(); i++)
              sum += mir[i].GetWeight() * values(0, i);
            return HSum(sum);
          }
        catch (ExceptionNOSIMD e)
          {
            cout << IM(4) << e.What() << endl
                 << "switching back to standard evaluation (in SymbolicEnergy::Energy)" << endl;
            simd_evaluate = false;
            return Energy (fel, trafo, elx, lh);
          }
      }

    if (!element_boundary)
      {
        // const IntegrationRule& ir = GetIntegrationRule(trafo.GetElementType(), 2*fel.Order());
        const IntegrationRule& ir = GetIntegrationRule(fel, lh);
        BaseMappedIntegrationRule & mir = trafo(ir, lh);
        
        ProxyUserData ud(trial_proxies.Size(), lh);
        const_cast<ElementTransformation&>(trafo).userdata = &ud;
        ud.fel = &fel;
        // ud.elx = &elx;
        // ud.lh = &lh;
        
        for (ProxyFunction * proxy : trial_proxies)
          {
            ud.AssignMemory (proxy, ir.Size(), proxy->Dimension(), lh);
            proxy->Evaluator()->Apply(fel, mir, elx, ud.GetMemory(proxy), lh);
          }
        FlatMatrix<> values(mir.Size(), 1, lh);
        cf -> Evaluate(mir, values);
        
        double sum = 0;
        for (int i = 0; i < mir.Size(); i++)
          sum += mir[i].GetWeight() * values(i,0);
        return sum;
      }
    else // element_boundary
      {
        double sum = 0;
        auto eltype = trafo.GetElementType();
        int nfacet = ElementTopology::GetNFacets(eltype);
        
        Facet2ElementTrafo transform(eltype); 
        
        for (int k = 0; k < nfacet; k++)
          {
            HeapReset hr(lh);
            ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
            
            const IntegrationRule & ir_facet = GetIntegrationRule(etfacet, 2*fel.Order());
            IntegrationRule & ir_facet_vol = transform(k, ir_facet, lh);
            BaseMappedIntegrationRule & mir = trafo(ir_facet_vol, lh);
            mir.ComputeNormalsAndMeasure (eltype, k);
            
            ProxyUserData ud(trial_proxies.Size(), lh);    
            const_cast<ElementTransformation&>(trafo).userdata = &ud;
            ud.fel = &fel;
            // ud.elx = &elx;
            // ud.lh = &lh;
            
            for (ProxyFunction * proxy : trial_proxies)
              {
                ud.AssignMemory (proxy, ir_facet.GetNIP(), proxy->Dimension(), lh);
                proxy->Evaluator()->Apply(fel, mir, elx, ud.GetMemory(proxy), lh);
              }

            FlatMatrix<> values(mir.Size(), 1, lh);
            cf -> Evaluate(mir, values);
            
            for (int i = 0; i < mir.Size(); i++)
              sum += mir[i].GetWeight() * values(i,0);
          }
        return sum;
      }
  }

  void
  SymbolicEnergy :: ApplyElementMatrix (const FiniteElement & fel, 
                                        const ElementTransformation & trafo, 
                                        const FlatVector<double> elx, 
                                        FlatVector<double> ely,
                                        void * precomputed,
                                        LocalHeap & lh) const
  {
    // static Timer t("SymbolicEnergy::ApplyElementMatrix", 2); 
    
    ProxyUserData ud(trial_proxies.Size(), lh);        
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    ud.fel = &fel;
    // ud.elx = &elx;
    // ud.lh = &lh;

    if (simd_evaluate && !element_boundary)
      {
        try
          {
            // RegionTimer reg(t);            

            HeapReset hr(lh);
            // const SIMD_IntegrationRule& ir = GetSIMDIntegrationRule(trafo.GetElementType(), 2*fel.Order());
            const SIMD_IntegrationRule& ir = Get_SIMD_IntegrationRule(fel, lh);
            auto & mir = trafo(ir, lh);
            
            for (ProxyFunction * proxy : trial_proxies)
              ud.AssignMemory (proxy, ir.GetNIP(), proxy->Dimension(), lh);
            for (ProxyFunction * proxy : trial_proxies)
              proxy->Evaluator()->Apply(fel, mir, elx, ud.GetAMemory(proxy));

            ely = 0;
            FlatMatrix<SIMD<double>> val(1, ir.Size(), lh);
            for (auto proxy : trial_proxies)
              {
                HeapReset hr(lh);
                FlatMatrix<SIMD<double>> proxyvalues(proxy->Dimension(), ir.Size(), lh);
                FlatMatrix<AutoDiff<1,SIMD<double>>> vals(1, ir.Size(), lh);
                for (int k = 0; k < proxy->Dimension(); k++)
                  {
                    ud.trialfunction = proxy;
                    ud.trial_comp = k;
                    // cf -> EvaluateDeriv (mir, AFlatMatrix<>(val), AFlatMatrix<>(proxyvalues.Rows(k,k+1)));
                    cf -> Evaluate(mir, vals);
                    for (size_t j = 0; j < ir.Size(); j++)
                      proxyvalues(k,j) = vals(0,j).DValue(0);
                  }

                for (int i = 0; i < proxyvalues.Height(); i++)
                  {
                    auto row = proxyvalues.Row(i);
                    for (int j = 0; j < row.Size(); j++)
                      row(j) *= mir[j].GetMeasure() * ir[j].Weight();
                  }
                
                proxy->Evaluator()->AddTrans(fel, mir, proxyvalues, ely);
              }
          }
        catch (ExceptionNOSIMD e)
          {
            cout << IM(4) << e.What() << endl
                 << "switching back to standard evaluation (in SymbolicEnergy::CalcLinearized)" << endl;              
            simd_evaluate = false;
            ApplyElementMatrix (fel, trafo, elx, ely, precomputed, lh);
          }
        return;
      }


    if (!element_boundary)
      {
        HeapReset hr(lh);
        // const IntegrationRule& ir = GetIntegrationRule(trafo.GetElementType(), 2*fel.Order());
        const IntegrationRule& ir = GetIntegrationRule(fel, lh);
        BaseMappedIntegrationRule & mir = trafo(ir, lh);
        
        for (ProxyFunction * proxy : trial_proxies)
          ud.AssignMemory (proxy, ir.GetNIP(), proxy->Dimension(), lh);
        
        for (ProxyFunction * proxy : trial_proxies)
          proxy->Evaluator()->Apply(fel, mir, elx, ud.GetMemory(proxy), lh);
        
        ely = 0;
        FlatVector<> ely1(ely.Size(), lh);
        
        FlatMatrix<> val(mir.Size(), 1,lh), deriv(mir.Size(), 1,lh);
        
        for (auto proxy : trial_proxies)
          {
            HeapReset hr(lh);
            FlatMatrix<> proxyvalues(mir.Size(), proxy->Dimension(), lh);
            for (int k = 0; k < proxy->Dimension(); k++)
              {
                ud.trialfunction = proxy;
                ud.trial_comp = k;
                cf -> EvaluateDeriv (mir, val, deriv);
                proxyvalues.Col(k) = deriv.Col(0);
              }
            
            for (int i = 0; i < mir.Size(); i++)
              proxyvalues.Row(i) *= mir[i].GetWeight();
            
            proxy->Evaluator()->ApplyTrans(fel, mir, proxyvalues, ely1, lh);
            ely += ely1;
          }
      }
    else  // element_boundary
      {
        ely = 0;
        auto eltype = trafo.GetElementType();
        int nfacet = ElementTopology::GetNFacets(eltype);
        
        Facet2ElementTrafo transform(eltype); 
        
        for (int k = 0; k < nfacet; k++)
          {
            HeapReset hr(lh);
            ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
            
            const IntegrationRule & ir_facet = GetIntegrationRule(etfacet, 2*fel.Order());
            IntegrationRule & ir_facet_vol = transform(k, ir_facet, lh);
            BaseMappedIntegrationRule & mir = trafo(ir_facet_vol, lh);
            mir.ComputeNormalsAndMeasure (eltype, k);
            
            
            ProxyUserData ud(trial_proxies.Size(), lh);    
            const_cast<ElementTransformation&>(trafo).userdata = &ud;
            ud.fel = &fel;
            // ud.elx = &elx;
            // ud.lh = &lh;
            
            for (ProxyFunction * proxy : trial_proxies)
              {
                ud.AssignMemory (proxy, ir_facet.GetNIP(), proxy->Dimension(), lh);
                proxy->Evaluator()->Apply(fel, mir, elx, ud.GetMemory(proxy), lh);
              }
        
            FlatVector<> ely1(ely.Size(), lh);
            FlatMatrix<> val(mir.Size(), 1,lh), deriv(mir.Size(), 1,lh);
            
            for (auto proxy : trial_proxies)
              {
                HeapReset hr(lh);
                FlatMatrix<> proxyvalues(mir.Size(), proxy->Dimension(), lh);
                for (int k = 0; k < proxy->Dimension(); k++)
                  {
                    ud.trialfunction = proxy;
                    ud.trial_comp = k;
                    cf -> EvaluateDeriv (mir, val, deriv);
                    proxyvalues.Col(k) = deriv.Col(0);
                  }
                
                for (int i = 0; i < mir.Size(); i++)
                  proxyvalues.Row(i) *= mir[i].GetWeight();
                
                proxy->Evaluator()->ApplyTrans(fel, mir, proxyvalues, ely1, lh);
                ely += ely1;
              }
          }
      }
  }
}
