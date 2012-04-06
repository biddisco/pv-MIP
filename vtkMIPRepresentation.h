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
// .NAME vtkMIPRepresentation
// .SECTION Description
// vtkMIPRepresentation is a representation that uses the vtkMIPMapper
// for rendering glyphs.

#ifndef __vtkMIPRepresentation_h
#define __vtkMIPRepresentation_h

#include "vtkGeometryRepresentation.h"
#include "vtkStringArray.h"
#include "vtkSmartPointer.h"

class vtkMIPPainter;

class VTK_EXPORT vtkMIPRepresentation : public vtkGeometryRepresentation
{
public:
  static vtkMIPRepresentation* New();
  vtkTypeMacro(vtkMIPRepresentation, vtkGeometryRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // To simplify the GUI interaction, we make one particle type active
  // so that SetBrightness, SetActiveScalars etc act on that type.
  void SetActiveParticleType(int p);
  vtkGetMacro(ActiveParticleType, int);

  //**************************************************************************
  // Forwarded to vtkMIPMapper
  //**************************************************************************
  virtual void SetInputArrayToProcess(int idx, int port, int connection,
                              int fieldAssociation,
                              const char *name);

  void SetTypeScalars(const char *);
  void SetActiveScalars(const char *);
  const char *GetTypeScalars();
  const char *GetActiveScalars();

  void   SetTypeActive(int l);
  int    GetTypeActive();

  // Gather all the settings in one call for feeding back to the gui display
  vtkStringArray *GetActiveParticleSettings();

//BTX
protected:
  vtkMIPRepresentation();
  ~vtkMIPRepresentation();

  // Description:
  // Fill input port information.
  virtual int FillInputPortInformation(int port, vtkInformation* info);

  // Description:
  // Execute the pipeline, not used, but can instantiate filters for extra processing
  // in here.
  virtual int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*);

  //
  vtkMIPPainter  *MIPPainter;
  vtkMIPPainter  *LODMIPPainter;
//  vtkMIPMapper          *MIPMapper;
//  vtkMIPMapper          *LODMIPMapper;
  int                    ActiveParticleType;
  vtkSmartPointer<vtkStringArray> Settings;

private:
  vtkMIPRepresentation(const vtkMIPRepresentation&); // Not implemented
  void operator=(const vtkMIPRepresentation&); // Not implemented
//ETX
};

#endif
