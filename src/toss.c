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

#ifdef OS2
#define INCL_DOSFILEMGR /* for hidden() routine */
#include <os2.h>
#endif


#include <string.h>
#include <stdlib.h>
#ifndef UNIX
#  include <share.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#ifdef __EMX__
#include <sys/types.h>
#endif
#include <sys/stat.h>
#if ((!(defined(_MSC_VER) && (_MSC_VER >= 1200))) && (!defined(__TURBOC__)))
#include <unistd.h>
#endif
#if defined(A_HIDDEN) && !defined(_A_HIDDEN)
#define _A_HIDDEN A_HIDDEN
#endif

#if defined(__WATCOMC__) || defined(__TURBOC__) || defined(__DJGPP__)
#include <dos.h>
#include <process.h>
#endif

#include <fidoconf/fidoconf.h>
#include <fidoconf/adcase.h>
#include <fidoconf/common.h>
#include <fidoconf/dirlayer.h>
#include <fidoconf/xstr.h>
#include <fidoconf/afixcmd.h>
#include <fidoconf/recode.h>
#include <fidoconf/crc.h>

#include <smapi/progprot.h>
#include <smapi/compiler.h>

#include <fcommon.h>
#include <global.h>
#include <toss.h>
#include <areafix.h>
#include <version.h>
#include <add_desc.h>
#include <seenby.h>
#include <filecase.h>
#include "hatch.h"
#include "report.h"


void writeNetmail(s_message *msg, char *areaName)
{
   HAREA  netmail;
   HMSG   msgHandle;
   UINT   len = msg->textLength;
   char   *bodyStart;             /* msg-body without kludgelines start */
   char   *ctrlBuf;               /* Kludgelines */
   XMSG   msgHeader;
   s_area *nmarea;

   if ((nmarea=getNetMailArea(config, areaName))==NULL) nmarea = &(config->netMailAreas[0]);

   netmail = MsgOpenArea((UCHAR *) nmarea->fileName, MSGAREA_CRIFNEC, (word)nmarea->msgbType);

   if (netmail != NULL) {
      msgHandle = MsgOpenMsg(netmail, MOPEN_CREATE, 0);

      if (msgHandle != NULL) {
         msgHeader = createXMSG(config,msg,NULL,MSGLOCAL,NULL);
         /* Create CtrlBuf for SMAPI */
         ctrlBuf = (char *) CopyToControlBuf((UCHAR *) msg->text, (UCHAR **) &bodyStart, &len);
         /* write message */
         MsgWriteMsg(msgHandle, 0, &msgHeader, (UCHAR *) bodyStart, len, len, strlen(ctrlBuf)+1, (UCHAR *) ctrlBuf);
         free(ctrlBuf);
         MsgCloseMsg(msgHandle);

         w_log( LL_POSTING , "Wrote Netmail to: %u:%u/%u.%u",
                 msg->destAddr.zone, msg->destAddr.net, msg->destAddr.node, msg->destAddr.point);
      } else {
         w_log( LL_ERROR, "Could not write message to %s", areaName);
      } /* endif */

      MsgCloseArea(netmail);
   } else {
/*     printf("%u\n", msgapierr); */
      w_log( LL_ERROR, "Could not open netmailarea %s", areaName);
   } /* endif */
}

void writeTic(char *ticfile,s_ticfile *tic)
{
    FILE *tichandle;
    unsigned int i;
    s_filearea *filearea = NULL;

    tichandle=fopen(ticfile,"wb");

    fprintf(tichandle,"Created by HTick, written by Gabriel Plutzar\r\n");
    fprintf(tichandle,"File %s\r\n",tic->file);
    fprintf(tichandle,"Area %s\r\n",tic->area);

    filearea=getFileArea(config,tic->area);

    if(!tic->areadesc && filearea && filearea->description)
    {
        tic->areadesc = sstrdup(filearea->description);
        if (config->outtab) {
            recodeToTransportCharset(tic->areadesc);
        }
    }
    if (tic->areadesc)
        fprintf(tichandle,"Areadesc %s\r\n",tic->areadesc);

    for (i=0;i<tic->anzdesc;i++)
        fprintf(tichandle,"Desc %s\r\n",tic->desc[i]);

    for (i=0;i<tic->anzldesc;i++)
        fprintf(tichandle,"LDesc %s\r\n",tic->ldesc[i]);

    if (tic->replaces)
        fprintf(tichandle,"Replaces %s\r\n",tic->replaces);
    if (tic->from.zone!=0)
        fprintf(tichandle,"From %s\r\n",aka2str(tic->from));
    if (tic->to.zone!=0)
        fprintf(tichandle,"To %s\r\n",aka2str(tic->to));
    if (tic->origin.zone!=0)
        fprintf(tichandle,"Origin %s\r\n",aka2str(tic->origin));
    if (tic->size!=0)
        fprintf(tichandle,"Size %u\r\n",tic->size);
    if (tic->date!=0)
        fprintf(tichandle,"Date %lu\r\n",tic->date);
    if (tic->crc!=0)
        fprintf(tichandle,"Crc %08lX\r\n",tic->crc);

    for (i=0;i<tic->anzpath;i++)
        fprintf(tichandle,"Path %s\r\n",tic->path[i]);

    for (i=0;i<tic->anzseenby;i++)
        fprintf(tichandle,"Seenby %s\r\n",aka2str(tic->seenby[i]));

    if (tic->password)
        fprintf(tichandle,"Pw %s\r\n",tic->password);

    fclose(tichandle);
}

void disposeTic(s_ticfile *tic)
{
   unsigned int i;

   nfree(tic->seenby);
   nfree(tic->file);
   nfree(tic->area);
   nfree(tic->areadesc);
   nfree(tic->replaces);
   nfree(tic->password);

   for (i=0;i<tic->anzpath;i++)
      nfree(tic->path[i]);
   nfree(tic->path);

   for (i=0;i<tic->anzdesc;i++)
      nfree(tic->desc[i]);
   nfree(tic->desc);

   for (i=0;i<tic->anzldesc;i++)
      nfree(tic->ldesc[i]);
   nfree(tic->ldesc);
}

/* Read tic-file and store values into 2nd parameter.
 * Return 1 if success and 0 if error
 */
int parseTic(char *ticfile,s_ticfile *tic)
{
   FILE *tichandle;
   char *line, *token, *param, *linecut = "";
   s_link *ticSourceLink=NULL;

#if defined(UNIX) || defined(__DJGPP__)
   tichandle=fopen(ticfile,"r");
#else
   // insure that ticfile won't be removed while parsing
   int fh = 0;
   fh = sopen( ticfile, O_RDWR | O_BINARY, SH_DENYWR);
   if( fh<0 ){
     w_log(LL_ERROR, "Can't open '%s': %s", ticfile, strerror(errno));
     return 0;
   }
   tichandle = fdopen(fh,"r");
#endif

   if(!tichandle){
     w_log(LL_ERROR, "Can't open '%s': %s", ticfile, strerror(errno));
     return 0;
   }

   memset(tic,'\0',sizeof(s_ticfile));

   while ((line = readLine(tichandle)) != NULL) {
      line = trimLine(line);

      if (*line!=0 || *line!=10 || *line!=13 || *line!=';' || *line!='#') {
         if (config->MaxTicLineLength) {
            linecut = (char *) smalloc(config->MaxTicLineLength+1);
            strncpy(linecut,line,config->MaxTicLineLength);
            linecut[config->MaxTicLineLength] = 0;
            token=strtok(linecut, " \t");
         } else token=strtok(line, " \t");
         param=stripLeadingChars(strtok(NULL, "\0"), "\t");
         if (token && param) {
            if (stricmp(token,"created")==0);
            else if (stricmp(token,"file")==0) tic->file = sstrdup(param);
            else if (stricmp(token,"areadesc")==0) tic->areadesc = sstrdup(param);
            else if (stricmp(token,"area")==0) tic->area = sstrdup(param);
            else if (stricmp(token,"desc")==0) {
               tic->desc=
               srealloc(tic->desc,(tic->anzdesc+1)*sizeof(*tic->desc));
               tic->desc[tic->anzdesc]=sstrdup(param);
               tic->anzdesc++;
            }
            else if (stricmp(token,"replaces")==0) tic->replaces = sstrdup(param);
            else if (stricmp(token,"pw")==0) tic->password = sstrdup(param);
            else if (stricmp(token,"size")==0) tic->size=atoi(param);
            else if (stricmp(token,"crc")==0) tic->crc = strtoul(param,NULL,16);
            else if (stricmp(token,"date")==0) tic->date=atoi(param);

            else if (stricmp(token,"from")==0) {
	       string2addr(param,&tic->from);
	       ticSourceLink = getLinkFromAddr(config, tic->from);
	    }
	    else if (stricmp(token,"to")==0) string2addr(param,&tic->to);
            else if ((stricmp(token,"Destination")==0) &&
		     (ticSourceLink && !ticSourceLink->FileFixFSC87Subset))
	                string2addr(param,&tic->to);
            else if (stricmp(token,"origin")==0) string2addr(param,&tic->origin);
            else if (stricmp(token,"magic")==0);
            else if (stricmp(token,"seenby")==0) {
               s_addr Aka;
               string2addr(param,&Aka);
               seenbyAdd ( &tic->seenby, &tic->anzseenby, &Aka);
            }
            else if (stricmp(token,"path")==0) {
               tic->path=
                  srealloc(tic->path,(tic->anzpath+1)*sizeof(*tic->path));
               tic->path[tic->anzpath]=sstrdup(param);
               tic->anzpath++;
            }
            else if (stricmp(token,"ldesc")==0) {
               tic->ldesc=
                  srealloc(tic->ldesc,(tic->anzldesc+1)*sizeof(*tic->ldesc));
               tic->ldesc[tic->anzldesc]=sstrdup(param);
               tic->anzldesc++;
            }
            else {
               /*   printf("Unknown Keyword %s in Tic File\n",token); */
               if (ticSourceLink && !ticSourceLink->FileFixFSC87Subset) w_log( '7', "Unknown Keyword %s in Tic File",token);
            }
         } /* endif */
         if (config->MaxTicLineLength) nfree(linecut);
      } /* endif */
      nfree(line);
   } /* endwhile */

  fclose(tichandle);

  if (!tic->anzdesc) {
     tic->desc = srealloc(tic->desc,sizeof(*tic->desc));
     tic->desc[0] = sstrdup("no desc");
     tic->anzdesc = 1;
  }

  return(1);
}


int readCheck(s_filearea *echo, s_link *link)
{

  /* rc == '\x0000' access o'k
   rc == '\x0001' no access group
   rc == '\x0002' no access level
   rc == '\x0003' no access export
   rc == '\x0004' not linked
   rc == '\x0005' pause
   */

  unsigned int i;

  for (i=0; i<echo->downlinkCount; i++)
  {
    if (link == echo->downlinks[i]->link) break;
  }

  if (i == echo->downlinkCount) return 4;

  /* pause */
  if ((link->Pause & FPAUSE) == FPAUSE && !echo->noPause) return 5;

/* Do not check for groupaccess here, use groups only (!) for Filefix */
/*  if (strcmp(echo->group,"0")) {
      if (link->numAccessGrp) {
          if (config->numPublicGroup) {
              if (!grpInArray(echo->group,link->AccessGrp,link->numAccessGrp) &&
                  !grpInArray(echo->group,config->PublicGroup,config->numPublicGroup))
                  return 1;
          } else if (!grpInArray(echo->group,link->AccessGrp,link->numAccessGrp)) return 1;
      } else if (config->numPublicGroup) {
          if (!grpInArray(echo->group,config->PublicGroup,config->numPublicGroup)) return 1;
      } else return 1;
  }*/

  if (echo->levelread > link->level) return 2;

  if (i < echo->downlinkCount)
  {
    if (echo->downlinks[i]->export == 0) return 3;
  }

  return 0;
}

int writeCheck(s_filearea *echo, s_addr *aka)
{

  /* rc == '\x0000' access o'k
   rc == '\x0001' no access group
   rc == '\x0002' no access level
   rc == '\x0003' no access import
   rc == '\x0004' not linked
   */

  unsigned int i;

  s_link *link;

  if (!addrComp(*aka,*echo->useAka)) return 0;

  link = getLinkForFileArea (config, aka2str(*aka), echo);
  if (link == NULL) return 4;

  for (i=0; i<echo->downlinkCount; i++)
  {
    if (link == echo->downlinks[i]->link) break;
  }
  if (i == echo->downlinkCount) return 4;

/* Do not check for groupaccess here, use groups only (!) for Filefix */
/*  if (strcmp(echo->group,"0")) {
      if (link->numAccessGrp) {
         if (config->numPublicGroup) {
            if (!grpInArray(echo->group,link->AccessGrp,link->numAccessGrp) &&
                !grpInArray(echo->group,config->PublicGroup,config->numPublicGroup))
               return 1;
         } else
            if (!grpInArray(echo->group,link->AccessGrp,link->numAccessGrp)) return 1;
      } else
         if (config->numPublicGroup) {
            if (!grpInArray(echo->group,config->PublicGroup,config->numPublicGroup)) return 1;
         } else return 1;
   }*/

  if (echo->levelwrite > link->level) return 2;

  if (i < echo->downlinkCount)
  {
    if (link->import == 0) return 3;
  }

  return 0;
}

void doSaveTic(char *ticfile,s_ticfile *tic, s_filearea *filearea)
{
    char *filename = NULL;
    unsigned int i;
    s_savetic *savetic;

    for (i = 0; i< config->saveTicCount; i++)
    {
        savetic = &(config->saveTic[i]);
        if (patimat(tic->area,savetic->fileAreaNameMask)==1) {

            char *ticFname = GetFilenameFromPathname(ticfile);

            w_log(LL_FILENAME,"Saving Tic-File %s to %s", ticFname, savetic->pathName);
            xscatprintf(&filename,"%s%s", savetic->pathName,ticFname);
            if (copy_file(ticfile,filename)!=0) {
                w_log(LL_ERROR,"File %s not found or not moveable",ticfile);
            };
            if( filearea &&
               !filearea->pass && !filearea->sendorig && savetic->fileAction)
            {
                char *from = NULL, *to = NULL;
                xstrscat(&from, filearea->pathName, tic->file, NULL);
                xstrscat(&to,   savetic->pathName, tic->file, NULL);
                if( savetic->fileAction == 1)
                   copy_file(from,to);
                if( savetic->fileAction == 2)
                   link_file(from,to);
                nfree(from); nfree(to);
            }
            break;
        };

    };
    nfree(filename);
    return;
}

int sendToLinks(int isToss, s_filearea *filearea, s_ticfile *tic,
                char *filename)
/*
   isToss == 1 - tossing
   isToss == 0 - hatching
*/
{
   unsigned int i, z;
   char descr_file_name[256], newticedfile[256], fileareapath[256];
   char timestr[40];
   time_t acttime;
   s_addr *old_seenby = NULL;
   s_addr old_from, old_to;
   int old_anzseenby = 0;
   int readAccess;
   int cmdexit;
   char *comm;
   char *p;
   unsigned int minLinkCount;


   if (isToss == 1) minLinkCount = 2; // uplink and downlink
   else minLinkCount = 1;             // only downlink

   if (!filearea->pass)
      strcpy(fileareapath,filearea->pathName);
   else
      strcpy(fileareapath,config->passFileAreaDir);
   p = strrchr(fileareapath,PATH_DELIM);
   if(p) strLower(p+1);

   _createDirectoryTree(fileareapath);

   if (isToss == 1 && tic->replaces!=NULL && !filearea->pass && !filearea->noreplace) {
       /* Delete old file[s] */
       int num_files;
       char *repl;

       repl = strrchr(tic->replaces,PATH_DELIM);
       if (repl==NULL) repl = tic->replaces;
       else repl++;
       num_files = removeFileMask(fileareapath,repl);
       if (num_files>0) {
           w_log(LL_DEL,"Removed %d file[s]. Filemask: %s",num_files,repl);
       }
   }


   strcpy(newticedfile,fileareapath);
   strcat(newticedfile,MakeProperCase(tic->file));

   if(!filearea->pass && filearea->noreplace && fexist(newticedfile)) {
       w_log(LL_ERROR,"File %s already exist in filearea %s. Can't replace it",tic->file,tic->area);
       return(3);
   }

   if (isToss == 1) {
       if (!filearea->sendorig) {
           if( fexist(filename) ){
              unlink(newticedfile);
              if (move_file(filename,newticedfile)!=0) {
               w_log(LL_ERROR,"File %s not moveable to %s",filename,newticedfile);
                return(2);
              } else {
                w_log('6',"Moved %s to %s",filename,newticedfile);
              }
           }else
              w_log(LL_ERROR,"File %s not found",filename);
       } else {
           if( fexist(filename) ){
             unlink(newticedfile);
             if (copy_file(filename,newticedfile)!=0) {
               w_log(LL_ERROR,"File %s not moveable to %s",filename,newticedfile);
               return(2);
             } else {
               w_log('6',"Put %s to %s",filename,newticedfile);
             }
           }else
              w_log(LL_ERROR,"File %s not found",filename);
           strcpy(newticedfile,config->passFileAreaDir);
           strcat(newticedfile,MakeProperCase(tic->file));
           if( fexist(filename) ){
             unlink(newticedfile);
             if (move_file(filename,newticedfile)!=0) {
               w_log(LL_ERROR,"File %s not moveable to %s",filename,newticedfile);
               return(2);
             } else {
               w_log('6',"Moved %s to %s",filename,newticedfile);
             }
           }else
              w_log(LL_ERROR,"File %s not found",filename);
       }
   } else if (strcasecmp(filename,newticedfile) != 0){
       if( fexist(filename) ){
         unlink(newticedfile);
         if (copy_file(filename,newticedfile)!=0) {
               w_log(LL_ERROR,"File %s not moveable to %s",filename,newticedfile);
           return(2);
         } else {
             w_log('6',"Put %s to %s",filename,newticedfile);
         }
       }else
          w_log(LL_ERROR,"File %s not found",filename);
   }

   if (tic->anzldesc==0 && config->fileDescName && !filearea->nodiz && isToss)
         GetDescFormDizFile(newticedfile, tic);

   if (!filearea->pass) {
      strcpy(descr_file_name, filearea->pathName);
      strcat(descr_file_name, "files.bbs");
      adaptcase(descr_file_name);
      removeDesc(descr_file_name,tic->file);
      if (tic->anzldesc>0)
         add_description (descr_file_name, tic->file, tic->ldesc, tic->anzldesc);
      else
         add_description (descr_file_name, tic->file, tic->desc, tic->anzdesc);
   }

   if (filearea->downlinkCount>=minLinkCount) {
      /* Adding path & seenbys */
      time(&acttime);
      strcpy(timestr,asctime(gmtime(&acttime)));
      timestr[strlen(timestr)-1]=0;
      if (timestr[8]==' ') timestr[8]='0';


      tic->path=srealloc(tic->path,(tic->anzpath+1)*sizeof(*tic->path));
      tic->path[tic->anzpath] = NULL;
      xscatprintf(&tic->path[tic->anzpath],"%s %lu %s UTC %s",
          aka2str(*filearea->useAka), (unsigned long) time(NULL), timestr,versionStr);
      tic->anzpath++;
   }

   if (isToss == 1) {
      // Save seenby structure
      old_seenby = smalloc(tic->anzseenby*sizeof(s_addr));
      memcpy(old_seenby,tic->seenby,tic->anzseenby*sizeof(s_addr));
      old_anzseenby = tic->anzseenby;
      memcpy(&old_from,&tic->from,sizeof(s_addr));
      memcpy(&old_to,&tic->to,sizeof(s_addr));
   }

   for (i=0;i<filearea->downlinkCount;i++) {
       s_link* downlink = filearea->downlinks[i]->link;
       if ( (seenbyComp (tic->seenby, tic->anzseenby,downlink->hisAka) ) &&
           (readCheck(filearea, downlink) == 0)
           )
       {  // if link is not in seen-by list &
           // if link can recive files from filearea
           // Adding Downlink to Seen-By
           /*
           tic->seenby=srealloc(tic->seenby,(tic->anzseenby+1)*sizeof(s_addr));
           memcpy(&tic->seenby[tic->anzseenby],&downlink->hisAka,sizeof(s_addr));
           tic->anzseenby++;
           */
           seenbyAdd(&tic->seenby,&tic->anzseenby,&downlink->hisAka);
       }
   }

   seenbySort(tic->seenby,tic->anzseenby);

   /* Checking to whom I shall forward */
   for (i=0;i<filearea->downlinkCount;i++) {
      s_link* downlink = filearea->downlinks[i]->link;
      if (  addrComp(old_from,downlink->hisAka)   !=0 &&
            addrComp(old_to,downlink->hisAka)     !=0 &&
            addrComp(tic->origin,downlink->hisAka)!=0)
      {
         /* Forward file to */

         readAccess = readCheck(filearea, downlink);
         switch (readAccess) {
         case 0: break;
         case 5:
            w_log(LL_FROUTE,"Link %s paused",
                    aka2str(downlink->hisAka));
            break;
         case 4:
            w_log(LL_FROUTE,"Link %s not subscribe to File Area %s",
                    aka2str(old_from), tic->area);
            break;
         case 3:
            w_log(LL_FROUTE,"Not export to link %s",
                    aka2str(downlink->hisAka));
            break;
         case 2:
            w_log(LL_FROUTE,"Link %s no access level",
                    aka2str(downlink->hisAka));
            break;
         case 1:
            w_log(LL_FROUTE,"Link %s no access group",
                    aka2str(downlink->hisAka));
            break;
         }

         if (readAccess == 0) {
            if (isToss == 1 && seenbyComp(old_seenby, old_anzseenby, downlink->hisAka) == 0)
            {
               w_log(LL_FROUTE,"File %s already seenby %s",
                       tic->file,
                       aka2str(downlink->hisAka));
            } else {
               PutFileOnLink(newticedfile, tic,  downlink);
            }
         } /* if readAccess == 0 */
      } /* Forward file */
   }
   if (config->announceSpool) doSaveTic4Report(tic);

//   if (!filearea->hide) {
   // report about new files - if filearea not hidden
   /* execute external program */
   for (z = 0; z < config->execonfileCount; z++) {
     if (stricmp(filearea->areaName,config->execonfile[z].filearea) != 0) continue;
       if (patimat(tic->file,config->execonfile[z].filename) == 0) continue;
       else {
         comm = (char *) smalloc(strlen(config->execonfile[z].command)+1
                                +(!filearea->pass ? strlen(filearea->pathName) : strlen(config->passFileAreaDir))
                                +strlen(tic->file)+1);
         if (comm == NULL) {
            w_log( LL_ERROR, "Exec failed - not enough memory");
            continue;
         }
         sprintf(comm,"%s %s%s",config->execonfile[z].command,
                                (!filearea->pass ? filearea->pathName : config->passFileAreaDir),
                                tic->file);
         w_log( LL_EXEC, "Executing %s", comm);
         if ((cmdexit = system(comm)) != 0) {
           w_log( LL_ERROR, "Exec failed, code %d", cmdexit);
         }
         nfree(comm);
       }
   }

   if (isToss == 1) nfree(old_seenby);
   return(0);
}

#if !defined(UNIX)


/* FIXME: This code is nonportable and should therefore really be part
          of a porting library like huskylib or fidoconf!!!
*/

#if defined(__MINGW32__) && defined(__NT__)
/* we can't include windows.h for several reasons ... */
int __stdcall GetFileAttributesA(char *);
#define GetFileAttributes GetFileAttributesA
#endif


int hidden (char *filename)
{
#if (defined(__TURBOC__) && !defined(OS2)) || defined(__DJGPP__)
   unsigned fattrs;
   _dos_getfileattr(filename, &fattrs);
   return fattrs & _A_HIDDEN;
#elif defined (WINNT) || defined(__NT__)
   unsigned fattrs;
   fattrs = (GetFileAttributes(filename) & 0x2) ? _A_HIDDEN : 0;
   return fattrs & _A_HIDDEN;
#elif defined (OS2)
   FILESTATUS3 fstat3;
   const char *p;
   char *q,*backslashified;

   /* Under OS/2 users can also use "forward slashes" in filenames because
      the OS/2 C libraries support this, but the OS/2 API itself does not
      support this, and as we are calling an OS/2 API here we must first
      transform slashes into backslashes */

   backslashified=(char *)smalloc(strlen(filename)+1);
   for (p=filename,q=backslashified;*p;q++,p++)
   {
       if (*p=='/')
           *q='\\';
       else
           *q=*p;
   }
   *q='\0';

   DosQueryPathInfo((PSZ)backslashified,1,&fstat3,sizeof(fstat3));

   free(backslashified);

   return fstat3.attrFile & FILE_HIDDEN;
#else
#error "Don't know how to check for hidden files on this platform"
   return 0; /* well, we can't check if we don't know about the host */
#endif
}
#endif


int processTic(char *ticfile, e_tossSecurity sec)
{
   s_ticfile tic;
   size_t j;
   FILE   *flohandle;
   DIR    *dir;
   struct dirent  *file;

   char ticedfile[256], linkfilepath[256];
   char dirname[256], *realfile, *findfile, *pos;
   char *newticfile;
   s_filearea *filearea;
   s_link *from_link, *to_link;
   int busy;
   unsigned long crc;
   struct stat stbuf;
   int writeAccess;
   int fileisfound = 0;
   int rc = 0;
   char *tic_origin;

   w_log('6',"Processing Tic-File %s",ticfile);

   parseTic(ticfile,&tic);

   w_log('6',"File: %s size: %ld area: %s from: %s orig: %s",
         tic.file, tic.size, tic.area, aka2str(tic.from), tic_origin=aka2str5d(tic.origin));
   nfree(tic_origin);

   if (tic.to.zone!=0) {
      if (!isOurAka(config,tic.to)) {
         /* Forwarding tic and file to other link? */
         to_link=getLinkFromAddr(config,tic.to);
         if ( (to_link != NULL) && (to_link->forwardPkts != fOff) ) {
            if ( (to_link->forwardPkts==fSecure) && (sec != secProtInbound) && (sec != secLocalInbound) );
            else { /* Forwarding */
                busy = 0;
                if (createOutboundFileName(to_link,
                     to_link->fileEchoFlavour, FLOFILE)==0) {
                  strcpy(linkfilepath,to_link->floFile);
                  if (!busy) { // FIXME: it always not busy!!!
                     *(strrchr(linkfilepath,PATH_DELIM))=0;
                     newticfile=makeUniqueDosFileName(linkfilepath,"tic",config);
                     if (move_file(ticfile,newticfile)==0) {
                        strcpy(ticedfile,ticfile);
                        *(strrchr(ticedfile,PATH_DELIM)+1)=0;
                        j = strlen(ticedfile);
                        strcat(ticedfile,tic.file);
                        adaptcase(ticedfile);
                        strcpy(tic.file, ticedfile + j);
                        flohandle=fopen(to_link->floFile,"a");
                        fprintf(flohandle,"^%s\n",ticedfile);
                        fprintf(flohandle,"^%s\n",newticfile);
                        fclose(flohandle);
                        remove(to_link->bsyFile);
                        nfree(to_link->bsyFile);
                        nfree(to_link->floFile);
                     }
                  }
               }
               doSaveTic(ticfile,&tic,NULL);
               disposeTic(&tic);
               return(0);
            }
         }
         /* not to us and no forward */
         w_log(LL_ERROR,"Tic File adressed to %s, not to us", aka2str(tic.to));
         disposeTic(&tic);
         return(4);
      }
   }

   /* Security Check */
   from_link=getLinkFromAddr(config,tic.from);
   if (from_link == NULL) {
      w_log(LL_ERROR,"Link for Tic From Adress '%s' not found",
              aka2str(tic.from));
      disposeTic(&tic);
      return(1);
   }

   if (tic.password && ((from_link->ticPwd==NULL) ||
       (stricmp(tic.password,from_link->ticPwd)!=0))) {
      w_log(LL_ERROR,"Wrong Password %s from %s",
              tic.password,aka2str(tic.from));
      disposeTic(&tic);
      return(1);
   }

   strcpy(ticedfile,ticfile);
   *(strrchr(ticedfile,PATH_DELIM)+1)=0;
   j = strlen(ticedfile);
   strcat(ticedfile,tic.file);
   adaptcase(ticedfile);
   strcpy(tic.file, ticedfile + j);

   /* Recieve file? */
   if (!fexist(ticedfile)) {
      if (from_link->delNotRecievedTIC) {
         w_log('6',"File %s from filearea %s not received, remove his TIC",tic.file,tic.area);
         disposeTic(&tic);
         return(0);
      } else {
         w_log('6',"File %s from filearea %s not received, wait",tic.file,tic.area);
         disposeTic(&tic);
         return(5);
      }
   }

#if !defined(UNIX)
   if(hidden(ticedfile)) {
      w_log('6',"File %s from filearea %s not completely received, wait",tic.file,tic.area);
      disposeTic(&tic);
      return(5);
   }
#endif


   filearea=getFileArea(config,tic.area);

   if (filearea==NULL && from_link->autoFileCreate) {
       char *descr = NULL;
       if(tic.areadesc)           descr = sstrdup(tic.areadesc);
       if(config->intab && descr) recodeToInternalCharset(descr);
       autoCreate(tic.area,descr,&(tic.from),NULL);
       filearea=getFileArea(config,tic.area);
       nfree(descr);
   }

   if (filearea == NULL) {
      w_log(LL_ERROR,"Cannot open or create File Area %s",tic.area);
      if (!quiet) fprintf(stderr,"Cannot open or create File Area %s !\n",tic.area);
      disposeTic(&tic);
      return(2);
   }
   /* Check CRC Value and reject faulty files depending on noCRC flag */
   if (!filearea->noCRC) {
      crc = filecrc32(ticedfile);
      if (tic.crc != crc) {
          strcpy(dirname, ticedfile);
          pos = strrchr(dirname, PATH_DELIM);
          if (pos) {
              *pos = 0;
              findfile = pos+1;
              pos = strrchr(findfile, '.');
              if (pos) {

                  *(++pos) = 0;
                  strcat(findfile, "*");
#ifdef DEBUG_HPT
                  printf("NoCRC! dirname = %s, findfile = %s\n", dirname, findfile);
#endif
                  dir = opendir(dirname);

                  if (dir) {
                      while ((file = readdir(dir)) != NULL) {
                          if (patimat(file->d_name, findfile)) {
                              stat(file->d_name,&stbuf);
                              if (stbuf.st_size == tic.size) {
                                  crc = filecrc32(file->d_name);
                                  if (crc == tic.crc) {
                                      fileisfound = 1;
                                      sprintf(dirname+strlen(dirname), "%c%s", PATH_DELIM, file->d_name);
                                      break;
                                  }
                              }

                          }
                      }
                      closedir(dir);
                  }
              }
          }

          if (fileisfound) {
              realfile = smalloc(strlen(dirname)+1);
              strcpy(realfile, dirname);
              *(strrchr(dirname, PATH_DELIM)) = 0;
              findfile = makeUniqueDosFileName(dirname,"tmp",config);
              if (rename(ticedfile, findfile) != 0 ) {
                  w_log('9',"Can't file %s rename to %s", ticedfile, findfile);
                  nfree(findfile);
                  nfree(realfile);
                  disposeTic(&tic);
                  return(3);
              }
              if (rename(realfile, ticedfile) != 0) {
                  w_log('9',"Can't file %s rename to %s", realfile, ticedfile);
                  nfree(findfile);
                  nfree(realfile);
                  disposeTic(&tic);
                  return(3);
              }
              if (rename(findfile, realfile) != 0) {
                  remove(findfile);
              }
              nfree(findfile);
              nfree(realfile);
          } else {
              w_log(LL_ERROR,"Wrong CRC for file %s - in tic:%08lx, need:%08lx",tic.file,tic.crc,crc);
              disposeTic(&tic);
              return(3);
          }
      }
   }


   stat(ticedfile,&stbuf);
   tic.size = stbuf.st_size;
   if (!tic.size) {
      w_log('6',"File %s from filearea %s has zero size",tic.file,tic.area);
      disposeTic(&tic);
      return(5);
   }

   writeAccess = writeCheck(filearea,&tic.from);

   switch (writeAccess) {
   case 0: break;
   case 4:
      w_log(LL_ERROR,"Link %s not subscribed to File Area %s",
              aka2str(tic.from), tic.area);
      disposeTic(&tic);
      return(3);
   case 3:
      w_log(LL_ERROR,"Not import from link %s",
              aka2str(from_link->hisAka));
      disposeTic(&tic);
      return(3);
   case 2:
      w_log(LL_ERROR,"Link %s no access level",
      aka2str(from_link->hisAka));
      disposeTic(&tic);
      return(3);
   case 1:
      w_log(LL_ERROR,"Link %s no access group",
      aka2str(from_link->hisAka));
      disposeTic(&tic);
      return(3);
   }

   rc = sendToLinks(1, filearea, &tic, ticedfile);

   doSaveTic(ticfile,&tic,filearea);
   disposeTic(&tic);
   return(rc);
}

void processDir(char *directory, e_tossSecurity sec)
{
   DIR            *dir;
   struct dirent  *file;
   char           *dummy;
   int            rc;

   if (directory == NULL) return;

   dir = opendir(directory);
   if (dir == NULL) return;

   while ((file = readdir(dir)) != NULL) {
#ifdef DEBUG_HPT
      printf("testing %s\n", file->d_name);
#endif

      if (patimat(file->d_name, "*.TIC") == 1) {
         dummy = (char *) smalloc(strlen(directory)+strlen(file->d_name)+1);
         strcpy(dummy, directory);
         strcat(dummy, file->d_name);

#if !defined(UNIX)
         if (!hidden(dummy)) {
#endif
            rc=processTic(dummy,sec);

            switch (rc) {
               case 1:  /* pktpwd problem */
                  changeFileSuffix(dummy, "sec");
                  break;
               case 2:  /* could not open file */
                  changeFileSuffix(dummy, "acs");
                  break;
               case 3:  /* not/wrong pkt */
                  changeFileSuffix(dummy, "bad");
                  break;
               case 4:  /* not to us */
                  changeFileSuffix(dummy, "ntu");
                  break;
               case 5:  /* file not recieved */
                  break;
               default:
                  remove (dummy);
                  break;
            } /* switch */
#if !defined(UNIX)
         }
#endif
         nfree(dummy);
      } /* if */
   } /* while */
   closedir(dir);
}

void checkTmpDir(void)
{
    char tmpdir[256], newticedfile[256], newticfile[256];
    DIR            *dir=NULL;
    struct dirent  *file=NULL;
    char           *ticfile=NULL;
    s_link         *link=NULL;
    s_ticfile      tic;
    s_filearea *filearea=NULL;
    FILE *flohandle=NULL;
    int error = 0;

   w_log('6',"Checking tmp dir");
   strcpy(tmpdir, config->busyFileDir);
   dir = opendir(tmpdir);
   if (dir == NULL) return;

   while ((file = readdir(dir)) != NULL) {
      if (strlen(file->d_name) != 12) continue;
      //if (!file->d_size) continue;
      ticfile = (char *) smalloc(strlen(tmpdir)+strlen(file->d_name)+1);
      strcpy(ticfile, tmpdir);
      strcat(ticfile, file->d_name);
      if (stricmp(file->d_name+8, ".TIC") == 0) {
         memset(&tic,0,sizeof(tic));
         parseTic(ticfile,&tic);
         link = getLinkFromAddr(config, tic.to);
	 // createFlo doesn't  support ASO!!!
         //if (createFlo(link,cvtFlavour2Prio(link->fileEchoFlavour))==0) {
        if (createOutboundFileName(link, link->fileEchoFlavour, FLOFILE)==0) {
	     filearea=getFileArea(config,tic.area);
	     if (filearea!=NULL) {
               if (!filearea->pass && !filearea->sendorig) strcpy(newticedfile,filearea->pathName);
               else strcpy(newticedfile,config->passFileAreaDir);
               strcat(newticedfile,tic.file);
               adaptcase(newticedfile);
               if (!link->noTIC) {
                  if (config->separateBundles) {
                     strcpy(newticfile,link->floFile);
                     sprintf(strrchr(newticfile, '.'), ".sep%c", PATH_DELIM);
                  } else {
                     strcpy(newticfile,config->ticOutbound);
                  }
                  _createDirectoryTree(newticfile);
                  strcat(newticfile,strrchr(ticfile,PATH_DELIM)+1);
                  if( !fexist(newticfile) )
                     w_log(LL_ERROR,"File %s not found",newticfile);
                  else if (move_file(ticfile,newticfile)!=0) {
                     w_log(LL_ERROR,"File %s not moveable to %s", ticfile, newticfile);
                     error = 1;
                  }
               } else remove(ticfile);
               if (!error) {
                  flohandle=fopen(link->floFile,"a");
                  fprintf(flohandle,"%s\n",newticedfile);
                  if (!link->noTIC)
                     fprintf(flohandle,"^%s\n",newticfile);
                  fclose(flohandle);

                  w_log('6',"Forwarding save file %s for %s",
                     tic.file, aka2str(link->hisAka));
               }
            } /* if filearea */
         } /* if createFlo */
         remove(link->bsyFile);
         nfree(link->bsyFile);
         nfree(link->floFile);
         disposeTic(&tic);
      } /* if ".TIC" */
      nfree(ticfile);
   } /* while */
   closedir(dir);
}


int putMsgInArea(s_area *echo, s_message *msg, int strip, dword forceattr)
{
    char *ctrlBuff, *textStart, *textWithoutArea;
    UINT textLength = (UINT) msg->textLength;
    HAREA harea = NULL;
    HMSG  hmsg;
    XMSG  xmsg;
    char /**slash,*/ *p, *q, *tiny;
    int rc = 0;

    if (echo->msgbType==MSGTYPE_PASSTHROUGH) {
        w_log(LL_ERROR, "Can't put message to passthrough area %s!", echo->areaName);
        return rc;
    }

    if (!msg->netMail) {
	msg->destAddr.zone  = echo->useAka->zone;
	msg->destAddr.net   = echo->useAka->net;
	msg->destAddr.node  = echo->useAka->node;
	msg->destAddr.point = echo->useAka->point;
    }

	harea = MsgOpenArea((UCHAR *) echo->fileName, MSGAREA_CRIFNEC,
			(word)(echo->msgbType | (msg->netMail ? 0 : MSGTYPE_ECHO)));
    if (harea!= NULL) {
	    hmsg = MsgOpenMsg(harea, MOPEN_CREATE, 0);
	if (hmsg != NULL) {


	    textWithoutArea = msg->text;

	    if ((strip==1) && (strncmp(msg->text, "AREA:", 5) == 0)) {
		// jump over AREA:xxxxx\r
		while (*(textWithoutArea) != '\r') textWithoutArea++;
		textWithoutArea++;
		textLength -= (size_t) (textWithoutArea - msg->text);
	    }

	    if (echo->killSB) {
		tiny = strrstr(textWithoutArea, " * Origin:");
		if (tiny == NULL) tiny = textWithoutArea;
		if (NULL != (p = strstr(tiny, "\rSEEN-BY: "))) {
		    p[1]='\0';
		    textLength = (size_t) (p - textWithoutArea + 1);
		}
	    } else if (echo->tinySB) {
		tiny = strrstr(textWithoutArea, " * Origin:");
		if (tiny == NULL) tiny = textWithoutArea;
		if (NULL != (p = strstr(tiny, "\rSEEN-BY: "))) {
		    p++;
		    if (NULL != (q = strstr(p,"\001PATH: "))) {
			// memmove(p,q,strlen(q)+1);
			memmove(p,q,textLength-(size_t)(q-textWithoutArea)+1);
			textLength -= (size_t) (q - p);
		    } else {
			p[0]='\0';
			textLength = (size_t) (p - textWithoutArea);
		    }
		}
	    }

	
	    ctrlBuff = (char *) CopyToControlBuf((UCHAR *) textWithoutArea,
						 (UCHAR **) &textStart,
						 &textLength);
	    // textStart is a pointer to the first non-kludge line
	    xmsg = createXMSG(config,msg, NULL, forceattr,NULL);

	    if (MsgWriteMsg(hmsg, 0, &xmsg, (byte *) textStart, (dword)
			    textLength, (dword) textLength,
			    (dword)strlen(ctrlBuff), (byte*)ctrlBuff)!=0)
		w_log(LL_ERROR, "Could not write msg in %s!", echo->fileName);
	    else rc = 1; // normal exit

	    if (MsgCloseMsg(hmsg)!=0) {
		w_log(LL_ERROR, "Could not close msg in %s!", echo->fileName);
		rc = 0;
	    }
	    nfree(ctrlBuff);

	} else w_log(LL_ERROR, "Could not create new msg in %s!", echo->fileName);
	/* endif */
    MsgCloseArea(harea);
    } else w_log(LL_ERROR, "Could not open/create EchoArea %s!", echo->fileName);
    /* endif */
    return rc;
}

int putMsgInBadArea(s_message *msg, s_addr pktOrigAddr)
{
    char *tmp = NULL, *line = NULL, *textBuff=NULL, *areaName=NULL, *reason = NULL;

    // get real name area
    line = strchr(msg->text, '\r');
    if (strncmp(msg->text,"AREA:",5)==0) {
        *line = 0;
        xstrcat(&areaName, msg->text+5);
        *line = '\r';
    }
	reason = "report to passthrough area";
    tmp = msg->text;

    while ((line = strchr(tmp, '\r')) != NULL) {
	if (*(line+1) == '\x01') tmp = line+1;
	else { tmp = line+1; *line = 0; break; }
    }
	
    xstrscat(&textBuff, msg->text, "\rFROM: ", aka2str(pktOrigAddr), "\rREASON: ", reason, "\r", NULL);

    if (areaName) xscatprintf(&textBuff, "AREANAME: %s\r\r", areaName);
    xstrcat(&textBuff, tmp);
    nfree(areaName);
    nfree(msg->text);
    msg->text = textBuff;
    msg->textLength = strlen(msg->text);
    if (putMsgInArea(&(config->badArea), msg, 0, 0)) {
        return 1;
    }
    return 0;
}

void writeMsgToSysop(s_message *msg, char *areaName, char* origin)
{
   s_area       *echo;

   xscatprintf(&(msg->text), " \r--- %s\r * Origin: %s (%s)\r",
       (config->tearline) ? config->tearline : "",
       (config->origin) ? config->origin : config->name,
       aka2str(msg->origAddr));

   msg->textLength = strlen(msg->text);
   if (msg->netMail == 1) writeNetmail(msg, areaName);
   else {
      echo = getArea(config, areaName);
      if (echo != &(config->badArea) && echo->msgbType != MSGTYPE_PASSTHROUGH) {
          putMsgInArea(echo, msg,1,MSGLOCAL);
          w_log( LL_POSTING , "Post report message to %s area", echo->areaName);
      } else {
          w_log( LL_POSTING , "Post report message to %s area", config->badArea.areaName);
          putMsgInBadArea(msg, msg->origAddr);
      }
   }
}

void toss()
{

   w_log( LL_INFO, "Start tossing...");

   processDir(config->localInbound, secLocalInbound);
   processDir(config->protInbound, secProtInbound);
   processDir(config->inbound, secInbound);

}
