/******************************************************************************/
/*                                                                            */
/*                      X r d S s i F i l e R e q . c c                       */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSfs/XrdSfsDio.hh"
#include "XrdSfs/XrdSfsXio.hh"
#include "XrdSsi/XrdSsiFile.hh"
#include "XrdSsi/XrdSsiFileReq.hh"
#include "XrdSsi/XrdSsiRRInfo.hh"
#include "XrdSsi/XrdSsiSfs.hh"
#include "XrdSsi/XrdSsiStream.hh"
#include "XrdSsi/XrdSsiTrace.hh"
#include "XrdSys/XrdSysError.hh"
  
/******************************************************************************/
/*                          L o c a l   M a c r o s                           */
/******************************************************************************/
  
#define DEBUGXQ(x) DEBUG(rID <<sessN <<stID[myState] <<x)

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdSsi
{
extern XrdOucTrace    Trace;
extern XrdSysError    Log;
extern XrdScheduler  *Sched;
};

using namespace XrdSsi;

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
XrdSysMutex     XrdSsiFileReq::aqMutex;
XrdSsiFileReq  *XrdSsiFileReq::freeReq = 0;
int             XrdSsiFileReq::freeCnt = 0;
int             XrdSsiFileReq::freeMax = 256;
int             XrdSsiFileReq::cbRetD  = SFS_DATA;

const char     *XrdSsiFileReq::stID[XrdSsiFileReq::rsEnd] =
                                   {" [wtReq] ", " [xqReq] ",
                                    " [doRsp] ", " [odRsp] ", " [erRsp] "
                                   };

/******************************************************************************/
/*                              A c t i v a t e                               */
/******************************************************************************/
  
void XrdSsiFileReq::Activate(XrdOucBuffer *oP, XrdSfsXioHandle *bR, int rSz)
{
   EPNAME("Activate");

// Do some debugging
//
   DEBUGXQ((oP ? "oucbuff" : "sfsbuff") <<" rqsz=" <<rSz);

// Set request buffer pointers
//
   oucBuff = oP;
   sfsBref = bR;
   reqSize = rSz;

// Now schedule ourselves to process this request indicating we are active
//
   isActive = true;
   Sched->Schedule((XrdJob *)this);
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/

XrdSsiFileReq *XrdSsiFileReq::Alloc(XrdOucErrInfo *eiP,
                                    XrdSsiSession *sP,
                                    const char    *sID,
                                    const char    *cID,
                                    int            rnum)
{
   XrdSsiFileReq *nP;

// Check if we can grab this from out queue
//
   aqMutex.Lock();
   if ((nP = freeReq))
      {freeCnt--;
       freeReq = nP->nextReq;
       aqMutex.UnLock();
       nP->Init(cID);
      } else {
       aqMutex.UnLock();
       nP = new XrdSsiFileReq(cID);
      }

// Initialize for processing
//
   if (nP)
      {nP->sessN  = sID;
       nP->sessP  = sP;
       nP->cbInfo = eiP;
       nP->reqID = rnum;
       snprintf(nP->rID, sizeof(rID), "%d:", rnum);
      }

// Return the pointer
//
   return nP;
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdSsiFileReq::DoIt()
{
   EPNAME("DoIt");

// Check if we were cancelled prior to being dispatched, otherwise process it
//
   myMutex.Lock();
   if (isActive)
      {isExported = true;
       myState = xqReq;
       myMutex.UnLock();
       DEBUGXQ("Calling session Process");
       if (sessP->ProcessRequest((XrdSsiRequest *)this)) return;
       if ( RespP()->rType != XrdSsiRespInfo::isError)
          {int eCode;
           const char *eText = eInfo.Get(eCode);
           if (!eCode) eCode = EPROTO;
           if (!eText) eText = "Server session logic error!";
           if (SetErrResponse(eText, eCode))
              Log.Emsg("DoIt", tident, "Invalid session process return!!");
          }
      } else {
       myMutex.UnLock();
       DEBUGXQ("Skipped session Process");
       Recycle();
      }
}

/******************************************************************************/
/*                                  D o n e                                   */
/******************************************************************************/

// Gets invoked only after sync() waitresp signal
  
void XrdSsiFileReq::Done(int &retc, XrdOucErrInfo *eiP, const char *name)
{
   EPNAME("Done");
   XrdSysMutexHelper mHelper(&myMutex);

// Do some debugging
//
   DEBUGXQ("wtrsp sent; resp "<<(RespP()->rType ? "pend" : "here"));

// We are invoked when sync() waitresp has been sent, check if a response was
// posted while this was going on. If so, make sure to send a wakepup.
//
   if (RespP()->rType == XrdSsiRespInfo::isNone) respWait = true;
      else WakeUp();
}

/******************************************************************************/
/* Private:                         E m s g                                   */
/******************************************************************************/

int XrdSsiFileReq::Emsg(const char    *pfx,    // Message prefix value
                        int            ecode,  // The error code
                        const char    *op)     // Operation being performed
{
   char buffer[2048];

// Get correct error code
//
   if (ecode < 0) ecode = -ecode;

// Format the error message
//
   XrdOucERoute::Format(buffer, sizeof(buffer), ecode, op, sessN);

// Put the message in the log
//
   Log.Emsg(pfx, tident, buffer);

// Place the error message in the error object and return
//
   if (cbInfo) cbInfo->setErrInfo(ecode, buffer);
   return SFS_ERROR;
}

/******************************************************************************/

int XrdSsiFileReq::Emsg(const char    *pfx,    // Message prefix value
                        XrdSsiErrInfo &eObj,   // The error description
                        const char    *op)     // Operation being performed
{
   const char *eMsg;
   char buffer[2048];
   int  eNum;

// Get correct error code and message
//
   eMsg = eObj.Get(eNum);
   if (eNum <= 0) eNum = EFAULT;
   if (!eMsg || !(*eMsg)) eMsg = "reason unknown";

// Format the error message
//
   snprintf(buffer, sizeof(buffer), "Unable to %s %s; %s", op, sessN, eMsg);

// Put the message in the log
//
   Log.Emsg(pfx, tident, buffer);

// Place the error message in the error object and return
//
   if (cbInfo) cbInfo->setErrInfo(eNum, buffer);
   return SFS_ERROR;
}
  
/******************************************************************************/
/*                              F i n a l i z e                               */
/******************************************************************************/
  
void XrdSsiFileReq::Finalize()
{
   EPNAME("Finalize");
   XrdSysMutexHelper mHelper(&myMutex);
   bool cancel = (myState != odRsp);

// Do some debugging
//
   DEBUGXQ("active="<<isActive<<" exported="<<isExported<<" cancel="<<cancel);

// If we are not yet exported then simply recycle ourselves
//
   if (!isActive) {mHelper.UnLock(); Recycle(); return;}

// When isActive && !isExported implies a scheduled thread against this object.
// That thread will then be responsible for recycling this object.
//
   if (!isExported)
      {isActive = false;
       sessN = "???";
       cbInfo = 0;
       return;
      }

// If session process was called then tell the session we are finished.
//
   if (strBuff) {strBuff->Recycle(); strBuff = 0;}
   isActive = false; isExported = false;
   mHelper.UnLock();
   Finished(cancel);

// We now have control of the request object. Terminate respWait state if on.
//
   if (respWait) WakeUp();

// Now we can recycle ourselves
//
   Recycle();
}

/******************************************************************************/
/*                            G e t R e q u e s t                             */
/******************************************************************************/
  
char *XrdSsiFileReq::GetRequest(int &rLen)
{
   EPNAME("GetRequest");

// Do some debugging
//
   DEBUGXQ("sz=" <<reqSize);

// The request may come from a ouc buffer or an sfs buffer
//
   rLen = reqSize;
   if (oucBuff) return oucBuff->Data();
   return sfsBref->Buffer();
}

/******************************************************************************/
/* Private:                         I n i t                                   */
/******************************************************************************/

void XrdSsiFileReq::Init(const char *cID)
{
   tident     = (cID ? strdup(cID) : strdup("???"));
   nextReq    = 0;
   cbInfo     = 0;
   sessN      = "???";
   sessP      = 0;
   oucBuff    = 0;
   sfsBref    = 0;
   strBuff    = 0;
   reqSize    = 0;
   respBuf    = 0;
   respOff    = 0;
   fileSz     = 0; // Also does respLen = 0;
   myState    = wtReq;
  *rID        = 0;
   isActive   = false;
   isExported = false;
   respWait   = false;
   strmEOF    = false;
}

/******************************************************************************/
/* Protected:            P r o c e s s R e s p o n s e                        */
/******************************************************************************/
  
bool XrdSsiFileReq::ProcessResponse(const XrdSsiRespInfo &Resp, bool isOK)
{
   EPNAME("ProcessResponse");
   XrdSysMutexHelper mHelper(&myMutex);

// Make sure we are still in execute state
//
   if (!isActive) return false;
   myState = doRsp;
   respOff = 0;

// Handle the response
//
   DEBUGXQ("Response present wtr=" <<respWait);
   switch(Resp.rType)
         {case XrdSsiRespInfo::isData:
               DEBUGXQ("Resp data sz="<<Resp.blen);
               respLen = Resp.blen;
               break;
          case XrdSsiRespInfo::isError:
               DEBUGXQ("Resp err rc="<<Resp.eNum<<" msg="<<Resp.eMsg);
               respLen = 0;
               break;
          case XrdSsiRespInfo::isFile:
               DEBUGXQ("Resp file fd="<<Resp.fdnum<<" sz="<<Resp.fsize);
               fileSz  = Resp.fsize;
               respOff = 0;
               break;
          case XrdSsiRespInfo::isStream:
               DEBUGXQ("Resp strm");
               respLen = 0;
               break;
          default:
               DEBUGXQ("Resp invalid!!!!");
               return false;
               break;
         }

// If the client is waiting for the response, wake up the client to get it.
//
   if (respWait) WakeUp();
   return true;
}
  
/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

XrdSfsXferSize XrdSsiFileReq::Read(bool           &done,      // Out
                                   char           *buff,      // Out
                                   XrdSfsXferSize  blen)      // In
/*
  Function: Read `blen' bytes at `offset' into 'buff' and return the actual
            number of bytes read.

  Input:    buff      - Address of the buffer in which to place the data.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be returned.

  Output:   Returns the number of bytes read upon success and SFS_ERROR o/w.
*/
{
   static const char *epname = "read";
   XrdSfsXferSize nbytes;
   XrdSsiRespInfo const *Resp = RespP();

// A read should never be issued unless a response has been set
//
   if (myState != doRsp)
      {done = true;
       return (myState == odRsp ? 0 : Emsg(epname, ENOMSG, "read"));
      }

// Fan out based on the kind of response we have
//
   switch(Resp->rType)
         {case XrdSsiRespInfo::isData:
               if (respLen <= 0) {done = true; myState = odRsp; return 0;}
               if (blen >= respLen)
                  {memcpy(buff, Resp->buff+respOff, respLen);
                   blen = respLen; myState = odRsp; done = true;
                  } else {
                   memcpy(buff, Resp->buff+respOff, blen);
                   respLen -= blen; respOff += blen;
                  }
               return blen;
               break;
          case XrdSsiRespInfo::isError:
               cbInfo->setErrInfo(Resp->eNum, Resp->eMsg);
               myState = odRsp; done = true;
               return SFS_ERROR;
               break;
          case XrdSsiRespInfo::isFile:
               if (fileSz <= 0) {done = true; myState = odRsp; return 0;}
               nbytes = pread(Resp->fdnum, buff, blen, respOff);
               if (nbytes <= 0)
                  {done = true;
                   if (!nbytes) {myState = odRsp; return 0;}
                   myState = erRsp;
                   return Emsg(epname, errno, "read");
                  }
               respOff += nbytes; fileSz -= nbytes;
               return nbytes;
               break;
          case XrdSsiRespInfo::isStream:
               nbytes = (Resp->strmP->Type() == XrdSsiStream::isActive ?
                         readStrmA(Resp->strmP, buff, blen)
                      :  readStrmP(Resp->strmP, buff, blen));
               done = strmEOF;
               return nbytes;
               break;
          default: break;
         };

// We should never get here
//
   myState = erRsp;
   done    = true;
   return Emsg(epname, EFAULT, "read");
}

/******************************************************************************/
/* Private:                    r e a d S t r m A                              */
/******************************************************************************/
  
XrdSfsXferSize XrdSsiFileReq::readStrmA(XrdSsiStream *strmP,
                                        char *buff, XrdSfsXferSize blen)
{
   static const char *epname = "readStrmA";
   XrdSsiErrInfo  eObj;
   XrdSfsXferSize xlen = 0;


// Copy out data from the stream to fill the buffer
//
do{if (strBuff)
      {if (respLen > blen)
          {memcpy(buff, strBuff->data+respOff, blen);
           respLen -= blen; respOff += blen;
           return xlen+blen;
          }
       memcpy(buff, strBuff->data+respOff, respLen);
       xlen += respLen;
       strBuff->Recycle(); strBuff = 0;
       if (respLen == blen) return xlen;
       blen -= respLen; buff += respLen;
      }

   if (strmEOF) myState = odRsp;
      else {respLen = blen; respOff = 0;
            strBuff = strmP->GetBuff(eObj, respLen, strmEOF);
           }
  } while(strBuff);

// On eof, if we have some data to return, do so
//
   if (strmEOF) {myState = odRsp; return xlen;}

// Report the error
//
   myState = erRsp; strmEOF = true;
   return Emsg(epname, eObj, "read stream");
}

/******************************************************************************/
/* Private:                    r e a d S t r m P                              */
/******************************************************************************/
  
XrdSfsXferSize XrdSsiFileReq::readStrmP(XrdSsiStream *strmP,
                                        char *buff, XrdSfsXferSize blen)
{
   static const char *epname = "readStrmP";
   XrdSsiErrInfo  eObj;
   XrdSfsXferSize xlen = 0;
   int dlen = 0;

// Copy out data from the stream to fill the buffer
//
   while(!strmEOF && (dlen = strmP->SetBuff(eObj, buff, blen, strmEOF)) > 0)
        {xlen += dlen;
         if (dlen == blen) return xlen;
         if (dlen  > blen) {eObj.Set(0,EOVERFLOW); break;}
         buff += dlen; blen -= dlen;
        }

// Check if we ended with an zero length read
//
   if (strmEOF || !dlen)  {myState = odRsp; strmEOF = true; return xlen;}

// Return an error
//
   myState = erRsp; strmEOF = true;
   return Emsg(epname, eObj, "read stream");
}
  
/******************************************************************************/
/* Private:                      R e c y c l e                                */
/******************************************************************************/
  
void XrdSsiFileReq::Recycle()
{

// If we have an oucbuffer then we need to recycle it, otherwise if we have
// and sfs buffer, put it on the defered release queue.
//
        if (oucBuff) {oucBuff->Recycle(); oucBuff = 0;}
   else if (sfsBref) {sfsBref->Recycle(); sfsBref = 0;}
   reqSize = 0;

// Add to queue unless we have too many of these
//
   aqMutex.Lock();
   if (tident) {free(tident); tident = 0;}
   if (freeCnt >= freeMax) {aqMutex.UnLock(); delete this;}
      else {nextReq = freeReq;
            freeReq = this;
            freeCnt++;
            aqMutex.UnLock();
           }
}

/******************************************************************************/
/*                               R e l B u f f                                */
/******************************************************************************/
  
void XrdSsiFileReq::RelBuff()
{
   EPNAME("RelBuff");
   XrdSysMutexHelper mHelper(&myMutex);

// Do some debugging
//
   DEBUGXQ("called");

// Release buffers
//
        if (oucBuff) {oucBuff->Recycle(); oucBuff = 0;}
   else if (sfsBref) {sfsBref->Recycle(); sfsBref = 0;}
   reqSize = 0;
}

/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/

int XrdSsiFileReq::Send(XrdSfsDio *sfDio, XrdSfsXferSize blen)
{
   static const char *epname = "send";
   XrdSsiRespInfo const *Resp = RespP();
   XrdOucSFVec sfVec[2];
   int rc;

// A send should never be issued unless a response has been set. Return a
// continuation which will cause Read() to be called to return the error.
//
   if (myState != doRsp) return 1;

// Fan out based on the kind of response we have
//
   switch(Resp->rType)
         {case XrdSsiRespInfo::isData:
               if (blen > 0)
                  {sfVec[1].buffer = (char *)Resp->buff+respOff;
                   sfVec[1].fdnum  = -1;
                   if (blen > respLen)
                      {blen = respLen; myState = odRsp;
                      } else {
                       respLen -= blen; respOff += blen;
                      }
                  } else blen = 0;
               break;
          case XrdSsiRespInfo::isError:
               return 1; // Causes error to be returned via Read()
               break;
          case XrdSsiRespInfo::isFile:
               if (fileSz > 0)
                  {sfVec[1].offset = respOff; sfVec[1].fdnum = Resp->fdnum;
                   if (blen > fileSz)
                      {blen = fileSz; myState = odRsp;}
                   respOff += blen; fileSz -= blen;
                  } else blen = 0;
               break;
          case XrdSsiRespInfo::isStream:
               if (Resp->strmP->Type() == XrdSsiStream::isPassive) return 1;
               return sendStrmA(Resp->strmP, sfDio, blen);
               break;
          default: myState = erRsp;
                   return Emsg(epname, EFAULT, "send");
                   break;
         };

// Send off the data
//
   if (!blen) {sfVec[1].buffer = rID; myState = odRsp;}
   sfVec[1].sendsz = blen;
   rc = sfDio->SendFile(sfVec, 2);

// If send succeeded, indicate the action to be taken
//
   if (!rc) return myState != odRsp;

// The send failed, diagnose the problem
//
   rc = (rc < 0 ? EIO : EFAULT);
   myState = erRsp;
   return Emsg(epname, rc, "send");
}

/******************************************************************************/
/* Private:                    s e n d S t r m A                              */
/******************************************************************************/
  
int XrdSsiFileReq::sendStrmA(XrdSsiStream *strmP,
                             XrdSfsDio *sfDio, XrdSfsXferSize blen)
{
   static const char *epname = "sendStrmA";
   XrdSsiErrInfo  eObj;
   XrdOucSFVec    sfVec[2];
   int rc;

// Check if we need a buffer
//
   if (!strBuff)
      {respLen = blen;
       if (strmEOF || !(strBuff = strmP->GetBuff(eObj, respLen, strmEOF)))
          {myState = odRsp; strmEOF = true;
           if (!strmEOF) Emsg(epname, eObj, "read stream");
           return 1;
          }
       respOff = 0;
      }

// Complete the sendfile vector
//
   sfVec[1].buffer = strBuff->data+respOff;
   sfVec[1].fdnum  = -1;
   if (respLen > blen)
      {sfVec[1].sendsz = blen;
       respLen -= blen; respOff += blen;
      } else {
       sfVec[1].sendsz = respLen;
       respLen = 0;
      }

// Send off the data
//
   rc = sfDio->SendFile(sfVec, 2);

// Release any completed buffer
//
   if (strBuff && !respLen) {strBuff->Recycle(); strBuff = 0;}

// If send succeeded, indicate the action to be taken
//
   if (!rc) return myState != odRsp;

// The send failed, diagnose the problem
//
   rc = (rc < 0 ? EIO : EFAULT);
   myState = erRsp; strmEOF = true;
   return Emsg(epname, rc, "send");
}

/******************************************************************************/
/*                          W a n t R e s p o n s e                           */
/******************************************************************************/
  
bool XrdSsiFileReq::WantResponse(XrdOucEICB *rCB, long long rArg)
{
   XrdSysMutexHelper mHelper(&myMutex);

// Check if a response is here
//
   if (RespP()->rType) return true;
//    {int mlen;
//     WakeInfo((XrdSsiRRInfoRdy *)eInfo->getMsgBuff(mlen));
//     eInfo->setErrCode(sizeof(XrdSsiRRInfoRdy));
//     return true;
//    }

// Defer this by and record the callback arguments
//
   respCB    = rCB;
   respCBarg = rArg;
   respWait  = true;
   return false;
}

/******************************************************************************/
/*                              W a k e I n f o                               */
/******************************************************************************/
  
void XrdSsiFileReq::WakeInfo(XrdSsiRRInfo *rdyInfo) // myMutex is locked!
{

// Set appropriate response information
//
   rdyInfo->RR.Id = reqID;
   switch(RespP()->rType)
         {case XrdSsiRespInfo::isData:
               rdyInfo->RR.Size = htonl(respLen);
               break;
          case XrdSsiRespInfo::isError:
               rdyInfo->RR.Size = -1;
               break;
          case XrdSsiRespInfo::isFile:
               if (fileSz & 0xffffffff80000000LL) rdyInfo->RR.Size = 0;
                  rdyInfo->RR.Size = htonl(static_cast<int>(fileSz));
               break;
          case XrdSsiRespInfo::isStream:
               rdyInfo->RR.Size = 0;
               break;
          default:
               rdyInfo->RR.Size = htonl(-2);
               break;
         }
}
  
/******************************************************************************/
/* Private:                       W a k e U p                                 */
/******************************************************************************/
  
void XrdSsiFileReq::WakeUp() // Called with myMutex locked!
{
   EPNAME("WakeUp");
   XrdOucErrInfo *wuInfo = new XrdOucErrInfo(tident,(XrdOucEICB *)0,respCBarg);
   int mlen;

// We will be placing the response in the cbInfo object
//
   WakeInfo((XrdSsiRRInfo *)wuInfo->getMsgBuff(mlen));
   wuInfo->setErrCode(sizeof(XrdSsiRRInfo));

// Do some debugging
//
   DEBUGXQ("");

// Tell the client to issue a read now. We don't need a callback on this so
// the callback handler will delete the errinfo object for us.
//
   wuInfo->setErrInfo(0,rID);
   respCB->Done(cbRetD, wuInfo, sessN);

// Finish up;
//
   respWait = false;
}
