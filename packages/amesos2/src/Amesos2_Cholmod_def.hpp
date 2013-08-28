// @HEADER
//
// ***********************************************************************
//
//           Amesos2: Templated Direct Sparse Solver Package
//                  Copyright 2011 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ***********************************************************************
//
// @HEADER

/**
   \file   Amesos2_Cholmod_def.hpp
   \author Kevin Deweese <kdeweese@sandia.gov>
   \date   Wed Jul  24 15::48:51 2013

   \brief  Definitions for the Amesos2 Cholmod solver interface
*/


#ifndef AMESOS2_CHOLMOD_DEF_HPP
#define AMESOS2_CHOLMOD_DEF_HPP

#include <Teuchos_Tuple.hpp>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_StandardParameterEntryValidators.hpp>

#include "Amesos2_SolverCore_def.hpp"
#include "Amesos2_Cholmod_decl.hpp"


namespace Amesos2 {


template <class Matrix, class Vector>
Cholmod<Matrix,Vector>::Cholmod(
  Teuchos::RCP<const Matrix> A,
  Teuchos::RCP<Vector>       X,
  Teuchos::RCP<const Vector> B )
  : SolverCore<Amesos2::Cholmod,Matrix,Vector>(A, X, B)
  , nzvals_()                   // initialize to empty arrays
  , rowind_()
  , colptr_()
  , firstsolve(true)
  , map()
{
  CHOL::cholmod_start(&data_.c);
  (&data_.c)->nmethods=9;
  //(&data_.c)->method[0].ordering=CHOLMOD_AMD;
  //(&data_.c)->print=5;
}


template <class Matrix, class Vector>
Cholmod<Matrix,Vector>::~Cholmod( )
{
  
  CHOL::cholmod_free_factor (&(data_.L),&(data_.c));

  CHOL::cholmod_finish(&data_.c);
}

template<class Matrix, class Vector>
int
Cholmod<Matrix,Vector>::preOrdering_impl()
{
  {
#ifdef HAVE_AMESOS2_TIMERS
    Teuchos::TimeMonitor preOrderTimer(this->timers_.preOrderTime_);
#endif
    data_.L = CHOL::cholmod_analyze (data_.A, &(data_.c));
  }

  return(0);
}


template <class Matrix, class Vector>
int
Cholmod<Matrix,Vector>::symbolicFactorization_impl()
{

  return(0);
}


template <class Matrix, class Vector>
int
Cholmod<Matrix,Vector>::numericFactorization_impl()
{
  using Teuchos::as;


  int info = 0;
  if ( this->root_ ){

#ifdef HAVE_AMESOS2_DEBUG
    TEUCHOS_TEST_FOR_EXCEPTION( data_.A->ncol != as<size_t>(this->globalNumCols_),
                        std::runtime_error,
                        "Error in converting to cholmod_sparse: wrong number of global columns." );
    TEUCHOS_TEST_FOR_EXCEPTION( data_.A->nrow != as<size_t>(this->globalNumRows_),
                        std::runtime_error,
                        "Error in converting to cholmod_sparse: wrong number of global rows." );
#endif


    { // Do factorization
#ifdef HAVE_AMESOS2_TIMERS
      Teuchos::TimeMonitor numFactTimer(this->timers_.numFactTime_);
#endif

#ifdef HAVE_AMESOS2_VERBOSE_DEBUG
      std::cout << "Cholmod:: Before numeric factorization" << std::endl;
      std::cout << "nzvals_ : " << nzvals_.toString() << std::endl;
      std::cout << "rowind_ : " << rowind_.toString() << std::endl;
      std::cout << "colptr_ : " << colptr_.toString() << std::endl;
#endif
      //cholmod_print_sparse(data_.A,"A",&(data_.c));
      CHOL::cholmod_factorize(data_.A, data_.L, &(data_.c));
      //cholmod_print_factor(data_.L,"L",&(data_.c));
      info = (&data_.c)->status;
      
    }
    
  }

  /* All processes should have the same error code */
  Teuchos::broadcast(*(this->matrixA_->getComm()), 0, &info);

  //global_size_type info_st = as<global_size_type>(info);
  //TEUCHOS_TEST_FOR_EXCEPTION( (info_st > 0) && (info_st <= this->globalNumCols_),
  //std::runtime_error,
  //"Factorization complete, but matrix is singular. Division by zero eminent");
  TEUCHOS_TEST_FOR_EXCEPTION( info == 2,
    std::runtime_error,
    "Memory allocation failure in Cholmod factorization");

  return(info);
}


template <class Matrix, class Vector>
int
Cholmod<Matrix,Vector>::solve_impl(const Teuchos::Ptr<MultiVecAdapter<Vector> >       X,
                                   const Teuchos::Ptr<const MultiVecAdapter<Vector> > B) const
{
  using Teuchos::as;

  const global_size_type ld_rhs = this->root_ ? X->getGlobalLength() : 0;
  const size_t nrhs = X->getGlobalNumVectors();
  
  Teuchos::RCP<const Tpetra::Map<local_ordinal_type,
    global_ordinal_type, node_type> > map2;

  const size_t val_store_size = as<size_t>(ld_rhs * nrhs);
  Teuchos::Array<chol_type> xValues(val_store_size);
  Teuchos::Array<chol_type> bValues(val_store_size);

  {                             // Get values from RHS B
#ifdef HAVE_AMESOS2_TIMERS
    Teuchos::TimeMonitor mvConvTimer(this->timers_.vecConvTime_);
    Teuchos::TimeMonitor redistTimer( this->timers_.vecRedistTime_ );
#endif
    Util::get_1d_copy_helper<MultiVecAdapter<Vector>,
                             chol_type>::do_get(B, bValues(),
                                               as<size_t>(ld_rhs),
                                               ROOTED, this->rowIndexBase_);
      
  }


  int ierr = 0; // returned error code


  if ( this->root_ ) {


    {
#ifdef HAVE_AMESOS2_TIMERS
      Teuchos::TimeMonitor mvConvTimer(this->timers_.vecConvTime_);
#endif

      function_map::cholmod_allocate_dense(as<int>(this->globalNumRows_),as<int>(nrhs),as<int>(ld_rhs),bValues.getRawPtr(),&data_.btemp);
      function_map::cholmod_allocate_dense(as<int>(this->globalNumRows_),as<int>(nrhs),as<int>(ld_rhs),xValues.getRawPtr(),&data_.xtemp);

      data_.b = &(data_.btemp);
      data_.x = &(data_.xtemp);
    }


    {                           // Do solve!
#ifdef HAVE_AMESOS2_TIMERS
    Teuchos::TimeMonitor solveTimer(this->timers_.solveTime_);
#endif


    CHOL::cholmod_dense *Y = NULL;
    CHOL::cholmod_dense *E = NULL;
    CHOL::cholmod_solve2(CHOLMOD_A,data_.L,data_.b,NULL,&data_.x,NULL,&Y,&E,&data_.c);
    ierr = (&data_.c)->status;
    CHOL::cholmod_free_dense(&Y,&data_.c);
    CHOL::cholmod_free_dense(&E,&data_.c);

    }

    
  }

  /* All processes should have the same error code */
  Teuchos::broadcast(*(this->getComm()), 0, &ierr);

  TEUCHOS_TEST_FOR_EXCEPTION( ierr == -2,
                      std::runtime_error,
                      "Ran out of memory" );

  /* Update X's global values */
  {
#ifdef HAVE_AMESOS2_TIMERS
    Teuchos::TimeMonitor redistTimer(this->timers_.vecRedistTime_);
#endif
    
    Util::put_1d_data_helper<
      MultiVecAdapter<Vector>,chol_type>::do_put(X, xValues(),
                                         as<size_t>(ld_rhs),
                                         ROOTED, this->rowIndexBase_);
    
  }

  
  return(ierr);
}


template <class Matrix, class Vector>
bool
Cholmod<Matrix,Vector>::matrixShapeOK_impl() const
{
  return( this->matrixA_->getGlobalNumRows() == this->matrixA_->getGlobalNumCols() );
}


template <class Matrix, class Vector>
void
Cholmod<Matrix,Vector>::setParameters_impl(const Teuchos::RCP<Teuchos::ParameterList> & parameterList )
{
  using Teuchos::RCP;
  using Teuchos::getIntegralValue;
  using Teuchos::ParameterEntryValidator;

  RCP<const Teuchos::ParameterList> valid_params = getValidParameters_impl();


  if( parameterList->isParameter("Trans") ){
    RCP<const ParameterEntryValidator> trans_validator = valid_params->getEntry("Trans").validator();
    parameterList->getEntry("Trans").setValidator(trans_validator);

  }

  if( parameterList->isParameter("IterRefine") ){
    RCP<const ParameterEntryValidator> refine_validator = valid_params->getEntry("IterRefine").validator();
    parameterList->getEntry("IterRefine").setValidator(refine_validator);


  }

  if( parameterList->isParameter("ColPerm") ){
    RCP<const ParameterEntryValidator> colperm_validator = valid_params->getEntry("ColPerm").validator();
    parameterList->getEntry("ColPerm").setValidator(colperm_validator);


  }

  (&data_.c)->dbound = parameterList->get<double>("dbound", 0.0);

  bool prefer_upper = parameterList->get<bool>("PreferUpper", true);
  
  (&data_.c)->prefer_upper = prefer_upper ? 1 : 0;

  (&data_.c)->print = parameterList->get<int>("print",3);

  (&data_.c)->nmethods = parameterList->get<int>("nmethods",0);

}


template <class Matrix, class Vector>
Teuchos::RCP<const Teuchos::ParameterList>
Cholmod<Matrix,Vector>::getValidParameters_impl() const
{
  using std::string;
  using Teuchos::tuple;
  using Teuchos::ParameterList;
  using Teuchos::EnhancedNumberValidator;
  using Teuchos::setStringToIntegralParameter;
  using Teuchos::stringToIntegralParameterEntryValidator;

  static Teuchos::RCP<const Teuchos::ParameterList> valid_params;

  if( is_null(valid_params) ){
    Teuchos::RCP<Teuchos::ParameterList> pl = Teuchos::parameterList();


    Teuchos::RCP<EnhancedNumberValidator<int> > print_validator
      = Teuchos::rcp( new EnhancedNumberValidator<int>(0,5));

    Teuchos::RCP<EnhancedNumberValidator<int> > nmethods_validator
      = Teuchos::rcp( new EnhancedNumberValidator<int>(0,9));

    pl->set("nmethods", 0, "Specifies the number of different ordering methods to try", nmethods_validator);

    pl->set("print", 3, "Specifies the verbosity of the print statements", print_validator);

    pl->set("dbound", 0.0,
            "Specifies the smallest absolute value on the diagonal D for the LDL' factorization");


    pl->set("Equil", true, "Whether to equilibrate the system before solve");

    pl->set("PreferUpper", true,
            "Specifies whether the matrix will be " 
            "stored in upper triangular form.");

    valid_params = pl;
  }

  return valid_params;
}


template <class Matrix, class Vector>
bool
Cholmod<Matrix,Vector>::loadA_impl(EPhase current_phase)
{
  using Teuchos::as;

#ifdef HAVE_AMESOS2_TIMERS
  Teuchos::TimeMonitor convTimer(this->timers_.mtxConvTime_);
#endif


  // Only the root image needs storage allocated
  if( this->root_ ){
    nzvals_.resize(this->globalNumNonZeros_);
    rowind_.resize(this->globalNumNonZeros_);
    colptr_.resize(this->globalNumCols_ + 1);
  }

  int nnz_ret = 0;
  {
#ifdef HAVE_AMESOS2_TIMERS
    Teuchos::TimeMonitor mtxRedistTimer( this->timers_.mtxRedistTime_ );
#endif

    TEUCHOS_TEST_FOR_EXCEPTION( this->rowIndexBase_ != this->columnIndexBase_,
                        std::runtime_error,
                        "Row and column maps have different indexbase ");
    Util::get_ccs_helper<
    MatrixAdapter<Matrix>,chol_type,int,int>::do_get(this->matrixA_.ptr(),
                                                    nzvals_(), rowind_(),
                                                    colptr_(), nnz_ret, ROOTED,
                                                    ARBITRARY,
                                                    this->rowIndexBase_);
  }

  if( this->root_ ){
    TEUCHOS_TEST_FOR_EXCEPTION( nnz_ret != as<int>(this->globalNumNonZeros_),
                        std::runtime_error,
                        "Did not get the expected number of non-zero vals");

    function_map::cholmod_allocate_sparse(as<size_t>(this->globalNumRows_),as<size_t>(this->globalNumCols_),as<size_t>(this->globalNumNonZeros_),0,colptr_.getRawPtr(),nzvals_.getRawPtr(),rowind_.getRawPtr(),&data_.Atemp);
        
    data_.A = &(data_.Atemp);
    
    

  }


  return true;
}


template<class Matrix, class Vector>
const char* Cholmod<Matrix,Vector>::name = "Cholmod";
  

} // end namespace Amesos2

#endif  // AMESOS2_CHOLMOD_DEF_HPP
