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
#include "vtkDataObject.h"
#include "vtkDefaultPainter.h"
#include "vtkMIPPainter.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
// we inherit changes to these filters from GeometryRepresentation
#include "vtkPainterPolyDataMapper.h"
#include "vtkPVCacheKeeper.h"
#include "vtkPVUpdateSuppressor.h"
#include "vtkPVLODActor.h"
#include "vtkUnstructuredDataDeliveryFilter.h"
#include "vtkQuadricClustering.h"

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
  //
  // The default Painter based Mapper : vtkCompositePolyDataMapper2 does not
  // pass the ComputeBounds through to the individual painters, so our screenspace
  // compositing from IceT is not handled well. 
  // Since we can't handle multiblock data anyway, use a PolyDataPainter mapper
  //
  this->Mapper->Delete();
  this->LODMapper->Delete();
  this->Mapper = vtkPainterPolyDataMapper::New();
  this->LODMapper = vtkPainterPolyDataMapper::New();
  //
  this->SetupDefaults();
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
void vtkMIPRepresentation::SetupDefaults()
{
  // we changed the default Mapper so we must modify the connections affected
  this->Mapper->SetInputConnection(this->UpdateSuppressor->GetOutputPort());
  this->LODMapper->SetInputConnection(this->LODUpdateSuppressor->GetOutputPort());
  // Actors
  this->Actor->SetMapper(this->Mapper);
  this->Actor->SetLODMapper(this->LODMapper);

  // override some settings made in GeometryRepresentation to ensure we get points
  // as output and don't bother copying stuff we don't need.
  this->DeliveryFilter->SetOutputDataType(VTK_POLY_DATA);
  this->LODDeliveryFilter->SetOutputDataType(VTK_POLY_DATA);
  this->Decimator->SetCopyCellData(0);
  // We don't want the MultiBlockMaker as we don't support multiblock
  // connect the GeometryFilter to the CacheKeeper and bypass multiblockmaker.
  // The MIPDefaultPainter removes the composite painter from the painter chain

  this->CacheKeeper->SetInputConnection(this->GeometryFilter->GetOutputPort());

  // Setup painters
  vtkPainterPolyDataMapper* painterMapper = vtkPainterPolyDataMapper::SafeDownCast(this->Mapper);
  this->MIPDefaultPainter->SetDelegatePainter(painterMapper->GetPainter()->GetDelegatePainter());
  painterMapper->SetPainter(this->MIPDefaultPainter);
  this->MIPDefaultPainter->SetMIPPainter(this->MIPPainter);
  // Setup LOD painters
  painterMapper = vtkPainterPolyDataMapper::SafeDownCast(this->LODMapper);
  this->LODMIPDefaultPainter->SetDelegatePainter(painterMapper->GetPainter()->GetDelegatePainter());
  painterMapper->SetPainter(this->LODMIPDefaultPainter);
  this->LODMIPDefaultPainter->SetMIPPainter(this->LODMIPPainter);
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
