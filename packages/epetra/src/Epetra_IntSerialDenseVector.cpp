
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

#include "Epetra_IntSerialDenseVector.h"

//=============================================================================
Epetra_IntSerialDenseVector::Epetra_IntSerialDenseVector()
  : Epetra_IntSerialDenseMatrix()
{
	SetLabel("Epetra::IntSerialDenseVector");
}

//=============================================================================
Epetra_IntSerialDenseVector::Epetra_IntSerialDenseVector(int Length)
  : Epetra_IntSerialDenseMatrix(Length, 1)
{
	SetLabel("Epetra::IntSerialDenseVector");
}

//=============================================================================
Epetra_IntSerialDenseVector::Epetra_IntSerialDenseVector(Epetra_DataAccess CV, int* Values, int Length)
  : Epetra_IntSerialDenseMatrix(CV, Values, Length, Length, 1)
{
	SetLabel("Epetra::IntSerialDenseVector");
}

//=============================================================================
Epetra_IntSerialDenseVector::Epetra_IntSerialDenseVector(const Epetra_IntSerialDenseVector& Source)
  : Epetra_IntSerialDenseMatrix(Source)
{}

//=============================================================================
Epetra_IntSerialDenseVector::~Epetra_IntSerialDenseVector()
{}

//=========================================================================
Epetra_IntSerialDenseVector& Epetra_IntSerialDenseVector::operator = (const Epetra_IntSerialDenseVector& Source) {
	Epetra_IntSerialDenseMatrix::operator=(Source); // call this->Epetra_IntSerialDenseMatrix::operator =
	return(*this);
}

//=============================================================================
int Epetra_IntSerialDenseVector::MakeViewOf(const Epetra_IntSerialDenseVector& Source) {
	int errorcode = Epetra_IntSerialDenseMatrix::MakeViewOf(Source);
	return(errorcode);
}

//=========================================================================
void Epetra_IntSerialDenseVector::Print(ostream& os) const {
	if(CV_ == Copy)
		os << "Data access mode: Copy" << endl;
	else
		os << "Data access mode: View" << endl;
	if(A_Copied_)
		os << "A_Copied: yes" << endl;
	else
		os << "A_Copied: no" << endl;
	os << "Length(M): " << M_ << endl;
	if(M_ == 0)
		os << "(vector is empty, no values to display)";
	else
		for(int i = 0; i < M_; i++)
      os << (*this)(i) << " ";
	os << endl;
}

//=========================================================================
int Epetra_IntSerialDenseVector::Random() {
	int errorcode = Epetra_IntSerialDenseMatrix::Random();
	return(errorcode);
}
