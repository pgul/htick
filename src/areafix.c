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
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MSDOS
#include <fidoconfig.h>
#else
#include <fidoconf.h>
#endif

#include <common.h>
#include <fcommon.h>
#include <global.h>
#include <pkt.h>
#include <version.h>
#include <toss.h>
#include <patmat.h>
#include <ctype.h>
#include <progprot.h>
#include <strsep.h>

char *aka2str(s_addr *aka) {
	int i=1,j;
	char *straka;
	
	// how many bytes allocate? :)
	if (aka->zone!=0) for (j=aka->zone/10; j > 0; i++) j=j/10;
	if (aka->net!=0) for (i++, j=aka->net/10; j > 0; i++) j=j/10; else i++;
	if (aka->node!=0) for (i++, j=aka->node/10; j > 0; i++) j=j/10; else i++;
	if (aka->point!=0) {
		i++; for (i++, j=aka->point/10; j > 0; i++) j=j/10;
	}

	straka=(char*) malloc(i+3);
	
	if (aka->point!=0) sprintf(straka,"%u:%u/%u.%u",aka->zone,aka->net,aka->node,aka->point);
	else sprintf(straka,"%u:%u/%u",aka->zone,aka->net,aka->node);
	
	return straka;
}	

int subscribeCheck(s_filearea area, s_message *msg) {
	int i;
	
        for (i = 0; i<area.downlinkCount;i++) {
           if (addrComp(msg->origAddr, area.downlinks[i]->hisAka)==0) return 0;
	}
	
	return 1;
}

int subscribeAreaCheck(s_filearea *area, s_message *msg, char *areaname) {
   int rc;
   char *upName, *upAreaName;

        upName = (char *) malloc(strlen(areaname)+1);
        upAreaName = (char *) malloc(strlen(area->areaName)+1);
        strcpy(upName, areaname);
        strcpy(upAreaName, area->areaName);

        upName = strUpper(upName);
        upAreaName = strUpper(upAreaName);

	if (patmat(upAreaName,upName)==1) {
		rc=subscribeCheck(*area, msg);
		// 0 - already subscribed
		// 1 - need subscribe
        } else rc = 2;

        free(upName);
        free(upAreaName);

	// this is another area
	return rc;
}

// add string to file
int addstring(FILE *f, char *straka) {
	char *cfg;
	long areapos,endpos,cfglen;
	
	//current position
	fseek(f,-1,SEEK_CUR);
	areapos=ftell(f);
	
	// end of file
	fseek(f,0l,SEEK_END);
	endpos=ftell(f);
	cfglen=endpos-areapos;
	
	// storing end of file...
	cfg = (char*) calloc((size_t) cfglen+1, sizeof(char));
	fseek(f,-cfglen,SEEK_END);
	fread(cfg,sizeof(char),(size_t) cfglen,f);
	
	// write config
	fseek(f,-cfglen,SEEK_END);
	fputs(" ",f);
	fputs(straka,f);
	fputs(cfg,f);
	
	free(cfg);
	return 0;
}

int delstring(FILE *f, char *fileName, char *straka, int before_str) {
	int al,i=1;
	char *cfg, c, j='\040';
	long areapos,endpos,cfglen;

	al=strlen(straka);

	// search for the aka string
	while ((i!=0) && ((j!='\040') || (j!='\011'))) {
		for (i=al; i>0; i--) {
			fseek(f,-2,SEEK_CUR);
			c=fgetc(f);
			if (straka[i-1]!=tolower(c)) {j = c; break;}
		}
	}
	
	//current position
	areapos=ftell(f);

	// end of file
	fseek(f,0l,SEEK_END);
	endpos=ftell(f);
	cfglen=endpos-areapos-al;
	
	// storing end of file...
	cfg=(char*) calloc((size_t) cfglen+1,sizeof(char));
	fseek(f,-cfglen-1,SEEK_END);
	fread(cfg,sizeof(char),(size_t) (cfglen+1),f);
	
	// write config
	fseek(f,-cfglen-al-1-before_str,SEEK_END);
	fputs(cfg,f);

	truncate(fileName,endpos-al-before_str);
	
	fseek(f,areapos-1,SEEK_SET);

	free(cfg);
	return 0;
}

void removelink (s_link *link, s_filearea *area) {
	int i;
	s_link *links;

	for (i=0; i < area->downlinkCount; i++) {
           links = area->downlinks[i];
           if (addrComp(link->hisAka, links->hisAka)==0) break;
	}
	
	area->downlinks[i] = area->downlinks[area->downlinkCount-1];
	area->downlinkCount--;
}

char *list(s_message *msg, s_link *link) {

	int i,n1=0,n2=0,rc;
	char *report, addline[256];

	report=(char*) calloc(1,sizeof(char));

        for (i=0; i< config->fileAreaCount; i++) {

                if (config->fileAreas[i].hide==1) continue; // do not display hidden areas..
//		if ((config->fileAreas[i].group!='\060') && (link->TossGrp==NULL)) continue;
// 		if ((config->fileAreas[i].group!='\060') && (link->TossGrp!=NULL) && (strchr(link->TossGrp,config->fileAreas[i].group)==NULL)) continue;

		report=(char*) realloc(report, strlen(report)+strlen(config->fileAreas[i].areaName)+3);
		rc=subscribeCheck(config->fileAreas[i],msg);
		if (!rc) {
			strcat(report,"*");
			n1++;
		} else strcat(report," ");
		strcat(report,config->fileAreas[i].areaName);
		strcat(report,"\r");
		n2++;
	}
	
	sprintf(addline,"\r ---\r\r %i areas of %i\r\r",n1,n2);
	report=(char*) realloc(report, strlen(report)+strlen(addline)+1);
	strcat(report, addline);

	sprintf(addline,"filefix: list sent to %s",link->name);
	writeLogEntry(htick_log, '8', addline);

	return report;
}


char *help(s_link *link) {
	FILE *f;
	int i=1;
	char *help, addline[256];
	long endpos;

	if (config->filefixhelp!=NULL) {
		if ((f=fopen(config->filefixhelp,"r")) == NULL)
			{
				fprintf(stderr,"filefix: cannot open help file \"%s\"\n",
						config->filefixhelp);
				return NULL;
			}
		
		fseek(f,0l,SEEK_END);
		endpos=ftell(f);
		
		help=(char*) calloc((size_t) endpos,sizeof(char));

		fseek(f,0l,SEEK_SET);
		fread(help,1,(size_t) endpos,f);
		
		for (i=0; i<endpos; i++) if (help[i]=='\n') help[i]='\r';

		fclose(f);

		sprintf(addline,"filefix: help sent to %s",link->name);
		writeLogEntry(htick_log, '8', addline);

		return help;
	}

	return NULL;
}

char *available(s_link *link) {                                                 
/*        FILE *f;                                                                
        int i=1;                                                                
        char *avail, addline[256];                                              
        long endpos;                                                            
                                                                                
        if (config->available!=NULL) {                                          
                if ((f=fopen(config->available,"r")) == NULL)                   
                        {                                                       
                                fprintf(stderr,"areafix: cannot open Available Areas file \"%s\"\n",                                                            
                                                config->areafixhelp);           
                                return NULL;                                    
                        }                                                       
                                                                                
                fseek(f,0l,SEEK_END);                                           
                endpos=ftell(f);                                                
                                                                                
                avail=(char*) calloc((size_t) endpos,sizeof(char*));            
                                                                                
                fseek(f,0l,SEEK_SET);                                           
                fread(avail,1,(size_t) endpos,f);                               
                for (i=0; i<endpos; i++) if (avail[i]=='\n') avail[i]='\r';     
                                                                                
                fclose(f);                                                      
                                                                                
                sprintf(addline,"areafix: Available Area List sent to %s",link->name);                                                                          
                writeLogEntry(htick_log, '8', addline);                               
                                   
                return avail;                                                   
        }                                                                       
  */                                                                              
        return NULL;                                                            
}                                                                               

int changeconfig(char *fileName, char *areaName, s_link *link, int action) {
	FILE *f;
	char *cfgline, *token, *running, *straka;

	if ((f=fopen(fileName,"r+")) == NULL)
		{
			fprintf(stderr,"areafix: cannot open config file %s \n", fileName);
			return 1;
		}
	
	while ((cfgline = readLine(f)) != NULL) {
		cfgline = trimLine(cfgline);
		if ((cfgline[0] != '#') && (cfgline[0] != 0)) {
			
			running = cfgline;
                        //token = strtok_r(cfgline, " \t", &running);
			token = strseparate(&running, " \t");
			
			if (stricmp(token, "include")==0) {
				token=strseparate(&running, " \t");
                                //token = strtok_r(NULL, " \t", &running);
				changeconfig(token, areaName, link, action);
			}			
			else if (stricmp(token, "filearea")==0) {
				token = strseparate(&running, " \t"); 
                                //token = strtok_r(NULL, " \t", &running);
				if (stricmp(token, areaName)==0)
					switch 	(action) {
					case 0: 
						straka=aka2str(&(link->hisAka));
						addstring(f,straka);
						free(straka);
						break;
 					case 1:
						straka=aka2str(&(link->hisAka));
						delstring(f,fileName,straka,1);
						free(straka);
						break;
					default: break;
					}
			}
			
		}
		free(cfgline);
	}
	
	fclose(f);
	return 0;
}

void changeHeader(s_message *msg, s_link *link) {
	s_addr *ourAka;
	char *subject = "FileFix report", *tmp;
	
	ourAka=link->ourAka;
	
	msg->destAddr.zone = link->hisAka.zone;
	msg->destAddr.net = link->hisAka.net;
	msg->destAddr.node = link->hisAka.node;
	msg->destAddr.point = link->hisAka.point;

	msg->origAddr.zone = ourAka->zone;
	msg->origAddr.net = ourAka->net;
	msg->origAddr.node = ourAka->node;
	msg->origAddr.point = ourAka->point;
	
	tmp = msg->subjectLine;
	tmp = (char*) realloc(tmp, strlen(subject)+1);
	strcpy(tmp,subject);
	msg->subjectLine = tmp;
	free(msg->toUserName);
	msg->toUserName = msg->fromUserName;
	msg->fromUserName = strdup("HTick FileFix");
}

int strip(char *text) {
	char *str;
	str = strchr( text, (int) '\r' );
	return( str!=NULL ? (int)(str-text)+1 : strlen(text) );
}

char *subscribe(s_link *link, s_message *msg, char *cmd) {
	int i, rc=2;
	char *line, *report, addline[256], logmsg[256], *header = "Result of your query: ";
	s_filearea *area=NULL;

	line = cmd;

	if (line[0]=='+') line++;

        report=(char*) malloc(strlen(header)+strlen(cmd)+1+1);
        strcpy(report, header);
        strcat(report, cmd);
        strcat(report,"\r");
	
	for (i = 0; i< config->fileAreaCount; i++) {
		rc=subscribeAreaCheck(&(config->fileAreas[i]),msg,line);
                if ( rc==2 ) continue;

		area = &(config->fileAreas[i]);
/*
		// link not allowed to subscribe this area (we hide it)
		if ((area->group!='\060') && (link->TossGrp==NULL)) rc=2;
		if ((area->group!='\060') && (link->TossGrp!=NULL)) {
		    if (strchr(link->TossGrp,area->group)==NULL) rc=2;
		}
*/		
                switch (rc) {
                case 0: sprintf(addline,"you are already linked to area %s\r", area->areaName);
                        break;
                case 2:	if (strstr(line,"*") != NULL) continue;
			sprintf(addline,"no area '%s' in my config\r",line);
                        break;
                case 1:	changeconfig (getConfigFileName(), area->areaName, link, 0);
                        area->downlinks = realloc(area->downlinks, sizeof(s_link*)*(area->downlinkCount+1));
                        area->downlinks[area->downlinkCount] = link;
                        area->downlinkCount++;
                        sprintf(addline,"area %s subscribed\r",area->areaName);
                        sprintf(logmsg,"filefix: %s subscribed to %s",link->name,area->areaName);
                        writeLogEntry(htick_log, '8', logmsg);
                        break;
		default: continue;
                }
        
                report=(char*) realloc(report, strlen(report)+strlen(addline)+1);
                strcat(report, addline);

        }
	return report;
}

char *unsubscribe(s_link *link, s_message *msg, char *cmd) {
	int i, rc = 2;
	char *line, *report, addline[256], logmsg[256], *header = "Result of your query: ";
	s_filearea *area;
	
	line = cmd;
	
	if (line[1]=='-') return NULL;
	line++;
	
	report=(char*) malloc(strlen(header)+strlen(cmd)+1+1);
	strcpy(report, header);
	strcat(report, cmd);
	strcat(report, "\r");
	
	for (i = 0; i< config->fileAreaCount; i++) {
		rc=subscribeAreaCheck(&(config->fileAreas[i]),msg,line);
		if ( rc==2 ) continue;
		
		area = &(config->fileAreas[i]);
/*
		// link don't know about unavilable areas
		if ((area->group!='\060') && (link->TossGrp==NULL)) rc=2;
		if ((area->group!='\060') && (link->TossGrp!=NULL)) {
		    if (strchr(link->TossGrp,area->group)==NULL) rc=2;
		}
*/
		
		switch (rc) {
		case 1: 
			if (strstr(line,"*") != NULL) continue;
			else sprintf(addline,"you are not linked to area %s\r",area->areaName);
			break;
		case 2:
			if (strstr(line,"*") != NULL) continue;
			sprintf(addline,"no area '%s' in my config\r",line);
			break;
		case 0:
			changeconfig (getConfigFileName(), area->areaName, link, 1);
			removelink(link, area);
			sprintf(addline,"area %s unsubscribed\r",area->areaName);
			sprintf(logmsg,"filefix: %s unsubscribed from %s",link->name,area->areaName);
			writeLogEntry(htick_log, '8', logmsg);
			break;
		default: continue;
		}

    		report=(char*) realloc(report, strlen(report)+strlen(addline)+1);
		strcat(report, addline);

	}
	return report;
}

int tellcmd(char *cmd) {
	char *line;
	
	line = cmd;

	switch (line[0]) {
	case '%': 
		line++;
		if (stricmp(line,"list")==0) return 1;
		if (stricmp(line,"help")==0) return 2;
                if (stricmp(line,"avail")==0) return 5;     
                if (stricmp(line,"available")==0) return 5; 
                if (stricmp(line,"all")==0) return 5;       	
                break;
	case '\01': return 0;
	case '-'  : return 4;
	default: return 3;
        }
        return 0;
}

char *processcmd(s_link *link, s_message *msg, char *line, int cmd) {

	char *report;

	switch (cmd) {
	case 1: report = list (msg, link);
		break;
	case 2:	report = help (link);
		break;
	case 3: report = subscribe (link,msg,line);
		break;
	case 4: report = unsubscribe (link,msg,line);
		break;
        case 5: report = available (link);
                break;                    
	default: return NULL;
	}

	return report;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-


int processFileFix(s_message *msg)
{
	int i, security=1;
	s_link *link = NULL;
	char *tmp, *textBuff, *report, *preport, logmsg[256];
	INT32 textlength;

	// find link
	link=getLinkFromAddr(*config, msg->origAddr);


	// security ckeck. link,araefixing & password.
		if (link != NULL) {
			if (link->AreaFix==1) {
				if (link->areaFixPwd!=NULL) {
					if (stricmp(link->areaFixPwd,msg->subjectLine)==0) security=0;
                                        else security=1;
                                }
			} else security=1;
		} else security=1;

	
	tmp=(char*) calloc(128,sizeof(char));
	createKludges(tmp, NULL, link->ourAka, &(msg->origAddr));
	report=(char*) calloc(strlen(tmp)+1,sizeof(char));
	strcpy(report,tmp);
	

	if (!security) {
		
		textBuff=(char *) calloc(strlen(msg->text),sizeof(char));
		strcpy(textBuff,msg->text);
		
		i=strip(textBuff);
		tmp=strtok(textBuff,"\n\r\t");
		textBuff+=i;
		
		while(tmp != NULL) {
			if (tmp != NULL) {
				preport = processcmd( link, msg, tmp, tellcmd(tmp) );
				if (preport!=NULL) {
					report=(char*) realloc(report,strlen(report)+strlen(preport)+1);
					strcat(report,preport);
					free(preport);
				}
			}
			tmp = strtok(NULL, "\n\r\t");
		}
		
		tmp=(char*) realloc(tmp,80*sizeof(char));
		sprintf(tmp, " \r--- %s\r", versionStr);
		report=(char*) realloc(report,strlen(report)+strlen(tmp)+1);
		strcat(report,tmp);

	} else {
		
		tmp=(char*) realloc(tmp,80*sizeof(char));
		sprintf(tmp, " \r security violation!\r\r--- hpt areafix\r");
		report=(char*) realloc(report,strlen(report)+strlen(tmp)+1);
		strcat(report,tmp);
	}
	

	if (link!=NULL) {
		textlength=strlen(report);
		msg->textLength=(int)textlength;
		free(msg->text);
		msg->text=report;
		changeHeader(msg,link);
	}

	writeNetmail(msg);

	sprintf(logmsg,"filefix: sucessfully done for %s",link->name);
	writeLogEntry(htick_log, '8', logmsg);
	
	free(tmp);
	return 0;
}
