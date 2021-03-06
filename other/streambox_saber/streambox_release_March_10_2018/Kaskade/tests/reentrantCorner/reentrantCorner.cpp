/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                       */
/*  This file is part of the library KASKADE 7                 */
/*    see http://www.zib.de/en/numerik/software/kaskade-7.html         */
/*                                       */
/*  Copyright (C) 2002-2011 Zuse Institute Berlin              */
/*                                       */
/*  KASKADE 7 is distributed under the terms of the ZIB Academic License.  */
/*    see $KASKADE/academic.txt                        */
/*                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 * @file
 * @ingroup tests
 * @brief  Test with embedded error estimation on a domain with reentrant corner.
 * 
 * Testprogram for Kaskade: tests wether the error between calculated and exact solution behaves as expected. 
 * This test was built by using the embedded error estimation example (2D) and changing the domain. 
 * 
 * Test uses direct solver or IterateType::CG with preconditioner PrecondType::ICC, PrecondType::ICC0 or PrecondType::BOOMERAMG. 
 * Uses ContinuousLagrangeMapper.
 * 
 * Since the exact solution is not known, the solution, that is obtained by the finest refinement, is taken for error calculation.
 */

#include <iostream>

#include <boost/timer/timer.hpp>

#include "dune/grid/config.h"
#include "dune/grid/uggrid.hh"

#include "fem/assemble.hh"
#include "fem/embedded_errorest.hh"
#include "fem/norms.hh"
// #include "fem/hierarchicspace.hh"
#include "fem/lagrangespace.hh"
#include "linalg/trivialpreconditioner.hh"
#include "linalg/direct.hh"
#include "linalg/iccprecond.hh"
#include "linalg/icc0precond.hh"
#include "linalg/hyprecond.hh"       // BoomerAMG
#include "linalg/cg.hh"
#include "utilities/enums.hh"
#include "utilities/kaskopt.hh"

using namespace Kaskade;
#include "reentrantCorner.hh"

int main(int argc, char *argv[])
{
  using namespace boost::fusion;

  int verbosityOpt = 0;
  bool dump = false; 
  constexpr int dim=2;    
  using Grid = Dune::UGGrid<dim>;
  using LeafView = Grid::LeafGridView;
  using H1Space = FEFunctionSpace<ContinuousLagrangeMapper<double,LeafView> >;
  // using H1Space = FEFunctionSpace<ContinuousHierarchicMapper<double,LeafView> >;
  using Spaces = boost::fusion::vector<H1Space const*>;
  using VariableDescriptions = boost::fusion::vector<VariableDescription<0,1,0> >;
  using VariableSet = VariableSetDescription<Spaces,VariableDescriptions>;
  using Functional = ReentrantCornerFunctional<double,VariableSet>;
  constexpr int nvars = Functional::AnsatzVars::noOfVariables;
  constexpr int neq = Functional::TestVars::noOfVariables;
  using Assembler = VariationalFunctionalAssembler<LinearizationAt<Functional> >;
  using Rhs = Assembler::TestVariableSet::CoefficientVectorRepresentation<>::type;
  using CoefficientVectors = VariableSet::CoefficientVectorRepresentation<0,neq>::type;
  using LinearSpace = VariableSet::CoefficientVectorRepresentation<0,neq>::type;

  std::unique_ptr<boost::property_tree::ptree> pt = getKaskadeOptions(argc, argv, verbosityOpt, dump);

  std::cout << "Start heat program using embedded error estimation on a domain with reentrant corner (einspringender Ecke)" << std::endl;

  int  refinements = 2;
  int verbosity   = getParameter(pt, "verbosity", 1);
  // if true, then the test result will be written in a file
  bool result = getParameter(pt, "result",0);

  constexpr int orderLength = 5;
  constexpr int maxRefSteps = 45;
  //ansatz orders to be tested
  int order[orderLength] = {1,2,3,4,5};
  //error values for corresponding ansatz order
  //double errors[orderLength] = {3.4e-4,1.3e-4,6.0e-6,9.0e-7,1.5e-7};
  double errors[orderLength] = {1.4e-3,3.1e-4,4.2e-05,5.0e-05,6.7e-05};
  // former values for error limits
  //double errors[orderLength] = {1e-3,3e-4,2e-5,3e-6,1.5e-7};
  // values for order of convergence for corresponding ansatz order
  double conOrder[orderLength] = {0.7,0.9,1.2,1.4,1.47};
  
  double atol = 1e-7;
  double rtol = 1e-7;
  
  bool valid = true;
  std::stringstream message("Test succeeded", std::stringstream::out);
  
  DirectType directType;
  MatrixProperties property;
  PrecondType precondType = PrecondType::NONE;
  std::string empty;
  
  int direct, onlyLowerTriangle = false;

  std::string s("names.type.");
  std::string solverTypeI = getParameter(pt, "solver.type", empty);
  std::string solverTypeII;
  s += solverTypeI;
  direct = getParameter(pt, s, 0);

  std::string directSolver = getParameter(pt, "solver.direct", empty);
  s = "names.direct." + directSolver;
  directType = static_cast<DirectType>(getParameter(pt, s, 2));

  // Remark: in this example only PrecondType::NONE,PrecondType::JACOBI, PrecondType::ICC, PrecondType::ICC0, PrecondType::BOOMERAMG are used.
  std::string preconditioner = getParameter(pt, "solver.preconditioner", empty);
  s = "names.preconditioner." + preconditioner;
  precondType = static_cast<PrecondType>(getParameter(pt, s, 0));

  property = MatrixProperties::SYMMETRIC;
  if(verbosity > 0) {
    std::cout << "discretization is symmetric" << std::endl;
  }
  
  if ( (directType == DirectType::MUMPS)||(directType == DirectType::PARDISO) || (precondType == PrecondType::ICC) )
  {
    onlyLowerTriangle = true;
    std::cout << 
      "Note: direct solver MUMPS/PARADISO or PrecondType::ICC preconditioner ===> onlyLowerTriangle is set to true!" 
      << std::endl;
  }

  //iterate over the ansatz orders
  for(int i = 0;i < orderLength && valid;i++) {
    if(verbosity > 0) {
      std::cout << "original mesh shall be refined : " << refinements << " times" << std::endl;
      std::cout << "discretization order         : " << order[i] << std::endl;
      std::cout << "output level (verbosity)     : " << verbosity << std::endl;
    }

    //   two-dimensional space: dim=2
    Dune::GridFactory<Grid> factory;

    // create Grid on a domain with reentrant corner
    // vertex coordinates v[0], v[1]
    Dune::FieldVector<double,dim> v;  
    v[0]=0; v[1]=0; factory.insertVertex(v);
    v[0]=0.5; v[1]=0; factory.insertVertex(v);
    v[0]=1; v[1]=0; factory.insertVertex(v);
    v[0]=1; v[1]=0.499; factory.insertVertex(v);
    v[0]=0.5; v[1]=0.5; factory.insertVertex(v);
    v[0]=1; v[1]=0.501; factory.insertVertex(v);
    v[0]=1; v[1]=1; factory.insertVertex(v);
    v[0]=0.5; v[1]=1; factory.insertVertex(v);
    v[0]=0; v[1]=1; factory.insertVertex(v);
    v[0]=0; v[1]=0.5; factory.insertVertex(v);
    // triangle defined by 3 vertex indices
    std::vector<unsigned int> vid(3);
    Dune::GeometryType gt(Dune::GeometryType::simplex,2);
    vid[0]=0; vid[1]=1; vid[2]=9; factory.insertElement(gt,vid);
    vid[0]=1; vid[1]=4; vid[2]=9; factory.insertElement(gt,vid);
    vid[0]=1; vid[1]=3; vid[2]=4; factory.insertElement(gt,vid);
    vid[0]=1; vid[1]=2; vid[2]=3; factory.insertElement(gt,vid);
    vid[0]=5; vid[1]=6; vid[2]=7; factory.insertElement(gt,vid);
    vid[0]=4; vid[1]=5; vid[2]=7; factory.insertElement(gt,vid);
    vid[0]=4; vid[1]=7; vid[2]=9; factory.insertElement(gt,vid);
    vid[0]=7; vid[1]=8; vid[2]=9; factory.insertElement(gt,vid);
    std::unique_ptr<Grid> grid( factory.createGrid() ) ;
    // the coarse grid will be refined 
    grid->globalRefine(refinements);
    // some information on the refined mesh
    if(verbosity > 0) {
      std::cout << std::endl;
      std::cout << "Grid: " << grid->size(0) << " triangles, " << std::endl;
      std::cout << "      " << grid->size(1) << " edges, " << std::endl;
      std::cout << "      " << grid->size(2) << " points" << std::endl << std::endl;
    }
      
    // a gridmanager is constructed 
    // as connector between geometric and algebraic information
    GridManager<Grid> gridManager(std::move(grid));  
    gridManager.setVerbosity(verbosity);
    gridManager.enforceConcurrentReads(true);
    
    // construction of finite element space for the scalar solution T
    H1Space temperatureSpace(gridManager,gridManager.grid().leafGridView(),
    order[i]);
    Spaces spaces(&temperatureSpace);
    // VariableDescription<int spaceId, int components, int Id>
    // spaceId: number of associated FEFunctionSpace
    // components: number of components in this variable
    // Id: number of this variable
    std::string varNames[1] = { "T" };
    VariableSet variableSet(spaces,varNames);
    Functional F;
    
    if(verbosity > 0) {
      std::cout << "no of variables = " << nvars << std::endl;
      std::cout << "no of equations = " << neq   << std::endl;
    }
    
    //construct Galerkin representation
    Assembler assembler(gridManager,spaces);
    
    // keep the solutions of different refinements
    std::vector<VariableSet::VariableSet> xx;
    xx.reserve(maxRefSteps);
    for(int i=0;i<maxRefSteps;i++) {
      xx.push_back(VariableSet::VariableSet(variableSet));
    }
    
    size_t nnz  = assembler.nnz(0,neq,0,nvars,onlyLowerTriangle);
    size_t size = variableSet.degreesOfFreedom(0,nvars);
    if ( verbosity>0) std::cout << "init mesh: nnz = " << nnz << ", dof = " << size << std::endl;
    
    std::vector<std::pair<double,double> > tol(1);   
    tol[0] = std::make_pair(atol,rtol); 
    if(verbosity > 0) {
      std::cout << std::endl << "Accuracy: atol = " << atol << ",  rtol = " << rtol << std::endl;
    }

    bool accurate = true;
    int refSteps = -1;

    //keep degrees of freedom
    std::vector<double> dofs;
    dofs.reserve(maxRefSteps);
    
    do {
      refSteps++;
      if(refSteps >= maxRefSteps || size > 2e5) 
      {
        refSteps--;
        std::cout << "maxRefSteps = " << maxRefSteps << ",  size = " << size << std::endl;
        break;
      }

      boost::timer::cpu_timer assembTimer;
      VariableSet::VariableSet x(variableSet);
      assembler.assemble(linearization(F,x));
      CoefficientVectors solution(VariableSet::CoefficientVectorRepresentation<0,neq>::init(spaces));
      solution = 0;
      CoefficientVectors rhs(assembler.rhs());
      AssembledGalerkinOperator<Assembler,0,1,0,1> A(assembler, onlyLowerTriangle);
      MatrixAsTriplet<double> tri = A.get<MatrixAsTriplet<double> >();
      if ( verbosity>0) std::cout << "assemble: " << (double)assembTimer.elapsed().user/1e9 << "s\n";

      if (direct)
      {
        solverTypeII = directSolver;
        boost::timer::cpu_timer directTimer;
        directInverseOperator(A,directType,property).applyscaleadd(-1.0,rhs,solution);
        x.data = solution.data;

        if ( verbosity>0) std::cout << "direct solve: " << (double)(directTimer.elapsed().user)/1e9 << "s\n";
      }
      else
      {
        solverTypeII = "CG, " + preconditioner;
        int iteSteps = getParameter(pt, "solver.iteMax", 1000);
        double iteEps = getParameter(pt, "solver.iteEps", 1.0e-10);
        //if ( verbosity>0) std::cout << "iterative solver: steps = " << iteSteps << 
        //   ", eps = " << iteEps << std::endl;
        boost::timer::cpu_timer iteTimer;
        Dune::InverseOperatorResult res;
        const DefaultDualPairing<LinearSpace,LinearSpace> defaultScalarProduct{};
        StrakosTichyPTerminationCriterion<double> termination(iteEps,iteSteps);
        int lookAhead = getParameter(pt, "solver.lookAhead", 3);
        termination.setLookAhead(lookAhead);
        
        switch (precondType)
        {
          case PrecondType::ICC0:
          {
            std::cout << "selected preconditioner: ICC0" << std::endl;
            ICC_0Preconditioner<AssembledGalerkinOperator<Assembler,0,1,0,1> > icc0(A);
            CG<LinearSpace,LinearSpace> cg(A,icc0,defaultScalarProduct,termination,verbosity);
            cg.apply(solution,rhs,res);
          }
          break;
          case PrecondType::ICC:
          {
            std::cout << "selected preconditioner: ICC" << std::endl;
            if (property != MatrixProperties::SYMMETRIC) 
            {
              std::cout << "PrecondType::ICC preconditioner of TAUCS lib has to be used with matrix.property==MatrixProperties::SYMMETRIC\n";
              std::cout << "i.e., call the executable with option --solver.property MatrixProperties::SYMMETRIC\n\n";
            }
            double dropTol = getParameter(pt, "solver.ICC.dropTol", 0.01);;
            ICCPreconditioner<AssembledGalerkinOperator<Assembler,0,1,0,1> > icc(A,dropTol);
            CG<LinearSpace,LinearSpace> cg(A,icc,defaultScalarProduct,termination,verbosity);
            cg.apply(solution,rhs,res);
          }
          break;
          case PrecondType::BOOMERAMG:
          default:
          {
            int steps = getParameter(pt, "solver.BOOMERAMG.steps", iteSteps);
            int coarsentype = getParameter(pt, "solver.BOOMERAMG.coarsentype", 21);
            int interpoltype = getParameter(pt, "solver.BOOMERAMG.interpoltype", 0);
            int cycleType = getParameter(pt, "solver.BOOMERAMG.cycleType", 1);
            int relaxType = getParameter(pt, "solver.BOOMERAMG.relaxType", 3);
            int variant = getParameter(pt, "solver.BOOMERAMG.variant", 0);
            int overlap = getParameter(pt, "solver.BOOMERAMG.overlap", 1);
            double tol = getParameter(pt, "solver.BOOMERAMG.tol", iteEps);
            double strongThreshold = getParameter(pt, "solver.BOOMERAMG.strongThreshold", (dim==2)?0.25:0.6);
            BoomerAMG<AssembledGalerkinOperator<Assembler,0,1,0,1> >
            BoomerAMGPrecon(A,steps,coarsentype,interpoltype,tol,cycleType,relaxType,
            strongThreshold,variant,overlap,1,verbosity);
            //Dune::LoopSolver<LinearSpace> cg(A,BoomerAMGPrecon,iteEps,iteSteps,verbosity);
            CG<LinearSpace,LinearSpace> cg(A,BoomerAMGPrecon,defaultScalarProduct,termination,verbosity);
            cg.apply(solution,rhs,res);
          }
          break;
        }
        solution *= -1.0;
        x.data = solution.data;
    
        if ( verbosity>0) std::cout << "iterative solve eps= " << iteEps << ": " 
            << (res.converged?"converged":"failed") << " after "
            << res.iterations << " steps, rate="
            << res.conv_rate << ", time=" << (double)(iteTimer.elapsed().user)/1e9 << "s\n";
      }
      // VariableSet::VariableSet xx may be used beyond the do...while loop  
      xx[refSteps].data = x.data;
      dofs.push_back(size);
      
      VariableSet::VariableSet e = x;
      projectHierarchically(variableSet,e);
      e -= x;    
    
      accurate = embeddedErrorEstimator(variableSet,e,x,IdentityScaling(),tol,gridManager,verbosity);
      nnz = assembler.nnz(0,1,0,1,onlyLowerTriangle);;
      size = variableSet.degreesOfFreedom(0,1);
      if ( verbosity>0) std::cout << "new mesh: nnz = " << nnz << ", dof = " << size << std::endl;
            
    }  while (!accurate); 
    
    //calculate and keep errors
    if ( refSteps < 3 ) {
      message << "Test failed: refSteps = " << refSteps << " is too small" ;
      break;}
    else {
      
      std::vector<double> ress(refSteps-2);
      for(int i = 0; i < refSteps-2;i++) {
        L2Norm l2;
        xx[i] -= xx[refSteps];
        ress[i] = l2( boost::fusion::at_c<0>(xx[i].data) );
      }
      
      if(verbosity > 0) {
        std::cout << "error = " << ress.back() << std::endl;
      }
  
      if(verbosity > 0) {
      std::cout << "error in l2norm: " << ress.back() << "  has to be smaller than  " 
                << errors[i] << std::endl;
      }
      
      // query whether error is low enough
      if(ress.back() > errors[i]) {
        valid = false;
        message << "Test failed: The error was too high at the test with ansatz functions of order " 
                << order[i] << ".";
        message << "\n             err = " << ress.back() << ", threshold = " << errors[i] << ".";
      }
      
      // calculate order of convergence by linear regression
      double a11=0, a12=0, a22=ress.size(), b1=0,b2=0;
      for(int j=0;j<ress.size();j++) {
        dofs[j] = std::log(dofs[j]);
        ress[j] = std::log(ress[j]);
        a11 += dofs[j]*dofs[j];
        a12 += dofs[j];
        b1 += dofs[j]*ress[j];
        b2 += ress[j];
      }
      double det = 1.0/(a11*a22-a12*a12);
      double convOrd = det*(a22*b1-a12*b2);
      double logc = det*(a11*b2-a12*b1);
      convOrd = -convOrd;
      if(verbosity > 0) {
        std::cout << "error = c*(1/N)^p with" << std::endl;
        std::cout << "p = " << convOrd << std::endl;
        std::cout << "log(c) = " << logc << std::endl;
      }
      if(convOrd < conOrder[i]) {
        valid = false;
        message << "Test failed: The order of convergence was too low at the test with ansatz functions of order " << order[i] << ".";
      }
    }
  }
  
  std::cout << "End test program" << std::endl;
  std::cout << message.str() << std::endl;
  if(result) {
    std::string description = "Test with embedded error estimation, einspringende Ecke. Used " 
                              + solverTypeI + " solver " + solverTypeII + ":";
    std::ofstream outfile("../testResult.txt", std::ofstream::out | std::ofstream::app);
    outfile << description << std::endl << message.str() << std::endl << std::endl;
    outfile.close();
  }
}
