/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMIPMapper.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkMIPMapper - vtkMIP.
// .SECTION Description
// .SECTION Implementation
//
// .SECTION See Also

#ifndef __vtkMIPMapper_h
#define __vtkMIPMapper_h

#include "vtkPainterPolyDataMapper.h"

#include <vtkstd/vector> // needed for our arrays
#include <vtkstd/string> // needed for our arrays

class vtkMultiProcessController;

class VTK_EXPORT vtkMIPMapper : public vtkPainterPolyDataMapper
{
public:
  static vtkMIPMapper* New();
  vtkTypeMacro(vtkMIPMapper, vtkMapper);

  void Render(vtkRenderer *, vtkActor *);

  // Description:
  // Each particle may have a type, this must be an integer
  // usuall, dark matter, gas, star, etc are denoted by their type Id
  // one array maps all particles
  vtkSetStringMacro(TypeScalars);
  vtkGetStringMacro(TypeScalars);

  // Description:
  // Each particle may be visible or invisible, the active array is used to pass
  // a yes/no value. usually an int array (or char) is used.
  // one array maps all particles
  vtkSetStringMacro(ActiveScalars);
  vtkGetStringMacro(ActiveScalars);

  // Description:
  // There may be N particle types. Each type has its own colour table,
  // Intensity array, brigthness etc. The remaining values are defined per
  // particle type.
  void SetNumberOfParticleTypes(int N);
  vtkGetMacro(NumberOfParticleTypes, int);

  void SetIntensityScalars(int ptype, const char *s);
  const char *GetIntensityScalars(int ptype);

  void SetRadiusScalars(int ptype, const char *s);
  const char *GetRadiusScalars(int ptype);

  void SetBrightness(int ptype, double);
  double GetBrightness(int ptype);

  void SetLogIntensity(int ptype, int);
  int GetLogIntensity(int ptype);

  // don't need this?
  void SetLogColour(int ptype, int);
  int GetLogColour(int ptype);

  void SetTypeActive(int ptype, int);
  int GetTypeActive(int ptype);

  vtkGetMacro(GrayAbsorption,double);
  vtkSetMacro(GrayAbsorption,double);

  // we need to override the bounds so that IceT composites the whole image 
  // and not only the projected piece bounds from each process
  void GetBounds(double *bounds);
  double *GetBounds();

//BTX
  // Description:
  // Set/Get the controller used for coordinating parallel writing
  // (set to the global controller by default)
  // If not using the default, this must be called before any
  // other methods.
  virtual void SetController(vtkMultiProcessController* controller);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);
//ETX

protected:
   vtkMIPMapper();
  ~vtkMIPMapper();

  virtual int FillInputPortInformation(int port, vtkInformation *info);

  char   *TypeScalars;
  char   *ActiveScalars;
  double  GrayAbsorption;
  int     NumberOfParticleTypes;
  std::vector<std::string> IntensityScalars;
  std::vector<std::string> RadiusScalars;
  std::vector<double>      Brightness;
  std::vector<int>         LogIntensity;
  std::vector<int>         LogColour;
  std::vector<int>         TypeActive;

  vtkMultiProcessController* Controller;

private:
  vtkMIPMapper(const vtkMIPMapper&); // Not implemented.
  void operator=(const vtkMIPMapper&); // Not implemented.
};

#endif
