#ifndef __CMS_CLIENT__
#define __CMS_CLIENT__
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s C l i e n t . h h                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

class  XrdOucEnv;
class  XrdOucErrInfo;
class  XrdOucLogger;
struct XrdSfsPrep;

// The following return conventions are use by Forward(), Locate(), & Prepare()
//
// Return Val   Resp.errcode          Resp.errtext
// ---------    -------------------   --------
// -EREMOTE     port (0 for default)  Host name
// -EINPROGRESS n/a                   n/a
// -EEXIST      Length of errtext     Data to be returned to client as response
// > 0          Wait time (= retval)  Reason for wait
// < 0          Error number          Error message
// = 0          Not applicable        Not applicable (see below)
//                                    Forward() -> Request forwarded
//                                    Locate()  -> Redirection does not apply
//                                    Prepare() -> Request submitted
//

class XrdCmsClient
{
public:
// Added() notifies the cms of a newly added file or a file whose state has
//         changed. It is only used on data server nodes.
//
virtual void   Added(const char *path, int Pend=0) = 0;

// Configue() is called internally to configure the client.
//
virtual int    Configure(char *cfn) = 0;

// Forward() relays a meta-operation to all nodes in the cluster. It is only
//           used on manager nodes.
//
virtual int    Forward(XrdOucErrInfo &Resp,   const char *cmd,
                       const char    *arg1=0, const char *arg2=0,
                       const char    *arg3=0, const char *arg4=0) = 0;

// isRemote() returns true of this client is configured for a manager node.
//
virtual int    isRemote() {return myPersona == XrdCmsClient::amRemote;}

// Locate() is called to retrieve file location information. It is only used
//          on a manager node.
//
virtual int    Locate(XrdOucErrInfo &Resp, const char *path, int flags,
                      XrdOucEnv  *Info=0) = 0;

// Prepare() is called to start the preparation of a file for future processing.
//           It is only used on a manager node.
//
virtual int    Prepare(XrdOucErrInfo &Resp, XrdSfsPrep &pargs) = 0;

// Removed() is called when a file or directory has been deleted. It is only
//           called on a data server node.
//
virtual void   Removed(const char *path) = 0;

// Resume() and Suspend() server complimentary functions and, by default,
//          persist across server restarts. A temporary suspend/resume may be
//          requested by passing a value of 0. Suspend() informs cluster 
//          managers that data services are suspended. Resume() re-enables
//          data services.
//
virtual void   Resume (int Perm=1) = 0;
virtual void   Suspend(int Perm=1) = 0;

// The followin set of functions can be used to control whether or not clients
// are dispatched to this data server based on a virtual resource.
//
// Resource() should be called first and enables the Reserve() & Release()
//            methods. It's argument a positive integer that specifies the
//            amount of resource units that are available. It may be called
//            at any time (though usually it is not) and returns the previous
//            value. This first call will return 0.
// Reserve()  decreases the amount of resources available by the value passed
//            as the argument (default is 1). When the available resources
//            bcome non-positive, a temporary suspend is activated preventing
//            additional clients from being dispatched to this data server.
//            Reserve() returns the amount of resource left.
// Release()  increases the amount of resource available by the value passed
//            as the argument (default 1). The total amount is capped by the
//            amount specified by Resource(). When a transition is made from
//            a non-positive to a positive amount, resume is activated that
//            allows additional clients to be dispatched to this data server.
//            Release() returns the amount of resource left.
//
virtual int    Resource(int n) = 0;
virtual int    Reserve (int n=1) = 0;
virtual int    Release (int n=1) = 0;

// Space() is used to obtain overall cluster space information. It is used only
//         on a manager nodes.
//
virtual int    Space(XrdOucErrInfo &Resp, const char *path) = 0;

// The Persona settings indicate what this client is configured to do:
//
        enum   Persona {amLocal, amProxy, amRemote, amTarget};

               XrdCmsClient(Persona acting) {myPersona = acting;}
virtual       ~XrdCmsClient() {}

protected:

Persona        myPersona;
};
#endif
