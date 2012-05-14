/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMIPPainter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkMIPPainter.h"

#include "vtksys/ios/sstream"

#include "vtkgl.h"
#include "vtkMapper.h"
#include "vtkActor.h"
#include "vtkRenderer.h"
#include "vtkPolyData.h"
#include "vtkPoints.h"
#include "vtkCamera.h"

#include "vtkBitArray.h"
#include "vtkBoundingBox.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCompositeDataSet.h"
#include "vtkDataArray.h"
#include "vtkDataSetAttributes.h"
#include "vtkGraphicsFactory.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkLookupTable.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkSmartPointer.h"
#include "vtkTimerLog.h"
#include "vtkTransform.h"
#include "vtkScalarsToColorsPainter.h"
#include "vtkCellArray.h"
  //
#ifdef VTK_USE_MPI
#include "vtkMPICommunicator.h"
#endif
#include "vtkMultiProcessController.h"

#include <assert.h>

#undef min
#undef max
#include <algorithm>

#include "vtkOpenGL.h"
#include "vtkgl.h"
#include "IceTConfig.h"

//----------------------------------------------------------------------------
vtkInstantiatorNewMacro(vtkMIPPainter);
vtkCxxSetObjectMacro(vtkMIPPainter, Controller, vtkMultiProcessController);
vtkCxxSetObjectMacro(vtkMIPPainter, ScalarsToColorsPainter, vtkScalarsToColorsPainter);
//----------------------------------------------------------------------------

template<typename T> class RGB_tuple
  {
  public:
    T r, g, b;

    RGB_tuple () {}
    RGB_tuple (T rv, T gv, T bv)
      : r (rv), g (gv), b (bv) {}
    template<typename T2> explicit RGB_tuple (const RGB_tuple<T2> &orig)
      : r(orig.r), g(orig.g), b(orig.b) {}

    const RGB_tuple &operator= (const RGB_tuple &Col2)
      { r=Col2.r; g=Col2.g; b=Col2.b; return *this; }
    const RGB_tuple &operator+= (const RGB_tuple &Col2)
      { r+=Col2.r; g+=Col2.g; b+=Col2.b; return *this; }
    const RGB_tuple &operator*= (T fac)
      { r*=fac; g*=fac; b*=fac; return *this; }
    RGB_tuple operator+ (const RGB_tuple &Col2) const
      { return RGB_tuple (r+Col2.r, g+Col2.g, b+Col2.b); }
    RGB_tuple operator- (const RGB_tuple &Col2) const
      { return RGB_tuple (r-Col2.r, g-Col2.g, b-Col2.b); }
    template<typename T2> RGB_tuple operator* (T2 factor) const
      { return RGB_tuple (r*factor, g*factor, b*factor); }
    template<typename T2> friend inline RGB_tuple operator* (T2 factor, const RGB_tuple &Col)
      { return RGB_tuple (Col.r*factor, Col.g*factor, Col.b*factor); }

    void Set (T r2, T g2, T b2)
      { r=r2; g=g2; b=b2; }

    friend std::ostream &operator<< (std::ostream &os, const RGB_tuple &c)
      {
      os << "(" << c.r << ", " << c.g << ", " << c.b << ")";
      return os;
      }
  };

//----------------------------------------------------------------------------
vtkMIPPainter *vtkMIPPainter::New()
{
  vtkObject* ret = new vtkMIPPainter();
  return static_cast<vtkMIPPainter *>(ret);
}
// ---------------------------------------------------------------------------
vtkMIPPainter::vtkMIPPainter()
{
  this->TypeScalars            = NULL;
  this->ActiveScalars          = NULL;
  this->NumberOfParticleTypes  = 0;
  this->SetNumberOfParticleTypes(1); 
  this->ScalarsToColorsPainter = NULL;
  this->Controller             = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
  //
  this->ArrayName = NULL;
  this->ArrayId = -1;
  this->ArrayComponent = 0;
  this->ArrayAccessMode = VTK_GET_ARRAY_BY_ID;
}
// ---------------------------------------------------------------------------
vtkMIPPainter::~vtkMIPPainter()
{
  delete []this->TypeScalars;
  delete []this->ActiveScalars;
}
// ---------------------------------------------------------------------------
/*
int vtkMIPPainter::FillInputPortInformation(int port,
  vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  return 1;
}
*/
// ---------------------------------------------------------------------------
//double *vtkMIPPainter::GetBounds()
//{
//  this->GetBounds(this->Bounds);
//  return this->Bounds;
//}
// ---------------------------------------------------------------------------
/*
void vtkMIPPainter::GetBounds(double *bounds)
{
  //
  // Define box...
  //
  this->GetInput()->GetBounds(bounds);
  //
#ifdef VTK_USE_MPI
  vtkMPICommunicator *communicator = vtkMPICommunicator::SafeDownCast(
    vtkMultiProcessController::GetGlobalController()->GetCommunicator());
  if (communicator)
  {
    double mins[3] = {bounds[0], bounds[2], bounds[4]};
    double maxes[3] = {bounds[1], bounds[3], bounds[5]};
    double globalMins[3], globalMaxes[3];
    communicator->AllReduce(mins, globalMins, 3, vtkCommunicator::MIN_OP);
    communicator->AllReduce(maxes, globalMaxes, 3, vtkCommunicator::MAX_OP);
    bounds[0] = globalMins[0];  bounds[1] = globalMaxes[0];
    bounds[2] = globalMins[1];  bounds[3] = globalMaxes[1];
    bounds[4] = globalMins[2];  bounds[5] = globalMaxes[2];
  }
#endif
}
*/
// ---------------------------------------------------------------------------
void vtkMIPPainter::SetNumberOfParticleTypes(int N)
{
  this->NumberOfParticleTypes = std::max(N,this->NumberOfParticleTypes);
  this->TypeActive.resize(this->NumberOfParticleTypes,0);
}
// ---------------------------------------------------------------------------
void vtkMIPPainter::SetTypeActive(int ptype, int a)
{
  if (a!=this->TypeActive[ptype]) {
    this->TypeActive[ptype] = a;
    this->Modified();
  }
}
// ---------------------------------------------------------------------------
int vtkMIPPainter::GetTypeActive(int ptype)
{
  return this->TypeActive[ptype];
}
//-----------------------------------------------------------------------------
void vtkMIPPainter::ProcessInformation(vtkInformation* info)
{
  if (info->Has(vtkScalarsToColorsPainter::SCALAR_MODE()))
    {
    this->SetScalarMode(info->Get(vtkScalarsToColorsPainter::SCALAR_MODE()));
    }

  if (info->Has(vtkScalarsToColorsPainter::ARRAY_ACCESS_MODE()))
    {
    this->SetArrayAccessMode(info->Get(vtkScalarsToColorsPainter::ARRAY_ACCESS_MODE()));
    }

  if (info->Has(vtkScalarsToColorsPainter::ARRAY_ID()))
    {
    this->SetArrayId(info->Get(vtkScalarsToColorsPainter::ARRAY_ID()));
    }

  if (info->Has(vtkScalarsToColorsPainter::ARRAY_NAME()))
    {
    this->SetArrayName(info->Get(vtkScalarsToColorsPainter::ARRAY_NAME()));
    }

  if (info->Has(vtkScalarsToColorsPainter::ARRAY_COMPONENT()))
    {
    this->SetArrayComponent(info->Get(vtkScalarsToColorsPainter::ARRAY_COMPONENT()));
    }
  }
//-----------------------------------------------------------------------------
// IceT is not exported by paraview, so rather than force lots of include dirs
// and libs, just manually set some defs which will keep the compiler happy
//-----------------------------------------------------------------------------
typedef IceTUnsignedInt32       IceTEnum;
typedef IceTInt32               IceTInt;
extern "C" ICET_EXPORT void icetGetIntegerv(IceTEnum pname, IceTInt *params);
#define ICET_STATE_ENGINE_START (IceTEnum)0x00000000
#define ICET_NUM_TILES          (ICET_STATE_ENGINE_START | (IceTEnum)0x0010)
#define ICET_TILE_VIEWPORTS     (ICET_STATE_ENGINE_START | (IceTEnum)0x0011)
// ---------------------------------------------------------------------------
void vtkMIPPainter::Render(vtkRenderer* ren, vtkActor* actor, 
  unsigned long typeflags, bool forceCompileOnly)
{
  int X = ren->GetSize()[0];
  int Y = ren->GetSize()[1];
//  vtkDataSet *dsn = actor->GetMapper()->GetInput();
//  vtkPointSet *input = vtkPointSet::SafeDownCast(actor->GetMapper()->GetInput());
  vtkDataObject *indo = this->GetInput();
  vtkPointSet *input = vtkPointSet::SafeDownCast(indo);
  vtkPoints *pts = input->GetPoints();
  //
  vtkDataArray *TypeArray = this->TypeScalars ? 
    input->GetPointData()->GetArray(this->TypeScalars) : NULL;  
  //
  vtkDataArray *ActiveArray = this->ActiveScalars ? 
    input->GetPointData()->GetArray(this->ActiveScalars) : NULL;  

  this->ProcessInformation(this->Information);

  int cellFlag=0;
  vtkDataSet* ds = static_cast<vtkDataSet*>(input);
  vtkDataArray* scalars = vtkAbstractMapper::GetScalars(ds,
    this->ScalarMode, this->ArrayAccessMode, this->ArrayId,
    this->ArrayName, cellFlag);

//  vtkAbstractMapper::
//    GetScalars(this->GetInput(), this->ScalarMode, this->ArrayAccessMode,
//               this->ArrayId, this->ArrayName, cellFlag);
  vtkScalarsToColors *lut = this->ScalarsToColorsPainter->GetLookupTable();


  //
  // if one process has no points, pts will be NULL
  //
  vtkIdType N = pts ? pts->GetNumberOfPoints() : 0;
  double bounds[6];
  input->GetBounds(bounds);
  double length = input->GetLength();
  double radius = N>0 ? length/N : length/1000.0;
  

  IceTInt ids;
  icetGetIntegerv(ICET_NUM_TILES,&ids);
  IceTInt *vp=new IceTInt[4*ids];
  icetGetIntegerv(ICET_TILE_VIEWPORTS,vp);

  //
  // We need the transform that reflects the transform point coordinates according to actor's transformation matrix
  //
  vtkCamera *cam = ren->GetActiveCamera();
  double zmin,zmax;
  ren->GetActiveCamera()->GetClippingRange(zmin, zmax);

  vtkMatrix4x4 *matrix = vtkMatrix4x4::New();
  matrix->DeepCopy(ren->GetActiveCamera()
    ->GetCompositeProjectionTransformMatrix(
    ren->GetTiledAspectRatio(),0,1));

  GLint  VV[4];
  glGetIntegerv(GL_VIEWPORT, VV);

  std::cout << VV[0] << " " << VV[1] << " " << VV[2] << " " << VV[3] << std::endl;

  // size of final image
  int viewsize[2], vieworigin[2];
  ren->GetTiledSizeAndOrigin( &viewsize[0],   &viewsize[1], 
                              &vieworigin[0], &vieworigin[1] );
  
  // viewport info
  double viewPortRatio[2];
  double *viewPort = ren->GetViewport();
  viewPortRatio[0] = (vp[2]*(viewPort[2]-viewPort[0])) / 2.0 +
      viewsize[0]*viewPort[0];
  viewPortRatio[1] = (vp[3]*(viewPort[3]-viewPort[1])) / 2.0 +
      viewsize[1]*viewPort[1];

/*
    // WE CANNOT USE THIS WITH Ice-T
//    vtkLinearTransform *viewCamera_Inv=r->GetActiveCamera()
//      ->GetViewTransformObject()->GetLinearInverse();
//    vtkBreakPoint::Break();
    // REQUIRED with Ice-T
    // We assume that at this point of the execution
    // modelview matrix  is actually on the view matrix, that is
    // model matrix is identity.

    GLfloat m[16];
    glGetFloatv(GL_MODELVIEW_MATRIX,m);
    int row=0;
    while(row<4)
      {
      int column=0;
      while(column<4)
        {
        matrix->SetElement(row,column,static_cast<double>(m[column*4+row]));
        ++column;
        }
      ++row;
      }
//    mat->Invert();
//    vtkMatrixToLinearTransform *viewCamera_Inv
//      =vtkMatrixToLinearTransform::New();
//    viewCamera_Inv->SetInput(mat);

*/





  double view[4];
  double pos[2];

  std::vector<double> mipValues(X*Y, VTK_DOUBLE_MIN);
  for (vtkIdType i=0; i<N; i++) {
    // what particle type is this
    int ptype = TypeArray ? TypeArray->GetTuple1(i) : 0;
    // clamp it to prevent array access faults
    ptype = ptype<this->NumberOfParticleTypes ? ptype : 0;
    // is this particle active, if not skip it
    bool active = this->TypeActive[ptype] && (ActiveArray ? (ActiveArray->GetTuple1(i)!=0) : 1);
    if (!active) continue;

    // if we are active, transform the point and do the mip comparison
    double *p = pts->GetPoint(i);

    // convert from world to view
    view[0] = p[0]*matrix->Element[0][0] + p[1]*matrix->Element[0][1] +
      p[2]*matrix->Element[0][2] + matrix->Element[0][3];
    view[1] = p[0]*matrix->Element[1][0] + p[1]*matrix->Element[1][1] +
      p[2]*matrix->Element[1][2] + matrix->Element[1][3];
    view[2] = p[0]*matrix->Element[2][0] + p[1]*matrix->Element[2][1] +
      p[2]*matrix->Element[2][2] + matrix->Element[2][3];
    view[3] = p[0]*matrix->Element[3][0] + p[1]*matrix->Element[3][1] +
      p[2]*matrix->Element[3][2] + matrix->Element[3][3];
    if (view[3] != 0.0) {
      pos[0] = view[0]/view[3];
      pos[1] = view[1]/view[3];
    }

    int ix = static_cast<int>((pos[0] + 1.0) * viewPortRatio[0] + 0.5);
    int iy = static_cast<int>((pos[1] + 1.0) * viewPortRatio[1] + 0.5);
    int C = scalars ? scalars->GetNumberOfComponents() : 1;
    double *tuple=new double[C];
    bool magnitude = (C>1);
    double scalar=0, mip;

    if (ix>=0 && ix<X && iy>=0 && iy<Y) {
      if (scalars && !magnitude) {
        scalar = scalars->GetTuple1(i);
        mip = mipValues[ix + iy*X];
      }
      else if (scalars && magnitude) {
        scalars->GetTuple(i,tuple);
        scalar = vtkMath::Norm(tuple,C);
        mip = mipValues[ix + iy*X];
      }
      if (scalar>mip) {
        mipValues[ix + iy*X] = scalar;
      }
    }
    delete [] tuple;
  }

    matrix->Delete();

  std::vector<double> mipCollected(X*Y, VTK_DOUBLE_MIN);
//  this->Controller->AllReduce(&mipValues[0], &mipCollected[0]/*(double*)MPI_IN_PLACE*/, X*Y, vtkCommunicator::MAX_OP);
  this->Controller->Reduce(&mipValues[0], &mipCollected[0]/*(double*)MPI_IN_PLACE*/, X*Y, vtkCommunicator::MAX_OP, 0);

  // only convert to colours on master process
  if (this->Controller->GetLocalProcessId()==0) {
    //
    // create an RGB image buffer
    //
    std::vector< RGB_tuple<float> > mipImage(X*Y, RGB_tuple<float>(0,0,0));
    //
    // map mipped scalar values to RGB colours using lookuptable
    // we do one lookup per final pixel, except empty pixels
    //
    RGB_tuple<double> background;
    ren->GetBackground(&background.r);
    for (int ix=0; ix<X; ix++) {
      for (int iy=0; iy<Y; iy++) {
        double pixval = mipCollected[ix + iy*X];
        RGB_tuple<float> &rgbVal = mipImage[ix + iy*X];
        //
        if (pixval==VTK_DOUBLE_MIN) {
          rgbVal.r = background.r;
          rgbVal.g = background.g;
          rgbVal.b = background.b;
        }
        else {
          unsigned char *rgba = lut->MapValue(pixval);
          if (pixval>0) {
            rgbVal.r = (rgba[0]/255.0);
            rgbVal.g = (rgba[1]/255.0);
            rgbVal.b = (rgba[2]/255.0);
          }
          rgbVal.r = (rgba[0]/255.0);
          rgbVal.g = (rgba[1]/255.0);
          rgbVal.b = (rgba[2]/255.0);
        }
      }
    }

    //
    // copy to OpenGL image buffer
    //
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(viewport[0], viewport[2], viewport[1], viewport[3], -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // we draw our image just in front of the back clipping plane, 
    // so all other geometry will appear in front of it.
    glRasterPos3f(0, 0, -0.99);
    float *x0 = &mipImage[0].r;
    glDrawPixels(X, Y, (GLenum)(GL_RGB), (GLenum)(GL_FLOAT), (GLvoid*)(x0));

    glMatrixMode( GL_MODELVIEW );   
    glPopMatrix();
    glMatrixMode( GL_PROJECTION );
    glPopMatrix();
  }
}

