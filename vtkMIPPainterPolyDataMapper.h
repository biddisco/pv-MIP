/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMIPPainterPolyDataMapper.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef __vtkMIPPainterPolyDataMapper_h
#define __vtkMIPPainterPolyDataMapper_h

#include "vtkPainterPolyDataMapper.h"

class vtkPainter;

class VTK_EXPORT vtkMIPPainterPolyDataMapper : public vtkPainterPolyDataMapper
{
public:
  static vtkMIPPainterPolyDataMapper* New();
  vtkTypeMacro(vtkMIPPainterPolyDataMapper, vtkPainterPolyDataMapper);

protected:
   vtkMIPPainterPolyDataMapper();
  ~vtkMIPPainterPolyDataMapper();

private:
  vtkMIPPainterPolyDataMapper(const vtkMIPPainterPolyDataMapper&); // Not implemented.
  void operator=(const vtkMIPPainterPolyDataMapper&); // Not implemented.
};

#endif

