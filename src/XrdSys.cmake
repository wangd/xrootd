
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_SYS_VERSION   1.0.0 )
set( XRD_SYS_SOVERSION 1 )
set( XRD_SYS_PRELOAD_VERSION   0.0.1 )
set( XRD_SYS_PRELOAD_SOVERSION 0 )

#-------------------------------------------------------------------------------
# The XrdSys library
#-------------------------------------------------------------------------------
add_library(
  XrdSys
  SHARED
  XrdSys/XrdSysDir.cc    XrdSys/XrdSysFAttr.cc     XrdSys/XrdSysPlugin.cc 
  XrdSys/XrdSysTimer.cc
  XrdSys/XrdSysDNS.cc    XrdSys/XrdSysLogger.cc    XrdSys/XrdSysPriv.cc
  XrdSys/XrdSysXSLock.cc
  XrdSys/XrdSysError.cc  XrdSys/XrdSysPlatform.cc  XrdSys/XrdSysPthread.cc
)

target_link_libraries(
  XrdSys
  XrdClient
  XrdUtils
  pthread )

set_target_properties(
  XrdSys
  PROPERTIES
  VERSION   ${XRD_SYS_VERSION}
  SOVERSION ${XRD_SYS_SOVERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdSys 
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  DIRECTORY      XrdSys/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdSys
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )
