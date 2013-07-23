
include( XRootDCommon )

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_OUC_VERSION   1.0.0 )
set( XRD_OUC_SOVERSION 1 )
set( XRD_OUC_PRELOAD_VERSION   0.0.1 )
set( XRD_OUC_PRELOAD_SOVERSION 0 )

#-------------------------------------------------------------------------------
# The XrdOuc library
#-------------------------------------------------------------------------------
add_library(
  XrdOuc
  SHARED
XrdOuc/XrdOuca2x.cc        XrdOuc/XrdOucCallBack.cc  XrdOuc/XrdOucMsubs.cc      XrdOuc/XrdOucPup.cc     XrdOuc/XrdOucTokenizer.cc
XrdOuc/XrdOucArgs.cc       XrdOuc/XrdOucCRC.cc       XrdOuc/XrdOucName2Name.cc  XrdOuc/XrdOucReqID.cc   XrdOuc/XrdOucTrace.cc
XrdOuc/XrdOucCacheData.cc  XrdOuc/XrdOucEnv.cc       XrdOuc/XrdOucNList.cc      XrdOuc/XrdOucStream.cc  XrdOuc/XrdOucUtils.cc
XrdOuc/XrdOucCacheDram.cc  XrdOuc/XrdOucExport.cc    XrdOuc/XrdOucNSWalk.cc     XrdOuc/XrdOucString.cc
XrdOuc/XrdOucCacheReal.cc  XrdOuc/XrdOucHashVal.cc   XrdOuc/XrdOucProg.cc       XrdOuc/XrdOucSxeq.cc

)

target_link_libraries(
  XrdOuc
  XrdClient
  XrdUtils
  pthread )

set_target_properties(
  XrdOuc
  PROPERTIES
  VERSION   ${XRD_OUC_VERSION}
  SOVERSION ${XRD_OUC_SOVERSION}
  LINK_INTERFACE_LIBRARIES "" )

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdOuc 
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
  DIRECTORY      XrdOuc/
  DESTINATION    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/XrdOuc
  FILES_MATCHING
  PATTERN "*.hh"
  PATTERN "*.icc" )
