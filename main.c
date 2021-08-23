/*
# Copyright (C) 2010
#
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
*/

#define DEFISOMODULE	"SystemControl"		// Read ISO filename from this module
						// Offset of DEFISOMODULE textaddr that contains ISO filename
#define DEFISOPATHOFFSET	0x0000B108      // 0x88072408	5.00m33 on psp2000 - for depricated but optional function
#define DEFISOPATHADDR		0x88072408	// 0x88072408   5.00m33 on psp2000 - for depricated but optional function
#define DEFUMDIDADDR		0x880137B0	// 0x880137B0	5.00m33 on psp2000
#define NEW_ISOPATH_DETECT		1	// 0 = direct memory reading to find isopath, uses above DEFs
						// 1 = m33 function to find isopath
#define CFG_MAX_ENTRIES			64      // Do you really need more than 64 entries?
#define DEFAULT_LOGTOFILE       	0       // 0 = none, 1 = logging, 2 = lots-o-logging
#define WAITSCEMEDIA			1	// FIXME

#include <pspkernel.h>
#include <pspsdk.h>
#include <psputilsforkernel.h>
#include <pspsysmem_kernel.h>

#include <systemctrl.h>
#include <pspinit.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <psploadcore.h>
#include <pspumd.h>

#include <sys/unistd.h>

#include <psprtc.h>



#include <systemctrl_se.h>

PSP_MODULE_INFO("Pergame", PSP_MODULE_KERNEL, 1, 5);

//////////////////////////////////
///// wheee %data

typedef struct Cfg {
	char modname[64];
	char bootname[64];
	int level;
} Cfg;

typedef struct Loaded {
  char modname[64];
} Loaded;
//////////////////////////////////
/////
enum PspFwVersion {
  FW_500 = 0x05000010,
  FW_550 = 0x05050010,
};

//////////////////////////////////
///// whee our $variables
static unsigned int logtofile = DEFAULT_LOGTOFILE;
static unsigned int autosort = 0;
static unsigned int cfg_count = 0;
static Cfg * cfg = NULL;
static unsigned int loaded_count = 0;
static Loaded * loaded = NULL;
static unsigned int new_isopath_detect = NEW_ISOPATH_DETECT;
static unsigned int pergame_woke = 0;
static unsigned int addr_isopath = DEFISOPATHADDR;
static unsigned int cfg_isoaddr = 0;
static unsigned int addr_umdid = DEFUMDIDADDR;
static unsigned int cfg_umdaddr = 0;
static unsigned int offset_isopath = DEFISOPATHOFFSET;
static unsigned int cfg_offsetiso = 0;

static int sema = -1;
static int cfg_mem_id = -1;

static char * eboot;
static int bootdev, apitype, keyconf, fwversion;

char lstr[256];
char log_name[256];
char conf_name[256];

SceInt64 log_res = -1;
int logfd = -1;

static u32 tachyon;
static u32 baryon;

u32 sceSysreg_driver_E2A5D1EE(void);
u32 sceSyscon_driver_7EC5A957(u32 *baryon);

STMOD_HANDLER previous = NULL;
int OnModuleStart(SceModule2* mod);
//////////////////////////////////
///// macros macros macros macros macros macros macros macros macros macros macros macros macros macros macros macros macros macros macros macros
#define peekbyte(addr) (*((unsigned char*)(long)(addr)));

#define isspace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) =='\r' )

#define log(...)\
  sprintf( lstr, __VA_ARGS__ );\
  logfunc(lstr);

#define log2(...)\
  if (logtofile >= 2) {\
    sprintf( lstr, __VA_ARGS__ );\
    logfunc(lstr);\
  }
//////////////////////////////////
///// Append line to logfile. Still somewhat gimpy.
int filelog( const char * str ) {
  if (logfd < 0) {
    //sceIoWaitAsync(logfd,  &log_res);
    logfd = sceIoOpen( log_name, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777 );
    if (logfd >= 0) {
      //sceIoWaitAsync(logfd,  &log_res);
      sceIoWriteAsync(logfd, str, strlen( str ) );
      sceIoWaitAsync(logfd, &log_res);
      sceIoCloseAsync(logfd);
      logfd = -1;
    }
  } else {
    sceIoWaitAsync(logfd,  &log_res);
    sceIoWriteAsync(logfd, str, strlen( str ) );
  }
  return 0;
}


///////////////////////////////
///// Prepare line for logging
void logfunc( const char * str ) {
  if (logtofile > 0) {
    char lpfxstr[384];
    pspTime ctime;
    sceRtcGetCurrentClockLocalTime(&ctime);
    SceKernelThreadInfo thinfo;
    SceUID thid = sceKernelGetThreadId();
    memset(&thinfo, 0, sizeof(thinfo));                
    thinfo.size = sizeof(thinfo);      
    sceKernelReferThreadStatus(thid, &thinfo);
    int free=sceKernelPartitionTotalFreeMemSize(PSP_MEMORY_PARTITION_KERNEL);
    sprintf( lpfxstr, "[%02d:%02d:%02d.%04d] (%d free) %s - %s",ctime.hour,ctime.minutes,ctime.seconds,(ctime.microseconds/100), free ,thinfo.name, str);
    filelog(lpfxstr);
  }
}
//////////////////////////////
/////
int getpsptype( const u32 tach, const u32 bary ) {
  if (tach < 0) return 0;
  if (tach <= 0x00400000) return 1;
  if (bary <= 0x00243000) return 2;
  return 3;
}

//////////////////////////////
///// Thanks, cxmb!
int readLine( int fd, char * buf, int max_len )
{
  int i = 0, bytes;
  while( i < max_len && ( bytes = sceIoRead( fd, buf + i, 1 ) ) == 1 ) {
    if ( buf[i] == -1 || buf[i] == '\n' || buf[i] == '\r' ) break;
    buf[i]=tolower(buf[i]);
    i ++;
  }
  buf[i] = 0;
  if ( bytes != 1 && i == 0 ) return -1;
  return i;
}


//////////////////////////////////
///// Thanks, Linux!
char *strstrip(char *s) {
  size_t size;
  char *end;
  size = strlen(s);
  if (!size)
     return s;
  end = s + size - 1;
  while (end >= s && isspace(*end))
     end--;
  *(end + 1) = '\0';
  while (*s && isspace(*s))
     s++;
  return s;
}

//////////////////////////////////
//////////////////////////////////
//offset_isop..	0x0000B108      5.00m33 on psp2000 
//		0x0000AC3C	5.00m33 on psp1000
//        	0x0000AF78      5.50gen on psp2000
//		0x0000B208	// nickxab is running some weird shit

//addr_isopath	0x88072408   5.00m33 on psp2000
//		0x88071E3C   5.00m33 on psp1000
//		0x88072478   5.50gen on psp2000
//

//addr_umdid	0x880137B0	5.00m33 on psp2000
//		0x880137B0	5.00m33 on psp1000
//            	0x88013670	5.50gen on psp2000
//////////////////////////////////
///// yes this is messy crap. TODO: use struct to match hw/fw with addrs
int setup_addrs() {
  int pspmodel=getpsptype(tachyon, baryon);
  if ( (pspmodel < 1) || ( fwversion < 0x05000010 ) ) return 0;
  if (fwversion == FW_500) {
    addr_umdid=0x880137B0;
    if (pspmodel == 1) {
      addr_isopath=0x88071E3C; offset_isopath=0x0000AC3C;
    } else if (pspmodel == 2) {
      addr_isopath=0x88072408; offset_isopath=0x0000B108;
    }
  } else if (fwversion == FW_550) {
    addr_umdid=0x88013670;
    if (pspmodel == 1) {
      if(new_isopath_detect < 1) {
        log("Need value for isopath address on a psp1k with 5.50gen.\n");
        log("If you can, please provide them by submitting an issue at http://pergame.googlecode.com. Going with default (0x%08X), good luck.\n", addr_isopath);
      }
    } else if (pspmodel == 2) {
      addr_isopath=0x88072478; offset_isopath=0x0000AF78;
    }
  } else {
    log("Unsure of memory addresses for firmware 0x%08X, going with defaults for 5.00m33 on psp2k! Please see README about setting isoaddr and umdaddr values.\n",fwversion);
  }
  if (pspmodel == 3) log("pergame.prx is likely to croak on a 3k. Please see README about setting isoaddr and umdaddr values if you want to try anyway.\n");
  log("Memory addresses for fw 0x%08X on psp%dk: addr_isopath: 0x%08X, addr_umdid: 0x%08X, offset_isopath: 0x%08X\n",fwversion,pspmodel,addr_isopath,addr_umdid,offset_isopath);

  return 1;
}

//////////////////////////////////
///// Stuff contents of pergame.txt into cfg struct.
void parse_conf ( void ) {
  int conf_fd = sceIoOpen( conf_name, PSP_O_RDONLY, 0644 );
  if ( conf_fd >= 0 ) {
    int bytes=0;int line; char buf[256];
    for( line = 0; line < 1024; line++ ) {
      bytes=readLine(conf_fd, buf, 256);

      if ( (bytes > 5) && (cfg_count <= CFG_MAX_ENTRIES ) ) {
        strcpy(buf,strstrip(buf));
        if (strncmp(buf, "#", 1)) {
          //log2("    cfg line %d: '%s' (%d bytes)\n",(line+1),buf,bytes);

          char *modname;char *bootname;
          modname = strtok(buf, " \t\n\r");
          bootname = strtok(0, "#\r\n");
          if(bootname) {
            char tmodname[strlen(modname)+5];
            if(strncmp(modname, "/",1)==0) {
              strcpy(tmodname,"ms0:");
              strcat(tmodname,modname);
            } else {
              strcpy(tmodname,modname);
            }
            strcpy(bootname,strstrip(bootname));
            char tbootname[strlen(bootname)+5];
            if(strncmp(bootname,"/",1)==0) {
              strcpy(tbootname,"ms0:");
              strcat(tbootname,bootname);
            } else if ( (strlen(bootname) == 10) && ( bootname[4] == '-' ) ) {
              strncpy(tbootname,bootname,4);
              strncpy(tbootname+4,bootname+5,6);
            } else {
              strcpy(tbootname,bootname);
            }
            strncpy(cfg[cfg_count].modname, tmodname,strlen(tmodname));
            strncpy(cfg[cfg_count].bootname, tbootname,strlen(tbootname));
            cfg_count++;
            if ( cfg_count % 32 == 0 ) {
              Cfg * tmp = sceKernelAllocHeapMemory( cfg_mem_id, sizeof( Cfg ) * ( cfg_count + 32 ) );
              memcpy( tmp, cfg, sizeof( Cfg ) * cfg_count );
              sceKernelFreeHeapMemory( cfg_mem_id, cfg );
              cfg = tmp;
            }
          } else {
            if (strncmp(modname,"logging=1",9)==0) {
              logtofile=1;
              log("%s[%d] - Logging activated.\n",conf_name,(line+1));
            } else if (strncmp(modname,"logging=2",9)==0) {
              logtofile=2;
              log("%s[%d] - Verbose logging activated.\n",conf_name,(line+1));
            } else if (strncmp(modname,"autosort=1",10)==0) {
              autosort=1;
              log("%s[%d] - Autosort activated.\n",conf_name,(line+1));
            } else if ( (strncmp(modname,"isoaddr=0x",10)==0) && (strlen(modname)==18) ) {
              char addrstr[11];
              strncpy(addrstr,modname+8,11);
              cfg_isoaddr=strtol(addrstr, NULL, 16);
              log("%s[%d] - Forcing isoaddr to '%s' (0x%08X)\n",conf_name,(line+1),addrstr,cfg_isoaddr);
            } else if ( (strncmp(modname,"offsetiso=0x",12)==0) && (strlen(modname)==20) ) {
              char addrstr[11];
              strncpy(addrstr,modname+10,11);
              cfg_offsetiso=strtol(addrstr, NULL, 16);
              log("%s[%d] - Forcing offsetiso to '%s' (0x%08X)\n",conf_name,(line+1),addrstr,cfg_offsetiso);
            } else if ( (strncmp(modname,"umdaddr=0x",10)==0) && (strlen(modname)==18) ) {
              char addrstr[11];
              strncpy(addrstr,modname+8,11);
              cfg_umdaddr=strtol(addrstr, NULL, 16);
              log("%s[%d] - Forcing umdaddr to '%s' (0x%08X)\n",conf_name,(line+1),addrstr,cfg_umdaddr);
            } else if (strncmp(modname,"oldisopath=1",12)==0) {
              new_isopath_detect=0;
              log("%s[%d] - Ditching m33 function, using old isopath finder.\n",conf_name,(line+1));
            } else {
              log("cfgload - Syntax error in %s, line %d: ('%s', '%s')\n",conf_name,line,modname,bootname);
            }
          }
        }
      }
      if ( bytes < 0 ) { 
        break;
      }
    }
    sceIoClose(conf_fd);    
  }
} 


///////////////////////////////////
///// Load a module and blacklist it
int loadmod(const char *name, int argc, char **argv) {
  if ( strncmp( name, "!", 1 ) == 0 ) {
    char tmpname[64], blname[64];
    strncpy(tmpname,name+1,strlen(name));
    strcpy(tmpname,strstrip(tmpname));
    strcpy(blname,"");
    if(strncmp(tmpname,"/",1)==0) {
      strcpy(blname,"ms0:");
    }
    strcat(blname,tmpname);
    blname[strlen(blname)]='\0';
    log2("  blacklisting: '%s'\n",blname);
    strcpy(loaded[loaded_count].modname, blname);//, strlen(blname));
    loaded_count++;
    return 0;
  }

  int i;
  for ( i = 0; i < loaded_count; i ++ ) {
    if ( strncmp( name, loaded[i].modname, strlen(loaded[i].modname) ) == 0 ) {
      log("loadmod - '%s' already loaded, failed, or ignored.\n",name);
      return -1;
    }
  }

  strncpy(loaded[loaded_count].modname, name,strlen(name));
  loaded_count++;

  SceUID modid;
  int status;
  char args[1024];
  modid = sceKernelLoadModule(name, 0, NULL);
  if(modid >= 0) {
    modid = sceKernelStartModule(modid, (strlen(name) + 1), (void *) args, &status, NULL);
    log("loadmod - starting %s [%08x]\n", name, modid );
  } else {
    log("loadmod - %s - error: %08x\n", name, modid );
  }
  return modid;
}
///////////////////////////////////
///// Have sceMediaSync poke main thread. FIXME: Can't turn this off?
int OnModuleStart(SceModule2* mod) {
  if (!pergame_woke) {
    log2("start - %s [0x%08X]\n", mod->modname, mod->mem_id );
    if ( strcmp( mod->modname, "sceMediaSync" ) == 0 ) {
      sceKernelSignalSema( sema, 1 );
    }
  }
  return previous((!previous)?0:mod);
}

///////////////////////////////////
///// case insensitive memory read
int readmemchar ( char * buf, unsigned int addr , unsigned int len, int lc ) {
  //log2("readmem - 0x%08X, %d bytes\n", addr, len);
  int c;char byte;
  strcpy(buf,"_");
  for ( c=0; c < len ; c++ ) {
    byte=peekbyte(addr+c);
    if (byte == '\0') {
      buf[c]=byte;
      break;
    } else if( byte < 0x20 || byte == 0x7F ) {
      buf[c]='\0';
      break;
    } else {
      if (lc) {buf[c]=(char)tolower(byte);} else {buf[c]=byte;}
      buf[c+1]='\0';
    }
  }
  log("readmem - Fetched '%s' from 0x%08X, %d bytes.\n",buf,addr,c);
  return len;
}
///////////////////////////////////
///// yoink iso filename from SystemControl
int isopath ( char * buf ) {
  if(new_isopath_detect) {
    char * getiso = sctrlSEGetUmdFile();

    if ( (strlen(getiso) >= 5) && (strncmp(getiso,"ms0:/",4) == 0 ) ) {
      strncpy(buf,getiso,63);
      buf[63] = '\0';
      int i;
      for (i = 0; buf[i] != '\0'; i++) {
        buf[i] = (char)tolower(buf[i]);
      }
      log("isoread: sctrlSEGetUmdFile returned '%s'\n",getiso);
      return 1;
    } else {
      log2("isoread: sctrlSEGetUmdFile returned '%s' (invalid)\n",getiso);
    }
  }
  unsigned int isoaddr=-1;
  unsigned int offset=-1;
  if (cfg_isoaddr > 0) {
    isoaddr=cfg_isoaddr;
  } else if (addr_isopath > 0) {
    isoaddr=addr_isopath;
  }
  if(cfg_offsetiso > 0) {
    offset=cfg_offsetiso;
  } else if (offset_isopath > 0) {
    offset=offset_isopath;
  }  
  if (isoaddr >= 0) {
    readmemchar(buf,isoaddr,64,1);
    if ( (strlen(buf) >= 5) && ( strncmp(buf,"ms0:/",4) == 0 ) ) return 1;
    log2("isopath - could not find path to game iso at 0x%08X\n",isoaddr);
  }
  if (offset >= 0) {
    SceModule * sysctrl = ( SceModule * )sceKernelFindModuleByName( DEFISOMODULE );
    if (sysctrl) {
      SceKernelModuleInfo sysctrlmodinfo;
      unsigned int addr;
      sysctrlmodinfo.size=sizeof(SceKernelModuleInfo);
      sceKernelQueryModuleInfo(sysctrl->modid, &sysctrlmodinfo);
      addr=sysctrlmodinfo.text_addr;
      if (isoaddr != (addr + offset) ) {
        isoaddr=addr + offset;
        log2("isopath - Found (0x%08X) %s, text_addr (0x%08X) + offset (0x%08X) = isoaddr (0x%08X)\n",sysctrl->modid, sysctrl->modname, addr, offset, isoaddr);
        readmemchar(buf,isoaddr,64,1);
        if ( (strlen(buf) >= 5) && ( strncmp(buf,"ms0:/",4) == 0 ) ) return 1;
      }
    } else {
      log("isopath - could not find %s module.\n",DEFISOMODULE);
    }
  }
  return 0;
}

///////////////////////////////////
///// need better method of getting mitts into sceMediaSync
int install( void ) {
  log2("install - sctrlHENSetStartModuleHandler...\n");
  previous = sctrlHENSetStartModuleHandler(OnModuleStart);
  return 0;
}

///////////////////////////////////
///// load appropriate modules for 'oid' from cfg struct
int loadmods( const char * oid ) {
  log2("loadmods(%s)\n",oid);
  if (strlen(oid) < 3) return 0;

  char id[strlen(oid)];
  strcpy(id,oid);
  int si, i;

  for (si = 0; id[si] != '\0'; si++) {
    id[si] = (char)tolower(id[si]);
  }

  for ( i = 0; i < cfg_count; i ++ ) {
    if (strncmp( id, cfg[i].bootname, strlen(cfg[i].bootname) ) == 0 ) {
      log("loadmods - match: '%s' ('%s' <=> '%s')\n",cfg[i].modname,id,cfg[i].bootname);
      loadmod(cfg[i].modname, 0, NULL);
    }
  }
  return 0;
}
//////////////////////////////
///// load modules from PATH.EXT.plugins or PATH.plugins

int loadlocalmods(const char * opath) {
  char path[strlen(opath)+8];
  strcpy(path,opath);

  strcat(path,".plugins");
  //log2("Loadlocal - trying %s / %s\n",opath,path);
  
  int lconf_fd = sceIoOpen( path, PSP_O_RDONLY, 0644 );
  if ( (lconf_fd < 0 ) && (path[strlen(opath)-4] == '.' ) ) {
    strncpy(path+(strlen(opath)-4),".plugins",8);
    path[strlen(opath)+4]= '\0';
    //log2("Loadlocal - trying %s / %s\n",opath,path);
    lconf_fd = sceIoOpen( path, PSP_O_RDONLY, 0644 );
  }
  if (lconf_fd >= 0 ) {
    log("loadlocal - found %s.\n",path);
    int bytes=0;int line; char buf[256];
    for( line = 0; line < CFG_MAX_ENTRIES; line++ ) {
      bytes=readLine(lconf_fd, buf, 256);
      if (bytes > 5) {
        strcpy(buf,strstrip(buf));
        if (strncmp(buf, "#", 1)) {
          char tmodname[strlen(buf)+5];
          if(strncmp(buf, "/",1)==0) {
            strcpy(tmodname,"ms0:");
            strcat(tmodname,buf);
          } else {
            strcpy(tmodname,buf);
          }
          log("loadlocal(%s) - loading '%s'.\n",path,tmodname);
          loadmod(tmodname,0,NULL);
        }
      }
    }
    sceIoClose(lconf_fd);
  }

  return 0;
}
//////////////////////////////
int touch(const char * path) {
  if (!autosort) return 0;
  char touchpath[64];
  char * touchpathp;
  strcpy(touchpath,path);
  if (sceKernelBootFrom() != PSP_BOOT_DISC) {
    touchpathp = strrchr(touchpath, '/');
    *touchpathp = '\0';
  }

  SceIoStat stat;
  memset(&stat, 0, sizeof(SceIoStat));
  sceIoGetstat(touchpath, &stat);

  pspTime currtime;
  sceRtcGetCurrentClockLocalTime(&currtime);
  // HOLY CRAP SONY, WHY 2 IDENTICAL YET INCOMPATIBLE TIME STRUCTS? I HATE YOU LONG TIME.
  stat.st_mtime.year=currtime.year; stat.st_mtime.month=currtime.month;
    stat.st_mtime.day=currtime.day; stat.st_mtime.hour=currtime.hour;
    stat.st_mtime.minute=currtime.minutes; stat.st_mtime.second=currtime.seconds;

  sceIoChstat(touchpath, &stat, 0xffffffff);

  sceIoGetstat(touchpath, &stat);

  log("touch - path: '%s', mtime: %d/%d/%d %d:%d:%d\n",touchpath, stat.st_mtime.year,stat.st_mtime.month,stat.st_mtime.day,
    stat.st_mtime.hour,stat.st_mtime.minute,stat.st_mtime.second);
  return 1; 
}
////////////////////////////////
///// malloc'ing, config loading, address finding, info spewing.
int init_config ( void ) {
  
  sceIoRemove(log_name);
  log2("\n==========\nstart. conf: %s, free kmem: %d\n",conf_name,sceKernelPartitionTotalFreeMemSize(PSP_MEMORY_PARTITION_KERNEL));

  cfg_mem_id = sceKernelCreateHeap( PSP_MEMORY_PARTITION_KERNEL, 
      ( sizeof( Cfg ) * ( CFG_MAX_ENTRIES + 1) ) + 
      ( sizeof( Loaded ) * 17 )
    , 1, "cfg_heap");
  cfg = sceKernelAllocHeapMemory( cfg_mem_id, sizeof( Cfg ) * 32 );
  memset( cfg, 0, sizeof( Cfg ) * 32 );

  loaded = sceKernelAllocHeapMemory( cfg_mem_id, sizeof( Loaded ) * 16 );
  memset( cfg, 0, sizeof( Loaded ) * 16 );

  parse_conf();

  eboot = sceKernelInitFileName(); 
  bootdev = sceKernelBootFrom();
  apitype = sceKernelInitApitype(); 
  keyconf = InitForKernel_7233B5BC();
  tachyon = sceSysreg_driver_E2A5D1EE();
  sceSyscon_driver_7EC5A957(&baryon);
  fwversion = sceKernelDevkitVersion();
  int pspmodel=getpsptype(tachyon, baryon);

  log("gameinfo - bootfrom %04X(%s), apitype: %04X(%s), keyconf: %04X(%s), initfile: %s\n",
    bootdev, (bootdev==0x00?"flash":(bootdev==0x20?"disc":(bootdev==0x40?"ms":"?"))),
    apitype, (apitype>=0x210?"vsh":(apitype>=0x140?"ms":(apitype>=0x120?"disc":"?"))),
    keyconf, (keyconf==0x100?"vsh":(keyconf==0x200?"game":(keyconf==0x300?"pops":"?"))), 
    eboot);

  log("pspinfo - tachyon: 0x%08X, baryon: 0x%08X, psp = %dk, fw: 0x%08X\n",tachyon, baryon, pspmodel, fwversion);

  setup_addrs();

  return 1;
}

//////////////////////////////
///// sleep until sceMediaSync starts, then load stuff.
int main_thread ( SceSize args, void *argp ) {
#ifdef WAITSCEMEDIA
  sema = sceKernelCreateSema( "pergame_wake", 0, 0, 1, NULL );
  sceKernelWaitSemaCB( sema, 1, NULL );
  sceKernelDeleteSema(sema); sema = -1;
  log("main - Thread wake. %d cfg entries.\n",cfg_count);
  pergame_woke=1;
#endif
  if(logtofile >= 2) {
    int i;
    for ( i = 0; i < cfg_count; i ++ ) {
      log2("    cfg[%d]: modname: %s, bootname: %s\n", i, cfg[i].modname, cfg[i].bootname);
    }
  }
  if (sceKernelBootFrom() == PSP_BOOT_DISC) {
    int isoread=0; int umdread=0;
    char iso[64];
    char umdID[11];
    isoread=isopath(iso);
#ifdef USE_UMD_DATA_BIN
    umdread=getumdinfo(umdID);
#else
    if(cfg_umdaddr > 0) addr_umdid=cfg_umdaddr;
    umdread=readmemchar(umdID,addr_umdid,11,1);
    if(strlen(umdID) != 9) umdread=0;
#endif
    log("gameinfo - bootfrom %04X(%s), apitype: %04X(%s), keyconf: %04X(%s), initfile: %s, iso: '%s', umdid: '%s'\n",
      bootdev, (bootdev==0x00?"flash":(bootdev==0x20?"disc":(bootdev==0x40?"ms":"?"))),
      apitype, (apitype>=0x210?"vsh":(apitype>=0x140?"ms":(apitype>=0x120?"disc":"?"))),
      keyconf, (keyconf==0x100?"vsh":(keyconf==0x200?"game":(keyconf==0x300?"pops":"?"))), 
      eboot,(isoread?iso:"-error-"),(umdread?umdID:"-error-"));
    if(isoread) {
      loadlocalmods(iso);
      loadmods(iso);
      loadmods("iso");
      touch(iso);
    } else {
      loadmods("umd");
    }
    if(umdread) loadmods(umdID);
  } else {
    log("gameinfo - bootfrom %04X(%s), apitype: %04X(%s), keyconf: %04X(%s), initfile: %s\n",
      bootdev, (bootdev==0x00?"flash":(bootdev==0x20?"disc":(bootdev==0x40?"ms":"?"))),
      apitype, (apitype>=0x210?"vsh":(apitype>=0x140?"ms":(apitype>=0x120?"disc":"?"))),
      keyconf, (keyconf==0x100?"vsh":(keyconf==0x200?"game":(keyconf==0x300?"pops":"?"))), 
      eboot);

    loadlocalmods(eboot);
    loadmods(eboot);
    touch(eboot);
  }

  log2("main - cleanup start.\n");
  sceKernelFreeHeapMemory( cfg_mem_id, cfg );
  sceKernelFreeHeapMemory( cfg_mem_id, loaded );
  sceKernelDeleteHeap( cfg_mem_id );
  cfg = NULL;
  loaded = NULL;
  log("main - cleanup done.\n");

//  sceKernelDelayThread(100000);
//  sceKernelExitDeleteThread(0);
  
  return 0;
}

///////////////////////////////////
///// main()
int module_start(SceSize args, char *argp) {
  //logtofile=1;
  char * cwdp;
  strcpy(conf_name,argp);
  cwdp = strrchr(conf_name, '/');
  *cwdp = '\0';
  strcpy(log_name,conf_name);
  strcat(conf_name,"/pergame.txt");
  strcat(log_name,"/pergame.log");

  init_config();

#ifdef WAITSCEMEDIA
  install();
#endif
  int th = sceKernelCreateThread("pergame", main_thread, 0x18, 128*150, 0, NULL);// 8, 64*1024, 0, NULL);// 47, 0x400, 0x00001001, NULL);
  if( th >= 0 )
    sceKernelStartThread(th, 0, 0);
  return 0;
}
///////////////////////////////////


int module_stop(SceSize args, void *argp) {
  log("Baibai!\n");
  return 0;
}
