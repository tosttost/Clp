// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.


#include <cstdio>

#include "CoinPragma.hpp"
#include "CoinIndexedVector.hpp"
#include "CoinHelperFunctions.hpp"

#include "ClpSimplex.hpp"
#include "ClpFactorization.hpp"
#include "ClpQuadraticObjective.hpp"
// at end to get min/max!
#include "ClpPackedMatrix.hpp"
#include "ClpMessage.hpp"

//#############################################################################
// Constructors / Destructor / Assignment
//#############################################################################

//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
ClpPackedMatrix::ClpPackedMatrix () 
  : ClpMatrixBase(),
    matrix_(NULL),
    numberActiveColumns_(0),
    zeroElements_(false),
    hasGaps_(true)
{
  setType(1);
}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
ClpPackedMatrix::ClpPackedMatrix (const ClpPackedMatrix & rhs) 
: ClpMatrixBase(rhs)
{  
  matrix_ = new CoinPackedMatrix(*(rhs.matrix_));
  numberActiveColumns_ = rhs.numberActiveColumns_;
  zeroElements_ = rhs.zeroElements_;
  hasGaps_ = rhs.hasGaps_;
  int numberRows = getNumRows();
  if (rhs.rhsOffset_&&numberRows) {
    rhsOffset_ = ClpCopyOfArray(rhs.rhsOffset_,numberRows);
  } else {
    rhsOffset_=NULL;
  }
  
}

//-------------------------------------------------------------------
// assign matrix (for space reasons)
//-------------------------------------------------------------------
ClpPackedMatrix::ClpPackedMatrix (CoinPackedMatrix * rhs) 
: ClpMatrixBase()
{  
  matrix_ = rhs;
  zeroElements_ = false;
  hasGaps_ = true;
  numberActiveColumns_ = matrix_->getNumCols();
  setType(1);
  
}

ClpPackedMatrix::ClpPackedMatrix (const CoinPackedMatrix & rhs) 
: ClpMatrixBase()
{  
  matrix_ = new CoinPackedMatrix(rhs);
  numberActiveColumns_ = matrix_->getNumCols();
  zeroElements_ = false;
  hasGaps_ = true;
  setType(1);
  
}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
ClpPackedMatrix::~ClpPackedMatrix ()
{
  delete matrix_;
}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
ClpPackedMatrix &
ClpPackedMatrix::operator=(const ClpPackedMatrix& rhs)
{
  if (this != &rhs) {
    ClpMatrixBase::operator=(rhs);
    delete matrix_;
    matrix_ = new CoinPackedMatrix(*(rhs.matrix_));
    numberActiveColumns_ = rhs.numberActiveColumns_;
    zeroElements_ = rhs.zeroElements_;
    hasGaps_ = rhs.hasGaps_;
  }
  return *this;
}
//-------------------------------------------------------------------
// Clone
//-------------------------------------------------------------------
ClpMatrixBase * ClpPackedMatrix::clone() const
{
  return new ClpPackedMatrix(*this);
}
/* Subset clone (without gaps).  Duplicates are allowed
   and order is as given */
ClpMatrixBase * 
ClpPackedMatrix::subsetClone (int numberRows, const int * whichRows,
			      int numberColumns, 
			      const int * whichColumns) const 
{
  return new ClpPackedMatrix(*this, numberRows, whichRows,
				   numberColumns, whichColumns);
}
/* Subset constructor (without gaps).  Duplicates are allowed
   and order is as given */
ClpPackedMatrix::ClpPackedMatrix (
		       const ClpPackedMatrix & rhs,
		       int numberRows, const int * whichRows,
		       int numberColumns, const int * whichColumns)
: ClpMatrixBase(rhs)
{
  matrix_ = new CoinPackedMatrix(*(rhs.matrix_),numberRows,whichRows,
				 numberColumns,whichColumns);
  numberActiveColumns_ = matrix_->getNumCols();
  zeroElements_ = rhs.zeroElements_;
  hasGaps_ = rhs.hasGaps_;
}
ClpPackedMatrix::ClpPackedMatrix (
		       const CoinPackedMatrix & rhs,
		       int numberRows, const int * whichRows,
		       int numberColumns, const int * whichColumns)
: ClpMatrixBase()
{
  matrix_ = new CoinPackedMatrix(rhs,numberRows,whichRows,
				 numberColumns,whichColumns);
  numberActiveColumns_ = matrix_->getNumCols();
  zeroElements_ = false;
  hasGaps_=true;
  setType(1);
}

/* Returns a new matrix in reverse order without gaps */
ClpMatrixBase * 
ClpPackedMatrix::reverseOrderedCopy() const
{
  ClpPackedMatrix * copy = new ClpPackedMatrix();
  copy->matrix_= new CoinPackedMatrix();
  copy->matrix_->setExtraGap(0.0);
  copy->matrix_->setExtraMajor(0.0);
  copy->matrix_->reverseOrderedCopyOf(*matrix_);
  //copy->matrix_->removeGaps();
  copy->numberActiveColumns_ = copy->matrix_->getNumCols();
  copy->hasGaps_=false;
  return copy;
}
//unscaled versions
void 
ClpPackedMatrix::times(double scalar,
		   const double * x, double * y) const
{
  int iRow,iColumn;
  // get matrix data pointers
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  //memset(y,0,matrix_->getNumRows()*sizeof(double));
  for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
    CoinBigIndex j;
    double value = scalar*x[iColumn];
    if (value) {
      for (j=columnStart[iColumn];
	   j<columnStart[iColumn]+columnLength[iColumn];j++) {
	iRow=row[j];
	y[iRow] += value*elementByColumn[j];
      }
    }
  }
}
void 
ClpPackedMatrix::transposeTimes(double scalar,
				const double * x, double * y) const
{
  int iColumn;
  // get matrix data pointers
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  if (!hasGaps_) {
    if (scalar==1.0) {
      CoinBigIndex start=columnStart[0];
      for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	CoinBigIndex j;
	CoinBigIndex next=columnStart[iColumn+1];
	double value=y[iColumn];
	for (j=start;j<next;j++) {
	  int jRow=row[j];
	  value += x[jRow]*elementByColumn[j];
	}
	start=next;
	y[iColumn] = value;
      }
    } else if (scalar==-1.0) {
      CoinBigIndex start=columnStart[0];
      for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	CoinBigIndex j;
	CoinBigIndex next=columnStart[iColumn+1];
	double value=y[iColumn];
	for (j=start;j<next;j++) {
	  int jRow=row[j];
	  value -= x[jRow]*elementByColumn[j];
	}
	start=next;
	y[iColumn] = value;
      }
    } else {
      CoinBigIndex start=columnStart[0];
      for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	CoinBigIndex j;
	CoinBigIndex next=columnStart[iColumn+1];
	double value=0.0;
	for (j=start;j<next;j++) {
	  int jRow=row[j];
	  value += x[jRow]*elementByColumn[j];
	}
	start=next;
	y[iColumn] += value*scalar;
      }
    }
  } else {
    for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
      CoinBigIndex j;
      double value=0.0;
      for (j=columnStart[iColumn];
	   j<columnStart[iColumn]+columnLength[iColumn];j++) {
	int jRow=row[j];
	value += x[jRow]*elementByColumn[j];
      }
      y[iColumn] += value*scalar;
    }
  }
}
void 
ClpPackedMatrix::times(double scalar,
		       const double * x, double * y,
		       const double * rowScale, 
		       const double * columnScale) const
{
  if (rowScale) {
    int iRow,iColumn;
    // get matrix data pointers
    const int * row = matrix_->getIndices();
    const CoinBigIndex * columnStart = matrix_->getVectorStarts();
    const int * columnLength = matrix_->getVectorLengths(); 
    const double * elementByColumn = matrix_->getElements();
    for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
      CoinBigIndex j;
      double value = x[iColumn];
      if (value) {
	// scaled
	value *= scalar*columnScale[iColumn];
	for (j=columnStart[iColumn];
	     j<columnStart[iColumn]+columnLength[iColumn];j++) {
	  iRow=row[j];
	  y[iRow] += value*elementByColumn[j]*rowScale[iRow];
	}
      }
    }
  } else {
    times(scalar,x,y);
  }
}
void 
ClpPackedMatrix::transposeTimes( double scalar,
				 const double * x, double * y,
				 const double * rowScale, 
				 const double * columnScale,
				 double * spare) const
{
  if (rowScale) {
    int iColumn;
    // get matrix data pointers
    const int * row = matrix_->getIndices();
    const CoinBigIndex * columnStart = matrix_->getVectorStarts();
    const int * columnLength = matrix_->getVectorLengths(); 
    const double * elementByColumn = matrix_->getElements();
    if (!spare) {
      if (!hasGaps_) {
	CoinBigIndex start=columnStart[0];
	for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	  CoinBigIndex j;
	  CoinBigIndex next=columnStart[iColumn+1];
	  double value=0.0;
	  // scaled
	  for (j=start;j<next;j++) {
	    int jRow=row[j];
	    value += x[jRow]*elementByColumn[j]*rowScale[jRow];
	  }
	  start=next;
	  y[iColumn] += value*scalar*columnScale[iColumn];
	}
      } else {
	for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	  CoinBigIndex j;
	  double value=0.0;
	  // scaled
	  for (j=columnStart[iColumn];
	       j<columnStart[iColumn]+columnLength[iColumn];j++) {
	    int jRow=row[j];
	    value += x[jRow]*elementByColumn[j]*rowScale[jRow];
	  }
	  y[iColumn] += value*scalar*columnScale[iColumn];
	}
      }
    } else {
      // can use spare region
      int iRow;
      int numberRows = getNumRows();
      for (iRow=0;iRow<numberRows;iRow++)
	spare[iRow] = x[iRow]*rowScale[iRow];
      if (!hasGaps_) {
	CoinBigIndex start=columnStart[0];
	for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	  CoinBigIndex j;
	  CoinBigIndex next=columnStart[iColumn+1];
	  double value=0.0;
	  // scaled
	  for (j=start;j<next;j++) {
	    int jRow=row[j];
	    value += spare[jRow]*elementByColumn[j];
	  }
	  start=next;
	  y[iColumn] += value*scalar*columnScale[iColumn];
	}
      } else {
	for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	  CoinBigIndex j;
	  double value=0.0;
	  // scaled
	  for (j=columnStart[iColumn];
	       j<columnStart[iColumn]+columnLength[iColumn];j++) {
	    int jRow=row[j];
	    value += spare[jRow]*elementByColumn[j];
	  }
	  y[iColumn] += value*scalar*columnScale[iColumn];
	}
      }
      // no need to zero out
      //for (iRow=0;iRow<numberRows;iRow++)
      //spare[iRow] = 0.0;
    }
  } else {
    transposeTimes(scalar,x,y);
  }
}
/* Return <code>x * A + y</code> in <code>z</code>. 
	Squashes small elements and knows about ClpSimplex */
void 
ClpPackedMatrix::transposeTimes(const ClpSimplex * model, double scalar,
			      const CoinIndexedVector * rowArray,
			      CoinIndexedVector * y,
			      CoinIndexedVector * columnArray) const
{
  columnArray->clear();
  double * pi = rowArray->denseVector();
  int numberNonZero=0;
  int * index = columnArray->getIndices();
  double * array = columnArray->denseVector();
  int numberInRowArray = rowArray->getNumElements();
  // maybe I need one in OsiSimplex
  double zeroTolerance = model->factorization()->zeroTolerance();
  int numberRows = model->numberRows();
#ifndef NO_RTTI
  ClpPackedMatrix* rowCopy =
    dynamic_cast< ClpPackedMatrix*>(model->rowCopy());
#else
  ClpPackedMatrix* rowCopy =
    static_cast< ClpPackedMatrix*>(model->rowCopy());
#endif
  bool packed = rowArray->packedMode();
  double factor = 0.27;
  // We may not want to do by row if there may be cache problems
  // It would be nice to find L2 cache size - for moment 512K
  // Be slightly optimistic
  if (numberActiveColumns_*sizeof(double)>1000000) {
    if (numberRows*10<numberActiveColumns_)
      factor *= 0.333333333;
    else if (numberRows*4<numberActiveColumns_)
      factor *= 0.5;
    else if (numberRows*2<numberActiveColumns_)
      factor *= 0.66666666667;
    //if (model->numberIterations()%50==0)
    //printf("%d nonzero\n",numberInRowArray);
  }
  // if not packed then bias a bit more towards by column
  if (!packed)
    factor *= 0.9;
  assert (!y->getNumElements());
  if (numberInRowArray>factor*numberRows||!rowCopy) {
    // do by column
    // If no gaps - can do a bit faster
    if (!hasGaps_) {
      transposeTimesByColumn( model,  scalar,
			      rowArray, y, columnArray);
      return;
    }
    int iColumn;
    // get matrix data pointers
    const int * row = matrix_->getIndices();
    const CoinBigIndex * columnStart = matrix_->getVectorStarts();
    const int * columnLength = matrix_->getVectorLengths(); 
    const double * elementByColumn = matrix_->getElements();
    const double * rowScale = model->rowScale();
    if (packed) {
      // need to expand pi into y
      assert(y->capacity()>=numberRows);
      double * piOld = pi;
      pi = y->denseVector();
      const int * whichRow = rowArray->getIndices();
      int i;
      if (!rowScale) {
	// modify pi so can collapse to one loop
	for (i=0;i<numberInRowArray;i++) {
	  int iRow = whichRow[i];
	  pi[iRow]=scalar*piOld[i];
	}
	for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	  double value = 0.0;
	  CoinBigIndex j;
	  for (j=columnStart[iColumn];
	       j<columnStart[iColumn]+columnLength[iColumn];j++) {
	    int iRow = row[j];
	    value += pi[iRow]*elementByColumn[j];
	  }
	  if (fabs(value)>zeroTolerance) {
	    array[numberNonZero]=value;
	    index[numberNonZero++]=iColumn;
	  }
	}
      } else {
	// scaled
	// modify pi so can collapse to one loop
	for (i=0;i<numberInRowArray;i++) {
	  int iRow = whichRow[i];
	  pi[iRow]=scalar*piOld[i]*rowScale[iRow];
	}
	for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	  double value = 0.0;
	  CoinBigIndex j;
	  const double * columnScale = model->columnScale();
	  for (j=columnStart[iColumn];
	       j<columnStart[iColumn]+columnLength[iColumn];j++) {
	    int iRow = row[j];
	    value += pi[iRow]*elementByColumn[j];
	  }
	  value *= columnScale[iColumn];
	  if (fabs(value)>zeroTolerance) {
	    array[numberNonZero]=value;
	    index[numberNonZero++]=iColumn;
	  }
	}
      }
      // zero out
      for (i=0;i<numberInRowArray;i++) {
	int iRow = whichRow[i];
	pi[iRow]=0.0;
      }
    } else {
      if (!rowScale) {
	if (scalar==-1.0) {
	  for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	    double value = 0.0;
	    CoinBigIndex j;
	    for (j=columnStart[iColumn];
		 j<columnStart[iColumn]+columnLength[iColumn];j++) {
	      int iRow = row[j];
	      value += pi[iRow]*elementByColumn[j];
	    }
	    if (fabs(value)>zeroTolerance) {
	      index[numberNonZero++]=iColumn;
	      array[iColumn]=-value;
	    }
	  }
	} else if (scalar==1.0) {
	  for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	    double value = 0.0;
	    CoinBigIndex j;
	    for (j=columnStart[iColumn];
		 j<columnStart[iColumn]+columnLength[iColumn];j++) {
	      int iRow = row[j];
	      value += pi[iRow]*elementByColumn[j];
	    }
	    if (fabs(value)>zeroTolerance) {
	      index[numberNonZero++]=iColumn;
	      array[iColumn]=value;
	    }
	  }
	} else {
	  for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	    double value = 0.0;
	    CoinBigIndex j;
	    for (j=columnStart[iColumn];
		 j<columnStart[iColumn]+columnLength[iColumn];j++) {
	      int iRow = row[j];
	      value += pi[iRow]*elementByColumn[j];
	    }
	    value *= scalar;
	    if (fabs(value)>zeroTolerance) {
	      index[numberNonZero++]=iColumn;
	      array[iColumn]=value;
	    }
	  }
	}
      } else {
	// scaled
	if (scalar==-1.0) {
	  for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	    double value = 0.0;
	    CoinBigIndex j;
	    const double * columnScale = model->columnScale();
	    for (j=columnStart[iColumn];
		 j<columnStart[iColumn]+columnLength[iColumn];j++) {
	      int iRow = row[j];
	      value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	    }
	    value *= columnScale[iColumn];
	    if (fabs(value)>zeroTolerance) {
	      index[numberNonZero++]=iColumn;
	      array[iColumn]=-value;
	    }
	  }
	} else if (scalar==1.0) {
	  for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	    double value = 0.0;
	    CoinBigIndex j;
	    const double * columnScale = model->columnScale();
	    for (j=columnStart[iColumn];
		 j<columnStart[iColumn]+columnLength[iColumn];j++) {
	      int iRow = row[j];
	      value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	    }
	    value *= columnScale[iColumn];
	    if (fabs(value)>zeroTolerance) {
	      index[numberNonZero++]=iColumn;
	      array[iColumn]=value;
	    }
	  }
	} else {
	  for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	    double value = 0.0;
	    CoinBigIndex j;
	    const double * columnScale = model->columnScale();
	    for (j=columnStart[iColumn];
		 j<columnStart[iColumn]+columnLength[iColumn];j++) {
	      int iRow = row[j];
	      value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	    }
	    value *= scalar*columnScale[iColumn];
	    if (fabs(value)>zeroTolerance) {
	      index[numberNonZero++]=iColumn;
	      array[iColumn]=value;
	    }
	  }
	}
      }
    }
    columnArray->setNumElements(numberNonZero);
    y->setNumElements(0);
  } else {
    // do by row
    rowCopy->transposeTimesByRow(model, scalar, rowArray, y, columnArray);
  }
  if (packed)
    columnArray->setPackedMode(true);
  if (0) {
    columnArray->checkClean();
    int numberNonZero=columnArray->getNumElements();;
    int * index = columnArray->getIndices();
    double * array = columnArray->denseVector();
    int i;
    for (i=0;i<numberNonZero;i++) {
      int j=index[i];
      double value;
      if (packed)
	value=array[i];
      else
	value=array[j];
      printf("Ti %d %d %g\n",i,j,value);
    }
  }
}
/* Return <code>x * scalar * A + y</code> in <code>z</code>. 
   Note - If x packed mode - then z packed mode
   This does by column and knows no gaps
   Squashes small elements and knows about ClpSimplex */
void 
ClpPackedMatrix::transposeTimesByColumn(const ClpSimplex * model, double scalar,
					const CoinIndexedVector * rowArray,
					CoinIndexedVector * y,
					CoinIndexedVector * columnArray) const
{
  double * pi = rowArray->denseVector();
  int numberNonZero=0;
  int * index = columnArray->getIndices();
  double * array = columnArray->denseVector();
  int numberInRowArray = rowArray->getNumElements();
  // maybe I need one in OsiSimplex
  double zeroTolerance = model->factorization()->zeroTolerance();
  bool packed = rowArray->packedMode();
  // do by column
  int iColumn;
  // get matrix data pointers
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const double * elementByColumn = matrix_->getElements();
  const double * rowScale = model->rowScale();
  assert (!y->getNumElements());
  assert (numberActiveColumns_>0);
  if (packed) {
    // need to expand pi into y
    assert(y->capacity()>=model->numberRows());
    double * piOld = pi;
    pi = y->denseVector();
    const int * whichRow = rowArray->getIndices();
    int i;
    if (!rowScale) {
      // modify pi so can collapse to one loop
      for (i=0;i<numberInRowArray;i++) {
	int iRow = whichRow[i];
	pi[iRow]=scalar*piOld[i];
      }
      double value = 0.0;
      CoinBigIndex j;
      CoinBigIndex end = columnStart[1];
      for (j=columnStart[0]; j<end;j++) {
	int iRow = row[j];
	value += pi[iRow]*elementByColumn[j];
      }
      for (iColumn=0;iColumn<numberActiveColumns_-1;iColumn++) {
	CoinBigIndex start = end;
	end = columnStart[iColumn+2];
	if (fabs(value)>zeroTolerance) {
	  array[numberNonZero]=value;
	  index[numberNonZero++]=iColumn;
	}
	value = 0.0;
	for (j=start; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j];
	}
      }
      if (fabs(value)>zeroTolerance) {
	array[numberNonZero]=value;
	index[numberNonZero++]=iColumn;
      }
    } else {
      // scaled
      // modify pi so can collapse to one loop
      for (i=0;i<numberInRowArray;i++) {
	int iRow = whichRow[i];
	pi[iRow]=scalar*piOld[i]*rowScale[iRow];
      }
      const double * columnScale = model->columnScale();
      double value = 0.0;
      double scale=columnScale[0];
      CoinBigIndex j;
      CoinBigIndex end = columnStart[1];
      for (j=columnStart[0]; j<end;j++) {
	int iRow = row[j];
	value += pi[iRow]*elementByColumn[j];
      }
      for (iColumn=0;iColumn<numberActiveColumns_-1;iColumn++) {
	value *= scale;
	CoinBigIndex start = end;
	scale = columnScale[iColumn+1];
	end = columnStart[iColumn+2];
	if (fabs(value)>zeroTolerance) {
	  array[numberNonZero]=value;
	  index[numberNonZero++]=iColumn;
	}
	value = 0.0;
	for (j=start; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j];
	}
      }
      value *= scale;
      if (fabs(value)>zeroTolerance) {
	array[numberNonZero]=value;
	index[numberNonZero++]=iColumn;
      }
    }
    // zero out
    int numberRows = model->numberRows();
    if (numberInRowArray*4<numberRows) {
      for (i=0;i<numberInRowArray;i++) {
        int iRow = whichRow[i];
        pi[iRow]=0.0;
      }
    } else {
      CoinZeroN(pi,numberRows);
    }
  } else {
    if (!rowScale) {
      if (scalar==-1.0) {
	double value = 0.0;
	CoinBigIndex j;
	CoinBigIndex end = columnStart[1];
	for (j=columnStart[0]; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j];
	}
	for (iColumn=0;iColumn<numberActiveColumns_-1;iColumn++) {
	  CoinBigIndex start = end;
	  end = columnStart[iColumn+2];
	  if (fabs(value)>zeroTolerance) {
	    array[iColumn]=-value;
	    index[numberNonZero++]=iColumn;
	  }
	  value = 0.0;
	  for (j=start; j<end;j++) {
	    int iRow = row[j];
	    value += pi[iRow]*elementByColumn[j];
	  }
	}
	if (fabs(value)>zeroTolerance) {
	  array[iColumn]=-value;
	  index[numberNonZero++]=iColumn;
	}
      } else if (scalar==1.0) {
	double value = 0.0;
	CoinBigIndex j;
	CoinBigIndex end = columnStart[1];
	for (j=columnStart[0]; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j];
	}
	for (iColumn=0;iColumn<numberActiveColumns_-1;iColumn++) {
	  CoinBigIndex start = end;
	  end = columnStart[iColumn+2];
	  if (fabs(value)>zeroTolerance) {
	    array[iColumn]=value;
	    index[numberNonZero++]=iColumn;
	  }
	  value = 0.0;
	  for (j=start; j<end;j++) {
	    int iRow = row[j];
	    value += pi[iRow]*elementByColumn[j];
	  }
	}
	if (fabs(value)>zeroTolerance) {
	  array[iColumn]=value;
	  index[numberNonZero++]=iColumn;
	}
      } else {
	double value = 0.0;
	CoinBigIndex j;
	CoinBigIndex end = columnStart[1];
	for (j=columnStart[0]; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j];
	}
	for (iColumn=0;iColumn<numberActiveColumns_-1;iColumn++) {
	  value *= scalar;
	  CoinBigIndex start = end;
	  end = columnStart[iColumn+2];
	  if (fabs(value)>zeroTolerance) {
	    array[iColumn]=value;
	    index[numberNonZero++]=iColumn;
	  }
	  value = 0.0;
	  for (j=start; j<end;j++) {
	    int iRow = row[j];
	    value += pi[iRow]*elementByColumn[j];
	  }
	}
	value *= scalar;
	if (fabs(value)>zeroTolerance) {
	  array[iColumn]=value;
	  index[numberNonZero++]=iColumn;
	}
      }
    } else {
      // scaled
      if (scalar==-1.0) {
	const double * columnScale = model->columnScale();
	double value = 0.0;
	double scale=columnScale[0];
	CoinBigIndex j;
	CoinBigIndex end = columnStart[1];
	for (j=columnStart[0]; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	}
	for (iColumn=0;iColumn<numberActiveColumns_-1;iColumn++) {
	  value *= scale;
	  CoinBigIndex start = end;
	  end = columnStart[iColumn+2];
	  scale = columnScale[iColumn+1];
	  if (fabs(value)>zeroTolerance) {
	    array[iColumn]=-value;
	    index[numberNonZero++]=iColumn;
	  }
	  value = 0.0;
	  for (j=start; j<end;j++) {
	    int iRow = row[j];
	    value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	  }
	}
	value *= scale;
	if (fabs(value)>zeroTolerance) {
	  array[iColumn]=-value;
	  index[numberNonZero++]=iColumn;
	}
      } else if (scalar==1.0) {
	const double * columnScale = model->columnScale();
	double value = 0.0;
	double scale=columnScale[0];
	CoinBigIndex j;
	CoinBigIndex end = columnStart[1];
	for (j=columnStart[0]; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	}
	for (iColumn=0;iColumn<numberActiveColumns_-1;iColumn++) {
	  value *= scale;
	  CoinBigIndex start = end;
	  end = columnStart[iColumn+2];
	  scale = columnScale[iColumn+1];
	  if (fabs(value)>zeroTolerance) {
	    array[iColumn]=value;
	    index[numberNonZero++]=iColumn;
	  }
	  value = 0.0;
	  for (j=start; j<end;j++) {
	    int iRow = row[j];
	    value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	  }
	}
	value *= scale;
	if (fabs(value)>zeroTolerance) {
	  array[iColumn]=value;
	  index[numberNonZero++]=iColumn;
	}
      } else {
	const double * columnScale = model->columnScale();
	double value = 0.0;
	double scale=columnScale[0]*scalar;
	CoinBigIndex j;
	CoinBigIndex end = columnStart[1];
	for (j=columnStart[0]; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	}
	for (iColumn=0;iColumn<numberActiveColumns_-1;iColumn++) {
	  value *= scale;
	  CoinBigIndex start = end;
	  end = columnStart[iColumn+2];
	  scale = columnScale[iColumn+1]*scalar;
	  if (fabs(value)>zeroTolerance) {
	    array[iColumn]=value;
	    index[numberNonZero++]=iColumn;
	  }
	  value = 0.0;
	  for (j=start; j<end;j++) {
	    int iRow = row[j];
	    value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	  }
	}
	value *= scale;
	if (fabs(value)>zeroTolerance) {
	  array[iColumn]=value;
	  index[numberNonZero++]=iColumn;
	}
      }
    }
  }
  columnArray->setNumElements(numberNonZero);
  y->setNumElements(0);
  if (packed)
    columnArray->setPackedMode(true);
}
/* Return <code>x * A + y</code> in <code>z</code>. 
	Squashes small elements and knows about ClpSimplex */
void 
ClpPackedMatrix::transposeTimesByRow(const ClpSimplex * model, double scalar,
			      const CoinIndexedVector * rowArray,
			      CoinIndexedVector * y,
			      CoinIndexedVector * columnArray) const
{
  columnArray->clear();
  double * pi = rowArray->denseVector();
  int numberNonZero=0;
  int * index = columnArray->getIndices();
  double * array = columnArray->denseVector();
  int numberInRowArray = rowArray->getNumElements();
  // maybe I need one in OsiSimplex
  double zeroTolerance = model->factorization()->zeroTolerance();
  const int * column = getIndices();
  const CoinBigIndex * rowStart = getVectorStarts();
  const double * element = getElements();
  const int * whichRow = rowArray->getIndices();
  bool packed = rowArray->packedMode();
  if (numberInRowArray>2) {
    // do by rows
    // ** Row copy is already scaled
    int iRow;
    int i;
    int numberOriginal = 0;
    if (packed) {
      numberNonZero=0;
      // and set up mark as char array
      char * marked = (char *) (index+columnArray->capacity());
      double * array2 = y->denseVector();
#ifdef CLP_DEBUG
      for (i=0;i<numberActiveColumns_;i++) {
	assert(!marked[i]);
	assert(!array2[i]);
      }
#endif      
      for (i=0;i<numberInRowArray;i++) {
	iRow = whichRow[i]; 
	double value = pi[i]*scalar;
	CoinBigIndex j;
	for (j=rowStart[iRow];j<rowStart[iRow+1];j++) {
	  int iColumn = column[j];
	  if (!marked[iColumn]) {
	    marked[iColumn]=1;
	    index[numberNonZero++]=iColumn;
	  }
	  array2[iColumn] += value*element[j];
	}
      }
      // get rid of tiny values and zero out marked
      numberOriginal=numberNonZero;
      numberNonZero=0;
      for (i=0;i<numberOriginal;i++) {
	int iColumn = index[i];
	if (marked[iColumn]) {
	  double value = array2[iColumn];
	  array2[iColumn]=0.0; 
	  marked[iColumn]=0;
	  if (fabs(value)>zeroTolerance) {
	    array[numberNonZero]=value;
	    index[numberNonZero++]=iColumn;
	  }
	}
      }
    } else {
      double * markVector = y->denseVector();
      numberNonZero=0;
      // and set up mark as char array
      char * marked = (char *) markVector;
      for (i=0;i<numberOriginal;i++) {
	int iColumn = index[i];
	marked[iColumn]=0;
      }
      
      for (i=0;i<numberInRowArray;i++) {
	iRow = whichRow[i]; 
	double value = pi[iRow]*scalar;
	CoinBigIndex j;
	for (j=rowStart[iRow];j<rowStart[iRow+1];j++) {
	  int iColumn = column[j];
	  if (!marked[iColumn]) {
	    marked[iColumn]=1;
	    index[numberNonZero++]=iColumn;
	  }
	  array[iColumn] += value*element[j];
	}
      }
      // get rid of tiny values and zero out marked
      numberOriginal=numberNonZero;
      numberNonZero=0;
      for (i=0;i<numberOriginal;i++) {
	int iColumn = index[i];
	marked[iColumn]=0;
	if (fabs(array[iColumn])>zeroTolerance) {
	  index[numberNonZero++]=iColumn;
	} else {
	  array[iColumn]=0.0;
	}
      }
    }
  } else if (numberInRowArray==2) {
    // do by rows when two rows
    int numberOriginal;
    int i;
    CoinBigIndex j;
    numberNonZero=0;

    double value;
    if (packed) {
      int iRow0 = whichRow[0]; 
      int iRow1 = whichRow[1]; 
      double pi0 = pi[0];
      double pi1 = pi[1];
      if (rowStart[iRow0+1]-rowStart[iRow0]>
	  rowStart[iRow1+1]-rowStart[iRow1]) {
	// do one with fewer first
	iRow0=iRow1;
	iRow1=whichRow[0];
	pi0=pi1;
	pi1=pi[0];
      }
      // and set up mark as char array
      char * marked = (char *) (index+columnArray->capacity());
      int * lookup = y->getIndices();
      value = pi0*scalar;
      for (j=rowStart[iRow0];j<rowStart[iRow0+1];j++) {
	int iColumn = column[j];
	double value2 = value*element[j];
	array[numberNonZero] = value2;
	marked[iColumn]=1;
	lookup[iColumn]=numberNonZero;
	index[numberNonZero++]=iColumn;
      }
      numberOriginal = numberNonZero;
      value = pi1*scalar;
      for (j=rowStart[iRow1];j<rowStart[iRow1+1];j++) {
	int iColumn = column[j];
	double value2 = value*element[j];
	// I am assuming no zeros in matrix
	if (marked[iColumn]) {
	  int iLookup = lookup[iColumn];
	  array[iLookup] += value2;
	} else {
	  if (fabs(value2)>zeroTolerance) {
	    array[numberNonZero] = value2;
	    index[numberNonZero++]=iColumn;
	  }
	}
      }
      // get rid of tiny values and zero out marked
      int nDelete=0;
      for (i=0;i<numberOriginal;i++) {
	int iColumn = index[i];
	marked[iColumn]=0;
	if (fabs(array[i])<=zeroTolerance) 
	  nDelete++;
      }
      if (nDelete) {
	numberOriginal=numberNonZero;
	numberNonZero=0;
	for (i=0;i<numberOriginal;i++) {
	  int iColumn = index[i];
	  double value = array[i];
	  array[i]=0.0;
	  if (fabs(value)>zeroTolerance) {
	    array[numberNonZero]=value;
	    index[numberNonZero++]=iColumn;
	  }
	}
      }
    } else {
      int iRow = whichRow[0]; 
      value = pi[iRow]*scalar;
      for (j=rowStart[iRow];j<rowStart[iRow+1];j++) {
	int iColumn = column[j];
	double value2 = value*element[j];
	index[numberNonZero++]=iColumn;
	array[iColumn] = value2;
      }
      iRow = whichRow[1]; 
      value = pi[iRow]*scalar;
      for (j=rowStart[iRow];j<rowStart[iRow+1];j++) {
	int iColumn = column[j];
	double value2 = value*element[j];
	// I am assuming no zeros in matrix
	if (array[iColumn])
	  value2 += array[iColumn];
	else
	  index[numberNonZero++]=iColumn;
	array[iColumn] = value2;
      }
      // get rid of tiny values and zero out marked
      numberOriginal=numberNonZero;
      numberNonZero=0;
      for (i=0;i<numberOriginal;i++) {
	int iColumn = index[i];
	if (fabs(array[iColumn])>zeroTolerance) {
	  index[numberNonZero++]=iColumn;
	} else {
	  array[iColumn]=0.0;
	}
      }
    }
  } else if (numberInRowArray==1) {
    // Just one row
    int iRow=rowArray->getIndices()[0];
    numberNonZero=0;
    CoinBigIndex j;
    if (packed) {
      double value = pi[0]*scalar;
      for (j=rowStart[iRow];j<rowStart[iRow+1];j++) {
	int iColumn = column[j];
	double value2 = value*element[j];
	if (fabs(value2)>zeroTolerance) {
	  array[numberNonZero] = value2;
	  index[numberNonZero++]=iColumn;
	}
      }
    } else {
      double value = pi[iRow]*scalar;
      for (j=rowStart[iRow];j<rowStart[iRow+1];j++) {
	int iColumn = column[j];
	double value2 = value*element[j];
	if (fabs(value2)>zeroTolerance) {
	  index[numberNonZero++]=iColumn;
	  array[iColumn] = value2;
	}
      }
    }
  }
  columnArray->setNumElements(numberNonZero);
  y->setNumElements(0);
}
/* Return <code>x *A in <code>z</code> but
   just for indices in y.
   Squashes small elements and knows about ClpSimplex */
void 
ClpPackedMatrix::subsetTransposeTimes(const ClpSimplex * model,
			      const CoinIndexedVector * rowArray,
			      const CoinIndexedVector * y,
			      CoinIndexedVector * columnArray) const
{
  columnArray->clear();
  double * pi = rowArray->denseVector();
  double * array = columnArray->denseVector();
  int jColumn;
  // get matrix data pointers
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  const double * rowScale = model->rowScale();
  int numberToDo = y->getNumElements();
  const int * which = y->getIndices();
  assert (!rowArray->packedMode());
  columnArray->setPacked();
  if (!hasGaps_&&numberToDo>5) {
    // no gaps and a reasonable number
    if (!rowScale) {
      int iColumn = which[0];
      double value = 0.0;
      CoinBigIndex j;
      for (j=columnStart[iColumn];
	   j<columnStart[iColumn+1];j++) {
	int iRow = row[j];
	value += pi[iRow]*elementByColumn[j];
      }
      for (jColumn=0;jColumn<numberToDo-1;jColumn++) {
	int iColumn = which[jColumn+1];
	CoinBigIndex start = columnStart[iColumn];
	CoinBigIndex end = columnStart[iColumn+1];
	array[jColumn]=value;
	value = 0.0;
	for (j=start; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j];
	}
      }
      array[jColumn]=value;
    } else {
      // scaled
      const double * columnScale = model->columnScale();
      int iColumn = which[0];
      double value = 0.0;
      double scale=columnScale[iColumn];
      CoinBigIndex j;
      for (j=columnStart[iColumn];
	   j<columnStart[iColumn+1];j++) {
	int iRow = row[j];
	value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
      }
      for (jColumn=0;jColumn<numberToDo-1;jColumn++) {
	int iColumn = which[jColumn+1];
	value *= scale;
	scale = columnScale[iColumn];
	CoinBigIndex start = columnStart[iColumn];
	CoinBigIndex end = columnStart[iColumn+1];
	array[jColumn]=value;
	value = 0.0;
	for (j=start; j<end;j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	}
      }
      value *= scale;
      array[jColumn]=value;
    }
  } else {
    // gaps
    if (!rowScale) {
      for (jColumn=0;jColumn<numberToDo;jColumn++) {
	int iColumn = which[jColumn];
	double value = 0.0;
	CoinBigIndex j;
	for (j=columnStart[iColumn];
	     j<columnStart[iColumn]+columnLength[iColumn];j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j];
	}
	array[jColumn]=value;
      }
    } else {
      // scaled
      const double * columnScale = model->columnScale();
      for (jColumn=0;jColumn<numberToDo;jColumn++) {
	int iColumn = which[jColumn];
	double value = 0.0;
	CoinBigIndex j;
	for (j=columnStart[iColumn];
	     j<columnStart[iColumn]+columnLength[iColumn];j++) {
	  int iRow = row[j];
	  value += pi[iRow]*elementByColumn[j]*rowScale[iRow];
	}
	value *= columnScale[iColumn];
	array[jColumn]=value;
      }
    }
  }
}
/* Returns true if can combine transposeTimes and subsetTransposeTimes
   and if it would be faster */
bool 
ClpPackedMatrix::canCombine(const ClpSimplex * model,
                            const CoinIndexedVector * pi) const
{
  int numberInRowArray = pi->getNumElements();
  int numberRows = model->numberRows();
  bool packed = pi->packedMode();
  // factor should be smaller if doing both with two pi vectors 
  double factor = 0.27;
  // We may not want to do by row if there may be cache problems
  // It would be nice to find L2 cache size - for moment 512K
  // Be slightly optimistic
  if (numberActiveColumns_*sizeof(double)>1000000) {
    if (numberRows*10<numberActiveColumns_)
      factor *= 0.333333333;
    else if (numberRows*4<numberActiveColumns_)
      factor *= 0.5;
    else if (numberRows*2<numberActiveColumns_)
      factor *= 0.66666666667;
    //if (model->numberIterations()%50==0)
    //printf("%d nonzero\n",numberInRowArray);
  }
  // if not packed then bias a bit more towards by column
  if (!packed)
    factor *= 0.9;
  return ((numberInRowArray>factor*numberRows||!model->rowCopy())&&!hasGaps_);
}
// These have to match ClpPrimalColumnSteepest version
#define reference(i)  (((reference[i>>5]>>(i&31))&1)!=0)
// Updates two arrays for steepest 
void 
ClpPackedMatrix::transposeTimes2(const ClpSimplex * model,
                                 const CoinIndexedVector * pi1, CoinIndexedVector * dj1,
                                 const CoinIndexedVector * pi2, CoinIndexedVector * dj2,
                                 CoinIndexedVector * spare,
                                 double referenceIn, double devex,
                                 // Array for exact devex to say what is in reference framework
                                 unsigned int * reference,
                                 double * weights, double scaleFactor)
{
  // put row of tableau in dj1
  double * pi = pi1->denseVector();
  int numberNonZero=0;
  int * index = dj1->getIndices();
  double * array = dj1->denseVector();
  int numberInRowArray = pi1->getNumElements();
  double zeroTolerance = model->factorization()->zeroTolerance();
  bool packed = pi1->packedMode();
  // do by column
  int iColumn;
  // get matrix data pointers
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const double * elementByColumn = matrix_->getElements();
  const double * rowScale = model->rowScale();
  assert (!spare->getNumElements());
  assert (numberActiveColumns_>0);
  double * piWeight = pi2->denseVector();
  assert (!pi2->packedMode());
  bool killDjs = (scaleFactor==0.0);
  if (!scaleFactor)
    scaleFactor=1.0;
  if (packed) {
    // need to expand pi into y
    assert(spare->capacity()>=model->numberRows());
    double * piOld = pi;
    pi = spare->denseVector();
    const int * whichRow = pi1->getIndices();
    int i;
    if (!rowScale) {
      // modify pi so can collapse to one loop
      for (i=0;i<numberInRowArray;i++) {
	int iRow = whichRow[i];
	pi[iRow]=piOld[i];
      }
      CoinBigIndex j;
      CoinBigIndex end = columnStart[0];
      for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	CoinBigIndex start = end;
	end = columnStart[iColumn+1];
        ClpSimplex::Status status = model->getStatus(iColumn);
        if (status==ClpSimplex::basic||status==ClpSimplex::isFixed) continue;
        double value = 0.0;
	for (j=start; j<end;j++) {
	  int iRow = row[j];
	  value -= pi[iRow]*elementByColumn[j];
	}
	if (fabs(value)>zeroTolerance) {
          // and do other array
          double modification = 0.0;
          for (j=start; j<end;j++) {
            int iRow = row[j];
            modification += piWeight[iRow]*elementByColumn[j];
          }
          double thisWeight = weights[iColumn];
          double pivot = value*scaleFactor;
          double pivotSquared = pivot * pivot;
          thisWeight += pivotSquared * devex + pivot * modification;
          if (thisWeight<DEVEX_TRY_NORM) {
            if (referenceIn<0.0) {
              // steepest
              thisWeight = CoinMax(DEVEX_TRY_NORM,DEVEX_ADD_ONE+pivotSquared);
            } else {
              // exact
              thisWeight = referenceIn*pivotSquared;
              if (reference(iColumn))
                thisWeight += 1.0;
              thisWeight = CoinMax(thisWeight,DEVEX_TRY_NORM);
            }
          }
          weights[iColumn] = thisWeight;
          if (!killDjs) {
            array[numberNonZero]=value;
            index[numberNonZero++]=iColumn;
          }
	}
      }
    } else {
      // scaled
      // modify pi so can collapse to one loop
      for (i=0;i<numberInRowArray;i++) {
	int iRow = whichRow[i];
	pi[iRow]=piOld[i]*rowScale[iRow];
      }
      const double * columnScale = model->columnScale();
      CoinBigIndex j;
      CoinBigIndex end = columnStart[0];
      for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
	CoinBigIndex start = end;
	end = columnStart[iColumn+1];
        ClpSimplex::Status status = model->getStatus(iColumn);
        if (status==ClpSimplex::basic||status==ClpSimplex::isFixed) continue;
        double scale=columnScale[iColumn];
        double value = 0.0;
	for (j=start; j<end;j++) {
	  int iRow = row[j];
	  value -= pi[iRow]*elementByColumn[j];
	}
	value *= scale;
	if (fabs(value)>zeroTolerance) {
          double modification = 0.0;
          for (j=start; j<end;j++) {
            int iRow = row[j];
            modification += piWeight[iRow]*elementByColumn[j]*rowScale[iRow];
          }
          modification *= scale;
          double thisWeight = weights[iColumn];
          double pivot = value*scaleFactor;
          double pivotSquared = pivot * pivot;
          thisWeight += pivotSquared * devex + pivot * modification;
          if (thisWeight<DEVEX_TRY_NORM) {
            if (referenceIn<0.0) {
              // steepest
              thisWeight = CoinMax(DEVEX_TRY_NORM,DEVEX_ADD_ONE+pivotSquared);
            } else {
              // exact
              thisWeight = referenceIn*pivotSquared;
              if (reference(iColumn))
                thisWeight += 1.0;
              thisWeight = CoinMax(thisWeight,DEVEX_TRY_NORM);
            }
          }
          weights[iColumn] = thisWeight;
          if (!killDjs) {
            array[numberNonZero]=value;
            index[numberNonZero++]=iColumn;
          }
        }
      }
    }
    // zero out
    int numberRows = model->numberRows();
    if (numberInRowArray*4<numberRows) {
      for (i=0;i<numberInRowArray;i++) {
        int iRow = whichRow[i];
        pi[iRow]=0.0;
      }
    } else {
      CoinZeroN(pi,numberRows);
    }
  } else {
    if (!rowScale) {
      CoinBigIndex j;
      CoinBigIndex end = columnStart[0];
      for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
        CoinBigIndex start = end;
        end = columnStart[iColumn+1];
        ClpSimplex::Status status = model->getStatus(iColumn);
        if (status==ClpSimplex::basic||status==ClpSimplex::isFixed) continue;
        double value = 0.0;
        for (j=start; j<end;j++) {
          int iRow = row[j];
          value -= pi[iRow]*elementByColumn[j];
        }
        if (fabs(value)>zeroTolerance) {
          // and do other array
          double modification = 0.0;
          for (j=start; j<end;j++) {
            int iRow = row[j];
            modification += piWeight[iRow]*elementByColumn[j];
          }
          double thisWeight = weights[iColumn];
          double pivot = value*scaleFactor;
          double pivotSquared = pivot * pivot;
          thisWeight += pivotSquared * devex + pivot * modification;
          if (thisWeight<DEVEX_TRY_NORM) {
            if (referenceIn<0.0) {
              // steepest
              thisWeight = CoinMax(DEVEX_TRY_NORM,DEVEX_ADD_ONE+pivotSquared);
            } else {
              // exact
              thisWeight = referenceIn*pivotSquared;
              if (reference(iColumn))
                thisWeight += 1.0;
              thisWeight = CoinMax(thisWeight,DEVEX_TRY_NORM);
            }
          }
          weights[iColumn] = thisWeight;
          if (!killDjs) {
            array[iColumn]=value;
            index[numberNonZero++]=iColumn;
          }
        }
      }
    } else {
      // scaled
      const double * columnScale = model->columnScale();
      CoinBigIndex j;
      CoinBigIndex end = columnStart[0];
      for (iColumn=0;iColumn<numberActiveColumns_;iColumn++) {
        CoinBigIndex start = end;
        end = columnStart[iColumn+1];
        ClpSimplex::Status status = model->getStatus(iColumn);
        if (status==ClpSimplex::basic||status==ClpSimplex::isFixed) continue;
        double scale=columnScale[iColumn];
        double value = 0.0;
        for (j=start; j<end;j++) {
          int iRow = row[j];
          value -= pi[iRow]*elementByColumn[j]*rowScale[iRow];
        }
        value *= scale;
        if (fabs(value)>zeroTolerance) {
          double modification = 0.0;
          for (j=start; j<end;j++) {
            int iRow = row[j];
            modification += piWeight[iRow]*elementByColumn[j]*rowScale[iRow];
          }
          modification *= scale;
          double thisWeight = weights[iColumn];
          double pivot = value*scaleFactor;
          double pivotSquared = pivot * pivot;
          thisWeight += pivotSquared * devex + pivot * modification;
          if (thisWeight<DEVEX_TRY_NORM) {
            if (referenceIn<0.0) {
              // steepest
              thisWeight = CoinMax(DEVEX_TRY_NORM,DEVEX_ADD_ONE+pivotSquared);
            } else {
              // exact
              thisWeight = referenceIn*pivotSquared;
              if (reference(iColumn))
                thisWeight += 1.0;
              thisWeight = CoinMax(thisWeight,DEVEX_TRY_NORM);
            }
          }
          weights[iColumn] = thisWeight;
          if (!killDjs) {
            array[iColumn]=value;
            index[numberNonZero++]=iColumn;
          }
        }
      }
    }
  }
  dj1->setNumElements(numberNonZero);
  spare->setNumElements(0);
  if (packed)
    dj1->setPackedMode(true);
}
// Updates second array for steepest and does devex weights
void 
ClpPackedMatrix::subsetTimes2(const ClpSimplex * model,
                              CoinIndexedVector * dj1,
                            const CoinIndexedVector * pi2, CoinIndexedVector * dj2,
                            double referenceIn, double devex,
                            // Array for exact devex to say what is in reference framework
                            unsigned int * reference,
                            double * weights, double scaleFactor)
{
  int number = dj1->getNumElements();
  const int * index = dj1->getIndices();
  double * array = dj1->denseVector();
  assert( dj1->packedMode());

  // get matrix data pointers
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  const double * rowScale = model->rowScale();
  double * piWeight = pi2->denseVector();
  bool killDjs = (scaleFactor==0.0);
  if (!scaleFactor)
    scaleFactor=1.0;
  if (!rowScale) {
    for (int k=0;k<number;k++) {
      int iColumn = index[k];
      double pivot = array[k]*scaleFactor;
      if (killDjs)
        array[k]=0.0;
      // and do other array
      double modification = 0.0;
      for (CoinBigIndex j=columnStart[iColumn]; 
           j<columnStart[iColumn]+columnLength[iColumn];j++) {
        int iRow = row[j];
        modification += piWeight[iRow]*elementByColumn[j];
      }
      double thisWeight = weights[iColumn];
      double pivotSquared = pivot * pivot;
      thisWeight += pivotSquared * devex + pivot * modification;
      if (thisWeight<DEVEX_TRY_NORM) {
        if (referenceIn<0.0) {
          // steepest
          thisWeight = CoinMax(DEVEX_TRY_NORM,DEVEX_ADD_ONE+pivotSquared);
        } else {
          // exact
          thisWeight = referenceIn*pivotSquared;
          if (reference(iColumn))
            thisWeight += 1.0;
          thisWeight = CoinMax(thisWeight,DEVEX_TRY_NORM);
        }
      }
      weights[iColumn] = thisWeight;
    }
  } else {
    // scaled
    const double * columnScale = model->columnScale();
    for (int k=0;k<number;k++) {
      int iColumn = index[k];
      double pivot = array[k]*scaleFactor;
      double scale=columnScale[iColumn];
      if (killDjs)
        array[k]=0.0;
      // and do other array
      double modification = 0.0;
      for (CoinBigIndex j=columnStart[iColumn]; 
           j<columnStart[iColumn]+columnLength[iColumn];j++) {
        int iRow = row[j];
        modification += piWeight[iRow]*elementByColumn[j]*rowScale[iRow];
      }
      double thisWeight = weights[iColumn];
      modification *= scale;
      double pivotSquared = pivot * pivot;
      thisWeight += pivotSquared * devex + pivot * modification;
      if (thisWeight<DEVEX_TRY_NORM) {
        if (referenceIn<0.0) {
          // steepest
          thisWeight = CoinMax(DEVEX_TRY_NORM,DEVEX_ADD_ONE+pivotSquared);
        } else {
          // exact
          thisWeight = referenceIn*pivotSquared;
          if (reference(iColumn))
            thisWeight += 1.0;
          thisWeight = CoinMax(thisWeight,DEVEX_TRY_NORM);
        }
      }
      weights[iColumn] = thisWeight;
    }
  }
}
/// returns number of elements in column part of basis,
CoinBigIndex 
ClpPackedMatrix::countBasis(ClpSimplex * model,
			   const int * whichColumn, 
			   int numberBasic,
			    int & numberColumnBasic)
{
  const int * columnLength = matrix_->getVectorLengths(); 
  int i;
  CoinBigIndex numberElements=0;
  // just count - can be over so ignore zero problem
  for (i=0;i<numberColumnBasic;i++) {
    int iColumn = whichColumn[i];
    numberElements += columnLength[iColumn];
  }
  return numberElements;
}
void
ClpPackedMatrix::fillBasis(ClpSimplex * model,
			 const int * whichColumn, 
			 int & numberColumnBasic,
			 int * indexRowU, int * start,
			 int * rowCount, int * columnCount,
			 double * elementU)
{
  const int * columnLength = matrix_->getVectorLengths(); 
  int i;
  CoinBigIndex numberElements=start[0];
  // fill
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const double * rowScale = model->rowScale();
  const int * row = matrix_->getIndices();
  const double * elementByColumn = matrix_->getElements();
  if (!zeroElements_) {
    if (!rowScale) {
      // no scaling
      for (i=0;i<numberColumnBasic;i++) {
	int iColumn = whichColumn[i];
	CoinBigIndex j;
	for (j=columnStart[iColumn];
	     j<columnStart[iColumn]+columnLength[iColumn];j++) {
	  int iRow=row[j];
	  indexRowU[numberElements]=iRow;
	  rowCount[iRow]++;
	  elementU[numberElements++]=elementByColumn[j];
	}
	start[i+1]=numberElements;
	columnCount[i]=columnLength[iColumn];
      }
    } else {
      // scaling
      const double * columnScale = model->columnScale();
      for (i=0;i<numberColumnBasic;i++) {
	int iColumn = whichColumn[i];
	CoinBigIndex j;
	double scale = columnScale[iColumn];
	for (j=columnStart[iColumn];
	     j<columnStart[iColumn]+columnLength[iColumn];j++) {
	  int iRow = row[j];
	  indexRowU[numberElements]=iRow;
	  rowCount[iRow]++;
	  elementU[numberElements++]=
	    elementByColumn[j]*scale*rowScale[iRow];
	}
	start[i+1]=numberElements;
	columnCount[i]=columnLength[iColumn];
      }
    }
  } else {
    // there are zero elements so need to look more closely
    if (!rowScale) {
      // no scaling
      for (i=0;i<numberColumnBasic;i++) {
	int iColumn = whichColumn[i];
	CoinBigIndex j;
	for (j=columnStart[iColumn];
	     j<columnStart[iColumn]+columnLength[iColumn];j++) {
	  double value = elementByColumn[j];
	  if (value) {
	    int iRow = row[j];
	    indexRowU[numberElements]=iRow;
	    rowCount[iRow]++;
	    elementU[numberElements++]=value;
	  }
	}
	start[i+1]=numberElements;
	columnCount[i]=numberElements-start[i];
      }
    } else {
      // scaling
      const double * columnScale = model->columnScale();
      for (i=0;i<numberColumnBasic;i++) {
	int iColumn = whichColumn[i];
	CoinBigIndex j;
	double scale = columnScale[iColumn];
	for (j=columnStart[iColumn];
	     j<columnStart[iColumn]+columnLength[i];j++) {
	  double value = elementByColumn[j];
	  if (value) {
	    int iRow = row[j];
	    indexRowU[numberElements]=iRow;
	    rowCount[iRow]++;
	    elementU[numberElements++]=value*scale*rowScale[iRow];
	  }
	}
	start[i+1]=numberElements;
	columnCount[i]=numberElements-start[i];
      }
    }
  }
}
// Creates scales for column copy (rowCopy in model may be modified)
int 
ClpPackedMatrix::scale(ClpModel * model) const 
{
  int numberRows = model->numberRows();
  int numberColumns = matrix_->getNumCols();
  // If empty - return as sanityCheck will trap
  if (!numberRows||!numberColumns)
    return 1;
  ClpMatrixBase * rowCopyBase=model->rowCopy();
  if (!rowCopyBase) {
    // temporary copy
    rowCopyBase = reverseOrderedCopy();
  }
#ifndef NO_RTTI
  ClpPackedMatrix* rowCopy =
    dynamic_cast< ClpPackedMatrix*>(rowCopyBase);
  // Make sure it is really a ClpPackedMatrix
  assert (rowCopy!=NULL);
#else
  ClpPackedMatrix* rowCopy =
    static_cast< ClpPackedMatrix*>(rowCopyBase);
#endif

  const int * column = rowCopy->getIndices();
  const CoinBigIndex * rowStart = rowCopy->getVectorStarts();
  const double * element = rowCopy->getElements();
  double * rowScale = new double [numberRows];
  double * columnScale = new double [numberColumns];
  // we are going to mark bits we are interested in
  char * usefulRow = new char [numberRows];
  char * usefulColumn = new char [numberColumns];
  double * rowLower = model->rowLower();
  double * rowUpper = model->rowUpper();
  double * columnLower = model->columnLower();
  double * columnUpper = model->columnUpper();
  int iColumn, iRow;
  // mark free rows
  for (iRow=0;iRow<numberRows;iRow++) {
    usefulRow[iRow]=0;
    if (rowUpper[iRow]<1.0e20||
	rowLower[iRow]>-1.0e20)
      usefulRow[iRow]=1;
  }
  // mark empty and fixed columns
  // also see if worth scaling
  assert (model->scalingFlag()<4); // dynamic not implemented
  double largest=0.0;
  double smallest=1.0e50;
  // get matrix data pointers
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  for (iColumn=0;iColumn<numberColumns;iColumn++) {
    CoinBigIndex j;
    char useful=0;
    if (columnUpper[iColumn]>
	columnLower[iColumn]+1.0e-9) {
      for (j=columnStart[iColumn];
	   j<columnStart[iColumn]+columnLength[iColumn];j++) {
	iRow=row[j];
	if(elementByColumn[j]&&usefulRow[iRow]) {
	  useful=1;
	  largest = CoinMax(largest,fabs(elementByColumn[j]));
	  smallest = CoinMin(smallest,fabs(elementByColumn[j]));
	}
      }
    }
    usefulColumn[iColumn]=useful;
  }
  model->messageHandler()->message(CLP_PACKEDSCALE_INITIAL,*model->messagesPointer())
    <<smallest<<largest
    <<CoinMessageEol;
  if (smallest>=0.5&&largest<=2.0) {
    // don't bother scaling
    model->messageHandler()->message(CLP_PACKEDSCALE_FORGET,*model->messagesPointer())
      <<CoinMessageEol;
    delete [] rowScale;
    delete [] usefulRow;
    delete [] columnScale;
    delete [] usefulColumn;
    if (!model->rowCopy()) 
      delete rowCopyBase; // get rid of temporary row copy
    return 1;
  } else {
      // need to scale 
    int scalingMethod = model->scalingFlag();
    // and see if there any empty rows
    for (iRow=0;iRow<numberRows;iRow++) {
      if (usefulRow[iRow]) {
	CoinBigIndex j;
	int useful=0;
	for (j=rowStart[iRow];j<rowStart[iRow+1];j++) {
	  int iColumn = column[j];
	  if (usefulColumn[iColumn]) {
	    useful=1;
	    break;
	  }
	}
	usefulRow[iRow]=useful;
      }
    }
    double savedOverallRatio=0.0;
    double tolerance = 5.0*model->primalTolerance();
    double overallLargest=-1.0e-20;
    double overallSmallest=1.0e20;
    bool finished=false;
    // if scalingMethod 3 then may change
    while (!finished) {
      ClpFillN ( rowScale, numberRows,1.0);
      ClpFillN ( columnScale, numberColumns,1.0);
      overallLargest=-1.0e-20;
      overallSmallest=1.0e20;
      if (scalingMethod==1||scalingMethod==3) {
        // Maximum in each row
        for (iRow=0;iRow<numberRows;iRow++) {
          if (usefulRow[iRow]) {
            CoinBigIndex j;
            largest=1.0e-10;
            for (j=rowStart[iRow];j<rowStart[iRow+1];j++) {
              int iColumn = column[j];
              if (usefulColumn[iColumn]) {
                double value = fabs(element[j]);
                largest = CoinMax(largest,value);
                assert (largest<1.0e40);
              }
            }
            rowScale[iRow]=1.0/largest;
            overallLargest = CoinMax(overallLargest,largest);
            overallSmallest = CoinMin(overallSmallest,largest);
          }
        }
      } else {
        int numberPass=3;
#ifdef USE_OBJECTIVE
        // This will be used to help get scale factors
        double * objective = new double[numberColumns];
        memcpy(objective,model->costRegion(1),numberColumns*sizeof(double));
        double objScale=1.0;
#endif
        while (numberPass) {
          overallLargest=0.0;
          overallSmallest=1.0e50;
          numberPass--;
          // Geometric mean on row scales
          for (iRow=0;iRow<numberRows;iRow++) {
            if (usefulRow[iRow]) {
              CoinBigIndex j;
              largest=1.0e-20;
              smallest=1.0e50;
              for (j=rowStart[iRow];j<rowStart[iRow+1];j++) {
                int iColumn = column[j];
                if (usefulColumn[iColumn]) {
                  double value = fabs(element[j]);
                  // Don't bother with tiny elements
                  if (value>1.0e-20) {
                    value *= columnScale[iColumn];
                    largest = CoinMax(largest,value);
                    smallest = CoinMin(smallest,value);
                  }
                }
              }
              rowScale[iRow]=1.0/sqrt(smallest*largest);
              rowScale[iRow]=CoinMax(1.0e-10,CoinMin(1.0e10,rowScale[iRow]));
              overallLargest = CoinMax(largest*rowScale[iRow],overallLargest);
              overallSmallest = CoinMin(smallest*rowScale[iRow],overallSmallest);
            }
          }
#ifdef USE_OBJECTIVE
          largest=1.0e-20;
          smallest=1.0e50;
          for (iColumn=0;iColumn<numberColumns;iColumn++) {
            if (usefulColumn[iColumn]) {
              double value = fabs(objective[iColumn]);
              // Don't bother with tiny elements
              if (value>1.0e-20) {
                value *= columnScale[iColumn];
                largest = CoinMax(largest,value);
                smallest = CoinMin(smallest,value);
              }
            }
          }
          objScale=1.0/sqrt(smallest*largest);
#endif
          model->messageHandler()->message(CLP_PACKEDSCALE_WHILE,*model->messagesPointer())
            <<overallSmallest
            <<overallLargest
            <<CoinMessageEol;
          // skip last column round
          if (numberPass==1)
            break;
          // Geometric mean on column scales
          for (iColumn=0;iColumn<numberColumns;iColumn++) {
            if (usefulColumn[iColumn]) {
              CoinBigIndex j;
              largest=1.0e-20;
              smallest=1.0e50;
              for (j=columnStart[iColumn];
                   j<columnStart[iColumn]+columnLength[iColumn];j++) {
                iRow=row[j];
                double value = fabs(elementByColumn[j]);
                // Don't bother with tiny elements
                if (value>1.0e-20&&usefulRow[iRow]) {
                  value *= rowScale[iRow];
                  largest = CoinMax(largest,value);
                  smallest = CoinMin(smallest,value);
                }
              }
#ifdef USE_OBJECTIVE
              if (fabs(objective[iColumn])>1.0e-20) {
                double value = fabs(objective[iColumn])*objScale;
                largest = CoinMax(largest,value);
                smallest = CoinMin(smallest,value);
              }
#endif
              columnScale[iColumn]=1.0/sqrt(smallest*largest);
              columnScale[iColumn]=CoinMax(1.0e-10,CoinMin(1.0e10,columnScale[iColumn]));
            }
          }
        }
#ifdef USE_OBJECTIVE
        delete [] objective;
        printf("obj scale %g - use it if you want\n",objScale);
#endif
      }
      // If ranges will make horrid then scale
      for (iRow=0;iRow<numberRows;iRow++) {
        if (usefulRow[iRow]) {
          double difference = rowUpper[iRow]-rowLower[iRow];
          double scaledDifference = difference*rowScale[iRow];
          if (scaledDifference>tolerance&&scaledDifference<1.0e-4) {
            // make gap larger
            rowScale[iRow] *= 1.0e-4/scaledDifference;
            rowScale[iRow]=CoinMax(1.0e-10,CoinMin(1.0e10,rowScale[iRow]));
            //printf("Row %d difference %g scaled diff %g => %g\n",iRow,difference,
            // scaledDifference,difference*rowScale[iRow]);
          }
        }
      }
      // final pass to scale columns so largest is reasonable
      // See what smallest will be if largest is 1.0
      overallSmallest=1.0e50;
      for (iColumn=0;iColumn<numberColumns;iColumn++) {
        if (usefulColumn[iColumn]) {
          CoinBigIndex j;
          largest=1.0e-20;
          smallest=1.0e50;
          for (j=columnStart[iColumn];
               j<columnStart[iColumn]+columnLength[iColumn];j++) {
            iRow=row[j];
            if(elementByColumn[j]&&usefulRow[iRow]) {
              double value = fabs(elementByColumn[j]*rowScale[iRow]);
              largest = CoinMax(largest,value);
              smallest = CoinMin(smallest,value);
            }
          }
          if (overallSmallest*largest>smallest)
            overallSmallest = smallest/largest;
        }
      }
      if (scalingMethod==1||scalingMethod==2) {
        finished=true;
      } else if (savedOverallRatio==0.0) {
        savedOverallRatio=overallSmallest;
        scalingMethod=4;
      } else {
        assert (scalingMethod==4);
        if (overallSmallest>savedOverallRatio)
          finished=true; // geometric was better
        else
          scalingMethod=1; // redo equilibrium
#if 0
        if (model->logLevel()>2) {
          if (finished)
            printf("equilibrium ratio %g, geometric ratio %g , geo chosen\n",
                   savedOverallRatio,overallSmallest);
          else
            printf("equilibrium ratio %g, geometric ratio %g , equi chosen\n",
                   savedOverallRatio,overallSmallest);
        }
#endif
      }
    }
    //#define RANDOMIZE
#ifdef RANDOMIZE
    // randomize by up to 10%
    for (iRow=0;iRow<numberRows;iRow++) {
      double value = 0.5-CoinDrand48();//between -0.5 to + 0.5
      rowScale[iRow] *= (1.0+0.1*value);
    }
#endif
    overallLargest=1.0;
    if (overallSmallest<1.0e-1)
      overallLargest = 1.0/sqrt(overallSmallest);
    overallLargest = CoinMin(100.0,overallLargest);
    overallSmallest=1.0e50;
    for (iColumn=0;iColumn<numberColumns;iColumn++) {
      if (usefulColumn[iColumn]) {
	CoinBigIndex j;
	largest=1.0e-20;
	smallest=1.0e50;
	for (j=columnStart[iColumn];
	     j<columnStart[iColumn]+columnLength[iColumn];j++) {
	  iRow=row[j];
	  if(elementByColumn[j]&&usefulRow[iRow]) {
	    double value = fabs(elementByColumn[j]*rowScale[iRow]);
	    largest = CoinMax(largest,value);
	    smallest = CoinMin(smallest,value);
	  }
	}
	columnScale[iColumn]=overallLargest/largest;
        columnScale[iColumn]=CoinMax(1.0e-10,CoinMin(1.0e10,columnScale[iColumn]));
#ifdef RANDOMIZE
	double value = 0.5-CoinDrand48();//between -0.5 to + 0.5
	columnScale[iColumn] *= (1.0+0.1*value);
#endif
	double difference = columnUpper[iColumn]-columnLower[iColumn];
	double scaledDifference = difference*columnScale[iColumn];
	if (scaledDifference>tolerance&&scaledDifference<1.0e-4) {
	  // make gap larger
	  columnScale[iColumn] *= 1.0e-4/scaledDifference;
	  //printf("Column %d difference %g scaled diff %g => %g\n",iColumn,difference,
	  // scaledDifference,difference*columnScale[iColumn]);
	}
	overallSmallest = CoinMin(overallSmallest,smallest*columnScale[iColumn]);
      }
    }
    model->messageHandler()->message(CLP_PACKEDSCALE_FINAL,*model->messagesPointer())
      <<overallSmallest
      <<overallLargest
      <<CoinMessageEol;
    delete [] usefulRow;
    delete [] usefulColumn;
    // If quadratic then make symmetric
    ClpObjective * obj = model->objectiveAsObject();
#ifndef NO_RTTI
    ClpQuadraticObjective * quadraticObj = (dynamic_cast< ClpQuadraticObjective*>(obj));
#else
    ClpQuadraticObjective * quadraticObj = NULL;
    if (obj->type()==2)
      quadraticObj = (static_cast< ClpQuadraticObjective*>(obj));
#endif
    if (quadraticObj) {
      CoinPackedMatrix * quadratic = quadraticObj->quadraticObjective();
      int numberXColumns = quadratic->getNumCols();
      if (numberXColumns<numberColumns) {
	// we assume symmetric
	int numberQuadraticColumns=0;
	int i;
	//const int * columnQuadratic = quadratic->getIndices();
	//const int * columnQuadraticStart = quadratic->getVectorStarts();
	const int * columnQuadraticLength = quadratic->getVectorLengths();
	for (i=0;i<numberXColumns;i++) {
	  int length=columnQuadraticLength[i];
#ifndef CORRECT_COLUMN_COUNTS
	  length=1;
#endif
	  if (length)
	    numberQuadraticColumns++;
	}
	int numberXRows = numberRows-numberQuadraticColumns;
	numberQuadraticColumns=0;
	for (i=0;i<numberXColumns;i++) { 
	  int length=columnQuadraticLength[i];
#ifndef CORRECT_COLUMN_COUNTS
	  length=1;
#endif
	  if (length) {
	    rowScale[numberQuadraticColumns+numberXRows] = columnScale[i];
	    numberQuadraticColumns++;
	  }
	}    
	int numberQuadraticRows=0;
	for (i=0;i<numberXRows;i++) {
	  // See if any in row quadratic
	  CoinBigIndex j;
	  int numberQ=0;
	  for (j=rowStart[i];j<rowStart[i+1];j++) {
	    int iColumn = column[j];
	    if (columnQuadraticLength[iColumn])
	      numberQ++;
	  }
#ifndef CORRECT_ROW_COUNTS
	  numberQ=1;
#endif
	  if (numberQ) {
	    columnScale[numberQuadraticRows+numberXColumns] = rowScale[i];
	    numberQuadraticRows++;
	  }
	}
	// and make sure Sj okay
	for (iColumn=numberQuadraticRows+numberXColumns;iColumn<numberColumns;iColumn++) {
	  CoinBigIndex j=columnStart[iColumn];
	  assert(columnLength[iColumn]==1);
	  int iRow=row[j];
	  double value = fabs(elementByColumn[j]*rowScale[iRow]);
	  columnScale[iColumn]=1.0/value;
	}
      }
    }
    model->setRowScale(rowScale);
    model->setColumnScale(columnScale);
    if (model->rowCopy()) {
      // need to replace row by row
      double * newElement = new double[numberColumns];
      // scale row copy
      for (iRow=0;iRow<numberRows;iRow++) {
	CoinBigIndex j;
	double scale = rowScale[iRow];
	const double * elementsInThisRow = element + rowStart[iRow];
	const int * columnsInThisRow = column + rowStart[iRow];
	int number = rowStart[iRow+1]-rowStart[iRow];
	assert (number<=numberColumns);
	for (j=0;j<number;j++) {
	  int iColumn = columnsInThisRow[j];
	  newElement[j] = elementsInThisRow[j]*scale*columnScale[iColumn];
	}
	rowCopy->replaceVector(iRow,number,newElement);
      }
      delete [] newElement;
    } else {
      // no row copy
      delete rowCopyBase;
    }
    return 0;
  }
}
/* Unpacks a column into an CoinIndexedvector
 */
void 
ClpPackedMatrix::unpack(const ClpSimplex * model,CoinIndexedVector * rowArray,
		   int iColumn) const 
{
  const double * rowScale = model->rowScale();
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  CoinBigIndex i;
  if (!rowScale) {
    for (i=columnStart[iColumn];
	 i<columnStart[iColumn]+columnLength[iColumn];i++) {
      rowArray->add(row[i],elementByColumn[i]);
    }
  } else {
    // apply scaling
    double scale = model->columnScale()[iColumn];
    for (i=columnStart[iColumn];
	 i<columnStart[iColumn]+columnLength[iColumn];i++) {
      int iRow = row[i];
      rowArray->add(iRow,elementByColumn[i]*scale*rowScale[iRow]);
    }
  }
}
/* Unpacks a column into a CoinIndexedVector
** in packed format
Note that model is NOT const.  Bounds and objective could
be modified if doing column generation (just for this variable) */
void 
ClpPackedMatrix::unpackPacked(ClpSimplex * model,
			    CoinIndexedVector * rowArray,
			    int iColumn) const
{
  const double * rowScale = model->rowScale();
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  CoinBigIndex i;
  if (!rowScale) {
    CoinBigIndex j=columnStart[iColumn];
    rowArray->createPacked(columnLength[iColumn],
			   row+j,elementByColumn+j);
  } else {
    // apply scaling
    double scale = model->columnScale()[iColumn];
    int * index = rowArray->getIndices();
    double * array = rowArray->denseVector();
    int number = 0;
    for (i=columnStart[iColumn];
	 i<columnStart[iColumn]+columnLength[iColumn];i++) {
      int iRow = row[i];
      array[number]=elementByColumn[i]*scale*rowScale[iRow];
      index[number++]=iRow;
    }
    rowArray->setNumElements(number);
    rowArray->setPackedMode(true);
  }
}
/* Adds multiple of a column into an CoinIndexedvector
      You can use quickAdd to add to vector */
void 
ClpPackedMatrix::add(const ClpSimplex * model,CoinIndexedVector * rowArray,
		   int iColumn, double multiplier) const 
{
  const double * rowScale = model->rowScale();
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  CoinBigIndex i;
  if (!rowScale) {
    for (i=columnStart[iColumn];
	 i<columnStart[iColumn]+columnLength[iColumn];i++) {
      int iRow = row[i];
      rowArray->quickAdd(iRow,multiplier*elementByColumn[i]);
    }
  } else {
    // apply scaling
    double scale = model->columnScale()[iColumn]*multiplier;
    for (i=columnStart[iColumn];
	 i<columnStart[iColumn]+columnLength[iColumn];i++) {
      int iRow = row[i];
      rowArray->quickAdd(iRow,elementByColumn[i]*scale*rowScale[iRow]);
    }
  }
}
/* Adds multiple of a column into an array */
void 
ClpPackedMatrix::add(const ClpSimplex * model,double * array,
		    int iColumn, double multiplier) const
{
  const double * rowScale = model->rowScale();
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  CoinBigIndex i;
  if (!rowScale) {
    for (i=columnStart[iColumn];
	 i<columnStart[iColumn]+columnLength[iColumn];i++) {
      int iRow = row[i];
      array[iRow] += multiplier*elementByColumn[i];
    }
  } else {
    // apply scaling
    double scale = model->columnScale()[iColumn]*multiplier;
    for (i=columnStart[iColumn];
	 i<columnStart[iColumn]+columnLength[iColumn];i++) {
      int iRow = row[i];
      array[iRow] += elementByColumn[i]*scale*rowScale[iRow];
    }
  }
}
/* Checks if all elements are in valid range.  Can just
   return true if you are not paranoid.  For Clp I will
   probably expect no zeros.  Code can modify matrix to get rid of
   small elements.
   check bits (can be turned off to save time) :
   1 - check if matrix has gaps
   2 - check if zero elements
   4 - check and compress duplicates
   8 - report on large and small
*/
bool 
ClpPackedMatrix::allElementsInRange(ClpModel * model,
				    double smallest, double largest,
				    int check)
{
  int iColumn;
  // make sure matrix correct size
  matrix_->setDimensions(model->numberRows(),model->numberColumns());
  CoinBigIndex numberLarge=0;;
  CoinBigIndex numberSmall=0;;
  CoinBigIndex numberDuplicate=0;;
  int firstBadColumn=-1;
  int firstBadRow=-1;
  double firstBadElement=0.0;
  // get matrix data pointers
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  const double * elementByColumn = matrix_->getElements();
  int numberRows = matrix_->getNumRows();
  int numberColumns = matrix_->getNumCols();
  // Say no gaps
  hasGaps_=false;
  if (check==14) {
    for (iColumn=0;iColumn<numberColumns;iColumn++) {
      CoinBigIndex start = columnStart[iColumn];
      CoinBigIndex end = start + columnLength[iColumn];
      if (end!=columnStart[iColumn+1]) {
	hasGaps_=true;
	break;
      }
    }
    return true;
  }
  assert (check=15);
  int * mark = new int [numberRows];
  int i;
  for (i=0;i<numberRows;i++)
    mark[i]=-1;
  for (iColumn=0;iColumn<numberColumns;iColumn++) {
    CoinBigIndex j;
    CoinBigIndex start = columnStart[iColumn];
    CoinBigIndex end = start + columnLength[iColumn];
    if (end!=columnStart[iColumn+1])
      hasGaps_=true;
    for (j=start;j<end;j++) {
      double value = fabs(elementByColumn[j]);
      int iRow = row[j];
      if (iRow<0||iRow>=numberRows) {
#ifndef COIN_BIG_INDEX
	printf("Out of range %d %d %d %g\n",iColumn,j,row[j],elementByColumn[j]);
#elif COIN_BIG_INDEX==1
	printf("Out of range %d %ld %d %g\n",iColumn,j,row[j],elementByColumn[j]);
#else
	printf("Out of range %d %lld %d %g\n",iColumn,j,row[j],elementByColumn[j]);
#endif
	return false;
      }
      if (mark[iRow]==-1) {
	mark[iRow]=j;
      } else {
	// duplicate
	numberDuplicate++;
      }
      //printf("%d %d %d %g\n",iColumn,j,row[j],elementByColumn[j]);
      if (!value)
	zeroElements_ = true; // there are zero elements
      if (value<smallest) {
	numberSmall++;
      } else if (!(value<=largest)) {
	numberLarge++;
	if (firstBadColumn<0) {
	  firstBadColumn=iColumn;
	  firstBadRow=row[j];
	  firstBadElement=elementByColumn[j];
	}
      }
    }
    //clear mark
    for (j=columnStart[iColumn];
	 j<columnStart[iColumn]+columnLength[iColumn];j++) {
      int iRow = row[j];
      mark[iRow]=-1;
    }
  }
  delete [] mark;
  if (numberLarge) {
    model->messageHandler()->message(CLP_BAD_MATRIX,model->messages())
      <<numberLarge
      <<firstBadColumn<<firstBadRow<<firstBadElement
      <<CoinMessageEol;
    return false;
  }
  if (numberSmall) 
    model->messageHandler()->message(CLP_SMALLELEMENTS,model->messages())
      <<numberSmall
      <<CoinMessageEol;
  if (numberDuplicate) 
    model->messageHandler()->message(CLP_DUPLICATEELEMENTS,model->messages())
      <<numberDuplicate
      <<CoinMessageEol;
  if (numberDuplicate) 
    matrix_->eliminateDuplicates(smallest);
  else if (numberSmall) 
    matrix_->compress(smallest);
  // If smallest >0.0 then there can't be zero elements
  if (smallest>0.0)
    zeroElements_=false;
  return true;
}
/* Given positive integer weights for each row fills in sum of weights
   for each column (and slack).
   Returns weights vector
*/
CoinBigIndex * 
ClpPackedMatrix::dubiousWeights(const ClpSimplex * model,int * inputWeights) const
{
  int numberRows = model->numberRows();
  int numberColumns = matrix_->getNumCols();
  int number = numberRows+numberColumns;
  CoinBigIndex * weights = new CoinBigIndex[number];
  // get matrix data pointers
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  int i;
  for (i=0;i<numberColumns;i++) {
    CoinBigIndex j;
    CoinBigIndex count=0;
    for (j=columnStart[i];j<columnStart[i]+columnLength[i];j++) {
      int iRow=row[j];
      count += inputWeights[iRow];
    }
    weights[i]=count;
  }
  for (i=0;i<numberRows;i++) {
    weights[i+numberColumns]=inputWeights[i];
  }
  return weights;
}
/* Returns largest and smallest elements of both signs.
   Largest refers to largest absolute value.
*/
void 
ClpPackedMatrix::rangeOfElements(double & smallestNegative, double & largestNegative,
		       double & smallestPositive, double & largestPositive)
{
  smallestNegative=-COIN_DBL_MAX;
  largestNegative=0.0;
  smallestPositive=COIN_DBL_MAX;
  largestPositive=0.0;
  // get matrix data pointers
  const double * elementByColumn = matrix_->getElements();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * columnLength = matrix_->getVectorLengths(); 
  int numberColumns = matrix_->getNumCols();
  int i;
  for (i=0;i<numberColumns;i++) {
    CoinBigIndex j;
    for (j=columnStart[i];j<columnStart[i]+columnLength[i];j++) {
      double value = elementByColumn[j];
      if (value>0.0) {
	smallestPositive = CoinMin(smallestPositive,value);
	largestPositive = CoinMax(largestPositive,value);
      } else if (value<0.0) {
	smallestNegative = CoinMax(smallestNegative,value);
	largestNegative = CoinMin(largestNegative,value);
      }
    }
  }
}
// Says whether it can do partial pricing
bool 
ClpPackedMatrix::canDoPartialPricing() const
{
  return true;
}
// Partial pricing 
void 
ClpPackedMatrix::partialPricing(ClpSimplex * model, double startFraction, double endFraction,
			      int & bestSequence, int & numberWanted)
{
  numberWanted=currentWanted_;
  int start = (int) (startFraction*numberActiveColumns_);
  int end = CoinMin((int) (endFraction*numberActiveColumns_+1),numberActiveColumns_);
  const double * element =matrix_->getElements();
  const int * row = matrix_->getIndices();
  const CoinBigIndex * startColumn = matrix_->getVectorStarts();
  const int * length = matrix_->getVectorLengths();
  const double * rowScale = model->rowScale();
  const double * columnScale = model->columnScale();
  int iSequence;
  CoinBigIndex j;
  double tolerance=model->currentDualTolerance();
  double * reducedCost = model->djRegion();
  const double * duals = model->dualRowSolution();
  const double * cost = model->costRegion();
  double bestDj;
  if (bestSequence>=0)
    bestDj = fabs(model->clpMatrix()->reducedCost(model,bestSequence));
  else
    bestDj=tolerance;
  int sequenceOut = model->sequenceOut();
  int saveSequence = bestSequence;
  int lastScan = minimumObjectsScan_<0 ? end : start+minimumObjectsScan_; 
  int minNeg = minimumGoodReducedCosts_==-1 ? numberWanted : minimumGoodReducedCosts_;
  if (rowScale) {
    // scaled
    for (iSequence=start;iSequence<end;iSequence++) {
      if (iSequence!=sequenceOut) {
	double value;
	ClpSimplex::Status status = model->getStatus(iSequence);
	
	switch(status) {
	  
	case ClpSimplex::basic:
	case ClpSimplex::isFixed:
	  break;
	case ClpSimplex::isFree:
	case ClpSimplex::superBasic:
	  value=0.0;
	  // scaled
	  for (j=startColumn[iSequence];
	       j<startColumn[iSequence]+length[iSequence];j++) {
	    int jRow=row[j];
	    value -= duals[jRow]*element[j]*rowScale[jRow];
	  }
	  value = fabs(cost[iSequence] +value*columnScale[iSequence]);
	  if (value>FREE_ACCEPT*tolerance) {
	    numberWanted--;
	    // we are going to bias towards free (but only if reasonable)
	    value *= FREE_BIAS;
	    if (value>bestDj) {
	      // check flagged variable and correct dj
	      if (!model->flagged(iSequence)) {
		bestDj=value;
		bestSequence = iSequence;
	      } else {
		// just to make sure we don't exit before got something
		numberWanted++;
	      }
	    }
	  }
	  break;
	case ClpSimplex::atUpperBound:
	  value=0.0;
	  // scaled
	  for (j=startColumn[iSequence];
	       j<startColumn[iSequence]+length[iSequence];j++) {
	    int jRow=row[j];
	    value -= duals[jRow]*element[j]*rowScale[jRow];
	  }
	  value = cost[iSequence] +value*columnScale[iSequence];
	  if (value>tolerance) {
	    numberWanted--;
	    if (value>bestDj) {
	      // check flagged variable and correct dj
	      if (!model->flagged(iSequence)) {
		bestDj=value;
		bestSequence = iSequence;
	      } else {
		// just to make sure we don't exit before got something
		numberWanted++;
	      }
	    }
	  }
	  break;
	case ClpSimplex::atLowerBound:
	  value=0.0;
	  // scaled
	  for (j=startColumn[iSequence];
	       j<startColumn[iSequence]+length[iSequence];j++) {
	    int jRow=row[j];
	    value -= duals[jRow]*element[j]*rowScale[jRow];
	  }
	  value = -(cost[iSequence] +value*columnScale[iSequence]);
	  if (value>tolerance) {
	    numberWanted--;
	    if (value>bestDj) {
	      // check flagged variable and correct dj
	      if (!model->flagged(iSequence)) {
		bestDj=value;
		bestSequence = iSequence;
	      } else {
		// just to make sure we don't exit before got something
		numberWanted++;
	      }
	    }
	  }
	  break;
	}
      }
      if (numberWanted+minNeg<originalWanted_&&iSequence>lastScan) {
	// give up
	break;
      }
      if (!numberWanted)
	break;
    }
    if (bestSequence!=saveSequence) {
      // recompute dj
      double value=0.0;
      // scaled
      for (j=startColumn[bestSequence];
	   j<startColumn[bestSequence]+length[bestSequence];j++) {
	int jRow=row[j];
	value -= duals[jRow]*element[j]*rowScale[jRow];
      }
      reducedCost[bestSequence] = cost[bestSequence] +value*columnScale[bestSequence];
      savedBestSequence_ = bestSequence;
      savedBestDj_ = reducedCost[savedBestSequence_];
    }
  } else {
    // not scaled
    for (iSequence=start;iSequence<end;iSequence++) {
      if (iSequence!=sequenceOut) {
	double value;
	ClpSimplex::Status status = model->getStatus(iSequence);
	
	switch(status) {
	  
	case ClpSimplex::basic:
	case ClpSimplex::isFixed:
	  break;
	case ClpSimplex::isFree:
	case ClpSimplex::superBasic:
	  value=cost[iSequence];
	  for (j=startColumn[iSequence];
	       j<startColumn[iSequence]+length[iSequence];j++) {
	    int jRow=row[j];
	    value -= duals[jRow]*element[j];
	  }
	  value = fabs(value);
	  if (value>FREE_ACCEPT*tolerance) {
	    numberWanted--;
	    // we are going to bias towards free (but only if reasonable)
	    value *= FREE_BIAS;
	    if (value>bestDj) {
	      // check flagged variable and correct dj
	      if (!model->flagged(iSequence)) {
		bestDj=value;
		bestSequence = iSequence;
	      } else {
		// just to make sure we don't exit before got something
		numberWanted++;
	      }
	    }
	  }
	  break;
	case ClpSimplex::atUpperBound:
	  value=cost[iSequence];
	  // scaled
	  for (j=startColumn[iSequence];
	       j<startColumn[iSequence]+length[iSequence];j++) {
	    int jRow=row[j];
	    value -= duals[jRow]*element[j];
	  }
	  if (value>tolerance) {
	    numberWanted--;
	    if (value>bestDj) {
	      // check flagged variable and correct dj
	      if (!model->flagged(iSequence)) {
		bestDj=value;
		bestSequence = iSequence;
	      } else {
		// just to make sure we don't exit before got something
		numberWanted++;
	      }
	    }
	  }
	  break;
	case ClpSimplex::atLowerBound:
	  value=cost[iSequence];
	  for (j=startColumn[iSequence];
	       j<startColumn[iSequence]+length[iSequence];j++) {
	    int jRow=row[j];
	    value -= duals[jRow]*element[j];
	  }
	  value = -value;
	  if (value>tolerance) {
	    numberWanted--;
	    if (value>bestDj) {
	      // check flagged variable and correct dj
	      if (!model->flagged(iSequence)) {
		bestDj=value;
		bestSequence = iSequence;
	      } else {
		// just to make sure we don't exit before got something
		numberWanted++;
	      }
	    }
	  }
	  break;
	}
      }
      if (numberWanted+minNeg<originalWanted_&&iSequence>lastScan) {
	// give up
	break;
      }
      if (!numberWanted)
	break;
    }
    if (bestSequence!=saveSequence) {
      // recompute dj
      double value=cost[bestSequence];
      for (j=startColumn[bestSequence];
	   j<startColumn[bestSequence]+length[bestSequence];j++) {
	int jRow=row[j];
	value -= duals[jRow]*element[j];
      }
      reducedCost[bestSequence] = value;
      savedBestSequence_ = bestSequence;
      savedBestDj_ = reducedCost[savedBestSequence_];
    }
  }
  currentWanted_=numberWanted;
}
// Sets up an effective RHS 
void 
ClpPackedMatrix::useEffectiveRhs(ClpSimplex * model)
{
  delete [] rhsOffset_;
  int numberRows = model->numberRows();
  rhsOffset_ = new double[numberRows];
  rhsOffset(model,true);
}
// makes sure active columns correct
int 
ClpPackedMatrix::refresh(ClpSimplex * model)
{
  numberActiveColumns_ = matrix_->getNumCols();
  return 0;
}

/* Scales rowCopy if column copy scaled
   Only called if scales already exist */
void 
ClpPackedMatrix::scaleRowCopy(ClpModel * model) const 
{
  if (model->rowCopy()) {
    // need to replace row by row
    int numberRows = model->numberRows();
    int numberColumns = matrix_->getNumCols();
    double * newElement = new double[numberColumns];
    ClpMatrixBase * rowCopyBase=model->rowCopy();
#ifndef NO_RTTI
    ClpPackedMatrix* rowCopy =
      dynamic_cast< ClpPackedMatrix*>(rowCopyBase);
    // Make sure it is really a ClpPackedMatrix
    assert (rowCopy!=NULL);
#else
    ClpPackedMatrix* rowCopy =
      static_cast< ClpPackedMatrix*>(rowCopyBase);
#endif

    const int * column = rowCopy->getIndices();
    const CoinBigIndex * rowStart = rowCopy->getVectorStarts();
    const double * element = rowCopy->getElements();
    const double * rowScale = model->rowScale();
    const double * columnScale = model->columnScale();
    // scale row copy
    for (int iRow=0;iRow<numberRows;iRow++) {
      CoinBigIndex j;
      double scale = rowScale[iRow];
      const double * elementsInThisRow = element + rowStart[iRow];
      const int * columnsInThisRow = column + rowStart[iRow];
      int number = rowStart[iRow+1]-rowStart[iRow];
      assert (number<=numberColumns);
      for (j=0;j<number;j++) {
	int iColumn = columnsInThisRow[j];
	newElement[j] = elementsInThisRow[j]*scale*columnScale[iColumn];
      }
      rowCopy->replaceVector(iRow,number,newElement);
    }
    delete [] newElement;
  }
}
/* Realy really scales column copy 
   Only called if scales already exist.
   Up to user ro delete */
ClpMatrixBase * 
ClpPackedMatrix::scaledColumnCopy(ClpModel * model) const 
{
  // need to replace column by column
  int numberRows = model->numberRows();
  int numberColumns = matrix_->getNumCols();
  double * newElement = new double[numberRows];
  ClpPackedMatrix * copy = new ClpPackedMatrix(*this);
  const int * row = copy->getIndices();
  const CoinBigIndex * columnStart = copy->getVectorStarts();
  const int * length = copy->getVectorLengths();
  const double * element = copy->getElements();
  const double * rowScale = model->rowScale();
  const double * columnScale = model->columnScale();
  // scale column copy
  for (int iColumn=0;iColumn<numberColumns;iColumn++) {
    CoinBigIndex j;
    double scale = columnScale[iColumn];
    const double * elementsInThisColumn = element + columnStart[iColumn];
    const int * rowsInThisColumn = row + columnStart[iColumn];
    int number = length[iColumn];
    assert (number<=numberRows);
    for (j=0;j<number;j++) {
      int iRow = rowsInThisColumn[j];
      newElement[j] = elementsInThisColumn[j]*scale*rowScale[iRow];
    }
    copy->replaceVector(iColumn,number,newElement);
  }
  delete [] newElement;
  return copy;
}
// Really scale matrix
void 
ClpPackedMatrix::reallyScale(const double * rowScale, const double * columnScale)
{
  int numberColumns = matrix_->getNumCols();
  const int * row = matrix_->getIndices();
  const CoinBigIndex * columnStart = matrix_->getVectorStarts();
  const int * length = matrix_->getVectorLengths();
  double * element = matrix_->getMutableElements();
  // scale 
  for (int iColumn=0;iColumn<numberColumns;iColumn++) {
    CoinBigIndex j;
    double scale = columnScale[iColumn];
    for (j=columnStart[iColumn];j<columnStart[iColumn]+length[iColumn];j++) {
      int iRow = row[j];
      element[j] *= scale*rowScale[iRow];
    }
  }
}
/* Delete the columns whose indices are listed in <code>indDel</code>. */
void 
ClpPackedMatrix::deleteCols(const int numDel, const int * indDel)
{ 
  if (matrix_->getNumCols())
    matrix_->deleteCols(numDel,indDel);
  numberActiveColumns_ = matrix_->getNumCols();
  // may now have gaps
  hasGaps_=true;
  matrix_->setExtraGap(1.0e-50);
}
/* Delete the rows whose indices are listed in <code>indDel</code>. */
void 
ClpPackedMatrix::deleteRows(const int numDel, const int * indDel)
{ 
  if (matrix_->getNumRows())
    matrix_->deleteRows(numDel,indDel);
  numberActiveColumns_ = matrix_->getNumCols();
  // may now have gaps
  hasGaps_=true;
  matrix_->setExtraGap(1.0e-50);
}
// Append Columns
void 
ClpPackedMatrix::appendCols(int number, const CoinPackedVectorBase * const * columns)
{ 
  matrix_->appendCols(number,columns);
  numberActiveColumns_ = matrix_->getNumCols();
}
// Append Rows
void 
ClpPackedMatrix::appendRows(int number, const CoinPackedVectorBase * const * rows)
{ 
  matrix_->appendRows(number,rows);
  numberActiveColumns_ = matrix_->getNumCols();
  // may now have gaps
  hasGaps_=true;
}
/* Set the dimensions of the matrix. In effect, append new empty
   columns/rows to the matrix. A negative number for either dimension
   means that that dimension doesn't change. Otherwise the new dimensions
   MUST be at least as large as the current ones otherwise an exception
   is thrown. */
void 
ClpPackedMatrix::setDimensions(int numrows, int numcols) throw(CoinError)
{
  matrix_->setDimensions(numrows,numcols);
}
/* Append a set of rows/columns to the end of the matrix. Returns number of errors
   i.e. if any of the new rows/columns contain an index that's larger than the
   number of columns-1/rows-1 (if numberOther>0) or duplicates
   If 0 then rows, 1 if columns */
int 
ClpPackedMatrix::appendMatrix(int number, int type,
                            const CoinBigIndex * starts, const int * index,
                            const double * element, int numberOther)
{
  int numberErrors=0;
  // make sure other dimension is big enough
  if (type==0) {
    // rows
    if (matrix_->isColOrdered()&&numberOther>matrix_->getNumCols())
      matrix_->setDimensions(-1,numberOther);
    numberErrors=matrix_->appendRows(number,starts,index,element,numberOther);
  } else {
    // columns
    if (!matrix_->isColOrdered()&&numberOther>matrix_->getNumRows())
      matrix_->setDimensions(numberOther,-1);
    numberErrors=matrix_->appendCols(number,starts,index,element,numberOther);
  }
  return numberErrors;
}
