/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMIPDefaultPainter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMIPDefaultPainter.h"

#include "vtkGarbageCollector.h"
#include "vtkMIPPainter.h"
#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"
#include "vtkScalarsToColorsPainter.h"

vtkStandardNewMacro(vtkMIPDefaultPainter);
vtkCxxSetObjectMacro(vtkMIPDefaultPainter, MIPPainter, vtkMIPPainter);
//----------------------------------------------------------------------------
vtkMIPDefaultPainter::vtkMIPDefaultPainter()
{
  this->MIPPainter = vtkMIPPainter::New();
}

//----------------------------------------------------------------------------
vtkMIPDefaultPainter::~vtkMIPDefaultPainter()
{
  this->SetMIPPainter(0);
}

//----------------------------------------------------------------------------
void vtkMIPDefaultPainter::UpdateBounds(double bounds[6])
{
  if (this->MIPPainter->GetInput()!=this->GetInput()) {
    this->MIPPainter->SetInput(this->GetInput());
  }
  this->MIPPainter->UpdateBounds(bounds);
}

//----------------------------------------------------------------------------
void vtkMIPDefaultPainter::BuildPainterChain()
{
  // Override painters we don't want.
  this->SetDisplayListPainter(NULL);
  this->SetCompositePainter(NULL);
  this->SetCoincidentTopologyResolutionPainter(NULL);
  this->SetRepresentationPainter(NULL);
  // we need the scalars to colors painter, but we only want to use it inside the 
  // core MIP render code, so we will pass it through.
  // NB. the MIP handles scalar mapping specially.
  this->MIPPainter->SetScalarsToColorsPainter(this->GetScalarsToColorsPainter());
  this->SetScalarsToColorsPainter(NULL);
  // Lighting painter aborts render if no input, which locks up our collectives
  this->SetLightingPainter(NULL);
  this->SetClipPlanesPainter(NULL);
  // and set ours at the end of the chain
  this->vtkPainter::SetDelegatePainter(this->MIPPainter);
  // We can bypass superclass BuildPainterChain because none of the painters
  // are used at the current time.
  // this->Superclass::BuildPainterChain();
}

//----------------------------------------------------------------------------
void vtkMIPDefaultPainter::ReportReferences(vtkGarbageCollector *collector)
{
  this->Superclass::ReportReferences(collector);
  vtkGarbageCollectorReport(collector, this->MIPPainter, "MIPPainter");
}

//----------------------------------------------------------------------------
