/* strtol example */
#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


#include <sys/unistd.h>


#define isspace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) =='\r' )

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

int main ()
{
  char szNumbers[] = "2001 60c0c0 -1101110100110100100000 0x6fffff";
  char * pEnd;
  long int li1, li2, li3, li4;
  li1 = strtol (szNumbers,&pEnd,10);
  li2 = strtol (pEnd,&pEnd,16);
  li3 = strtol (pEnd,&pEnd,2);
  li4 = strtol (pEnd,NULL,0);
  printf ("The decimal equivalents are: %ld, %ld, %ld and %ld.\n", li1, li2, li3, li4);
  printf ("lol: %d\n",strtol("0x000000f2", NULL, 16));
  char *name="! /lol/lol.prx";
  if ( strncmp( name, "!", 1 ) == 0 ) {
    char tmpname[64], blname[64];
    strcpy(tmpname,name+1);
    strcpy(tmpname,strstrip(tmpname));
    strcpy(blname,"");

    if(strncmp(tmpname,"/",1)==0) {   
      strcpy(blname,"ms0:");
    }
    strcat(blname,tmpname);
    
    printf("  blacklisting: '%s', ('%s', '%s')\n",blname,tmpname,name);
  }

  return 0;
}
