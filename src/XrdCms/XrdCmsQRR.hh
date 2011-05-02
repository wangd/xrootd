#ifndef __XRDCMSQRR_HH__
#define __XRDCMSQRR_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d C m s Q R R . h h                           */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id: XrdCmsQRR.hh,v 1.4 2008/06/26 02:32:56 abh Exp $

#include "XrdCms/XrdCmsEvent.hh"
#include "XrdCms/XrdCmsSelect.hh"
#include "XrdCms/XrdCmsTypes.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdCmsQRRData;
class XrdCmsQRRJob;
class XrdCmsQRRSlot;
class XrdCmsQRRSlut;
  
class XrdCmsQRR : public XrdCmsEvent
{
public:
friend class XrdCmsQRRData;
friend class XrdCmsQRRJob;
friend class XrdCmsQRRSlot;

// Add() is called by the cache manager when a client wants a callback when
//       a file becomes ready (i.e., exits pending state).
//
short Add(XrdCmsSelect &Sel, SMask_t qmask, short &Snum);

// Del() is called by the cache manager when a cache line is being removed but
//       it still references a callback.
//
void  Del(XrdCmsKeyItem *Key, short Snum, int Ext=1);

// Init() is called by the configuration manager when a special path has been
//        defined via the "special" directive.
//
int   Init(int maxSlots, int maxData, int pTime, int sTime, int Tries,
           int rdThrds,  int sdThrds);

// Ready() is called by the cache manager when a file becomes ready (i.e.,
//         is no longer in pending state).
//
short Ready(XrdCmsKeyItem *Key, XrdCmsSelect &Sel, SMask_t amask, short Snum);

// Respond() runs in a separate thread and is responsible for final callback
//           processing. The callback either redirects clients of forces them
//           to re-issue the request so that we can restore state information.
//
void *Respond();

// Timeout() runs in a separate thread and is responsible for re-asking servers
//           whether the a file is still in pending state. This is only done
//           for files for which clients are waiting for an excessive time.
//
void *TimeOut();

// The following are additional methods defined for CmsEvent inheritance
//
XrdCmsEvent *addSrv(XrdCmsNode *nP, int notOK);
XrdCmsEvent *drpSrv(XrdCmsNode *nP);
XrdCmsEvent *rstSrv(XrdCmsNode *nP, int notOK);

      XrdCmsQRR() : eventP_adSrv(0), eventP_rsSrv(0), rqReady(0), sResume(0),
                    numSlots(2048), selWait(3), PollWindow(30), PollMax(9) {}
     ~XrdCmsQRR() {}

private:

void             AddGroup(XrdCmsQRRData *dP);
int              Ask(XrdCmsKey &kP, int NodeNum, int NodeIns);
void             DelGroup(XrdCmsQRRData *dP);
int              NumGroup(XrdCmsQRRData *dP);
void             Dispatch(XrdCmsQRRData *dP1st);
void             Redirect(XrdCmsQRRData *dP);
void             Redirect(XrdCmsQRRData *dP, SMask_t Mask);
int              Refresh(XrdCmsQRRJob *Job, int Beg, int End);
void             RespCont(XrdCmsQRRData *xP);
short            RespQueue(short Snum, short Nnum, int Nins);
void             Scan(XrdCmsQRRJob *,
                      int (XrdCmsQRR::*Scanner)(XrdCmsQRRJob *, int, int));
int              TimeOut2(XrdCmsQRRJob *Job, int Beg, int End);
void             UpdGroup(XrdCmsQRRData *dP, short newNum);

XrdCmsEvent     *eventP_adSrv;
XrdCmsEvent     *eventP_rsSrv;

XrdSysMutex      rqMutex;
XrdSysSemaphore  rqReady;
XrdSysSemaphore  sResume;
XrdSysMutex     *tsMutex;     // Target serialization mutex vector
XrdSysMutex      wqMutex;     // Pending  queue
XrdCmsQRRSlot   *Slot;
XrdCmsQRRSlut   *Slut;
unsigned char   *Slum;
XrdCmsQRRData   *Data;        // Request data elements

XrdSysMutex      vvMutex;  // This mutex protects the SMask_t vectors
SMask_t          availVec; // Available servers
SMask_t          readyVec; // Ready     servers
SMask_t          resumVec; // Resumed   servers
SMask_t          queryVec; // Servers that need to be asked for status

int              numSlots; // Number of slots (debugging)
int              numSlums; // Number of slums (debugging)
int              selWait;
int              PollWindow;
char             PollMax;
};

namespace XrdCms
{
extern XrdCmsQRR QRR;

static const int slumSgsz = 255;
}
#endif
