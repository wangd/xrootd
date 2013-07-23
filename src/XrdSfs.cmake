
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_OUC_VERSION   1.0.0 )
set( XRD_OUC_SOVERSION 1 )
set( XRD_OUC_PRELOAD_VERSION   0.0.1 )
set( XRD_OUC_PRELOAD_SOVERSION 0 )

#-------------------------------------------------------------------------------
# The XrdSfs library
#-------------------------------------------------------------------------------
add_library(
  XrdSfs
  SHARED
XrdSfs/XrdSfsCallBack.cc  XrdSfs/XrdSfsNative.cc

)

target_link_libraries(
  XrdSfs
  XrdClient
  XrdUtils
  pthread )

set_target_properties(
  XrdSfs
  PROPERTIES
  VERSION   ${XRD_OUC_VERSION}
  SOVERSION ${XRD_OUC_SOVERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdSfs 
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  DIRECTORY      XrdSfs/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdSfs
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )
