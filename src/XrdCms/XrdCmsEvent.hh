#ifndef __CMS_EVENT_H__
#define __CMS_EVENT_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d C m s E v e n t . h h                         */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id: XrdCmsEvent.hh,v 1.3 2008/12/12 03:55:10 abh Exp $

class XrdLink;
class XrdCmsNode;

// This abstract class defines the event interface. It has one real method
// that is used to register a derived object that will be used for event
// notification. The Add() and Rid() methods are called with the cluster
// locked. Therefore, processing should be short-lived.
//
class XrdCmsEvent
{
public:

// Called when a redirector node is added to the cluster.
//
virtual XrdCmsEvent *addRdr(XrdCmsNode *nP, int isSuspended) {return 0;}

// Called when a server/supervisor node is added to the cluster.
//
virtual XrdCmsEvent *addSrv(XrdCmsNode *nP, int isSuspended) {return 0;}

// Called when a redirector node is dopped from the cluster.
//
virtual XrdCmsEvent *drpRdr(XrdCmsNode *nP) {return 0;}

// Called when a server/supervisor node is dopped from the cluster.
//
virtual XrdCmsEvent *drpSrv(XrdCmsNode *nP) {return 0;}

// Called when a server/supervisor toggles state from resumed and suspened
//
virtual XrdCmsEvent *rstSrv(XrdCmsNode *nP, int isSuspended) {return 0;}

// Use the register method to set an event call-out. It returns the current
// event object or 0 if there was none. It is the caller's responsibility
// to cascade event notifications by returning the value Register() returned.

// The enum below is used to specify which events you wish to intercept:
//
enum Evt_ID {Evt_adRdr=0,   // Call addRdr()/drpRdr() when applicable
             Evt_adSrv,     // Call addSrv()/drpSrv() when applicable
             Evt_rsSrv      // Call rstSrv()          when applicable
            };

static XrdCmsEvent *Register(XrdCmsEvent *eP, Evt_ID evtype);

                    XrdCmsEvent() {}

virtual            ~XrdCmsEvent() {}

private:
};
namespace XrdCms
{
extern XrdCmsEvent *Evp_adRdr;
extern XrdCmsEvent *Evp_adSrv;
extern XrdCmsEvent *Evp_rsSrv;
}
#endif
