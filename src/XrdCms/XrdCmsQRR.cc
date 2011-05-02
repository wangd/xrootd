/******************************************************************************/
/*                                                                            */
/*                          X r d C m s Q R R . c c                           */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id: XrdCmsQRR.cc,v 1.4 2008/06/26 02:32:56 abh Exp $

// Original Version: 1.6 2007/07/31 02:25:16 abh

const char *XrdCmsQRRCVSID = "$Id: XrdCmsQRR.cc,v 1.4 2008/06/26 02:32:56 abh Exp $";

#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/types.h>

#include "XProtocol/XPtypes.hh"
#include "XProtocol/YProtocol.hh"

#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdCms/XrdCmsCache.hh"
#include "XrdCms/XrdCmsCluster.hh"
#include "XrdCms/XrdCmsNode.hh"
#include "XrdCms/XrdCmsQRR.hh"
#include "XrdCms/XrdCmsReq.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdCms;
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

// The following bit of ugliness allows us to have imbedded classes that can
// refer to themselves and beyond; something templates don't allow.
//
#define XRDCMSSDL(BaseType,Base,Elem)                                          \
struct                                                                         \
      {short   next;                                                           \
       short   prev;                                                           \
                                                                               \
void   InsAft(short Pnum)                                                      \
             {short This = Base[prev].Elem.next;                               \
                   Base[prev].Elem.next            = Base[Pnum].Elem.next;     \
              Base[Base[Pnum].Elem.next].Elem.prev = prev;                     \
                   Base[Pnum].Elem.next            = This;                     \
              prev = Pnum;                                                     \
             }                                                                 \
void   InsAft(BaseType *Node) {InsAft(static_cast<short>(Node-Base));}         \
void   InsBfr(short Nnum)     {InsAft(Base[Nnum].Elem.prev);}                  \
void   InsBfr(BaseType *Node) {InsAft(Node->Elem.prev);}                       \
void   Remove()                                                                \
             {short Nnum = Base[next].Elem.prev;                               \
              Base[next].Elem.prev = prev;                                     \
              Base[prev].Elem.next = next;                                     \
              next                 = Nnum;                                     \
              prev                 = Nnum;                                     \
             }                                                                 \
int    Singleton() {return Base[prev].Elem.next == next;}                                      \
BaseType *Next() {return &Base[next];}                                         \
BaseType *Pext() {return &Base[prev];}                                         \
      } Elem
  
/******************************************************************************/
/*       G l o b a l   O b j e c t s   &   S t a t i c   M e m b e r s        */
/******************************************************************************/
  
namespace XrdCms
{
extern XrdScheduler  *Sched;

static const int   RTabSZ=2011; // Prime number
static const int   RVecSZ=4;
static const int   RVecLN=RVecSZ*sizeof(short);

static       short RTab[RTabSZ][RVecSZ] = {{0,0}};
};

XrdCmsQRR          XrdCms::QRR;

/******************************************************************************/
/*                    C l a s s   X r d C m s Q R R J o b                     */
/******************************************************************************/

// When a new server enters the configuration we must ask it about paths that
// clients are waiting for. Note that any entry in the wait queue has a
// valid key cache pointer! The code here is largely driven by the Slum and Slut
// tables which can be scanned without a lock.  Whenever we find that an entry
// is in wait, we obtain a lock and then will be able to verify that the
// unlocked entry is still valid. We start off by waiting then see if there is
// anything in the queue to inquire about. This provides some settling time.
// Scanning is done via the Scan function which invokes or specialized method.
// The refresh scan is done in a separate thread to avoid slow downs.
//
  
class XrdCmsQRRJob : public XrdJob
{
public:

void DoIt() {XrdSysTimer::Snooze(3);
             QRR.Scan(this, &XrdCmsQRR::Refresh);
             delete this;
            }

     XrdCmsQRRJob(SMask_t mask, int nnum, int nins)
                 : XrdJob("QRR refresh job"),
                   NodeMask(mask), NodeNum(nnum), NodeIns(nins) {}
    ~XrdCmsQRRJob() {}

SMask_t NodeMask;
int     NodeNum;
int     NodeIns;
};

/******************************************************************************/
/*                   C l a s s   X r d C m s Q R R S l u t                    */
/******************************************************************************/
  
class XrdCmsQRRSlut
{
public:
             char pCnt;         // Number of times polled  (valid if >= 0)

                  XrdCmsQRRSlut() : pCnt(-1) {if (!Slut) Slut = this;}
                 ~XrdCmsQRRSlut() {}

static XrdCmsQRRSlut *Slut;
};

/******************************************************************************/
/*                 X r d C m s Q R R S l o t   S t a t i c s                  */
/******************************************************************************/
  
 XrdCmsQRRSlut *XrdCmsQRRSlut::Slut = 0;

/******************************************************************************/
/*                   C l a s s   X r d C m s Q R R S l o t                    */
/******************************************************************************/
  
class XrdCmsQRRSlot
{
public:

static XrdCmsQRRSlot *Alloc(XrdCmsSelect &Sel, SMask_t qm);

short  SNum() {return this-zeroSlot;}

void   Recycle();

static
void   setSlum(unsigned char *sP) {slumSlot = sP;}

static
int    Waiting() {return alloSlots;} // No lock, only advisory

       XrdCmsQRRSlot();
      ~XrdCmsQRRSlot() {}

       XrdCmsKeyItem  *CacheRef; // Cache line reference for this slot
union {SMask_t         qfVec;    // Possible servers to query
       XrdCmsQRRSlot  *Next;     // Next free slot
      };
       int             SlotIns;  // Slot instance
       short           SlotQ;    // First request for this slot

private:
static XrdSysMutex     myMutex;
static XrdCmsQRRSlot  *freeSlot;
static XrdCmsQRRSlot  *zeroSlot;
static unsigned char  *slumSlot;
static int             alloSlots;
};

/******************************************************************************/
/*                 X r d C m s Q R R S l o t   S t a t i c s                  */
/******************************************************************************/

XrdSysMutex           XrdCmsQRRSlot::myMutex;
XrdCmsQRRSlot        *XrdCmsQRRSlot::freeSlot = 0;
XrdCmsQRRSlot        *XrdCmsQRRSlot::zeroSlot = 0;
unsigned char        *XrdCmsQRRSlot::slumSlot = 0;
int                   XrdCmsQRRSlot::alloSlots= 0;

/******************************************************************************/
/*             X r d C m s Q R R S l o t   C o n s t r u c t o r              */
/******************************************************************************/

XrdCmsQRRSlot::XrdCmsQRRSlot() : qfVec(0), SlotIns(0), SlotQ(0)
{

// Place all slots, except slot 0, on the free list.
//
   if (zeroSlot)
      {Next     = freeSlot;
       freeSlot = this;
      } else zeroSlot = this;
}

/******************************************************************************/
/*                  X r d C m s Q R R S l o t : : A l l o c                   */
/******************************************************************************/
  
XrdCmsQRRSlot *XrdCmsQRRSlot::Alloc(XrdCmsSelect &Sel, SMask_t qm)
{
   static int myIns = 0;
   XrdCmsQRRSlot *sP;
   int slotNum;

// Allocate a slot and the corresponding slut entry. For the slut we know the
// poll count will be zero but we need to indicate the queue type. The caller
// better have the global mutex so that other threads see a consistent pic.
//
   myMutex.Lock();
   if ((sP = freeSlot))
      {freeSlot = sP->Next;
       slotNum = sP-zeroSlot;
       sP->CacheRef = Sel.Path.TODRef;
       sP->qfVec = qm;
       sP->SlotQ = 0;
       sP->SlotIns = myIns++;
       XrdCmsQRRSlut::Slut[slotNum].pCnt = 0;
       slumSlot[slotNum/slumSgsz]++;
       alloSlots++;
      }
   myMutex.UnLock();
   return sP;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
// Must be called with wqMutex locked!

void XrdCmsQRRSlot::Recycle()
{
   int Snum = SNum();

// Zorch the slot type. We can do this without a lock
//
  XrdCmsQRRSlut::Slut[Snum].pCnt  = -1;
  slumSlot[Snum/slumSgsz]--;
  SlotIns = 0;
  CacheRef= 0;

// Any chained elements off this slot should have been removed prior to
// recycling. However, we will do a sanity check, just in case.
//
   if (SlotQ)
      {Say.Emsg("Recycle", "recycled slot still has queued requests!!!");
       SlotQ = 0;
      }

// Free up this slot as well as any chained off this slot.
//
   myMutex.Lock();
   Next     = freeSlot;
   freeSlot = this;
   alloSlots--;
   myMutex.UnLock();
}

/******************************************************************************/
/*                   C l a s s   X r d C m s Q R R D a t a                    */
/******************************************************************************/

class XrdCmsQRRData
{
public:

static XrdCmsQRRData *Alloc();

       void           Dispatch();

       int            Orphan(int doAll=1);

       void           Recycle();

       XrdCmsQRRData();
      ~XrdCmsQRRData() {}

static XrdCmsQRRData  *zeroData;

union {XrdCmsQRRData  *Next;   // Free chain list when de-allocated
       XrdCmsReq      *Req;    // Encapsulated request responder when in waitQ
      };
       XRDCMSSDL(XrdCmsQRRData,XrdCmsQRRData::zeroData,SlotQ);
       XRDCMSSDL(XrdCmsQRRData,XrdCmsQRRData::zeroData,WaitQ);

union {int             rGroup; // Resource group when waiting for node
       int             NodeIns;// Validator for NodeNum upon dispatch
      };
       int             SlotIns;// Slot validator
       short           SlotNum;// Back pointer to slot when in waitQ
       short           NodeNum;// When redirectable contains node number
       short           DataNum;// Index of this object in Data[]
       char            Reserve;// Reserved byte
       char            Status;    // Status of this entry
static const int       Cont=0x01; // Continue  this entry
static const int       Hold=0x02; // Hold      this entry
static const int       Redr=0x04; // Redirect  this entry
static const int       Rtry=0x08; // Retry     this entry
static const int       Head=0x10; // rGroup    this entry (this is the head)

private:

void           Devour(SMask_t Mask);
XrdCmsQRRData *Dispose(XrdCmsQRRData *dP, SMask_t &Mask);

static XrdSysMutex     myMutex;
static XrdCmsQRRData  *freeData;
};

/******************************************************************************/
/*                 X r d C m s Q R R D a t a   S t a t i c s                  */
/******************************************************************************/
  
XrdSysMutex    XrdCmsQRRData::myMutex;
XrdCmsQRRData *XrdCmsQRRData::freeData = 0;
XrdCmsQRRData *XrdCmsQRRData::zeroData = 0;

/******************************************************************************/
/*             X r d C m s Q R R D a t a   C o n s t r u c t o r              */
/******************************************************************************/

XrdCmsQRRData::XrdCmsQRRData() : Next(0),rGroup(0),SlotNum(0),Status(0)
{
   static XrdCmsQRRData *lastData;

// Place all slots, except slot 0, on the free list.
//
   if (zeroData)
      {if (freeData) lastData->Next = this;
           else freeData = this;
       lastData = this;
      } else zeroData = this;

// Initialize links
//
   DataNum = this - zeroData;
   SlotQ.next = SlotQ.prev = WaitQ.next = WaitQ.prev = DataNum;
}

/******************************************************************************/
/*                  X r d C m s Q R R D a t a : : A l l o c                   */
/******************************************************************************/
  
XrdCmsQRRData *XrdCmsQRRData::Alloc()
{
   XrdCmsQRRData *dP;

   myMutex.Lock();
   if ((dP = freeData)) freeData = dP->Next;
   myMutex.UnLock();
   dP->DataNum = dP-zeroData;
   return dP;
}

/******************************************************************************/
/*                              D i s p a t c h                               */
/******************************************************************************/

void XrdCmsQRRData::Dispatch()
{
   SMask_t Resumed;

// Our purpose here is to constantly look for something to dispatch. Whenever
// a node transitions from notready to ready we are posted and we will scan
// through the waitq looking for something to give it. This is done is such a
// way that we can actually have multiple nodes to do a parallel search.
//
do{QRR.sResume.Wait();
   XrdSysTimer::Wait(9);  // Try to accumulate additional nodes
   QRR.vvMutex.Lock();
   Resumed = QRR.resumVec; QRR.resumVec = SMask_t(0);
   QRR.vvMutex.UnLock();
   if (Resumed) Devour(Resumed);
  } while(1);
}
  
/******************************************************************************/
/* Private:                       D e v o u r                                 */
/******************************************************************************/
  
void XrdCmsQRRData::Devour(SMask_t Mask)
{
   EPNAME("Devour");
   static const int Attempts = 30;
   XrdCmsQRRData *dP;
   int Tries;

// Scan through the wait queue. In order to not keep the queued locked, we
// will periodically unlock it if we don't find anything applicable right away.
// We make sure that we can resume travelling down the chain eventhough we
// unlocked it by inserting a known placeholder that will not go away.
//
   DEBUG("requests for nodevec 0x" <<std::hex <<Mask <<std::dec);
   QRR.wqMutex.Lock();
   dP = zeroData->WaitQ.Next();
   do {QRR.vvMutex.Lock(); Mask &= QRR.readyVec; QRR.vvMutex.UnLock();
       if (!Mask) return;
       Tries = Attempts;
       while(dP != zeroData && --Tries)
            {if (QRR.Slot[dP->SlotNum].CacheRef->Loc.hfvec & Mask
             &&  !(dP->Status & Hold)) dP = Dispose(dP, Mask);
                else dP = dP->WaitQ.Next();
            }
       if (Tries) break;
       WaitQ.InsBfr(dP);
       QRR.wqMutex.UnLock(); QRR.wqMutex.Lock();
       dP = WaitQ.Next();
      } while(dP != zeroData);
   QRR.wqMutex.UnLock();
}

/******************************************************************************/
/*                               D i s p o s e                                */
/******************************************************************************/
  
XrdCmsQRRData *XrdCmsQRRData::Dispose(XrdCmsQRRData *dP, SMask_t &Mask)
{
   XrdCmsQRRData *nP;

// The first step is to remoe this element from the slotq to keep the slot
// scheduler from trying to do anything with it. We also turn on the hold
// flag to keep other wait queue removers from touching this element. At this
// point we can unlock the waitq to let everyone else in.
//
   if (dP->Orphan(-1)) QRR.Slot[dP->SlotNum].Recycle();
   dP->Status |= Hold;
   QRR.wqMutex.UnLock();

// Now redirect the client and check if target is still available
//
   QRR.Redirect(dP, Mask);
   QRR.vvMutex.Lock(); Mask &= QRR.readyVec; QRR.vvMutex.UnLock();

// If target is still available then we will return the next node otherwise
// we will return a pointer to the anchor to stop any more dispatches.
//
   QRR.wqMutex.Lock();
   nP = (Mask ? dP->WaitQ.Next() : zeroData);

// Recycle this element and return the sequent
//
   dP->WaitQ.Remove();
   dP->Recycle();
   return nP;
}

/******************************************************************************/
/*                 X r d C m s Q R R D a t a : : O r p h a n                  */
/******************************************************************************/
  
// Must be called with wqMutex locked!

int XrdCmsQRRData::Orphan(int doAll)
{
   int noMore = 1;

// First remove ourselves from the slot queue (always can be done)
//
   if (doAll)
      {if (SlotQ.Singleton()) QRR.Slot[SlotNum].SlotQ = 0;
          else {QRR.Slot[SlotNum].SlotQ = SlotQ.next;
                SlotQ.Remove();
                noMore = 0;
               }
      }

// If this entry is in the rgroup table then we must either update or remove it
//
   if (Status & Head)
      {     if (WaitQ.Singleton()) QRR.DelGroup(this);
       else if (rGroup == zeroData[WaitQ.prev].rGroup)
               QRR.UpdGroup(this, WaitQ.prev);
       else QRR.DelGroup(this);
      }

// Remove ourselves from the waitq as well and indicate whether there are
// more entries in the associated slot list.
//
   if (doAll >= 0) WaitQ.Remove();
   return noMore;
}
  
/******************************************************************************/
/*                X r d C m s Q R R D a t a : : R e c y c l e                 */
/******************************************************************************/
  
void XrdCmsQRRData::Recycle()
{
   myMutex.Lock();
   Next     = freeData;
   freeData = this;
   myMutex.UnLock();
}

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/

void *XrdCmsQRR_StartDispatch(void *parg)
      {XrdCmsQRRData *dP = (XrdCmsQRRData *)parg;
       dP->Dispatch();
       return (void *)0;
      }

void *XrdCmsQRR_StartRespond(void *parg) {return QRR.Respond();}
  
void *XrdCmsQRR_StartTimeOut(void *parg) {return QRR.TimeOut();}
  
/******************************************************************************/
/*               X r d C m s Q R R   C l a s s   M e t h o d s                */
/******************************************************************************/
/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
short XrdCmsQRR::Add(XrdCmsSelect &Sel, SMask_t qmask, short &SlotNum)
{
// EPNAME("QRR Add");
   XrdCmsQRRSlot *sP;
   XrdCmsQRRData *dP;

// Obtain a data object for this information. If none, indicate to the
// caller that the client will need to re-issue the request.
//
   if (!(dP = XrdCmsQRRData::Alloc())) return 0;

// If a slot number given, check if it's the right slot and it is still queued.
// If so, piggy-back this request to existing one otherwise get a new slot. If
// can't get a new slot, tell caller the client will need to reissue request.
//
   wqMutex.Lock();
   if (Slot[SlotNum].CacheRef == Sel.Path.TODRef)
      {sP = &Slot[SlotNum]; sP->qfVec |= qmask;}
      else if ((sP = XrdCmsQRRSlot::Alloc(Sel, qmask))) SlotNum = sP->SNum();
              else {dP->Recycle(); wqMutex.UnLock(); return 0;}

// Chain this data request to the list for this slot. If none, make it first
//
   if (sP->SlotQ) dP->SlotQ.InsBfr(sP->SlotQ);
      else sP->SlotQ = dP->DataNum;

// Complete the data request element
//
   dP->rGroup  = Sel.rGroup;
   dP->Req     = Sel.InfoR;
   dP->SlotNum = SlotNum;
   dP->Status  = 0;

// If an rGroup was specified, insert this request by group. Otherwise, simply
// then chain this at the end of the full queue. We currently do not handle
// priority requests as this is somewhat messy with this complicated stucture.
//
   if (!dP->rGroup) dP->WaitQ.InsBfr(short(0));
      else AddGroup(dP);

// All done
//
   wqMutex.UnLock();
   return 1;
}

/******************************************************************************/
/* Private:                     A d d G r o u p                               */
/******************************************************************************/
  
void XrdCmsQRR::AddGroup(XrdCmsQRRData *dP)
{
   XrdCmsQRRData *xP;
   short *rVec = RTab[NumGroup(dP)];
   int i, theNode = dP->Req->Node(), theGrp = dP->rGroup;

// Check if this group already exists in the table. If so, assign the group
// leader to be the new request. This avoids reassignment as the leader (now in
// last place) will typically the last one to be dispatched.
//
   for (i = 0; i < RVecSZ && rVec[i]; i++)
       {xP = &Data[rVec[i]];
        if (xP->rGroup == theGrp && xP->Req->Node() == theNode)
           {Data[rVec[i]].Status &= XrdCmsQRRData::Head;
            dP->Status     |= XrdCmsQRRData::Head;
            dP->WaitQ.InsAft(rVec[i]);
            rVec[i] = theNode;
            return;
           }
       }

// No match, check if we can add another group or make room for one
//
   if (i >= RVecSZ)
      {Data[rVec[0]].Status &= ~XrdCmsQRRData::Head;
       memcpy(rVec, rVec+1, sizeof(RVecLN-sizeof(short)));
       i = RVecSZ-1;
      }

// Add the entry to the end of the queue
//
   rVec[i] = dP->DataNum;
   dP->Status |= XrdCmsQRRData::Head;
   dP->WaitQ.InsBfr(short(0));
}

/******************************************************************************/
/*                                a d d S r v                                 */
/******************************************************************************/
  
XrdCmsEvent *XrdCmsQRR::addSrv(XrdCmsNode *nP, int isSuspended)
{
   int NodeNum, NodeIns;

// Add the node to the available set
//
   wqMutex.Lock();
   availVec |= nP->Mask();
   wqMutex.UnLock();
   if (!isSuspended) rstSrv(nP, 0);

// Simply queue a background job to refresh ourselves with this server. This
// will prevent TCP request/response deadlocks as it's done asynchronously.
//
   NodeNum = nP->ID(NodeIns);
   Sched->Schedule((XrdJob *)new XrdCmsQRRJob(nP->Mask(), NodeNum, NodeIns));

// Cascade the event notifications
//
   return eventP_adSrv;
}
  
/******************************************************************************/
/* Private:                          A s k                                    */
/******************************************************************************/

int XrdCmsQRR::Ask(XrdCmsKey &Key, int NodeNum, int NodeIns)
{
// EPNAME("Ask");
   CmsStateRequest Sreq={{Key.Hash,
                          kYR_state,
                          kYR_raw,
                          htons(Key.Len+1)
                        }};
   struct iovec iov[] = {{(char *)&Sreq, sizeof(Sreq)}, {Key.Val, Key.Len+1}};

// DEBUG("node " <<NodeNum <<'.' <<NodeIns <<" for " <<Key.Val);

   return Cluster.Send(NodeNum, NodeIns, iov, 2, sizeof(Sreq)+Key.Len+1);
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
void XrdCmsQRR::Del(XrdCmsKeyItem *Key, short Snum, int Ext)
{
   XrdCmsQRRSlot *sP = &Slot[Snum];
   XrdCmsQRRData *dP = 0;
   short NextQ;

// If this is the correct slot then unhook any waiting elements and
// schedule them for a retry. External callers need to get the wqMutex.
// Internal callers must have it already locked. Upon return it is unlocked!
//
   if (Ext) wqMutex.Lock();
   if (sP->CacheRef == Key)
      {if ((NextQ = sP->SlotQ))
          {do {dP = &Data[NextQ];
               dP->Orphan(0);
               dP->Status |= XrdCmsQRRData::Rtry;
               NextQ = dP->SlotQ.next;
              } while(NextQ != sP->SlotQ);
           sP->SlotQ = 0;
        }
       sP->Recycle();
       wqMutex.UnLock();
       rqMutex.Lock();
       dP->SlotQ.InsBfr(Data->DataNum);
       rqReady.Post();
       rqMutex.UnLock();
      } else wqMutex.UnLock();
}

/******************************************************************************/
/* Private:                     D e l G r o u p                               */
/******************************************************************************/
  
void XrdCmsQRR::DelGroup(XrdCmsQRRData *dP)
{
   short *rVec = RTab[NumGroup(dP)];
   int i, k, theNum = dP->DataNum;

// Check if this group already exists in the table
//
   for (i = 0; i < RVecSZ && rVec[i]; i++)
       if (Data[rVec[i]].DataNum == theNum)
          {for (k = i+1; k < RVecSZ; k++) rVec[i] = rVec[k];
           rVec[RVecSZ-1] = 0;
           break;
          }
}

/******************************************************************************/
/*                                d r p S r v                                 */
/******************************************************************************/
  
XrdCmsEvent *XrdCmsQRR::drpSrv(XrdCmsNode *nP)
{
   SMask_t UnMask = ~(nP->Mask());

// Remove the node from the available set
//
   vvMutex.Lock();
   availVec &= UnMask;
   readyVec &= UnMask;
   resumVec &= UnMask;
   vvMutex.UnLock();

   return eventP_adSrv;
}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdCmsQRR::Init(int maxSlots, int maxData, int sTime, int pTime,
                    int Tries,    int rdThrds, int sdThrds)
{
   XrdCmsQRRData *dP;
   int rc;
   pthread_t tid;

// Set values
//
   if (pTime) PollWindow = pTime;
   if (Tries) PollMax    = Tries;
   if (sTime > 0) selWait= sTime;
   if (rdThrds <= 0) rdThrds = 8;
   if (sdThrds <= 0) sdThrds = 4;

// Allocate the slot and the slut and slum tables. The number of slots must be
// a multiple of the slumSgsz because of the way we do unlocked scans.
//
   numSlums = maxSlots/slumSgsz + (maxSlots%slumSgsz ? 1 : 0);
   numSlots = numSlums*slumSgsz;
   Slot     = new XrdCmsQRRSlot[numSlots];
   Slut     = new XrdCmsQRRSlut[numSlots];
   Slum     = (unsigned char *)malloc(numSlums);
   memset(Slum, 0, numSlums);
   XrdCmsQRRSlot::setSlum(Slum);

// Allocate the data table
//
   if (!maxData) maxData = numSlots + numSlots/3;
   Data = new XrdCmsQRRData[maxData];

// Allocate the target serialization table and initialize vectors
//
   tsMutex  = new XrdSysMutex[STMax];
   availVec = SMask_t(0);
   readyVec = SMask_t(0);
   resumVec = SMask_t(0);
   queryVec = SMask_t(0);

// The QRR dispatcher thread handles enablement of a one or more servers
//
   while(rdThrds--)
        {dP = XrdCmsQRRData::Alloc();
         if ((rc = XrdSysThread::Run(&tid, XrdCmsQRR_StartDispatch,
                                     (void *)dP, 0, "QRR Dispatch")))
            {Say.Emsg("Config", rc, "create QRR dispatch thread"); return 1;}
        }

// The QRR responder thread handles enablement of particular slot.
//
   while(sdThrds--)
        {if ((rc = XrdSysThread::Run(&tid, XrdCmsQRR_StartRespond, (void *)0,
                                     0, "QRR Responder")))
            {Say.Emsg("Config", rc, "create QRR respond thread"); return 1;}
        }

// Start the timeout thread
//
   if ((rc = XrdSysThread::Run(&tid, XrdCmsQRR_StartTimeOut, (void *)0,
                               0, "QRR Timeout")))
      {Say.Emsg("Config", rc, "create QRR timeout thread");
       return 1;
      }

// Register ourselves to intercept node additions and status changes
//
   eventP_adSrv = XrdCmsEvent::Register((XrdCmsEvent *)this,
                  XrdCmsEvent::Evt_adSrv);
   eventP_rsSrv = XrdCmsEvent::Register((XrdCmsEvent *)this,
                  XrdCmsEvent::Evt_rsSrv);

// All done
//
   return 0;
}

/******************************************************************************/
/* Private:                     N u m G r o u p                               */
/******************************************************************************/
  
int XrdCmsQRR::NumGroup(XrdCmsQRRData *dP)
{
// Develop an identifier for this request group within the requestor's node
// and convert that into an index into the group table.
//
   return static_cast<int>((static_cast<long long>(dP->Req->Node())<<32LL
                           |static_cast<long long>(dP->rGroup))%RTabSZ);
}

/******************************************************************************/
/*                                 R e a d y                                  */
/******************************************************************************/
  
short XrdCmsQRR::Ready(XrdCmsKeyItem *Key, XrdCmsSelect &Sel, 
                       SMask_t amask,      short Snum)
{
   EPNAME("QRR Ready");

// Check if it's the right slot and it is still queued.
//
   wqMutex.Lock();
   if (!Slot[Snum].SlotQ || (Slot[Snum].CacheRef != Key))
      {wqMutex.UnLock();
       DEBUG("slot " <<Snum <<" no longer valid for" <<Key->Key.Val);
       return 0;
      }

// If this is a non-node-specific call then we cannot dispatch
//
   if (Sel.NodeNum < 0 || Sel.NodeIns < 0)
      {wqMutex.UnLock();
       DEBUG("slot " <<Snum <<" no node addr for " <<Key->Key.Val);
       return Snum;
      }

// If the server claiming to have the file is not ready, we cannot dispatch
//
   if (!(readyVec & amask))
      {wqMutex.UnLock();
       DEBUG("slot " <<Snum <<" node not ready for " <<Key->Key.Val);
       return Snum;
      }

// Queue a data request. This will cascade to do all of the slot entries and
// unlock the mutex we are holding.
//
   return RespQueue(Snum, Sel.NodeNum, Sel.NodeIns);
}

/******************************************************************************/
/* Private:                     R e d i r e c t                               */
/******************************************************************************/
  
void XrdCmsQRR::Redirect(XrdCmsQRRData *dP)
{
   EPNAME("Redirect");
   char hName[256];
   int Port = 0, Node = dP->NodeNum;

// Verify that the node is valid
//
   if (Node < 0 || Node > STMax)
      {DEBUG("node " <<Node <<'.' <<dP->NodeIns <<" serialization failed.");
       dP->Req->Reply_Wait(1);
       return;
      }

// Obtain the destination mutex to serialize redirections to the target node.
// If we have no node, then this is just a pacing call. So, wait and then return
//
   tsMutex[Node].Lock();

// Get the target hostname and port number
//
   if (!Cluster.Dest(Node,dP->NodeIns,Port,hName,sizeof(hName)))
      {tsMutex[Node].UnLock();
       DEBUG("node " <<Node <<'.' <<dP->NodeIns <<" selection failed.");
       dP->Req->Reply_Wait(1);
       return;
      }

// Redirect this client
//
   DEBUG("sending client to " <<hName <<':' <<Port);
   dP->Req->Reply_Redirect(hName, Port);

// Wait for the required amount of time to give the target some room to suspend
//
   XrdSysTimer::Wait(selWait);
   tsMutex[Node].UnLock();
}

/******************************************************************************/
  
void XrdCmsQRR::Redirect(XrdCmsQRRData *dP, SMask_t Mask)
{
   EPNAME("Redirect");
   char hName[256];
   int Node, Port = 0;

// Get the target hostname and port number
//
   if ((Node = Cluster.Dest(Mask,Port,hName,sizeof(hName))) < 0)
      {DEBUG("node mask selection failed.");
       dP->Req->Reply_Wait(1);
       return;
      }

// Obtain the destination mutex to serialize redirections to the target node,
// then redirect the client.
//
   tsMutex[Node].Lock();
   DEBUG("sending client to " <<hName <<':' <<Port);
   dP->Req->Reply_Redirect(hName, Port);

// Wait for the required amount of time to give the target some room to suspend
//
   XrdSysTimer::Wait(selWait);
   tsMutex[Node].UnLock();
}

/******************************************************************************/
/* Private:                      R e f r e s h                                */
/******************************************************************************/
  
int XrdCmsQRR::Refresh(XrdCmsQRRJob *Job, int Beg, int End)
{
   char Path[MAXPATHLEN+1];
   XrdCmsKey theKey(Path);
   int i, NodeNum = Job->NodeNum, NodeIns = Job->NodeIns;

// Do the refresh on this segment of the slot table. A valid entry always has
// a valid pointer to the key cache.
//
// DEBUG("node " <<NodeNum <<'.' <<NodeIns <<" scanning slots " <<Beg <<'-' <<End);
   for (i = Beg; i < End; i++)
       {if (Slut[i].pCnt < 0) continue;
        wqMutex.Lock();
        if (Slut[i].pCnt >= 0)
           {XrdCmsKey &kRef = Slot[i].CacheRef->Key;
            strcpy(Path, kRef.Val);
            theKey.Hash =  kRef.Hash;
            theKey.Len  =  kRef.Len;
            wqMutex.UnLock();
            if (!Ask(theKey, NodeNum, NodeIns)) return 0;
            XrdSysTimer::Wait(3);
           } else wqMutex.UnLock();
       }
   return 1;
}

/******************************************************************************/
/* Private:                     R e s p C o n t                               */
/******************************************************************************/
  
void XrdCmsQRR::RespCont(XrdCmsQRRData *dP)
{
   static SMask_t oneMask(1);
   int reQ = 1;

// If the slot is no longer valid or if we need to redirect and the server
// is no longer ready, do not process any more entries. Note that RespQueue
// returns with wqMutex unlocked.
//
   wqMutex.Lock();
   if (Slot[dP->SlotNum].SlotIns == dP->SlotIns)
      {if (dP->Status & XrdCmsQRRData::Redr)
          {vvMutex.Lock();
           reQ = (readyVec & (oneMask << static_cast<long long>(dP->NodeNum)));
           vvMutex.UnLock();
          }
       if (reQ) RespQueue(dP->SlotNum, dP->NodeNum, dP->NodeIns);
      } else wqMutex.UnLock();
}

/******************************************************************************/
/*                               R e s p o n d                                */
/******************************************************************************/
  
void *XrdCmsQRR::Respond()
{
   XrdCmsQRRData *dP;
   int inQ;

// The purpose here is to either put clients back into re-try state because the
// file that they were waiting for lost all state information so that the retry
// can recreate that state information or to redirect the client to a server.
//
do{rqReady.Wait();
   inQ = 1;
   do{rqMutex.Lock();
      if (!(Data->SlotQ.Singleton()))
         {dP = Data->SlotQ.Next();
          dP->SlotQ.Remove();
          rqMutex.UnLock();
          if (dP->Status & XrdCmsQRRData::Redr) Redirect(dP);
             else dP->Req->Reply_Wait(1);
          if (dP->Status & XrdCmsQRRData::Cont) RespCont(dP);
          dP->Recycle();
         } else {rqMutex.UnLock(); inQ = 0;}
     } while(inQ);
   } while(1);

// Keep the compiler happy
//
   return (void *)0;
}

/******************************************************************************/
/* Private:                    R e s p Q u e u e                              */
/******************************************************************************/
  
short XrdCmsQRR::RespQueue(short Snum, short Nnum, int Nins)
{
   XrdCmsQRRSlot *sP;
   XrdCmsQRRData *dP;

// Find an entry in the slotq list that is being ignored.
//
   sP = &Slot[Snum];
   dP = &Data[sP->SlotQ];
   while(dP->Status & XrdCmsQRRData::Hold)
        {if (dP->SlotQ.next == sP->SlotQ) {wqMutex.UnLock(); return Snum;}
         dP = dP->SlotQ.Next();
        }

// Remove this entry from all queues and record where it is to be redirected
//
   if (dP->Orphan()) {sP->Recycle(); Snum = 0;}
   dP->NodeNum = Nnum;
   dP->NodeIns = Nins;
   dP->Status |= (XrdCmsQRRData::Redr|XrdCmsQRRData::Cont);

// Dispatch this request
//
   wqMutex.UnLock();
   rqMutex.Lock();
   dP->SlotQ.InsBfr(Data->DataNum);
   rqReady.Post();
   rqMutex.UnLock();
   return Snum;
}

/******************************************************************************/
/*                                r s t S r v                                 */
/******************************************************************************/
  
XrdCmsEvent *XrdCmsQRR::rstSrv(XrdCmsNode *nP, int notOK)
{
   SMask_t theMask = nP->Mask();

// Add the node to the ready set or remove it if it's not OK
//
   vvMutex.Lock();
   if (notOK)
      {readyVec &= ~theMask;
       resumVec &= ~theMask;
       vvMutex.UnLock();
      } else {
       readyVec |= theMask;
       resumVec |= theMask;
       vvMutex.UnLock();
       sResume.Post();
      }

   return eventP_rsSrv;
}
  
/******************************************************************************/
/* Private:                         S c a n                                   */
/******************************************************************************/
  
void XrdCmsQRR::Scan(XrdCmsQRRJob *Job,
                     int (XrdCmsQRR::*Scanner)(XrdCmsQRRJob *, int, int))
{
   int j, slotBeg = -slumSgsz;

// Do a very fast scan across the slot table using the slum table to avoid
// unused areas. When we find a segment that is in use, invoke the scanner.
//
   if (XrdCmsQRRSlot::Waiting())
      for (j = 0; j < numSlums; j++)
          {slotBeg += slumSgsz;
           if (Slum[j]
           && !(*this.*Scanner)(Job, slotBeg, slotBeg+slumSgsz)) break;
          }
}

/******************************************************************************/
/*                               T i m e O u t                                */
/******************************************************************************/
  
void *XrdCmsQRR::TimeOut()
{

// Periodically we go through the queue of waiting requests. If we find one
// that is stalled, we re-issue a update to see if that will dispatch the
// waiting request. After a set number of these broadcasts, we will simply
// put the requestor back into polling mode to free up the slot. Note that
// any entry in the wait queue has a valid key cache pointer! The code here
// is largely driven by the Slum and Slut tables which can be scanned without
// a lock. Whenever we find that an entry is in wait, we obtain a lock and then
// will be able to verify that the unlocked entry is still valid. This is driven
// by Scan() which invokes a specialized method.
//
   while(1)
        {do {XrdSysTimer::Snooze(PollWindow);} while(!XrdCmsQRRSlot::Waiting());
         wqMutex.Lock();
         queryVec = availVec;
         wqMutex.UnLock();
         Scan(0, &XrdCmsQRR::TimeOut2);
        };

// Keep the compiler happy
//
   return (void *)0;
}

/******************************************************************************/
/* Private:                     T i m e O u t 2                               */
/******************************************************************************/

int XrdCmsQRR::TimeOut2(XrdCmsQRRJob *Dummy, int Beg, int End)
{
   EPNAME("QRR TimeOut");
   static CmsStateRequest Ureq={{0, kYR_update, 0, 0}};
   XrdCmsQRRSlut *slutP;
   SMask_t   theMask;
   int i;
  
// Re-poll semi-old entries and retire the really old ones. We walk the
// the table linearaly so that we can obtain/release the mutex between each
// entry to avoid locking out cache placement and wait processing. New inserts
// will have a poll count of zero which will will skip but set their count to 1.
// Note that we will not query servers already queried but we do have to do a
// full scan inorder to catch any expiring entries.
//
// DEBUG("scanning slots " <<Beg <<" to " <<End);
   for (i = Beg; i < End; i++)
       {slutP = &Slut[i];
        if (slutP->pCnt < 0) continue;
        wqMutex.Lock();
        if (slutP->pCnt < 0) {                 wqMutex.UnLock(); continue;}
        if (!slutP->pCnt)    {slutP->pCnt = 1; wqMutex.UnLock(); continue;}
        if (slutP->pCnt >= PollMax)
           {DEBUG("expired slot for " <<Slot[i].CacheRef->Key.Val);
            Del(Slot[i].CacheRef, i, 0); // Returns with wqMutex unlocked!
           } else {
            slutP->pCnt++;
            if (queryVec && (theMask = Slot[i].qfVec & queryVec))
               {wqMutex.UnLock();
                DEBUG("refreshing status slot " <<i);
                Cluster.Broadcast(theMask, Ureq.Hdr, (void *)0, 0);
                queryVec &= ~theMask;

               } else wqMutex.UnLock();
           }
       }
   return 1;
}

/******************************************************************************/
/* Private:                     U p d G r o u p                               */
/******************************************************************************/
  
void XrdCmsQRR::UpdGroup(XrdCmsQRRData *dP, short newNum)
{
   EPNAME("UpdGroup");
   short *rVec = RTab[NumGroup(dP)];
   int i, theNum = dP->DataNum;

// Check if this group already exists in the table
//
   for (i = 0; i < RVecSZ && rVec[i]; i++)
       if (rVec[i] == theNum)
          {Data[newNum].Status |= XrdCmsQRRData::Head;
           DEBUG("Changed leader from " <<rVec[i] <<" to " <<newNum);
           rVec[i] = newNum;
          }
}
