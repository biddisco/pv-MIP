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
#include "vtkPointSet.h"
#include "vtkPointData.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkSmartPointer.h"
#include "vtkTimerLog.h"
#include "vtkTransform.h"
#include "vtkScalarsToColorsPainter.h"
#include "vtkColorTransferFunction.h"
#include "vtkDiscretizableColorTransferFunction.h"
#include "vtkCellArray.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
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
  //
  this->ScalarMode = VTK_SCALAR_MODE_DEFAULT;
  this->UseLookupTableScalarRange = 1; 
  this->ScalarRange[0] = 0.0;
  this->ScalarRange[1] = 1.0;
}
// ---------------------------------------------------------------------------
vtkMIPPainter::~vtkMIPPainter()
{
  delete []this->ArrayName;
  delete []this->TypeScalars;
  delete []this->ActiveScalars;
}
// ---------------------------------------------------------------------------
void vtkMIPPainter::UpdateBounds(double bounds[6])
{
  vtkPointSet *input = vtkPointSet::SafeDownCast(this->GetInput());
  // if it hasn't been set yet, abort.
  if (!input) return;
  input->GetBounds(bounds);
  //
  if (this->Controller) {
    double mins[3]  = {bounds[0], bounds[2], bounds[4]};
    double maxes[3] = {bounds[1], bounds[3], bounds[5]};
    double globalMins[3], globalMaxes[3];
    this->Controller->AllReduce(mins, globalMins, 3, vtkCommunicator::MIN_OP);
    this->Controller->AllReduce(maxes, globalMaxes, 3, vtkCommunicator::MAX_OP);
    bounds[0] = globalMins[0];  bounds[1] = globalMaxes[0];
    bounds[2] = globalMins[1];  bounds[3] = globalMaxes[1];
    bounds[4] = globalMins[2];  bounds[5] = globalMaxes[2];
  }
}
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
  if (info->Has(vtkScalarsToColorsPainter::USE_LOOKUP_TABLE_SCALAR_RANGE()))
    {
    this->SetUseLookupTableScalarRange(info->Get(vtkScalarsToColorsPainter::USE_LOOKUP_TABLE_SCALAR_RANGE()));
    }

  if (info->Has(vtkScalarsToColorsPainter::SCALAR_RANGE()))
    {
    this->SetScalarRange(info->Get(vtkScalarsToColorsPainter::SCALAR_RANGE()));
    }

  if (info->Has(vtkScalarsToColorsPainter::LOOKUP_TABLE()))
    {
    vtkScalarsToColors* lut = vtkScalarsToColors::SafeDownCast(
      info->Get(vtkScalarsToColorsPainter::LOOKUP_TABLE()));
    if (lut)
      {
      this->ScalarsToColorsPainter->SetLookupTable(lut);
      } 
    }

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
//----------------------------------------------------------------------------
void vtkMIP_FloatOrDoubleArrayPointer(vtkDataArray *dataarray, float *&F, double *&D) {
  if (dataarray && vtkFloatArray::SafeDownCast(dataarray)) {                       
    F = vtkFloatArray::SafeDownCast(dataarray)->GetPointer(0);                     
    D = NULL;                                                                      
  }                                                                                
  if (dataarray && vtkDoubleArray::SafeDownCast(dataarray)) {                      
    D = vtkDoubleArray::SafeDownCast(dataarray)->GetPointer(0);                    
    F = NULL;                                                                      
  }                                                                                
  if (dataarray && !F && !D) {                                                     
    vtkGenericWarningMacro(<< dataarray->GetName() << "must be float or double");  
  }                                                                                
}                                                                                  
//----------------------------------------------------------------------------
#define FloatOrDouble(F, D, index) F ? F[index] : D[index]
#define FloatOrDoubleSet(F, D) ((F!=NULL) || (D!=NULL))
//----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// IceT is not exported by paraview, so rather than force lots of include dirs
// and libs, just manually set some defs which will keep the compiler happy
//-----------------------------------------------------------------------------
typedef IceTUnsignedInt32       IceTEnum;
typedef IceTInt32               IceTInt;
typedef void *                  IceTContext;
extern "C" ICET_EXPORT void icetGetIntegerv(IceTEnum pname, IceTInt *params);
extern "C" ICET_EXPORT IceTContext icetGetContext(void);

#define ICET_STATE_ENGINE_START (IceTEnum)0x00000000
#define ICET_NUM_TILES          (ICET_STATE_ENGINE_START | (IceTEnum)0x0010)
#define ICET_TILE_VIEWPORTS     (ICET_STATE_ENGINE_START | (IceTEnum)0x0011)
// ---------------------------------------------------------------------------
void vtkMIPPainter::Render(vtkRenderer* ren, vtkActor* actor, 
  unsigned long typeflags, bool forceCompileOnly)
{
  int X = ren->GetSize()[0];
  int Y = ren->GetSize()[1];
  vtkDataObject *indo = this->GetInput();
  vtkPointSet *input = vtkPointSet::SafeDownCast(indo);
  vtkPoints *pts = input->GetPoints();
  //
  vtkDataArray *TypeArray = this->TypeScalars ? 
    input->GetPointData()->GetArray(this->TypeScalars) : NULL;  
  //
  vtkDataArray *ActiveArray = this->ActiveScalars ? 
    input->GetPointData()->GetArray(this->ActiveScalars) : NULL;  
  //
  // Make sure we have the right color array and other info
  //
  this->ProcessInformation(this->Information);
  //
  // Get the LUT and scalar array
  //
  int cellFlag=0;
  vtkDataSet* ds = static_cast<vtkDataSet*>(input);
  vtkDataArray* scalars = vtkAbstractMapper::GetScalars(ds,
    this->ScalarMode, this->ArrayAccessMode, this->ArrayId,
    this->ArrayName, cellFlag);
  vtkScalarsToColors *s2c = this->ScalarsToColorsPainter->GetLookupTable();
  if (!s2c) {
    this->ScalarsToColorsPainter->CreateDefaultLookupTable();
    s2c = vtkScalarsToColors::SafeDownCast(this->ScalarsToColorsPainter->GetLookupTable());
  }
  if (!this->UseLookupTableScalarRange) {
    s2c->SetRange(this->ScalarRange);
  }
  // We need the viewport/viewsize scaled by the Image Reduction Factor when downsampling
  // with client server. This is a nasty hack because we can't access this information
  // directly.
  // This is the reported size of final image, (which may be wrong)
  int viewsize[2], vieworigin[2];
  ren->GetTiledSizeAndOrigin( &viewsize[0],   &viewsize[1], 
                              &vieworigin[0], &vieworigin[1] );
  // Query IceT for the actual size
  IceTInt ids, vp[32*4] = {0, 0, viewsize[0], viewsize[1],};
  if (icetGetContext()!=NULL) {
    icetGetIntegerv(ICET_NUM_TILES,&ids);
    // when running on a single core, this returns nonsense
    if (ids>0 && ids<32) {
      icetGetIntegerv(ICET_TILE_VIEWPORTS,vp);
    }
  }
  // Here we compute the actual viewport scaling factor with the correct adjusted sizes.
  double viewPortRatio[2];
  double *viewPort = ren->GetViewport();
  viewPortRatio[0] = (vp[2]*(viewPort[2]-viewPort[0])) / 2.0 + viewsize[0]*viewPort[0];
  viewPortRatio[1] = (vp[3]*(viewPort[3]-viewPort[1])) / 2.0 + viewsize[1]*viewPort[1];
  // Oops, we must use the IceT sizes not the renderwindow sizes.
  X = vp[2];
  Y = vp[3];

  //
  // We need the transform that reflects the transform point coordinates according to actor's transformation matrix
  //
  vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
  matrix->DeepCopy(
    ren->GetActiveCamera()->GetCompositeProjectionTransformMatrix(ren->GetTiledAspectRatio(),
    0,1));

  //
  // watch out, if one process has no points, pts array will be NULL
  //
  vtkIdType N = pts ? pts->GetNumberOfPoints() : 0;
  float *pointsF = NULL;
  double *pointsD = NULL;
  if (N>0) {
    vtkMIP_FloatOrDoubleArrayPointer(pts->GetData(), pointsF, pointsD);
  }
  
  //
  // array of final MIP values, one per pixel of final image
  //
  std::vector<double> mipValues(X*Y, VTK_DOUBLE_MIN);
  //
  // transform all points from world coordinates into viewport positions
  //
  int C = scalars ? scalars->GetNumberOfComponents() : 1;
#pragma omp parallel for  
  for (vtkIdType i=0; i<N; i++) {
    // for openmp, disable activeparticles
    // what particle type is this
    int ptype = 0; // TypeArray ? TypeArray->GetTuple1(i) : 0;
    // clamp it to prevent array access faults
    // ptype = ptype<this->NumberOfParticleTypes ? ptype : 0;
    
    // is this particle active, if not skip it
//    bool active = this->TypeActive[ptype] && (ActiveArray ? (ActiveArray->GetTuple1(i)!=0) : 1);
//    if (!active) continue;

    // if we are active, transform the point and do the mip comparison
    //
    double p[3];
    if (pointsF) {
      p[0] = pointsF[i*3+0];
      p[1] = pointsF[i*3+1];
      p[2] = pointsF[i*3+2];
    }
    else {
      p[0] = pointsD[i*3+0];
      p[1] = pointsD[i*3+1];
      p[2] = pointsD[i*3+2];
    }

    double view[4], pos[2];
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
    double tuple[12]; // max tensor arrays size?
    bool magnitude = (C>1);
    double mip;
    
    // plot the point if it exceeds the previous max value at that pixel
    if (ix>=0 && ix<X && iy>=0 && iy<Y) {
      if (scalars && !magnitude) {
        scalars->GetTuple(i,tuple);
        mip = mipValues[ix + iy*X];
      }
      else if (scalars && magnitude) {
        scalars->GetTuple(i,tuple);
        tuple[0] = vtkMath::Norm(tuple,C);
        mip = mipValues[ix + iy*X];
      }
      else { 
        tuple[0] = 0;
      }
      if (tuple[0]>mip) {
        mipValues[ix + iy*X] = tuple[0];
      }
    }
  }

  //
  // Now Gather results from all processes and perform the Max (or other) operation
  //
  std::vector<double> mipCollected(X*Y, VTK_DOUBLE_MIN);
  this->Controller->Reduce(&mipValues[0], &mipCollected[0]/*(double*)MPI_IN_PLACE*/, X*Y, vtkCommunicator::MAX_OP, 0);

  //
  // only convert to colours on master process
  //
  if (this->Controller->GetLocalProcessId()==0) {
    //
    // create an RGB image buffer
    //
//    std::vector< RGB_tuple<float> > mipImage(X*Y, RGB_tuple<float>(0,0,0));
    //
    // map mipped scalar values to RGB colours using lookuptable
    // we do one lookup per final pixel, except empty pixels
    //
    RGB_tuple<double> background;
    RGB_tuple<unsigned char> backgroundchar;
    ren->GetBackground(&background.r);
    if (vtkColorTransferFunction::SafeDownCast(s2c)) {
      vtkColorTransferFunction::SafeDownCast(s2c)->SetNanColor(&background.r);
    }
    backgroundchar.r = static_cast<unsigned char>(background.r*255.0 +0.5);
    backgroundchar.g = static_cast<unsigned char>(background.g*255.0 +0.5);
    backgroundchar.b = static_cast<unsigned char>(background.b*255.0 +0.5);

//#define LUT_METHOD 1
#define OLD_METHOD 1
#ifdef LUT_METHOD
    double nan = vtkMath::Nan();
#pragma omp parallel for  
    for (int ix=0; ix<X; ix++) {
      for (int iy=0; iy<Y; iy++) {
        double pixval = mipCollected[ix + iy*X];
        if (pixval==VTK_DOUBLE_MIN) {
          mipCollected[ix + iy*X] = nan;
        }
      }
    }
    std::vector< RGB_tuple<unsigned char> > mipImageChar(X*Y, RGB_tuple<unsigned char>(0,0,0));
    s2c->MapScalarsThroughTable2(&mipCollected[0], 
                                 &mipImageChar[0].r,
                                 VTK_DOUBLE, 
                                 X*Y,
                                 1,
                                 VTK_RGB);
#endif

#ifdef OLD_METHOD
    std::vector< RGB_tuple<unsigned char> > mipImageChar(X*Y, RGB_tuple<unsigned char>(0,0,0));
    // call before entering openMP block to ensure thread safe build first time
    s2c->Build();
#pragma omp parallel for  
    for (int ix=0; ix<X; ix++) {
      for (int iy=0; iy<Y; iy++) {
        double pixval = mipCollected[ix + iy*X];
        RGB_tuple<unsigned char> &rgbVal = mipImageChar[ix + iy*X];
        //
        if (pixval==VTK_DOUBLE_MIN) {
          rgbVal.r = backgroundchar.r;
          rgbVal.g = backgroundchar.g;
          rgbVal.b = backgroundchar.b;
        }
        else {
          // @TODO : MapValue appears to be thread safe if s2c is a vtkDiscretizableColorTransferFunction
          unsigned char *rgba = s2c->MapValue(pixval);
          rgbVal.r = rgba[0];
          rgbVal.g = rgba[1];
          rgbVal.b = rgba[2];
/*
          // @TODO : not sure that MapValue is thread safe
          double rgb[3];
          s2c->vtkScalarsToColors::GetColor(pixval, rgb);
          rgbVal.r = static_cast<unsigned char>(rgb[0]*255 + 0.5);
          rgbVal.g = static_cast<unsigned char>(rgb[1]*255 + 0.5);
          rgbVal.b = static_cast<unsigned char>(rgb[2]*255 + 0.5);
*/
        }
      }
    }
#endif
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

#ifdef OLD_METHOD
    unsigned char *xx0 = &mipImageChar[0].r;    
    glDrawPixels(X, Y, (GLenum)(GL_RGB), (GLenum)(GL_UNSIGNED_BYTE), (GLvoid*)(xx0));
#endif

#ifdef LUT_METHOD
    unsigned char *xx0 = &mipImageChar[0].r;    
    glDrawPixels(X, Y, (GLenum)(GL_RGB), (GLenum)(GL_UNSIGNED_BYTE), (GLvoid*)(xx0));
#endif

    glMatrixMode( GL_MODELVIEW );   
    glPopMatrix();
    glMatrixMode( GL_PROJECTION );
    glPopMatrix();
  }
}

