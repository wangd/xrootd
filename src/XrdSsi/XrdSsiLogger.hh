#ifndef __XRDSSILOGGER_HH__
#define __XRDSSILOGGER_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d S s i L o g g e r . h h                        */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/* Produced by Andrew Hanushevsky for Stanford University under contract      */
/*            DE-AC02-76-SFO0515 with the Deprtment of Energy                 */
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

#include <stdarg.h>
 
//-----------------------------------------------------------------------------
//! The XrdSsiLogger object is used to route messages to the default logfile.
//-----------------------------------------------------------------------------

class XrdSsiLogger
{
public:

//-----------------------------------------------------------------------------
//! Insert a space delimited error message into the logfile.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    <timestamp> pfx: txt1 [txt2] [txt3]\n
//!         pfx  =0 -> add message to the log without a timestamp or prefix.
//! @param  msg  the message to added to the log.
//-----------------------------------------------------------------------------

static void Msg(const char *pfx,    const char *txt1,
                const char *txt2=0, const char *txt3=0);

//-----------------------------------------------------------------------------
//! Insert a formated error message into the log file using variable args.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    <timestamp> <pfx>: <formated_text>\n
//!         pfx  =0 -> add message to the log without a timestamp or prefix.
//! @param  fmt  the message formatting template (i.e. sprintf format). Note
//!              that a newline character is always appended to the message.
//! @param  ...  the arguments that should be used with the template. The
//!              formatted message is truncated at 2048 bytes.
//-----------------------------------------------------------------------------

static void Msgf(const char *pfx, const char *fmt, ...);

//-----------------------------------------------------------------------------
//! Insert a formated error message into the log file using a va_list.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    <timestamp> <pfx>: <formated_text>\n
//!         pfx  =0 -> add message to the log without a timestamp or prefix.
//! @param  fmt  the message formatting template (i.e. sprintf format). Note
//!              that a newline character is always appended to the message.
//! @param  aP   the arguments that should be used with the template. The
//!              formatted message is truncated at 2048 bytes.
//-----------------------------------------------------------------------------

static void Msgv(const char *pfx, const char *fmt, va_list aP);

//-----------------------------------------------------------------------------
//! Define helper functions to allow ostream cerr output to appear in the log.
//! The following two functions are used with the macros below.
//! The SSI_LOG macro preceeds the message with a timestamp; SSI_SAY does not.
//! The endl ostream output item is automatically added to all output!
//-----------------------------------------------------------------------------

#define SSI_LOG(x) {cerr <<XrdSSiLogger::TBeg()      <<x; XrdSsiLogger::TEnd();}
#define SSI_SAY(x)        {XrdSSiLogger::TBeg();cerr <<x; XrdSsiLogger::TEnd();}

static const char *TBeg();
static void        TEnd();

//-----------------------------------------------------------------------------
//! Constructor and destructor
//-----------------------------------------------------------------------------

         XrdSsiLogger() {}
        ~XrdSsiLogger() {}
};
#endif
