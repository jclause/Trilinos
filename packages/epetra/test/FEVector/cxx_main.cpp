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

// Epetra_FEVector Test routine

#include "Epetra_Time.h"
#include "Epetra_BlockMap.h"
#include "Epetra_FEVector.h"
#include "ExecuteTestProblems.h"
#ifdef EPETRA_MPI
#include "Epetra_MpiComm.h"
#include <mpi.h>
#else
#include "Epetra_SerialComm.h"
#endif
#include "../epetra_test_err.h"

int main(int argc, char *argv[]) {

  int ierr = 0;

#ifdef EPETRA_MPI

  // Initialize MPI

  MPI_Init(&argc,&argv);
  int size, rank; // Number of MPI processes, My process ID

  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  Epetra_MpiComm Comm(MPI_COMM_WORLD);

#else

  int size = 1; // Serial case (not using MPI)
  int rank = 0;
  Epetra_SerialComm Comm;

#endif

  Comm.SetTracebackMode(0); // This should shut down any error tracing
  bool verbose = false;

  // Check if we should print results to standard out
  if (argc>1) if (argv[1][0]=='-' && argv[1][1]=='v') verbose = true;

#ifdef EPETRA_MPI
  int localverbose = verbose ? 1 : 0;
  int globalverbose=0;
  MPI_Allreduce(&localverbose, &globalverbose, 1, MPI_INT, MPI_SUM,
		MPI_COMM_WORLD);
  verbose = (globalverbose>0);
#endif

  //  char tmp;
  //  if (rank==0) cout << "Press any key to continue..."<< endl;
  //  if (rank==0) cin >> tmp;
  //  Comm.Barrier();

  int MyPID = Comm.MyPID();
  int NumProc = Comm.NumProc(); 
  if (verbose) cout << Comm <<endl;

  // Redefine verbose to only print on PE 0
  //if (verbose && rank!=0) verbose = false;

  int NumVectors = 1;
  int NumMyElements = 4;
  int NumGlobalElements = NumMyElements*NumProc;
  int IndexBase = 0;
  int ElementSize = 1;
  
  if (verbose) {
    cout << "\n*********************************************************"<<endl;
    cout << "Checking Epetra_BlockMap(NumGlobalElements, NumMyElements, ElementSize, IndexBase, Comm)" << endl;
    cout << "*********************************************************" << endl;
  }

  Epetra_BlockMap BlockMap(NumGlobalElements, NumMyElements,
                           ElementSize, IndexBase, Comm);
  EPETRA_TEST_ERR(MultiVectorTests(BlockMap, NumVectors, verbose),ierr);

  Comm.Barrier();
  if (verbose)cout << endl;
  Comm.Barrier();

  EPETRA_TEST_ERR( fevec1(Comm, verbose), ierr);

  Comm.Barrier();
  if (verbose)cout << endl;
  Comm.Barrier();

  EPETRA_TEST_ERR( fevec2(Comm, verbose), ierr);

#ifdef EPETRA_MPI
  MPI_Finalize();
#endif

  return ierr;
}

