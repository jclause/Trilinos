//@HEADER
// ************************************************************************
// 
//          Trilinos: An Object-Oriented Solver Framework
//              Copyright (2001) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//   
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//   
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
// 
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
// 
// ************************************************************************
//@HEADER

#include "BuildTestProblems.h"

  int  BuildMatrixTests (Epetra_Vector & C,
		       const char TransA, const char TransB, 
		       const double alpha, 
		       Epetra_Vector& A, 
		       Epetra_Vector& B,
		       const double beta,
		       Epetra_Vector& C_GEMM ) {

    // For given values of TransA, TransB, alpha and beta, a (possibly
    // zero) filled Epetra_Vector C, and allocated
    // Epetra_Vectors A, B and C_GEMM this routine will generate values for 
    // Epetra_Vectors A, B and C_GEMM such that, if A, B and (this) are 
    // used with GEMM in this class, the results should match the results 
    // generated by this routine.

    // Test for Strided multivectors (required for GEMM ops)

    if (!A.ConstantStride()   ||
	!B.ConstantStride()      ||
	!C_GEMM.ConstantStride() ||
	!C.ConstantStride()) return(-1); // Error 

    int i, j;
    double fi, fj;  // Used for casting loop variables to floats

    // Get a view of the Vectors

    double *Ap      = 0;
    double *Bp      = 0;
    double *Cp      = 0;
    double *C_GEMMp = 0;

    int A_nrows = A.MyLength();
    int A_ncols = A.NumVectors();
    int B_nrows = B.MyLength();
    int B_ncols = B.NumVectors();
    int C_nrows = C.MyLength();
    int C_ncols = C.NumVectors();
    int C_GEMM_nrows = C_GEMM.MyLength();
    int C_GEMM_ncols = C_GEMM.NumVectors();
    int A_Stride         = 0;
    int B_Stride         = 0;
    int C_Stride         = 0;
    int C_GEMM_Stride    = 0;

    A.ExtractView(&Ap);
    B.ExtractView(&Bp);
    C.ExtractView(&Cp);
    C_GEMM.ExtractView(&C_GEMMp);

      // Define some useful constants


    int opA_ncols = (TransA=='N') ? A.NumVectors() : A.MyLength();
    int opB_nrows = (TransB=='N') ? B.MyLength() : B.NumVectors();
  
    int C_global_inner_dim  = (TransA=='N') ? A.NumVectors() : A.GlobalLength();
  

    bool A_is_local = (!A.DistributedGlobal());
    bool B_is_local = (!B.DistributedGlobal());
    bool C_is_local = (!C.DistributedGlobal());

    int A_IndexBase = A.Map().IndexBase();
    int B_IndexBase = B.Map().IndexBase();
  
    // Build two new maps that we can use for defining global equation indices below
    Epetra_Map * A_Map = new Epetra_Map(-1, A_nrows, A_IndexBase, A.Map().Comm());
    Epetra_Map * B_Map = new Epetra_Map(-1, B_nrows, B_IndexBase, B.Map().Comm());

    int* A_MyGlobalElements = new int[A_nrows];
    A_Map->MyGlobalElements(A_MyGlobalElements);
    int* B_MyGlobalElements = new int[B_nrows];
    B_Map->MyGlobalElements(B_MyGlobalElements);

  // Check for compatible dimensions

    if (C.MyLength()        != C_nrows     ||
	opA_ncols      != opB_nrows   ||
	C.NumVectors()    != C_ncols     ||
	C.MyLength()        != C_GEMM.MyLength()        ||
	C.NumVectors()    != C_GEMM.NumVectors()      ) {
      delete A_Map;
      delete B_Map;
      delete [] A_MyGlobalElements;
      delete [] B_MyGlobalElements;
      return(-2); // Return error
    }

    bool Case1 = ( A_is_local &&  B_is_local &&  C_is_local);  // Case 1 above
    bool Case2 = (!A_is_local && !B_is_local &&  C_is_local && TransA=='T' );// Case 2
    bool Case3 = (!A_is_local &&  B_is_local && !C_is_local && TransA=='N');// Case 3
  
    // Test for meaningful cases

    if (!(Case1 || Case2 || Case3)) {
      delete A_Map;
      delete B_Map;
      delete [] A_MyGlobalElements;
      delete [] B_MyGlobalElements;
      return(-3); // Meaningless case
    }

    /* Fill A, B and C with values as follows:

       If A_is_local is false:
       A(i,j) = A_MyGlobalElements[i]*j, i=1,...,numLocalEquations, j=1,...,NumVectors
       else
       A(i,j) = i*j,     i=1,...,numLocalEquations, j=1,...,NumVectors
       
       If B_is_local is false:
       B(i,j) = 1/(A_MyGlobalElements[i]*j), i=1,...,numLocalEquations, j=1,...,NumVectors
       
       else
       B(i,j) = 1/(i*j), i=1,...,numLocalEquations, j=1,...,NumVectors
       
       In addition, scale each entry by GlobalLength for A and
       1/GlobalLength for B--keeps the magnitude of entries in check
  
     
       C_GEMM will depend on A_is_local and B_is_local.  Three cases:
       
       1) A_is_local true and B_is_local true:
       C_GEMM will be local replicated and equal to A*B = i*NumVectors/j
       
       2) A_is_local false and B_is_local false
       C_GEMM will be local replicated = A(trans)*B(i,j) = i*numGlobalEquations/j
       
       3) A_is_local false B_is_local true
       C_GEMM will distributed global and equals A*B = A_MyGlobalElements[i]*NumVectors/j
       
    */

    // Define a scalar to keep magnitude of entries reasonable

    double sf = C_global_inner_dim;
    double sfinv = 1.0/sf;

    // Define A depending on A_is_local

    if (A_is_local)
      {
	for (j = 0; j <A_ncols ; j++) 
	  for (i = 0; i<A_nrows; i++)
	    { 
	      fi = i+1; // Get float version of i and j, offset by 1.
	      fj = j+1;
	      Ap[i + A_Stride*j] = (fi*sfinv)*fj;
	    }
      }
    else
      {
	for (j = 0; j <A_ncols ; j++) 
	  for (i = 0; i<A_nrows; i++)
	    { 
	      fi = A_MyGlobalElements[i]+1; // Get float version of i and j, offset by 1.
	      fj = j+1;
	      Ap[i + A_Stride*j] = (fi*sfinv)*fj;
	    }
      }
    
    // Define B depending on TransB and B_is_local
  
    if (B_is_local)
      {
	for (j = 0; j <B_ncols ; j++) 
	  for (i = 0; i<B_nrows; i++)
	    { 
	      fi = i+1; // Get float version of i and j, offset by 1.
	      fj = j+1;
	      Bp[i + B_Stride*j] = 1.0/((fi*sfinv)*fj);
	    }
      }
    else
      {
	for (j = 0; j <B_ncols ; j++) 
	  for (i = 0; i<B_nrows; i++)
	    { 
	      fi = B_MyGlobalElements[i]+1; // Get float version of i and j, offset by 1.
	      fj = j+1;
	      Bp[i + B_Stride*j] = 1.0/((fi*sfinv)*fj);
	    }
      }
    // Define C_GEMM depending on A_is_local and B_is_local.  C_GEMM is also a
    // function of alpha, beta, TransA, TransB: 
    
    //       C_GEMM = alpha*A(TransA)*B(TransB) + beta*C_GEMM
    
    if (Case1)
      {
	for (j = 0; j <C_ncols ; j++) 
	  for (i = 0; i<C_nrows; i++)
	    { 
	      // Get float version of i and j, offset by 1.
	      fi = (i+1)*C_global_inner_dim;
	      fj = j+1;
	      C_GEMMp[i + C_GEMM_Stride*j] = alpha * (fi/fj)
		+ beta * Cp[i + C_Stride*j];
	    }
      }
    else if (Case2)
      {
	for (j = 0; j <C_ncols ; j++)
	  for (i = 0; i<C_nrows; i++)
	    { 
	      // Get float version of i and j, offset by 1.
	      fi = (i+1)*C_global_inner_dim;
	      fj = j+1;
	      C_GEMMp[i + C_GEMM_Stride*j] = alpha * (fi/fj)
		+ beta * Cp[i + C_Stride*j];
	    }  
      }
    else
      {
	for (j = 0; j <C_ncols ; j++) 
	  for (i = 0; i<C_nrows; i++)
	    { 
	      // Get float version of i and j.
	      fi = (A_MyGlobalElements[i]+1)*C_global_inner_dim;
	      fj = j+1;
	      C_GEMMp[i + C_GEMM_Stride*j] = alpha * (fi/fj)
		+ beta * Cp[i + C_Stride*j];
	    }  
      }
    delete A_Map;
    delete B_Map;
    delete []A_MyGlobalElements;
    delete []B_MyGlobalElements;

    return(0);
  }
int  BuildVectorTests (Epetra_Vector & C, const double alpha, 
				Epetra_Vector& A, 
				Epetra_Vector& sqrtA,
				Epetra_Vector& B,
				Epetra_Vector& C_alphaA,
				Epetra_Vector& C_alphaAplusB,
				Epetra_Vector& C_plusB,
				double* const dotvec_AB,
				double* const norm1_A,
				double* const norm2_sqrtA,
				double* const norminf_A,
				double* const normw_A,
				Epetra_Vector& Weights,
				double* const minval_A,
				double* const maxval_A,
				double* const meanval_A ) {

  // For given values alpha and a (possibly zero) filled 
  // Epetra_Vector (the this object), allocated double * arguments dotvec_AB, 
  // norm1_A, and norm2_A, and allocated Epetra_Vectors A, sqrtA,
  // B, C_alpha, C_alphaAplusB and C_plusB, this method will generate values for 
  // Epetra_Vectors A, B and all of the additional arguments on
  // the list above such that, if A, B and (this) are used with the methods in 
  // this class, the results should match the results generated by this routine.
  // Specifically, the results in dotvec_AB should match those from a call to 
  // A.dotProd (B,dotvec).  Similarly for other routines.
  
  int i,j;
  double fi, fj;  // Used for casting loop variables to floats
  // Define some useful constants
  
  int A_nrows = A.MyLength();
  int A_ncols = A.NumVectors();
  int sqrtA_nrows = sqrtA.MyLength();
  int sqrtA_ncols = sqrtA.NumVectors();
  int B_nrows = B.MyLength();
  int B_ncols = B.NumVectors();
  
  double *Ap = 0;
  double *sqrtAp = 0;
  double *Bp = 0;
  double *Cp = 0;
  double *C_alphaAp = 0;
  double *C_alphaAplusBp = 0;
  double *C_plusBp = 0;
  double *Weightsp = 0;

  A.ExtractView(&Ap);
  sqrtA.ExtractView(&sqrtAp);
  B.ExtractView(&Bp);
  C.ExtractView(&Cp);
  C_alphaA.ExtractView(&C_alphaAp);
  C_alphaAplusB.ExtractView(&C_alphaAplusBp);
  C_plusB.ExtractView(&C_plusBp);
  Weights.ExtractView(&Weightsp);
  
  bool A_is_local = (A.MyLength() == A.GlobalLength());
  bool B_is_local = (B.MyLength() == B.GlobalLength());
  bool C_is_local = (C.MyLength()    == C.GlobalLength());

  int A_IndexBase = A.Map().IndexBase();
  int B_IndexBase = B.Map().IndexBase();
  
    // Build two new maps that we can use for defining global equation indices below
    Epetra_Map * A_Map = new Epetra_Map(-1, A_nrows, A_IndexBase, A.Map().Comm());
    Epetra_Map * B_Map = new Epetra_Map(-1, B_nrows, B_IndexBase, B.Map().Comm());

    int* A_MyGlobalElements = new int[A_nrows];
    A_Map->MyGlobalElements(A_MyGlobalElements);
    int* B_MyGlobalElements = new int[B_nrows];
    B_Map->MyGlobalElements(B_MyGlobalElements);

  // Check for compatible dimensions
  
  if (C.MyLength()        != A_nrows     ||
      A_nrows        != B_nrows     ||
      C.NumVectors()    != A_ncols     ||
      A_ncols        != B_ncols     ||
      sqrtA_nrows    != A_nrows     ||
      sqrtA_ncols    != A_ncols     ||
      C.MyLength()        != C_alphaA.MyLength()     ||
      C.NumVectors()    != C_alphaA.NumVectors() ||
      C.MyLength()        != C_alphaAplusB.MyLength()     ||
      C.NumVectors()    != C_alphaAplusB.NumVectors() ||
      C.MyLength()        != C_plusB.MyLength()      ||
      C.NumVectors()    != C_plusB.NumVectors()     ) return(-2); // Return error
  
 
  bool Case1 = ( A_is_local &&  B_is_local &&  C_is_local);  // Case 1
  bool Case2 = (!A_is_local && !B_is_local && !C_is_local);// Case 2
  
  // Test for meaningful cases
  
  if (!(Case1 || Case2)) return(-3); // Meaningless case
  
  /* Fill A and B with values as follows:
     
     If A_is_local is false:
     A(i,j) = A_MyGlobalElements[i]*j,     i=1,...,numLocalEquations, j=1,...,NumVectors
     
     else
     A(i,j) = i*j,     i=1,...,numLocalEquations, j=1,...,NumVectors

     If B_is_local is false:
     B(i,j) = 1/(A_MyGlobalElements[i]*j), i=1,...,numLocalEquations, j=1,...,NumVectors

     else
     B(i,j) = 1/(i*j), i=1,...,numLocalEquations, j=1,...,NumVectors

     In addition, scale each entry by GlobalLength for A and
     1/GlobalLength for B--keeps the magnitude of entries in check
  */

  //Define scale factor

  double sf = A.GlobalLength();
  double sfinv = 1.0/sf;

  // Define A

  if (A_is_local) {
    for (i = 0; i<A_nrows; i++) { 
      fi = i+1; // Get float version of i and j, offset by 1.
      Ap[i] = (fi*sfinv);
      sqrtAp[i] = sqrt(Ap[i]);
    }
  }
  else {
    for (i = 0; i<A_nrows; i++) { 
      fi = A_MyGlobalElements[i]+1; // Get float version of i and j, offset by 1.
      Ap[i] = (fi*sfinv);
      sqrtAp[i] = sqrt(Ap[i]);
    }
  }

  // Define B depending on TransB and B_is_local
  
  if (B_is_local) {
    for (i = 0; i<B_nrows; i++) { 
      fi = i+1; // Get float version of i and j, offset by 1.
      Bp[i] = 1.0/((fi*sfinv));
	    }
  }
  else {
    for (i = 0; i<B_nrows; i++) { 
      fi = B_MyGlobalElements[i]+1; // Get float version of i and j, offset by 1.
      Bp[i] = 1.0/((fi*sfinv));
    }
  }
  
  // Generate C_alphaA = alpha * A

  for (i = 0; i<A_nrows; i++) C_alphaAp[i] = alpha * Ap[i];

  // Generate C_alphaA = alpha * A + B

    for (i = 0; i<A_nrows; i++) C_alphaAplusBp[i] = alpha * Ap[i] + Bp[i];
  
  // Generate C_plusB = this + B

    for (i = 0; i<A_nrows; i++) C_plusBp[i] = Cp[i] + Bp[i];

  // Generate dotvec_AB.  Because B(i,j) = 1/A(i,j), dotvec[i] =  C.GlobalLength()

  for (i=0; i< A.NumVectors(); i++) dotvec_AB[i] = C.GlobalLength();

  // For the next two results we want to be careful how we do arithmetic 
  // to avoid very large numbers.
  // We are computing sfinv*(C.GlobalLength()*(C.GlobalLength()+1)/2)

      double result = C.GlobalLength();
      result *= sfinv;
      result /= 2.0;
      result *= (double)(C.GlobalLength()+1);

   // Generate norm1_A.  Can use formula for sum of first n integers.

  for (i=0; i< A.NumVectors(); i++) 
    // m1_A[i] = (i+1)*C.GlobalLength()*(C.GlobalLength()+1)/2;
    norm1_A[i] = result * ((double) (i+1));

  // Generate norm2_sqrtA.  Can use formula for sum of first n integers. 

  for (i=0; i< A.NumVectors(); i++) 
    // norm2_sqrtA[i] = sqrt((double) ((i+1)*C.GlobalLength()*(C.GlobalLength()+1)/2));
    norm2_sqrtA[i] = sqrt(result * ((double) (i+1)));

  // Generate norminf_A, minval_A, maxval_A, meanval_A. 

  for (i=0; i< A.NumVectors(); i++) 
    {
    norminf_A[i] = (double) (i+1);
    minval_A[i] =  (double) (i+1)/ (double) A.GlobalLength();
    maxval_A[i] = (double) (i+1);
    meanval_A[i] = norm1_A[i]/((double) (A.GlobalLength()));
    }

  // Define weights and expected weighted norm
      normw_A[0] = 1;
      for (j=0; j<A_nrows; j++) Weightsp[j] = Ap[j];

  delete A_Map;
  delete B_Map;
  delete [] A_MyGlobalElements;
  delete [] B_MyGlobalElements;
  
  return(0);
}

//========================================================================
