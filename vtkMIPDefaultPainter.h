/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMIPDefaultPainter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkMIPDefaultPainter - vtkDefaultPainter replacement that
//  inserts the vtkMIPPainter at the correct position in the painter
//  chain.
//
// .SECTION Description
//  vtkMIPDefaultPainter is a vtkDefaultPainter replacement
//  that inserts the vtkMIPPainter at the correct position in the painter
//  chain. also It removes a few other painters that interfere with operation.
//
// .SECTION See Also
//  vtkDefaultPainter vtkMIPPainter

#ifndef __vtkMIPDefaultPainter_h
#define __vtkMIPDefaultPainter_h

#include "vtkDefaultPainter.h"

class vtkMIPPainter;

class VTK_EXPORT vtkMIPDefaultPainter : public vtkDefaultPainter
{
public:
  static vtkMIPDefaultPainter* New();
  vtkTypeMacro(vtkMIPDefaultPainter, vtkDefaultPainter);

  // Description:
  // Get/Set the Surface LIC painter.
  void SetMIPPainter(vtkMIPPainter*);
  vtkGetObjectMacro(MIPPainter, vtkMIPPainter);

  // Description:
  // The MIP painter must return the complete bounds of the whole dataset
  // not just the local 'piece', otherwise the compositing blanks out parts it thinks
  // are not covered by any geometry.
  void UpdateBounds(double bounds[6]);

//BTX
protected:
   vtkMIPDefaultPainter();
  ~vtkMIPDefaultPainter();

  // Description:
  // Setups the the painter chain.
  virtual void BuildPainterChain();

  // Description:
  // Take part in garbage collection.
  virtual void ReportReferences(vtkGarbageCollector *collector);

  vtkMIPPainter *MIPPainter;
private:
  vtkMIPDefaultPainter(const vtkMIPDefaultPainter&); // Not implemented.
  void operator=(const vtkMIPDefaultPainter&); // Not implemented.
//ETX
};

#endif
