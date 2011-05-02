/******************************************************************************/
/*                                                                            */
/*                     X r d S f s C a l l B a c k . c c                      */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdSfs/XrdSfsCallBack.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XProtocol/XProtocol.hh"
  
/******************************************************************************/
/*                                C r e a t e                                 */
/******************************************************************************/
  
XrdSfsCallBack *XrdSfsCallBack::Create(XrdOucErrInfo *eInfo)
{
   XrdOucEICB        *orgCB;
   XrdSfsCallBack    *newCB;
   unsigned long long orgCBarg;

// Get the original callback object here. If there is none, then callbacks are
// not allowed for this particular request.
//
   if (!(orgCB = eInfo->getErrCB(orgCBarg))) return 0;

// Create a new callback object that will replace the original one. We will
// get a callback after the client is informed to wait for a callback as we
// can't send a response until after the client is placed in callback mode.
//
   newCB = new XrdSfsCallBack(eInfo, orgCB, orgCBarg);
   eInfo->setErrCB((XrdOucEICB *)newCB);
   return newCB;
}

/******************************************************************************/
/*                                  D o n e                                   */
/******************************************************************************/
  
void XrdSfsCallBack::Done(int &Result, XrdOucErrInfo *eInfo)
{

// We get invoked twice. The first time is when the initial response is sent to
// the client placing the client in callback mode. The second time is after the
// callback response is sent which allows us to delete this object.
//
   if (Final) delete this;
      else {Final = 1; cbSetup.Post();}
}

/******************************************************************************/
/* Private:                        R e p l y                                  */
/******************************************************************************/
  
void XrdSfsCallBack::Reply(int Result, int Ecode, const char *Emsg)
{

// Make sure the initial response to the client was sent
//
   cbSetup.Wait();

// Set the error information in the callback object and effect the original
// callback. We will be called back when the reply is sent (see Done()).
//
   setErrInfo(Ecode, (Emsg ? Emsg : ""));
   theCB->Done(Result, (XrdOucErrInfo *)this);
}

/******************************************************************************/
/*                           R e p l y _ E r r o r                            */
/******************************************************************************/
  
void XrdSfsCallBack::Reply_Error(int Ecode, const char *Emsg)
{

// Set error information in the callback object and complete callback. The
// error code mapping is a hack that should be corrected as the callback
// implementation should be responsible for doing this.
//
   Ecode = XProtocol::mapError(Ecode);
   Reply(SFS_ERROR, Ecode, Emsg);
}

/******************************************************************************/
/*                        R e p l y _ R e d i r e c t                         */
/******************************************************************************/
  
void XrdSfsCallBack::Reply_Redirect(const char *Host, int Port)
{

// Set error information in the callback object and complete callback
//
   Reply(SFS_REDIRECT, Port, Host);
}
  
/******************************************************************************/
/*                              R e p l y _ O K                               */
/******************************************************************************/
  
void XrdSfsCallBack::Reply_OK(const char *Data, int Dlen)
{

// Set error information in the callback object and complete callback
//
   if (Dlen) Reply(SFS_DATA, Dlen, Data);
      else   Reply(SFS_OK, 0, 0);
}
  
/******************************************************************************/
/*                           R e p l y _ R e t r y                            */
/******************************************************************************/
  
void XrdSfsCallBack::Reply_Retry(int wSec)
{

// Set error information in the callback object and complete callback
//
   Reply(SFS_STALL, wSec, 0);
}
