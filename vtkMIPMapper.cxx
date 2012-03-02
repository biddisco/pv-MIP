/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMIPMapper.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkMIPMapper.h"

#include "vtksys/ios/sstream"

#include "vtkgl.h"
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
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTimerLog.h"
#include "vtkTransform.h"
//
#ifdef VTK_USE_MPI
#include "vtkMPI.h"
#include "vtkMPICommunicator.h"
#endif
#include "vtkMultiProcessController.h"

#include <assert.h>

#undef min
#undef max
#include <algorithm>

//----------------------------------------------------------------------------
vtkInstantiatorNewMacro(vtkMIPMapper);
vtkCxxSetObjectMacro(vtkMIPMapper, Controller, vtkMultiProcessController);
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

typedef RGB_tuple<float> COLOUR;

struct particle_sim
  {
  COLOUR e;
  float x,y,z,r,I;
  unsigned short type;
  bool active;

  particle_sim (const COLOUR &e_, float x_, float y_, float z_, float r_,
                float I_, int type_, bool active_)
    : e(e_), x(x_), y(y_), z(z_), r(r_), I(I_), type(type_),
      active(active_) {}

  particle_sim () {}
  };

//----------------------------------------------------------------------------
vtkMIPMapper *vtkMIPMapper::New()
{
  vtkObject* ret = new vtkMIPMapper();
  return static_cast<vtkMIPMapper *>(ret);
}
// ---------------------------------------------------------------------------
vtkMIPMapper::vtkMIPMapper()
{
  this->TypeScalars      = NULL;
  this->ActiveScalars    = NULL;
  this->NumberOfParticleTypes = 0;
  this->SetNumberOfParticleTypes(1); 
  this->GrayAbsorption = 0.001;
  this->Controller = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
}
// ---------------------------------------------------------------------------
vtkMIPMapper::~vtkMIPMapper()
{
  delete []this->TypeScalars;
  delete []this->ActiveScalars;
}
// ---------------------------------------------------------------------------
int vtkMIPMapper::FillInputPortInformation(int port,
  vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  return 1;
}
// ---------------------------------------------------------------------------
double *vtkMIPMapper::GetBounds()
{
  this->GetBounds(this->Bounds);
  return this->Bounds;
}
// ---------------------------------------------------------------------------
void vtkMIPMapper::GetBounds(double *bounds)
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
// ---------------------------------------------------------------------------
void vtkMIPMapper::SetNumberOfParticleTypes(int N)
{
  this->NumberOfParticleTypes = std::max(N,this->NumberOfParticleTypes);
  this->IntensityScalars.resize(this->NumberOfParticleTypes,"");
  this->RadiusScalars.resize(this->NumberOfParticleTypes,"");
  this->Brightness.resize(this->NumberOfParticleTypes,10.5);
  this->LogIntensity.resize(this->NumberOfParticleTypes,0);
  this->TypeActive.resize(this->NumberOfParticleTypes,0);
  this->LogColour.resize(this->NumberOfParticleTypes,0);
}
// ---------------------------------------------------------------------------
void vtkMIPMapper::SetIntensityScalars(int ptype, const char *s)
{
  if (std::string(s)!=this->IntensityScalars[ptype]) {
    this->IntensityScalars[ptype] = s;
    this->Modified();
  }
}
// ---------------------------------------------------------------------------
const char *vtkMIPMapper::GetIntensityScalars(int ptype)
{
  return this->IntensityScalars[ptype].c_str();
}
// ---------------------------------------------------------------------------
void vtkMIPMapper::SetRadiusScalars(int ptype, const char *s)
{
  if (std::string(s)!=this->RadiusScalars[ptype]) {
    this->RadiusScalars[ptype] = s;
    this->Modified();
  }
}
// ---------------------------------------------------------------------------
const char *vtkMIPMapper::GetRadiusScalars(int ptype)
{
  return this->RadiusScalars[ptype].c_str();
}
// ---------------------------------------------------------------------------
void vtkMIPMapper::SetBrightness(int ptype, double b)
{
  if (b!=this->Brightness[ptype]) {
    this->Brightness[ptype] = b;
    this->Modified();
  }
}
// ---------------------------------------------------------------------------
double vtkMIPMapper::GetBrightness(int ptype)
{
  return this->Brightness[ptype];
}
// ---------------------------------------------------------------------------
void vtkMIPMapper::SetLogIntensity(int ptype, int l)
{
  if (l!=this->LogIntensity[ptype]) {
    this->LogIntensity[ptype] = l;
    this->Modified();
  }
}
// ---------------------------------------------------------------------------
int vtkMIPMapper::GetLogIntensity(int ptype)
{
  return this->LogIntensity[ptype];
}
// ---------------------------------------------------------------------------
void vtkMIPMapper::SetTypeActive(int ptype, int a)
{
  if (a!=this->TypeActive[ptype]) {
    this->TypeActive[ptype] = a;
    this->Modified();
  }
}
// ---------------------------------------------------------------------------
int vtkMIPMapper::GetTypeActive(int ptype)
{
  return this->TypeActive[ptype];
}
// ---------------------------------------------------------------------------
// don't need this?
void vtkMIPMapper::SetLogColour(int ptype, int l)
{
  if (l!=this->LogColour[ptype]) {
    this->LogColour[ptype] = l;
    this->Modified();
  }
}
// ---------------------------------------------------------------------------
int vtkMIPMapper::GetLogColour(int ptype)
{
  return this->LogColour[ptype];
}
// ---------------------------------------------------------------------------
template <typename T>
std::string NumToStrSPM(T data) {
  vtksys_ios::ostringstream oss;
//  oss.setf(0,ios::floatfield);
  oss.precision(5);  
  oss << data;
  return oss.str();
}
// ---------------------------------------------------------------------------
void vtkMIPMapper::Render(vtkRenderer *ren, vtkActor *actor)
{
  int X = ren->GetSize()[0];
  int Y = ren->GetSize()[1];
  vtkPointSet *input = this->GetInput();
  vtkPoints *pts = input->GetPoints();
  vtkSmartPointer<vtkPoints> transformedpts = vtkSmartPointer<vtkPoints>::New();
  //
  std::vector<vtkDataArray *> radiusarrays(this->NumberOfParticleTypes);
  std::vector<vtkDataArray *> intensityarrays(this->NumberOfParticleTypes);
  for (int i=0; i<this->NumberOfParticleTypes; i++) {
    radiusarrays[i] = (this->RadiusScalars[i].size()>0) ? 
      input->GetPointData()->GetArray(this->RadiusScalars[i].c_str()) : NULL;
    intensityarrays[i] = (this->IntensityScalars[i].size()>0) ? 
      input->GetPointData()->GetArray(this->IntensityScalars[i].c_str()) : NULL;  
  }
  //
  vtkDataArray *TypeArray = this->TypeScalars ? 
    input->GetPointData()->GetArray(this->TypeScalars) : NULL;  
  //
  vtkDataArray *ActiveArray = this->ActiveScalars ? 
    input->GetPointData()->GetArray(this->ActiveScalars) : NULL;  

  int cellFlag = 0;
  vtkDataArray *scalars = vtkAbstractMapper::
    GetScalars(this->GetInput(), this->ScalarMode, this->ArrayAccessMode,
               this->ArrayId, this->ArrayName, cellFlag);

  //
  // if one process has no points, pts will be NULL
  //
  vtkIdType N = pts ? pts->GetNumberOfPoints() : 0;
  double bounds[6];
  input->GetBounds(bounds);
  double length = input->GetLength();
  double radius = N>0 ? length/N : length/1000.0;
  
  //
  // We need the transform that reflects the transform point coordinates according to actor's transformation matrix
  //
  vtkCamera *cam = ren->GetActiveCamera();
  vtkTransform *transform = vtkTransform::New();
  transform->SetMatrix( cam->GetCompositeProjectionTransformMatrix( 
    ren->GetTiledAspectRatio(), 0, 1 ) 
    );
  double zmin,zmax;
  ren->GetActiveCamera()->GetClippingRange(zmin, zmax);

  vtkMatrix4x4 *matrix = vtkMatrix4x4::New();
  matrix->DeepCopy(ren->GetActiveCamera()
    ->GetCompositeProjectionTransformMatrix(
    ren->GetTiledAspectRatio(),0,1));

  // viewport info
  double viewPortRatio[2];
  int sizex,sizey;

  /* get physical window dimensions */
  if ( ren->GetVTKWindow() ) {
    double *viewPort = ren->GetViewport();
    sizex = ren->GetVTKWindow()->GetSize()[0];
    sizey = ren->GetVTKWindow()->GetSize()[1];
    viewPortRatio[0] = (sizex*(viewPort[2]-viewPort[0])) / 2.0 +
        sizex*viewPort[0];
    viewPortRatio[1] = (sizey*(viewPort[3]-viewPort[1])) / 2.0 +
        sizey*viewPort[1];
  }

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
    if (ix>=0 && ix<X && iy>=0 && iy<Y) {
      double scalar = scalars->GetTuple1(i);
      double    mip = mipValues[ix + iy*X];
      if (scalar>mip) {
        mipValues[ix + iy*X] = scalar;
      }
    }
    //      pref.r      = radiusarrays[pref.type] ? radiusarrays[pref.type]->GetTuple1(i) : radius;
    //      pref.I   = intensityarrays[pref.type] ? intensityarrays[pref.type]->GetTuple1(i) : 1.0;
  }

  this->Controller->AllReduce(&mipValues[0], &mipValues[0], X*Y, vtkCommunicator::MAX_OP);

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
        double pixval = mipValues[ix + iy*X];
        RGB_tuple<float> &rgbVal = mipImage[ix + iy*X];
        //
        if (pixval==VTK_DOUBLE_MIN) {
          rgbVal.r = background.r;
          rgbVal.g = background.g;
          rgbVal.b = background.b;
        }
        else {
          unsigned char *rgba = this->LookupTable->MapValue(pixval);
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
    glLoadIdentity();

//    for (int i=0; i<Y; i++) {
      glRasterPos3f(0, 0, -0.999);
      RGB_tuple<float> *ptr = &mipImage[0];
      float *x0 = &ptr->r;
      glDrawPixels(X, Y, (GLenum)(GL_RGB), (GLenum)(GL_FLOAT), (GLvoid*)(x0));
//    }

    glMatrixMode( GL_PROJECTION );
    glPopMatrix();
    glMatrixMode( GL_MODELVIEW );   
    glPopMatrix();
  }
}

