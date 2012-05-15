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
  // Lighting painter aborts render if no input, which locks up our collectives
  this->SetLightingPainter(NULL);
  this->SetClipPlanesPainter(NULL);
  // and set ours at the end of the chain
  this->SetDefaultPainterDelegate(this->MIPPainter);
  // allow superclass to pieces everything together
  this->Superclass::BuildPainterChain();
  // We need the ScalarsToColors Painter as the MIP handles scalar mapping specially
  this->MIPPainter->SetScalarsToColorsPainter(this->GetScalarsToColorsPainter());
}

//----------------------------------------------------------------------------
void vtkMIPDefaultPainter::ReportReferences(vtkGarbageCollector *collector)
{
  this->Superclass::ReportReferences(collector);
  vtkGarbageCollectorReport(collector, this->MIPPainter, "MIPPainter");
}

//----------------------------------------------------------------------------
