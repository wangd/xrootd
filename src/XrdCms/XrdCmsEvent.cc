/******************************************************************************/
/*                                                                            */
/*                        X r d C m s E v e n t . c c                         */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id: XrdCmsEvent.cc,v 1.3 2008/12/12 03:55:10 abh Exp $

const char *XrdCmsEventCVSID = "$Id: XrdCmsEvent.cc,v 1.4 2008/06/26 02:32:56 abh Exp $";

#include "XrdCms/XrdCmsEvent.hh"
#include "XrdSys/XrdSysPthread.hh"

using namespace XrdCms;
  
/******************************************************************************/
/*       G l o b a l   O b j e c t s   &   S t a t i c   M e m b e r s        */
/******************************************************************************/
  
XrdCmsEvent          *XrdCms::Evp_adRdr = 0;
XrdCmsEvent          *XrdCms::Evp_adSrv = 0;
XrdCmsEvent          *XrdCms::Evp_rsSrv = 0;

/******************************************************************************/
/*                              R e g i s t e r                               */
/******************************************************************************/
  
XrdCmsEvent *XrdCmsEvent::Register(XrdCmsEvent *newP, Evt_ID evtype)
{
   static XrdSysMutex myMutex;
   XrdCmsEvent *oldP;

// Set new event pointer
//
   myMutex.Lock();
   switch(evtype)
         {case Evt_adRdr: oldP = Evp_adRdr; Evp_adRdr = newP; break;
          case Evt_adSrv: oldP = Evp_adSrv; Evp_adSrv = newP; break;
          case Evt_rsSrv: oldP = Evp_rsSrv; Evp_rsSrv = newP; break;
          default: oldP = 0; break;
         }
   myMutex.UnLock();

// Return previous event pointer
//
   return oldP;
}
