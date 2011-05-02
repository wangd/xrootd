#ifndef _XRDSFSCALLBACK_H_
#define _XRDSFSCALLBACK_H_
/******************************************************************************/
/*                                                                            */
/*                     X r d S f s C a l l B a c k . h h                      */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSys/XrdSysPthread.hh"
  
// This class provides an easy-to-use interface to generate callback events.
//
// 1) Create a callback object using Create()
// 2) Return to the caller with return code SFS_STARTED. This will cause the
//    client to wait for a callback. Note that you must queue the callback
//    object for processing in some other thread as your will be returning.
// 3) After processing is completed, use ONE of the reply methods associated
//    with the clallback object to tell the client how to proceed. Once you
//    called any of the reply methods, the object subject of the reply is
//    automatically deleted as only one callback is allowed per instance.
//
class XrdSfsCallBack : public XrdOucErrInfo, public XrdOucEICB
{
public:

// Create() creates a callback object based on callback information present
//          in the XrdOucErrInfo object associated with the request. If the
//          request does not allow callback's, a null pointer is returned.
//
static XrdSfsCallBack *Create(XrdOucErrInfo *eInfo);

// The following are reply method to complete the callback. The callback object
// is automatically deleted upon return.

// Reply_Error()    calls back the client indicating that the request failed
//                  with the given error number and message text.
//
       void         Reply_Error(int Ecode, const char *Emsg=0);

// Reply_Redirect() calls back the client indicating that the request should be
//                  tried again at the indicated host:port.
//
       void         Reply_Redirect(const char *Host, int Port);

// Reply_OK()       calls back the client indicating that the request was
//                  completed. The client can also be given optional response
//                  data (ASCII text up to 2K).
//
       void         Reply_OK(const char *Data=0, int Dlen=0);

// Reply_Retry()    calls back the client indicating that the request should be
//                  tried again after waiting for 'sec' seconds.
//
       void         Reply_Retry(int sec=0);

// The following two methods are for internal use only but need to be public.
//
void Done(int &Result, XrdOucErrInfo *eInfo);

int  Same(unsigned long long arg1, unsigned long long arg2) {return 0;}

                   ~XrdSfsCallBack() {}

private:
                    XrdSfsCallBack(XrdOucErrInfo *eInfo, XrdOucEICB *cbP,
                                   unsigned long long cbArg)
                                 : XrdOucErrInfo(eInfo->getErrUser(), 0, cbArg),
                                   cbSetup(0), theCB(cbP), Final(0) {}

void Reply(int Result, int Ecode, const char *Emsg);

XrdSysSemaphore     cbSetup;
XrdOucEICB         *theCB;
int                 Final;
};
#endif
