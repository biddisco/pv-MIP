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

#include "vtksys/ios/sstream"

#include "vtkCompositePolyDataMapper2.h"
#include "vtkDataObject.h"
#include "vtkMIPMapper.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMPIMoveData.h"
#include "vtkObjectFactory.h"
#include "vtkPVArrowSource.h"
#include "vtkPVLODActor.h"
#include "vtkPVRenderView.h"
#include "vtkQuadricClustering.h"
#include "vtkRenderer.h"
#include "vtkUnstructuredDataDeliveryFilter.h"
#include "vtkOrderedCompositeDistributor.h"
#include "vtkPVCacheKeeper.h"

vtkStandardNewMacro(vtkMIPRepresentation);
//----------------------------------------------------------------------------
vtkMIPRepresentation::vtkMIPRepresentation()
{
  this->MIPMapper    = vtkMIPMapper::New();
  this->Mapper       = this->MIPMapper;
  this->LODMIPMapper = vtkMIPMapper::New();
  this->LODMapper    = this->LODMIPMapper;

//  this->GrayAbsorption = 0.0001;
//  this->Brightness = 10.5;
//  this->LogIntensity = 1;
  this->ActiveParticleType = 0;

  this->Mapper->SetInputConnection(this->Distributor->GetOutputPort());
  this->LODMapper->SetInputConnection(this->LODDeliveryFilter->GetOutputPort());

  this->Actor->SetMapper(this->Mapper);
  this->Actor->SetLODMapper(this->LODMapper);
  this->Actor->SetProperty(this->Property);

  // override some settings made in GeometryRepresentation
  this->DeliveryFilter->SetOutputDataType(VTK_POLY_DATA);
  this->LODDeliveryFilter->SetOutputDataType(VTK_POLY_DATA);
  this->Decimator->SetCopyCellData(0);
  //  we don't want the MultiBlockMaker used
  this->CacheKeeper->SetInputConnection(this->GeometryFilter->GetOutputPort());

  this->ColorArrayName = 0;
  this->ColorAttributeType = POINT_DATA;
  this->Representation = POINTS;

  // Not insanely thrilled about this API on vtkProp about properties, but oh
  // well. We have to live with it.
  vtkInformation* keys = vtkInformation::New();
  this->Actor->SetPropertyKeys(keys);
  keys->Delete();

  Settings = vtkSmartPointer<vtkStringArray>::New();

}
//----------------------------------------------------------------------------
vtkMIPRepresentation::~vtkMIPRepresentation()
{
  // Geometry Representation base class will delete the Mapper and LODMapper which point to our classes
  this->MIPMapper = NULL;
  this->LODMIPMapper = NULL;
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
  if (this->MIPMapper) this->MIPMapper->SetNumberOfParticleTypes(p+1);
  if (this->LODMIPMapper) this->LODMIPMapper->SetNumberOfParticleTypes(p+1);
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
  if (this->MIPMapper) this->MIPMapper->SetTypeActive(this->ActiveParticleType, l);
  if (this->LODMIPMapper) this->LODMIPMapper->SetTypeActive(this->ActiveParticleType, l);
}
//----------------------------------------------------------------------------
int vtkMIPRepresentation::GetTypeActive()
{
  return this->MIPMapper->GetTypeActive(this->ActiveParticleType);
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
  if (this->MIPMapper) this->MIPMapper->SetTypeScalars(s);
  if (this->LODMIPMapper) this->LODMIPMapper->SetTypeScalars(s);
}
//----------------------------------------------------------------------------
const char *vtkMIPRepresentation::GetTypeScalars()
{
  if (this->MIPMapper) return this->MIPMapper->GetTypeScalars();
  return NULL;
}
//----------------------------------------------------------------------------
void vtkMIPRepresentation::SetActiveScalars(const char *s)
{
  if (this->MIPMapper) this->MIPMapper->SetActiveScalars(s);
  if (this->LODMIPMapper) this->LODMIPMapper->SetActiveScalars(s);
}
//----------------------------------------------------------------------------
const char *vtkMIPRepresentation::GetActiveScalars()
{
  if (this->MIPMapper) return this->MIPMapper->GetActiveScalars();
  return NULL;
}
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
