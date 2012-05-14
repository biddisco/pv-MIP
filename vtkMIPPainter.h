/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMIPPainter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkMIPPainter - vtkMIP.
// .SECTION Description
// .SECTION Implementation
//
// .SECTION See Also

#ifndef __vtkMIPPainter_h
#define __vtkMIPPainter_h

#include "vtkPolyDataPainter.h"

#include <vtkstd/vector> // needed for our arrays
#include <vtkstd/string> // needed for our arrays

class vtkMultiProcessController;
class vtkScalarsToColorsPainter;

class VTK_EXPORT vtkMIPPainter : public vtkPolyDataPainter
{
public:
  static vtkMIPPainter* New();
  vtkTypeMacro(vtkMIPPainter, vtkPolyDataPainter);

  // Description:
  void Render(vtkRenderer* renderer, vtkActor* actor, 
    unsigned long typeflags, bool forceCompileOnly);

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

  void SetTypeActive(int ptype, int);
  int GetTypeActive(int ptype);

  // we need to override the bounds so that IceT composites the whole image 
  // and not only the projected piece bounds from each process
//  void GetBounds(double *bounds);
//  double *GetBounds();

  // Description:
  // The MIP Renderer needs to manually convert scalars to colours
  // so we must have a copy of the painter used by the rest of the rendering pipeline
  virtual void SetScalarsToColorsPainter(vtkScalarsToColorsPainter* ScalarsToColorsPainter);
  vtkGetObjectMacro(ScalarsToColorsPainter, vtkScalarsToColorsPainter);

//BTX
  // Description:
  // Set/Get the controller used for coordinating parallel writing
  // (set to the global controller by default)
  // If not using the default, this must be called before any
  // other methods.
  virtual void SetController(vtkMultiProcessController* controller);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);
//ETX

  vtkSetMacro(ArrayAccessMode, int);
  vtkSetMacro(ArrayId, int);
  vtkSetStringMacro(ArrayName);
  vtkSetMacro(ArrayComponent, int);
  vtkSetMacro(ScalarMode, int);

  // Description:
  // The MIP painter must return the complete bounds of the whole dataset
  // not just the local 'piece', otherwise the compositing blanks out parts it thinks
  // are not covered by any geometry.
  void UpdateBounds(double bounds[6]);

protected:
   vtkMIPPainter();
  ~vtkMIPPainter();

  // Description:
  // Called before RenderInternal() if the Information has been changed
  // since the last time this method was called.
  virtual void ProcessInformation(vtkInformation*);

//  virtual int FillInputPortInformation(int port, vtkInformation *info);

  char             *TypeScalars;
  char             *ActiveScalars;
  int               NumberOfParticleTypes;
  std::vector<int>  TypeActive;

  vtkMultiProcessController *Controller;
  vtkScalarsToColorsPainter *ScalarsToColorsPainter;

  int ArrayAccessMode;
  int ArrayComponent;
  int ArrayId;
  char* ArrayName;
  int ScalarMode;

private:
  vtkMIPPainter(const vtkMIPPainter&); // Not implemented.
  void operator=(const vtkMIPPainter&); // Not implemented.
};

#endif
