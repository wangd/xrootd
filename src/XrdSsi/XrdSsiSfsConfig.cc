/******************************************************************************/
/*                                                                            */
/*                    X r d S s i S f s C o n f i g . c c                     */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC02-76-SFO0515 with the Deprtment of Energy              */
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

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdCms/XrdCmsRole.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdSsi/XrdSsiSfs.hh"
#include "XrdSsi/XrdSsiFile.hh"
#include "XrdSsi/XrdSsiFileReq.hh"
#include "XrdSsi/XrdSsiLogger.hh"
#include "XrdSsi/XrdSsiSfsConfig.hh"
#include "XrdSsi/XrdSsiService.hh"
#include "XrdSsi/XrdSsiTrace.hh"

#include "XrdSsi/XrdSsiCms.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucTrace.hh"

#include "XrdVersion.hh"

#ifdef AIX
#include <sys/mode.h>
#endif

/******************************************************************************/
/*                               E x t e r n s                                */
/******************************************************************************/

class XrdScheduler;
  
namespace XrdSsi
{
       XrdSsiCms      *SsiCms;

       XrdScheduler   *Sched;

       XrdOucBuffPool *BuffPool;

       XrdSsiService  *Service;

       XrdSsiLogger    SsiLogger;

extern XrdOucTrace     Trace;

extern XrdSysError     Log;
};

using namespace XrdSsi;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdSsiSfsConfig::XrdSsiSfsConfig()
{
   static XrdVERSIONINFODEF(myVer, ssi, XrdVNUMBER, XrdVERSION);
   char *bp;

// Establish defaults
//
   ConfigFN      = 0;
   CmsLib        = 0;
   CmsParms      = 0;
   SsiCms        = 0;
   SvcLib        = 0;
   SvcParms      = 0;
   myRole        = 0;
   maxRSZ        = 2097152;
   isServer      = true;
   myHost        = getenv("XRDHOST");
   myProg        = getenv("XRDPROG");
   myInsName     = XrdOucUtils::InstName(1);
   myVersion     = &myVer;
   myPort = (bp = getenv("XRDPORT")) ? strtol(bp, (char **)NULL, 10) : 0;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSsiSfsConfig::~XrdSsiSfsConfig()
{
   if (ConfigFN) free(ConfigFN);
   if (CmsLib)   free(CmsLib);
   if (CmsParms) free(CmsParms);
   if (SvcLib)   free(SvcLib);
   if (SvcParms) free(SvcParms);
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/

bool XrdSsiSfsConfig::Configure(const char *cFN)
{
   char *var;
   const char *tmp;
   int  cfgFD, retc, NoGo = 0;
   XrdOucEnv myEnv;
   XrdOucStream cStrm(&Log, getenv("XRDINSTANCE"), &myEnv, "=====> ");

// Print warm-up message
//
   Log.Say("++++++ ssi phase 1 initialization started.");

// Preset all variables with common defaults
//
   if (getenv("XRDDEBUG")) Trace.What = TRACE_ALL | TRACE_Debug;

// If there is no config file, return with an error.
//
   if( !cFN || !*cFN)
     {Log.Emsg("Config", "Configuration file not specified.");
      return false;
     }

// Try to open the configuration file.
//
   ConfigFN = strdup(cFN);
   if ( (cfgFD = open(cFN, O_RDONLY, 0)) < 0)
      {Log.Emsg("Config", errno, "open config file", cFN);
       return false;
      }
   cStrm.Attach(cfgFD);

// Now start reading records until eof.
//
   cFile = &cStrm;
   while((var = cFile->GetMyFirstWord()))
        {if (!strncmp(var, "ssi.", 4)
         ||  !strcmp(var, "all.role"))
            {if (ConfigXeq(var+4)) {cFile->Echo(); NoGo=1;}}
        }

// Now check if any errors occured during file i/o
//
   if ((retc = cStrm.LastError()))
       NoGo = Log.Emsg("Config", -retc, "read config file", cFN);
   cStrm.Close();

// Make sure we are configured as a server
//
   if (!isServer)
      {Log.Emsg("Config", "ssi only supports server roles but role is not "
                             "defined as 'server'.");
       return false;
      }

// All done
//
   tmp = (NoGo ? " failed." : " completed.");
   Log.Say("------ ssi phase 1 initialization", tmp);
   return !NoGo;
}
  
/******************************************************************************/

bool XrdSsiSfsConfig::Configure(XrdOucEnv *envP)
{
   static char theSSI[] = {'s', 's', 'i', 0};
   static char **myArgv, *dfltArgv[] = {0, 0};
   XrdOucEnv    *xrdEnvP;
   const char *tmp;
   int myArgc, NoGo;

// Print warm-up message
//
   Log.Say("++++++ ssi phase 2 initialization started.");

// Now find the scheduler
//
   if (envP && !(Sched = (XrdScheduler *)envP->GetPtr("XrdScheduler*")))
      {Log.Emsg("Config", "Scheduler pointer is undefined!");
       NoGo = 1;
      } else NoGo = 0;

// Find our arguments, if any
//
   if ((xrdEnvP = (XrdOucEnv *)envP->GetPtr("xrdEnv*"))
   &&  (myArgv  = (char **)xrdEnvP->GetPtr("xrdssi.argv**")))
      myArgc = xrdEnvP->GetInt("xrdssi.argc");

// Verify that we have some and substitute if not
//
   if (!myArgv || myArgc < 1)
      {if (!(dfltArgv[0] = (char *)xrdEnvP->GetPtr("argv[0]")))
          dfltArgv[0] = theSSI;
       myArgv = dfltArgv;
       myArgc = 1;
      }

// Now configure management functions
//
   if (!NoGo && envP && ConfigObj()) NoGo = 1;

// Now configure the cms
//
   if (!NoGo && envP && ConfigCms(envP)) NoGo = 1;

// Now configure the server
//
   if (!NoGo && ConfigSvc(myArgv, myArgc)) NoGo = 1;

// All done
//
   tmp = (NoGo ? " failed." : " completed.");
   Log.Say("------ ssi phase 2 initialization", tmp);
   return !NoGo;
}

/******************************************************************************/
/*                             C o n f i g C m s                              */
/******************************************************************************/

class XrdOss;
  
int XrdSsiSfsConfig::ConfigCms(XrdOucEnv *envP)
{
   static const int cmsOpt = XrdCms::IsTarget;
   XrdCmsClient *cmsP, *(*CmsGC)(XrdSysLogger *, int, int, XrdOss *);
   XrdSysLogger *myLogger = Log.logger();

// Check if we are configuring a simple standalone server
//
   if (!myRole)
      {myRole = strdup("standalone");
       Log.Say("Config Configuring standalone server.");
       return 0;
      }

// If a cmslib was specified then create a plugin object and get the client.
// Otherwise, simply get the default client.
//
   if (CmsLib)
      {XrdSysPlugin myLib(&Log, CmsLib, "cmslib", myVersion);
       CmsGC = (XrdCmsClient *(*)(XrdSysLogger *, int, int, XrdOss *))
                                  (myLib.getPlugin("XrdCmsGetClient"));
       if (!CmsGC) return 1;
       myLib.Persist();
       cmsP = CmsGC(myLogger, cmsOpt, myPort, 0);
      }
      else cmsP = XrdCms::GetDefaultClient(myLogger, cmsOpt, myPort);

// If we have a client object onfigure it
//
   if (!cmsP || !cmsP->Configure(ConfigFN, CmsParms, envP))
      {delete cmsP;
       Log.Emsg("Config", "Unable to create cluster object.");
       return 1;
      }

// Create the cluster onject and return
//
   SsiCms = new XrdSsiCms(cmsP);
   return 0;
}
  
/******************************************************************************/
/*                             C o n f i g O b j                              */
/******************************************************************************/

int XrdSsiSfsConfig::ConfigObj()
{
   static const int minRSZ = 8192;

// Allocate a buffer pool
//
   if (maxRSZ < minRSZ) maxRSZ = minRSZ;
   BuffPool = new XrdOucBuffPool(minRSZ, maxRSZ);
   XrdSsiFile::SetMaxSz(maxRSZ);
   return 0;
}
  
/******************************************************************************/
/*                             C o n f i g S v c                              */
/******************************************************************************/

int XrdSsiSfsConfig::ConfigSvc(char **myArgv, int myArgc)
{
   XrdSysPlugin *myLib;
   XrdSsiServService_t ep;

// Make sure a library was specified
//
   if (!SvcLib)
      {Log.Emsg("Config", "svclib not specified; service cannot be loaded.");
       return 1;
      }

// Create a plugin object
//
   if (!(myLib = new XrdSysPlugin(&Log, SvcLib, "svclib", myVersion)))
      return 1;

// Now get the entry point of the object creator
//
   ep = (XrdSsiServService_t)(myLib->getPlugin("XrdSsiGetServerService"));
   if (!ep) return 1;

// Get the Object now
//
   myLib->Persist(); delete myLib;
   Service = ep(&SsiLogger, (XrdSsiCluster *)SsiCms, ConfigFN,
                             SvcParms, myArgc, myArgv);
   return Service == 0;
}
  
/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/
  
int XrdSsiSfsConfig::ConfigXeq(char *var)
{

// Now assign the appropriate global variable
//
    if (!strcmp("cmslib", var)) return Xlib("cmslib", &CmsLib, &CmsParms);
    if (!strcmp("svclib", var)) return Xlib("svclib", &SvcLib, &SvcParms);
    if (!strcmp("opts",   var)) return Xopts();
    if (!strcmp("role",   var)) return Xrole();
    if (!strcmp("trace",  var)) return Xtrace();

// No match found, complain.
//
   Log.Say("Config warning: ignoring unknown directive '",var,"'.");
   cFile->Echo();
   return 0;
}

/******************************************************************************/
/*                                  x L i b                                   */
/******************************************************************************/
  
/* Function: Xlib

   Purpose:  To parse the directive: xxxlib <path> [<parms>]

             <path>    the path of the library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdSsiSfsConfig::Xlib(const char *lName, char **lPath, char **lParm)
{
   char *val, parms[2048];

// Get the path and parms
//
   if (!(val = cFile->GetWord()) || !val[0])
      {Log.Emsg("Config", lName, "not specified"); return 1;}

// Set the CmsLib pointer
//
   if (*lPath) free(*lPath);
   *lPath = strdup(val);

// Combine the path and parameters
//
   *parms = 0;
   if (!cFile->GetRest(parms, sizeof(parms)))
      {Log.Emsg("Config", lName, "parameters too long"); return 1;}

// Record the parameters, if any
//
   if (*lParm) free(*lParm);
   *lParm = (*parms ? strdup(parms) : 0);
   return 0;
}
  
/******************************************************************************/
/*                                 X o p t s                                  */
/******************************************************************************/
  

/* Function: Xopts

   Purpose:  To parse directive: opts  [files <n>] [requests <n>]
                                       [maxrsz <sz>] [auth {0 | 1 | 2}]

             auth     0 -> don't supply auth, 1 -> do, 2 -> do w/ real hostname
             files    the maximum number of file objects to hold in reserve.
             maxrsz   the maximum size of a request.
             requests the maximum number of requests objects to hold in reserve.

   Output: 0 upon success or 1 upon failure.
*/

int XrdSsiSfsConfig::Xopts()
{
   char *val;
   long long ppp, fObj = -1, rMax = -1, rObj = -1, fAut = -1;
   int  i;

   struct optsopts {const char *opname; long long *oploc; int maxv; int isSz;}
          opopts[] =
      {
       {"authinfo", &fAut,            2, 0},
       {"files",    &fObj,      64*1024, 0},
       {"maxrsz",   &rMax, 16*1024*1024, 1},
       {"requests", &rObj,      64*1024, 0}
      };
   int numopts = sizeof(opopts)/sizeof(struct optsopts);

   if (!(val = cFile->GetWord()))
      {Log.Emsg("Config", "opts option not specified"); return 1;}

   while (val)
         {for (i = 0; i < numopts; i++)
              if (!strcmp(val, opopts[i].opname))
                 {if (!(val = cFile->GetWord()))
                     {Log.Emsg("Config", "opts ", opopts[i].opname,
                               "argument not specified.");
                      return 1;
                     }
                  if (opopts[i].isSz)
                     {if (XrdOuca2x::a2sz(Log,"opts value", val, &ppp,
                                          0, opopts[i].maxv)) return 1;
                     } else {
                      if (XrdOuca2x::a2ll(Log,"opts value", val, &ppp,
                                          0, opopts[i].maxv)) return 1;
                     }
                  *opopts[i].oploc = ppp;
                  break;
                 }
          if (i >= numopts)
             Log.Say("Config warning: ignoring invalid opts option '",val,"'.");
          val = cFile->GetWord();
         }

// Set the values that were specified
//
//  if (fObj >= 0) XrdSsiFile   ::SetMax(static_cast<int>(fObj));
    if (fAut >= 0) XrdSsiFile   ::SetAuth(static_cast<int>(fAut));
    if (rMax >= 0) maxRSZ = static_cast<int>(rMax);
    if (rObj >= 0) XrdSsiFileReq::SetMax(static_cast<int>(rObj));

    return 0;
}

/******************************************************************************/
/*                                 x r o l e                                  */
/******************************************************************************/

/* Function: Xrole
   Purpose:  Parse: role { {[meta] | [proxy]} manager
                           | proxy | [proxy]  server
                           |         [proxy]  supervisor
                         } [if ...]

             manager    xrootd: act as a manager (redirecting server). Prefixes:
                                meta  - connect only to manager meta's
                                proxy - ignored
                        cmsd:   accept server subscribes and redirectors. Prefix
                                modifiers do the following:
                                meta  - No other managers apply
                                proxy - manage a cluster of proxy servers

             proxy      xrootd: act as a server but supply data from another 
                                server. No local cmsd is present or required.
                        cmsd:   Generates an error as this makes no sense.

             server     xrootd: act as a server (supply local data). Prefix
                                modifications do the following:
                                proxy - server is part of a cluster. A local
                                        cmsd is required.
                        cmsd:   subscribe to a manager, possibly as a proxy.

             supervisor xrootd: equivalent to manager.
                        cmsd:   equivalent to manager but also subscribe to a
                                manager. When proxy is specified, subscribe as
                                a proxy and only accept proxy servers.


             if         Apply the manager directive if "if" is true. See
                        XrdOucUtils:doIf() for "if" syntax.

   Output: 0 upon success or !0 upon failure.
*/

int XrdSsiSfsConfig::Xrole()
{
   XrdCmsRole::RoleID roleID;
   char *val, *Tok1, *Tok2;
   int rc;

// Get the first token
//
   if (!(val = cFile->GetWord()) || !strcmp(val, "if"))
      {Log.Emsg("Config", "role not specified"); return 1;}
   Tok1 = strdup(val);

// Get second token which might be an "if"
//
   if ((val = cFile->GetWord()) && strcmp(val, "if"))
      {Tok2 = strdup(val);
       val = cFile->GetWord();
      } else Tok2 = 0;

// Process the if at this point
//
   if (val && !strcmp("if", val))
      if ((rc = XrdOucUtils::doIf(&Log,*cFile,"role directive",
                             myHost,myInsName,myProg)) <= 0)
         {free(Tok1); if (Tok2) free(Tok2);
          if (!rc) cFile->noEcho();
          return (rc < 0);
         }

// Convert the role names to a role ID, if possible
//
   roleID = XrdCmsRole::Convert(Tok1, Tok2);

// Validate the role
//
   rc = 0;
   if (roleID == XrdCmsRole::noRole)
      {Log.Emsg("Config", "invalid role -", Tok1, Tok2); rc = 1;}

// Release storage and return if an error occured
//
   free(Tok1);
   if (Tok2) free(Tok2);
   if (rc) return rc;

// Fill out information
//
   if (myRole) free(myRole);
   myRole   = strdup(XrdCmsRole::Name(roleID));
   isServer = (roleID == XrdCmsRole::Server);
   return 0;
}

/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: Xtrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cummalative.

   Output: 0 upon success or !0 upon failure.
*/

int XrdSsiSfsConfig::Xtrace()
{
    static struct traceopts {const char *opname; int opval;} tropts[] =
       {
        {"all",      TRACE_ALL},
        {"debug",    TRACE_Debug}
       };
    int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);
    char *val;

    if (!(val = cFile->GetWord()))
       {Log.Emsg("Config", "trace option not specified"); return 1;}
    while (val)
         {if (!strcmp(val, "off")) trval = 0;
             else {if ((neg = (val[0] == '-' && val[1]))) val++;
                   for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, tropts[i].opname))
                           {if (neg) trval &= ~tropts[i].opval;
                               else  trval |=  tropts[i].opval;
                            break;
                           }
                       }
                   if (i >= numopts)
                      Log.Say("Config warning: ignoring invalid trace option '",val,"'.");
                  }
          val = cFile->GetWord();
         }
    Trace.What = trval;

// All done
//
   return 0;
}
