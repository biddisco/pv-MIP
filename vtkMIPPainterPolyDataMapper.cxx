/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMIPPainterPolyDataMapper.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMIPPainterPolyDataMapper.h"

#include "vtkObjectFactory.h"
#include "vtkMipPainter.h"

vtkStandardNewMacro(vtkMIPPainterPolyDataMapper);
//-----------------------------------------------------------------------------
vtkMIPPainterPolyDataMapper::vtkMIPPainterPolyDataMapper()
{
  // Allow superclass to setup everything else, but override main painter
  vtkMIPPainter* mip = vtkMIPPainter::New();
  this->Painter->SetDelegatePainter(mip);
  mip->Delete();
}

//-----------------------------------------------------------------------------
vtkMIPPainterPolyDataMapper::~vtkMIPPainterPolyDataMapper()
{
}

