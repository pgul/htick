/*****************************************************************************
 * AreaFix for HTICK (FTN Ticker / Request Processor)
 *****************************************************************************
 * Copyright (C) 1998-2000
 *
 * Max Levenkov
 *
 * Fido:     2:5000/117
 * Internet: sackett@mail.ru
 *
 * Novosibirsk, West Siberia, Russia
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if ((!(defined(_MSC_VER) && (_MSC_VER >= 1200)) ) && (!defined(__TURBOC__)))
#include <unistd.h>
#endif

#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#include <fidoconf/xstr.h>
#include <fidoconf/afixcmd.h>

#include <smapi/prog.h>
#include <smapi/patmat.h>

#include <fcommon.h>
#include <global.h>
#include <version.h>
#include <toss.h>
#include <areafix.h>
#include <hatch.h>
#include <scan.h>

unsigned char RetFix;

char *errorRQ(char *line)
{
   char *report = NULL, err[] = "Error line";
   xscatprintf(&report,"%s %s\r", line, err);
   return report;
}

int subscribeCheck(s_filearea area, s_message *msg, s_link *link)
{
  unsigned int i;
  int found = 0;

  for (i = 0; i<area.downlinkCount;i++)
  {
    if (addrComp(msg->origAddr, area.downlinks[i]->link->hisAka)==0) return 0;
  }

  if (area.group)
  {
    if (link->numAccessGrp > 0)
    {
      for (i = 0; i < link->numAccessGrp; i++)
	if (strcmp(area.group, link->AccessGrp[i]) == 0) found = 1;
    }

    if (config->numPublicGroup > 0)
    {
      for (i = 0; i < config->numPublicGroup; i++)
	if (strcmp(area.group, config->PublicGroup[i]) == 0) found = 1;
    }
  }
  else found = 1;

  if (found == 0) return 2;
  if (area.hide) return 3;
  return 1;
}


int subscribeAreaCheck(s_filearea *area, s_message *msg, char *areaname, s_link *link) {
	int rc=4;
	
	if (!areaname) return rc;
	
	if (patimat(area->areaName,areaname)==1) {
		rc=subscribeCheck(*area, msg, link);
		// 0 - already subscribed
		// 1 - need subscribe
		// 2 - no access group
		// 3 - area is hidden
		if (area->mandatory) rc = 2;
	} else rc = 4; // this is another area
	
	return rc;
}

void addlink(s_link *link, s_filearea *area) {
    s_arealink *arealink;

    area->downlinks = srealloc(area->downlinks, sizeof(s_arealink*)*(area->downlinkCount+1));
    area->downlinks[area->downlinkCount] = (s_arealink*)scalloc(1, sizeof(s_arealink));
    area->downlinks[area->downlinkCount]->link = link;
	arealink = area->downlinks[area->downlinkCount];
	
	if (link->numOptGrp > 0) {
		// default set export on, import on, mandatory off
		arealink->export = 1;
		arealink->import = 1;
		arealink->mandatory = 0;
		
		if (grpInArray(area->group,link->optGrp,link->numOptGrp)) {
			arealink->export = link->export;
			arealink->import = link->import;
			arealink->mandatory = link->mandatory;
		}
		
	} else {
		arealink->export = link->export;
		arealink->import = link->import;
		arealink->mandatory = link->mandatory;
	}
	if (area->mandatory) arealink->mandatory = 1;
	if (link->level < area->levelread)	arealink->export=0;
	if (link->level < area->levelwrite) arealink->import=0;
	// paused link can't receive mail
	if ((link->Pause & FPAUSE) == FPAUSE) arealink->export = 0;
    
    area->downlinkCount++;
}

void removelink(s_link *link, s_filearea *area) {
	unsigned int i;
	s_link *links;

	for (i=0; i < area->downlinkCount; i++) {
           links = area->downlinks[i]->link;
           if (addrComp(link->hisAka, links->hisAka)==0) break;
	}
	
	free(area->downlinks[i]);
	area->downlinks[i] = area->downlinks[area->downlinkCount-1];
	area->downlinkCount--;
}

char *unlinked(s_message *msg, s_link *link)
{
    unsigned int i, rc, n;
    char *report=NULL;
    s_filearea *area;
    
    area=config->fileAreas;
    
    xscatprintf(&report, "Unlinked areas to %s\r\r", aka2str(link->hisAka));
    
    for (i=n=0; i<config->fileAreaCount; i++) {
	rc=subscribeCheck(area[i], msg, link);
	if (rc == 1) {
        xscatprintf(&report, " %s\r", area[i].areaName);
        n++;
	}
    }
    xscatprintf(&report, "\r%u areas unlinked\r", n);
    w_log( '8', "FileFix: unlinked fileareas list sent to %s", aka2str(link->hisAka));

    return report;
}

char *list(s_message *msg, s_link *link) {

    unsigned int i,j,active,avail,rc,desclen;
    unsigned int *areaslen;
    unsigned int maxlen;
    char *report=NULL;
    int readdeny, writedeny;

   areaslen = smalloc(config->fileAreaCount * sizeof(int));

   maxlen = 0;
   for (i=0; i< config->fileAreaCount; i++) {
      areaslen[i]=strlen(config->fileAreas[i].areaName);
      if (areaslen[i]>maxlen) maxlen = areaslen[i];
   }
   xscatprintf(&report, "Available fileareas for %s\r\r", aka2str(link->hisAka));

   for (i=active=avail=0; i< config->fileAreaCount; i++) {

      rc=subscribeCheck(config->fileAreas[i],msg, link);
      if (rc < 2) {
         if (config->fileAreas[i].description!=NULL)
            desclen=strlen(config->fileAreas[i].description);
         else
           desclen=0;

         if (rc==0) {
            readdeny = readCheck(&config->fileAreas[i], link);
            writedeny = writeCheck(&config->fileAreas[i], &(link->hisAka));
            if (!readdeny && !writedeny)
               xstrcat(&report,"& ");
            else if (writedeny)
               xstrcat(&report,"+ ");
            else xstrcat(&report,"* ");
            active++;
            avail++;
         } else {
            xstrcat(&report,"  ");
            avail++;
         }
         xstrcat(&report, config->fileAreas[i].areaName);
         if (desclen!=0) {
            xstrcat(&report," ");
            for (j=0;j<(maxlen)-areaslen[i];j++) 
                xstrcat(&report,".");
            xstrcat(&report," ");
            xstrcat(&report,config->fileAreas[i].description);
         }
         xstrcat(&report,"\r");
      }
   }

   xscatprintf(&report, "\r '+'  You are receiving files from this area.\r '*'  You can send files to this file echo.\r '&'  You can send and receive files.\r\r%i areas available for %s, %i areas active\r", avail, aka2str(link->hisAka), active);

   w_log( '8', "FileFix: list sent to %s", aka2str(link->hisAka));

   nfree(areaslen);

   return report;
}

char *linked(s_message *msg, s_link *link, int action)
{
    unsigned int i, n, rc;
    char *report=NULL;
    int readdeny, writedeny;

    if ((link->Pause & FPAUSE) == FPAUSE) 
        xscatprintf(&report, "\rPassive fileareas on %s\r\r", aka2str(link->hisAka));
    else
        xscatprintf(&report, "\rActive fileareas on %s\r\r", aka2str(link->hisAka));
							
    
    for (i=n=0; i<config->fileAreaCount; i++) {
	rc=subscribeCheck(config->fileAreas[i], msg, link);
	if (rc==0) {
        if (action == 1) {
            readdeny = readCheck(&config->fileAreas[i], link);
            writedeny = writeCheck(&config->fileAreas[i], &(link->hisAka));
            if (!readdeny && !writedeny)
                xstrcat(&report,"& ");
            else if (writedeny)
                xstrcat(&report,"+ ");
            else 
                xstrcat(&report,"* ");
        } else {
            xstrcat(&report," ");
        }
	    xstrcat(&report, config->fileAreas[i].areaName);
	    xstrcat(&report, "\r");
	    n++;
	}
    }
    if (action == 1) 
        xscatprintf(&report, "\r '+'  You are receiving files from this area.\r '*'  You can send files to this file echo.\r '&'  You can send and receive files.\r\r%u areas linked for %s\r", n, aka2str(link->hisAka));
    else 
        xscatprintf(&report, "\r%u areas linked\r", n);
    return report;
}

char *help(s_link *link) {
	FILE *f;
	int i=1;
	char *help = NULL;
	long endpos;

	if (config->filefixhelp!=NULL) {
		if ((f=fopen(config->filefixhelp,"r")) == NULL)
			{
				if (!quiet) fprintf(stderr,"FileFix: cannot open help file \"%s\"\n",
						config->filefixhelp);
				return NULL;
			}
		
		fseek(f,0l,SEEK_END);
		endpos=ftell(f);
		
		help=(char*) scalloc((size_t) endpos + 1,sizeof(char));

		fseek(f,0l,SEEK_SET);
		endpos = fread(help,1,(size_t) endpos,f);
		
		for (i=0; i<endpos; i++) {
		   if (help[i]=='\r' && (i+1)<endpos && help[i+1]=='\n') help[i]=' ';
		   if (help[i]=='\n') help[i]='\r';
		}

		fclose(f);

		w_log( '8', "FileFix: help sent to %s", aka2str(link->hisAka));

		return help;
	}

	return NULL;
}

char *available(s_link *link) {                                                 
/*        FILE *f;                                                                
        int i=1;                                                                
        char *avail;                                              
        long endpos;                                                            

        if (config->available!=NULL) {                                          
                if ((f=fopen(config->available,"r")) == NULL)                   
                        {                                                       
                                if (!quiet) fprintf(stderr,"FileFix: cannot open Available Areas file \"%s\"\n",
                                                config->available);
                                return NULL;                                    
                        }                                                       

                fseek(f,0l,SEEK_END);                                           
                endpos=ftell(f);                                                

                avail=(char*) scalloc((size_t) endpos,sizeof(char*));            

                fseek(f,0l,SEEK_SET);                                           
                endpos = fread(avail,1,(size_t) endpos,f);                               
                for (i=0; i<endpos; i++) if (avail[i]=='\n') avail[i]='\r';     

                fclose(f);                                                      

                w_log( '8', "FileFix: Available Area List sent to %s",link->name);
                                   
                return avail;                                                   
        }                                                                       
  */                                                                              
        return NULL;                                                            
}                                                                               

int changeconfig(char *fileName, s_filearea *area, s_link *link, int action) {
    FILE *f_conf;
    char *cfgline = NULL, *line = NULL, *token = NULL, *areaName = NULL, *tmpPtr =NULL;
    long endpos, cfglen;
    long strbeg = 0, strend = -1;
    
    areaName = area->areaName;
    
    if (init_conf(fileName))
        return 1;
    
    while ((cfgline = configline()) != NULL) {
        line = sstrdup(cfgline);
        line = trimLine(line);
        line = stripComment(line);
        if (line[0] != 0) {
            tmpPtr = line = shell_expand(line);
            token = strseparate(&tmpPtr, " \t");
            if (stricmp(token, "filearea")==0) {
                token = strseparate(&tmpPtr, " \t"); 
                if (stricmp(token, areaName)==0) {
                    fileName = sstrdup(getCurConfName());
                    strend = get_hcfgPos();
                    break;
                }
            }
        }
        strbeg = get_hcfgPos();
        nfree(cfgline);
        nfree(line);
    }
    close_conf();
    nfree(line);
    if (strend == -1) {
        nfree(cfgline);
        return 1; // impossible
    }
    
    if ((f_conf=fopen(fileName,"r+b")) == NULL)
    {
        if (!quiet) fprintf(stderr, "FileFix: cannot open config file %s \n", fileName);
        nfree(cfgline);
        nfree(fileName);
        return 1;
    }
    nfree(fileName);
    
    fseek(f_conf, 0L, SEEK_END);
    endpos = ftell(f_conf);
    cfglen = endpos - strend;
    line = (char*) smalloc((size_t) cfglen+1);
    fseek(f_conf, strend, SEEK_SET);
    cfglen = fread(line, sizeof(char), cfglen, f_conf);
    line[cfglen]='\0';
    fseek(f_conf, strbeg, SEEK_SET);
    setfsize( fileno(f_conf), strbeg );
    
    switch (action) {
    case 0: 
        xstrscat(&cfgline, " ", aka2str(link->hisAka), NULL);
        break;
    case 1:
        DelLinkFromString(cfgline, link->hisAka);
        break;
    default: break;
    }
    fprintf(f_conf, "%s%s%s", cfgline, cfgEol(), line);
    fclose(f_conf);
    nfree(line);
    nfree(cfgline);
    return 0;
}

char *subscribe(s_link *link, s_message *msg, char *cmd) {
	unsigned int i, c, rc=4;
	char *line, *report = NULL;
	s_filearea *area;

	line = cmd;
	
	if (line[0]=='+') line++;
	
	report=(char*)scalloc(1, sizeof(char));

	for (i=0; i<config->fileAreaCount; i++) {
		rc=subscribeAreaCheck(&(config->fileAreas[i]),msg,line, link);
		if (rc == 4) continue;
		
		area = &(config->fileAreas[i]);
		
		for (c = 0; c<area->downlinkCount; c++) {
		    if (link == area->downlinks[c]->link) {
			if (area->downlinks[c]->mandatory) rc=5;
			break;
		    }
		}
		
		switch (rc) {
		    case 0: 
			xscatprintf(&report, "%s Already linked\r", area->areaName);
			w_log( '8', "FileFix: %s already linked to %s", aka2str(link->hisAka), area->areaName);
    			break;
		    case 1: 
		    case 3: 
			changeconfig (getConfigFileName(), area, link, 0);
			addlink(link, area);
			xscatprintf(&report, "%s Added\r",area->areaName);
			w_log( '8', "FileFix: %s subscribed to %s",aka2str(link->hisAka),area->areaName);
			break;
		    case 5: 
                xscatprintf(&report, "%s Link is not possible\r", area->areaName);
			w_log( '8', "FileFix: area %s -- link is not possible for %s", area->areaName, aka2str(link->hisAka));
			break;
		    default :
			w_log( '8', "FileFix: filearea %s -- no access for %s", area->areaName, aka2str(link->hisAka));
			continue;
		}
	}
	
	if (!report) {
	    xscatprintf(&report,"%s Not found\r",line);
	    w_log( '8', "FileFix: filearea %s is not found",line);
	}
	return report;
}

char *unsubscribe(s_link *link, s_message *msg, char *cmd) {
	unsigned int i, c, rc = 2;
	char *line;
	char *report=NULL;
	s_filearea *area;
	
	line = cmd;
	
	if (line[1]=='-') return NULL;
	line++;
	
	for (i = 0; i< config->fileAreaCount; i++) {
		rc=subscribeAreaCheck(&(config->fileAreas[i]),msg,line, link);
		if ( rc==4 ) continue;
	
		area = &(config->fileAreas[i]);
		
		for (c = 0; c<area->downlinkCount; c++) {
		    if (link == area->downlinks[c]->link) {
			if (area->downlinks[c]->mandatory) rc=5;
			break;
		    }
		}
		
		switch (rc) {
		case 0: removelink(link, area);
			changeconfig (getConfigFileName(),  area, link, 1);
			xscatprintf(&report, "%s Unlinked\r",area->areaName);
			w_log( '8', "FileFix: %s unlinked from %s",aka2str(link->hisAka),area->areaName);
			break;
		case 1: if (strstr(line, "*")) continue;
			xscatprintf(&report, "%s Not linked\r",line);
			w_log( '8', "FileFix: area %s is not linked to %s", area->areaName, aka2str(link->hisAka));
			break;
		case 5: 
            xscatprintf(&report, "%s Unlink is not possible\r", area->areaName);
			w_log( '8', "FileFix: area %s -- unlink is not possible for %s", area->areaName, aka2str(link->hisAka));
			break;
		default: w_log( '8', "FileFix: area %s -- no access for %s", area->areaName, aka2str(link->hisAka));
			continue;
		}
	}
	if (!report) {
		xscatprintf(&report, "%s Not found\r",line);
		w_log( '8', "FileFix: area %s is not found", line);
	}
	return report;
}

char *resend(s_link *link, s_message *msg, char *cmd)
{
    unsigned int rc, i;
    char *line;
    char *report=NULL, *token = NULL, *filename=NULL, *filearea=NULL;
    s_filearea *area = NULL;

    line = cmd;
    line=stripLeadingChars(line, " \t");
    token = strtok(line, " \t\0");
    if (token == NULL)
    {       
        xscatprintf(&report, "Error in line! Format: %%Resend <file> <filearea>\r");
        return report;
    }
    filename = sstrdup(token);
    token=stripLeadingChars(strtok(NULL, "\0"), " ");
    if (token == NULL)
    {
        nfree(filename);
        xscatprintf(&report, "Error in line! Format: %%Resend <file> <filearea>\r");
        return report;
    }
    filearea = sstrdup(token);
    area = getFileArea(config,filearea);
    if (area != NULL) {
        rc = 1;
        for (i = 0; i<area->downlinkCount;i++)
            if (addrComp(msg->origAddr, area->downlinks[i]->link->hisAka)==0)
                rc = 0;
            if (rc == 1 && area->mandatory == 1) rc = 5;
            else rc = send(filename,filearea,aka2str(link->hisAka));
            switch (rc) {
            case 0: xscatprintf(&report, "Send %s from %s for %s\r",
                        filename,filearea,aka2str(link->hisAka));
                break;
            case 1: xscatprintf(&report, "Error: Passthrough filearea %s!\r",filearea);
                w_log( '8', "FileFix %%Resend: Passthrough filearea %s", filearea);
                break;
            case 2: xscatprintf(&report, "Error: Filearea %s not found!\r",filearea);
                w_log( '8', "FileFix %%Resend: Filearea %s not found", filearea);
                break;
            case 3: xscatprintf(&report, "Error: File %s not found!\r",filename);
                w_log( '8', "FileFix %%Resend: File %s not found", filename);
                break;
            case 5: xscatprintf(&report, "Error: You don't have access for filearea %s!\r",filearea);
                w_log( '8', "FileFix %%Resend: Link don't have access for filearea %s", filearea);
                break;
            }
    } else {
        xscatprintf(&report, "Error: filearea %s not found!\r",filearea);
        w_log( '8', "FileFix %%Resend: Filearea %s not found", filearea);
    }
    
    nfree(filearea);
    nfree(filename);
    return report;
}


char *pause_link(s_message *msg, s_link *link)
{
    char *tmp = NULL;
    char *report=NULL;
    
    if ((link->Pause & FPAUSE) != FPAUSE) {
        if (Changepause(getConfigFileName(), link,0,FPAUSE) == 0) 
            return NULL;
    }
    xstrcat(&report, " System switched to passive\r");
    tmp = linked (msg, link, 0);
    xstrcat(&report, tmp);
    nfree(tmp);

    return report;
}

char *resume_link(s_message *msg, s_link *link)
{
    char *tmp = NULL, *report=NULL;
    
    if ((link->Pause & FPAUSE) == FPAUSE) {
       if (Changepause(getConfigFileName(), link,0,FPAUSE) == 0)
          return NULL;
    }
    xstrcat(&report, " System switched to active\r");
    tmp = linked(msg, link, 0);
    xstrcat(&report, tmp);
    nfree(tmp);
    return report;
}

int tellcmd(char *cmd) {
	char *line = NULL;

	if (strncasecmp(cmd, "* Origin:", 9) == 0) return NOTHING;
	line = strpbrk(cmd, " \t");
	if (line && *cmd != '%') *line = 0;

	line = cmd;

	switch (line[0]) {
	case '%': 
		line++;
		if (*line == 0) return ERROR;
		if (stricmp(line,"list")==0) return LIST;
		if (stricmp(line,"help")==0) return HELP;
		if (stricmp(line,"unlinked")==0) return UNLINK;
		if (stricmp(line,"linked")==0) return LINKED;
		if (stricmp(line,"query")==0) return LINKED;
		if (stricmp(line,"pause")==0) return PAUSE;
		if (stricmp(line,"resume")==0) return RESUME;
		if (stricmp(line,"info")==0) return INFO;
		if (strncasecmp(line, "resend", 6)==0)
		   if (line[6] != 0) return RESEND;
		return ERROR;
	case '\001': return NOTHING;
	case '\000': return NOTHING;
	case '-'  : return DEL;
	case '+': line++; if (line[0]=='\000') return ERROR;
	default: return ADD;
	}
	
	return 0;
}

char *processcmd(s_link *link, s_message *msg, char *line, int cmd) {
	
	char *report = NULL;
	
	switch (cmd) {

	case NOTHING: return NULL;

	case LIST: report = list (msg, link);
		RetFix=LIST;
		break;
	case HELP: report = help (link);
		RetFix=HELP;
		break;
	case ADD: report = subscribe (link,msg,line);
		RetFix=ADD;
		break;
	case DEL: report = unsubscribe (link,msg,line);
		RetFix=DEL;
		break;
	case UNLINK: report = unlinked (msg, link);
		RetFix=UNLINK;
		break;
	case LINKED: report = linked (msg, link, 1);
		RetFix=LINKED;
		break;
	case PAUSE: report = pause_link (msg, link);
		RetFix=PAUSE;
		break;
	case RESUME: report = resume_link (msg, link);
		RetFix=RESUME;
		break;
/*	case INFO: report = info_link(msg, link);
		RetFix=INFO;
		break;*/
	case RESEND: report = resend(link, msg, line+7);
		RetFix=RESEND;
		break;
	case ERROR: report = errorRQ(line);
		RetFix=ERROR;
		break;
	default: return NULL;
	}
	
	return report;
}

char *areastatus(char *preport, char *text)
{
    char *pth = NULL, *ptmp = NULL, *tmp = NULL, *report = NULL, tmpBuff[256];
    pth = (char*)scalloc(1, sizeof(char));
    tmp = preport;
    ptmp = strchr(tmp, '\r');
    while (ptmp) {
		*ptmp=0;
		ptmp++;
        report=strchr(tmp, ' ');
		*report=0;
		report++;
        if (strlen(tmp) > 50) tmp[50] = 0;
		if (50-strlen(tmp) == 0) sprintf(tmpBuff, " %s  %s\r", tmp, report);
        else if (50-strlen(tmp) == 1) sprintf(tmpBuff, " %s   %s\r", tmp, report);
		else sprintf(tmpBuff, " %s %s  %s\r", tmp, print_ch(50-strlen(tmp)-1, '.'), report);
        pth=(char*)srealloc(pth, strlen(tmpBuff)+strlen(pth)+1);
		strcat(pth, tmpBuff);
		tmp=ptmp;
		ptmp = strchr(tmp, '\r');
    }
    tmp = (char*)scalloc(strlen(pth)+strlen(text)+1, sizeof(char));
    strcpy(tmp, text);
    strcat(tmp, pth);
    free(text);
    free(pth);
    return tmp;
}

void preprocText(char *preport, s_message *msg)
{
    msg->text = createKludges(config->disableTID, 
                              NULL, &msg->origAddr, &msg->destAddr,
                              versionStr);
    xstrcat(&msg->text, "\001FLAGS NPD\r");
    xstrcat(&msg->text, preport);
    xscatprintf(&msg->text, " \r--- %s FileFix\r", versionStr);
    msg->textLength=(int)strlen(msg->text);
}

char *textHead(void)
{
    char *text_head=NULL;
    
    xscatprintf(&text_head, " FileArea%sStatus\r",	print_ch(44,' '));
	xscatprintf(&text_head, " %s  -------------------------\r",print_ch(50, '-')); 
    return text_head;
}

void RetMsg(s_message *msg, s_link *link, char *report, char *subj)
{
    s_message *tmpmsg;
    
    tmpmsg = makeMessage(link->ourAka, &(link->hisAka), msg->toUserName, msg->fromUserName,
                         subj, 1,config->filefixKillReports);
    preprocText(report, tmpmsg);
    
    writeNetmail(tmpmsg, config->robotsArea);
    
    freeMsgBuffers(tmpmsg);
    free(tmpmsg);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-


int processFileFix(s_message *msg)
{
	int security=1, notforme = 0;
	s_link *link = NULL;
	s_link *tmplink = NULL;
	char *textBuff = NULL, *report=NULL, *preport = NULL, *token = NULL;
	
	// find link
	link=getLinkFromAddr(config, msg->origAddr);

	// this is for me?
	if (link!=NULL)	notforme=addrComp(msg->destAddr, *link->ourAka);
	
	// ignore msg for other link (maybe this is transit...)
	if (notforme) {
		return 2;
	}


	// security ckeck. link,araefixing & password.
    if (link != NULL) {
        if (link->FileFix==1) {
            if (link->fileFixPwd!=NULL) {
                if (stricmp(link->fileFixPwd,msg->subjectLine)==0) security=0;
                else security=3;
            }
        } else security=2;
    } else security=1;

	if (!security) {
		
		textBuff = msg->text;
		token = strseparate (&textBuff, "\n\r");

		while(token != NULL) {
			preport = processcmd( link, msg,  stripLeadingChars(token, " \t"), tellcmd (token) );
			if (preport != NULL) {
				switch (RetFix) {
				case LIST:
					RetMsg(msg, link, preport, "FileFix reply: list request");
					break;
				case HELP:
					RetMsg(msg, link, preport, "FileFix reply: help request");
					break;
				case ADD: case DEL:
					if (report == NULL) report = textHead();
					report = areastatus(preport, report);
					break;
				case UNLINK:
					RetMsg(msg, link, preport, "FileFix reply: unlinked request");
					break;
				case LINKED:
    					RetMsg(msg, link, preport, "FileFix reply: linked request");
					w_log( '8', "FileFix: linked fileareas list sent to %s", aka2str(link->hisAka));
					break;
				case PAUSE: case RESUME:
					RetMsg(msg, link, preport, "FileFix reply: node change request");
					break;
				case INFO:
					RetMsg(msg, link, preport, "FileFix reply: link information");
					break;
				case RESEND:
					RetMsg(msg, link, preport, "FileFix reply: resend request");
 					break;
				case ERROR:
					if (report == NULL) report = textHead();
					report = areastatus(preport, report);
					break;
				default: break;
				}
				
				free(preport);
			}
			token = strseparate (&textBuff, "\n\r");
		}
		
	} else {
		
		if (link == NULL) {
			tmplink = (s_link*)scalloc(1, sizeof(s_link));
			tmplink->ourAka = &(msg->destAddr);
			tmplink->hisAka.zone = msg->origAddr.zone;
			tmplink->hisAka.net = msg->origAddr.net;
			tmplink->hisAka.node = msg->origAddr.node;
			tmplink->hisAka.point = msg->origAddr.point;
			link = tmplink;
		}
		// security problem
		
		switch (security) {
		case 1:
            report = sstrdup(" \r your system is unknown\r");
			break;
		case 2:
            report = sstrdup(" \r filefix is turned off\r");
			break;
		case 3:
			report = sstrdup(" \r password error\r");
			break;
		default:
			report = sstrdup(" \r unknown error. mail to sysop.\r");
			break;
		}
		
	
		RetMsg(msg, link, report, "security violation");
		free(report);
		
		w_log( '8', "FileFix: security violation from %s", aka2str(link->hisAka));
		
		free(tmplink);
		
		return 0;
	}
	

	if ( report != NULL ) {
		preport=linked(msg, link, 0);
		xstrcat(&report, preport);
		free(preport);
		RetMsg(msg, link, report, "node change request");
		free(report);
	}

	w_log( '8', "FileFix: sucessfully done for %s",aka2str(link->hisAka));
	
	return 1;
}

void ffix(s_addr addr, char *cmd)
{
    s_link          *link   = NULL;
    s_message	    *tmpmsg = NULL;

    if (cmd) {
	link = getLinkFromAddr(config, addr);
	if (link) {
	    tmpmsg = makeMessage(&addr, link->ourAka, link->name,
				 link->RemoteRobotName ?
				 link->RemoteRobotName : "Filefix",
				 link->fileFixPwd ?
				 link->fileFixPwd : "", 1,
                 config->areafixKillReports);
	    tmpmsg->text = cmd;
        processFileFix(tmpmsg);
	    tmpmsg->text=NULL;
	    freeMsgBuffers(tmpmsg);
	} else w_log(LL_ERR, "FileFix: no such link in config: %s!", aka2str(addr));
    }
    else scan();
}