/*****************************************************************************
 * HTICK --- FTN Ticker / Request Processor
 *****************************************************************************
 * Copyright (C) 1999 by
 *
 * Gabriel Plutzar
 *
 * Fido:     2:31/1
 * Internet: gabriel@hit.priv.at
 *
 * Vienna, Austria, Europe
 *
 * This file is part of HTICK, which is based on HPT by Matthias Tichy, 
 * 2:2432/605.14 2:2433/1245, mtt@tichy.de
 *
 * HTICK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * HTICK is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HTICK; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************
 * $Id$
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <smapi/msgapi.h>

#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#include <fidoconf/afixcmd.h>
#include <huskylib/xstr.h>
#include <huskylib/recode.h>
#include <huskylib/log.h>
#include <areafix/areafix.h>

#include <fcommon.h>
#include <scan.h>
#include <global.h>
#include <htickafix.h>
#include <toss.h>

char *versionStr;

void convertMsgHeader( XMSG xmsg, s_message * msg )
{
  /*  convert header */
  msg->attributes = xmsg.attr;

  msg->origAddr = xmsg.orig;
  msg->destAddr = xmsg.dest;

  strcpy( ( char * )msg->datetime, ( char * )xmsg.__ftsc_date );
  xstrcat( &( msg->subjectLine ), ( char * )xmsg.subj );
  xstrcat( &( msg->toUserName ), ( char * )xmsg.to );
  xstrcat( &( msg->fromUserName ), ( char * )xmsg.from );

  /*  recoding subjectLine to Internal Charset */
  if( ( !config->recodeMsgBase ) && ( config->outtab != NULL ) )
  {
    recodeToInternalCharset( ( char * )msg->subjectLine );
    recodeToInternalCharset( ( char * )msg->fromUserName );
    recodeToInternalCharset( ( char * )msg->toUserName );
  }
}

void convertMsgText( HMSG SQmsg, s_message * msg, hs_addr ourAka )
{
  char *kludgeLines, viaLine[100];
  UCHAR *ctrlBuff;
  UINT32 ctrlLen;
  time_t tm;
  struct tm *dt;

  /*  get kludge lines */
  ctrlLen = MsgGetCtrlLen( SQmsg );
  ctrlBuff = ( unsigned char * )smalloc( ctrlLen + 1 );
  MsgReadMsg( SQmsg, NULL, 0, 0, NULL, ctrlLen, ctrlBuff );
  kludgeLines = ( char * )CvtCtrlToKludge( ctrlBuff );
  free( ctrlBuff );

  /*  make text */
  msg->textLength = MsgGetTextLen( SQmsg );

  time( &tm );
  dt = gmtime( &tm );
  sprintf( viaLine, "\001Via %u:%u/%u.%u @%04u%02u%02u.%02u%02u%02u.UTC %s",
           ourAka.zone, ourAka.net, ourAka.node, ourAka.point,
           dt->tm_year + 1900, dt->tm_mon + 1, dt->tm_mday, dt->tm_hour, dt->tm_min, dt->tm_sec,
           versionStr );

  msg->text =
      ( char * )scalloc( 1, msg->textLength + strlen( kludgeLines ) + strlen( viaLine ) + 1 );


  strcpy( msg->text, kludgeLines );
/*    strcat(msg->text, "\001TID: "); */
/*    strcat(msg->text, versionStr); */
/*    strcat(msg->text, "\r"); */

  MsgReadMsg( SQmsg, NULL, 0, msg->textLength, ( unsigned char * )msg->text + strlen( msg->text ),
              0, NULL );

  strcat( msg->text, viaLine );

  if( ( !config->recodeMsgBase ) && ( config->outtab != NULL ) )
    recodeToInternalCharset( ( char * )msg->text );

  free( kludgeLines );
}

void scanNMArea( s_area * afixarea )
{
  HAREA netmail;
  HMSG msg;
  unsigned long highMsg, i, j;
  XMSG xmsg;
  hs_addr dest;
  int for_us;
  s_message filefixmsg;
  int result = 0;

  memset( &filefixmsg, '\0', sizeof( s_message ) );
  netmail =
      MsgOpenArea( ( unsigned char * )afixarea->fileName, MSGAREA_NORMAL,
                   ( word ) afixarea->msgbType );
  if( netmail != NULL )
  {

    highMsg = MsgGetHighMsg( netmail );
    w_log( '1', "Scanning %s", afixarea->areaName );

    /*  scan all Messages for filefix */
    for( i = 1; i <= highMsg; i++ )
    {
      msg = MsgOpenMsg( netmail, MOPEN_RW, i );

      /*  msg does not exist */
      if( msg == NULL )
        continue;

      MsgReadMsg( msg, &xmsg, 0, 0, NULL, 0, NULL );
      dest = xmsg.dest;
      for_us = 0;
      for( j = 0; j < config->addrCount; j++ )
        if( addrComp( dest, config->addr[j] ) == 0 )
        {
          for_us = 1;
          break;
        }

      /*  if for filefix - process it */
      if( findInStrArray( robot->names, ( char * )xmsg.to ) >= 0 &&
          for_us && ( xmsg.attr & MSGREAD ) != MSGREAD )
      {
        convertMsgHeader( xmsg, &filefixmsg );
        convertMsgText( msg, &filefixmsg, config->addr[j] );

        result = processFileFix( &filefixmsg );
        if( result != 2 )
        {
          xmsg.attr |= MSGREAD;
          MsgWriteMsg( msg, 0, &xmsg, NULL, 0, 0, 0, NULL );
        }

        freeMsgBuffers( &filefixmsg );

        MsgCloseMsg( msg );
        if( robot->killRequests && result == 1 )
        {
          MsgKillMsg( netmail, i );
          --i;
        }
      }
      else
        MsgCloseMsg( msg );

    }                           /* endfor */

    MsgCloseArea( netmail );
  }
  else
  {
    w_log( '9', "Could not open %s", afixarea->areaName );
  }                             /* endif */
}

void scan( void )
{
  s_area *afixarea;

  w_log( LL_INFO, "Start filefix scan..." );
  if( ( afixarea = getNetMailArea( config, config->robotsArea ) ) == NULL )
  {
    afixarea = &( config->netMailAreas[0] );
  }

  scanNMArea( afixarea );
}
