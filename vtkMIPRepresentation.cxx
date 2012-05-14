/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMIPRepresentation.h"
#include "vtkMIPDefaultPainter.h"
//
#include "vtksys/ios/sstream"
//
#include "vtkCompositePolyDataMapper2.h"
#include "vtkDataObject.h"
#include "vtkDefaultPainter.h"
#include "vtkMIPPainter.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPVLODActor.h"
#include "vtkRenderer.h"

vtkStandardNewMacro(vtkMIPRepresentation);
//----------------------------------------------------------------------------
vtkMIPRepresentation::vtkMIPRepresentation()
{
  this->MIPDefaultPainter    = vtkMIPDefaultPainter::New();
  this->LODMIPDefaultPainter = vtkMIPDefaultPainter::New();
  this->MIPPainter           = vtkMIPPainter::New();
  this->LODMIPPainter        = vtkMIPPainter::New();
  this->ActiveParticleType   = 0;
  this->ColorArrayName       = 0;
  this->ColorAttributeType   = POINT_DATA;
  this->Representation       = POINTS;
  this->Settings             = vtkSmartPointer<vtkStringArray>::New();
  // Setup painters
  vtkCompositePolyDataMapper2* compositeMapper = vtkCompositePolyDataMapper2::SafeDownCast(this->Mapper);
  this->MIPDefaultPainter->SetDelegatePainter(compositeMapper->GetPainter()->GetDelegatePainter());
  compositeMapper->SetPainter(this->MIPDefaultPainter);
  this->MIPDefaultPainter->SetMIPPainter(this->MIPPainter);
  // Setup LOD painters
  compositeMapper = vtkCompositePolyDataMapper2::SafeDownCast(this->LODMapper);
  this->LODMIPDefaultPainter->SetDelegatePainter(compositeMapper->GetPainter()->GetDelegatePainter());
  compositeMapper->SetPainter(this->LODMIPDefaultPainter);
  this->LODMIPDefaultPainter->SetMIPPainter(this->LODMIPPainter);

  // Copied verbatim from example plugins:
  // Not insanely thrilled about this API on vtkProp about properties, but oh
  // well. We have to live with it.
  vtkInformation* keys = vtkInformation::New();
  this->Actor->SetPropertyKeys(keys);
  keys->Delete();
}
//----------------------------------------------------------------------------
vtkMIPRepresentation::~vtkMIPRepresentation()
{
  this->MIPDefaultPainter->Delete();
  this->LODMIPDefaultPainter->Delete();
  this->MIPPainter->Delete();
  this->LODMIPPainter->Delete();
}
//----------------------------------------------------------------------------
int vtkMIPRepresentation::FillInputPortInformation(int port,
  vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
int vtkMIPRepresentation::RequestData(vtkInformation* request,
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
void vtkMIPRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
//----------------------------------------------------------------------------
void vtkMIPRepresentation::SetActiveParticleType(int p)
{
  // this only allocates space in the mapper, it does not actually set the max
  if (this->MIPPainter) this->MIPPainter->SetNumberOfParticleTypes(p+1);
  if (this->LODMIPPainter) this->LODMIPPainter->SetNumberOfParticleTypes(p+1);
  // this is the active one
  this->ActiveParticleType = p;
}
//----------------------------------------------------------------------------
template <typename T>
std::string NumToStr(T data) {
  vtksys_ios::ostringstream oss;
//  oss.setf(0,ios::floatfield);
  oss.precision(5);  
  oss << data;
  return oss.str();
}
//----------------------------------------------------------------------------
vtkStringArray *vtkMIPRepresentation::GetActiveParticleSettings()
{
  this->Settings->Initialize();
  this->Settings->SetNumberOfComponents(1);
  this->Settings->SetNumberOfTuples(2);
  //
  this->Settings->SetValue(0, NumToStr<int>(this->ActiveParticleType).c_str());
  this->Settings->SetValue(1, NumToStr<int>(this->GetTypeActive()).c_str());
  //
  return this->Settings;
}
//----------------------------------------------------------------------------
void vtkMIPRepresentation::SetTypeActive(int l)
{
  if (this->MIPPainter) this->MIPPainter->SetTypeActive(this->ActiveParticleType, l);
  if (this->LODMIPPainter) this->LODMIPPainter->SetTypeActive(this->ActiveParticleType, l);
}
//----------------------------------------------------------------------------
int vtkMIPRepresentation::GetTypeActive()
{
  return this->MIPPainter->GetTypeActive(this->ActiveParticleType);
}
//----------------------------------------------------------------------------
void vtkMIPRepresentation::SetInputArrayToProcess(
  int idx, int port, int connection, int fieldAssociation, const char *name)
{
  switch (idx) {
    case 0: this->SetTypeScalars(name); break;
    case 1: this->SetActiveScalars(name); break;
  }
}
//----------------------------------------------------------------------------
void vtkMIPRepresentation::SetTypeScalars(const char *s)
{
  if (this->MIPPainter) this->MIPPainter->SetTypeScalars(s);
  if (this->LODMIPPainter) this->LODMIPPainter->SetTypeScalars(s);
}
//----------------------------------------------------------------------------
const char *vtkMIPRepresentation::GetTypeScalars()
{
  if (this->MIPPainter) return this->MIPPainter->GetTypeScalars();
  return NULL;
}
//----------------------------------------------------------------------------
void vtkMIPRepresentation::SetActiveScalars(const char *s)
{
  if (this->MIPPainter) this->MIPPainter->SetActiveScalars(s);
  if (this->LODMIPPainter) this->LODMIPPainter->SetActiveScalars(s);
}
//----------------------------------------------------------------------------
const char *vtkMIPRepresentation::GetActiveScalars()
{
  if (this->MIPPainter) return this->MIPPainter->GetActiveScalars();
  return NULL;
}
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
