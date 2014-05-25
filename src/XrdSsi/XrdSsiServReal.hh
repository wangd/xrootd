#ifndef __XRDSSISERVREAL_HH__
#define __XRDSSISERVREAL_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S s i S e r v R e a l . h h                      */
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

#include "XrdSsi/XrdSsiService.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdSsiSessReal;

class XrdSsiServReal : public XrdSsiService
{
public:

bool           Provision(XrdSsiService::Resource *resP, unsigned short tOut=0);

void           Recycle(XrdSsiSessReal *sObj);

bool           Stop();

               XrdSsiServReal(const char *contact, int hObj)
                             : manNode(strdup(contact)), freeSes(0),
                               freeCnt(0), freeMax(hObj), actvSes(0) {}

              ~XrdSsiServReal();
private:

XrdSsiSessReal *Alloc(const char *sName);
bool            GenURL(const char *sName,const char *avoid,char *buff,int blen);
XrdSsiSession  *RetErr(XrdSsiErrInfo &eInfo,const char *eTxt,int eNum,bool async);

char           *manNode;
XrdSysMutex     myMutex;
XrdSsiSessReal *freeSes;
int             freeCnt;
int             freeMax;
int             actvSes;
};
#endif
