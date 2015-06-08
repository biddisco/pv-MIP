PROJECT("pv_MIP")

#--------------------------------------------------
# cmake old/new compatibility
#--------------------------------------------------
cmake_minimum_required(VERSION 2.8)

#--------------------------------------------------
# cmake policies
# see http://www.cmake.org/cmake/help/v2.8.12/cmake.html#section_Policies
#--------------------------------------------------
foreach(p
    CMP0003 
    CMP0017
    CMP0022 
    CMP0023 
    )
  if(POLICY ${p})
    cmake_policy(SET ${p} NEW)
  endif()
endforeach()

#--------------------------------------------------
# Find and Use ParaView
#--------------------------------------------------
IF (ParaView_SOURCE_DIR)
  IF(PARAVIEW_BUILD_QT_GUI)
    INCLUDE(${QT_USE_FILE})
  ENDIF(PARAVIEW_BUILD_QT_GUI)
  INCLUDE_DIRECTORIES(
    ${HDF5_INCLUDE_DIR}
    ${PARAVIEW_INCLUDE_DIRS}
    ${PARAVIEW_GUI_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIR}
  )
ELSE (ParaView_SOURCE_DIR)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
ENDIF (ParaView_SOURCE_DIR)

#
# For the Painter we need access to some IceT viewport info
# It's awful, but there's no other way to get the image reduction factor
#
INCLUDE_DIRECTORIES(${ParaView_BINARY_DIR}/Utilities/IceT/src/include)

#------------------------------------------------------
# MPI support if ParaView compiled with MPI
#------------------------------------------------------
if(PARAVIEW_USE_MPI)
  FIND_PACKAGE(MPI)   
  INCLUDE_DIRECTORIES(${MPI_INCLUDE_PATH})
  ADD_DEFINITIONS(-DUSE_MPI)
  set(SPLOTCH_MPI_LIBS ${MPI_LIBRARY} ${MPI_C_LIBRARIES})
endif(PARAVIEW_USE_MPI)

#if(PARAVIEW_USE_ICE_T)
  ADD_DEFINITIONS(-DUSE_ICET)
#endif(PARAVIEW_USE_ICE_T)

# --------------------------------------------------
# We must build sub libraries with -fPIC
# --------------------------------------------------
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#--------------------------------------------------
# Set project include directories 
#--------------------------------------------------
INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
)

#--------------------------------------------------
# Source files, that are wrapped by Client/Server
#--------------------------------------------------
SET( MIP_plugin_WRAPPED_SRCS
  vtkMIPRepresentation.cxx  
  vtkMIPDefaultPainter.cxx
  vtkMIPPainter.cxx
)

#--------------------------------------------------
# Define Plugin
#--------------------------------------------------
SET(PLUGIN_NAME pv_MIP)

ADD_PARAVIEW_PLUGIN(
  ${PLUGIN_NAME}
  "1.0" 

  SERVER_MANAGER_XML 
    ${CMAKE_CURRENT_SOURCE_DIR}/vtkMIPRepresentation.xml
  SERVER_MANAGER_SOURCES
    ${MIP_plugin_WRAPPED_SRCS}
  SERVER_SOURCES
  GUI_RESOURCE_FILES
  GUI_INTERFACES 
    ${IFACES} 
  GUI_SOURCES 
    ${Plugin_SRCS} 
    ${MOC_SRCS} 
    ${UI_SRCS} 
    ${IFACE_SRCS}
)

TARGET_LINK_LIBRARIES(${PLUGIN_NAME} 
  PUBLIC
    ${MPI_LIBRARY}
)

SET_TARGET_PROPERTIES(${PLUGIN_NAME} PROPERTIES COMPILE_FLAGS "${OpenMP_CXX_FLAGS}")

#--------------------------------------------------------
# Create the UsePackage configuration for other projects
# that will be using this library
#--------------------------------------------------------
SET(MIP_plugin_INCLUDE_DIRS "${PROJECT_SOURCE_DIR};${PROJECT_BINARY_DIR}")

#--------------------------------------------------------
# Create the module import/export configuration header.
#--------------------------------------------------------
CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/module_configure.h.in
               ${PROJECT_BINARY_DIR}/pv_MIP_configure.h
               @ONLY IMMEDIATE)

#--------------------------------------------------
# Install
#--------------------------------------------------
SET(INSTALL_PATH 
  "${CMAKE_INSTALL_PREFIX}/lib/paraview-${PARAVIEW_VERSION_MAJOR}.${PARAVIEW_VERSION_MINOR}"
)

INSTALL(
  TARGETS 
    pv_MIP
  DESTINATION ${INSTALL_PATH}
)

#--------------------------------------------------
# Testing
#--------------------------------------------------
if(BUILD_TESTING)
  SET(PLUGIN_TEST_DIR ${PROJECT_BINARY_DIR}/Testing/Temporary)
  MAKE_DIRECTORY(${PLUGIN_TEST_DIR})
#  ADD_SUBDIRECTORY(Testing)
endif(BUILD_TESTING)  

