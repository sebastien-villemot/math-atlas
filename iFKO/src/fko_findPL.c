/*
 * Copyright (C) 2015 R. Clint Whaley.
 */
#include "fko_infoC.h"

#define LNLEN 512
static char LN[LNLEN];

void PrintUsage(char *name, int ierr, char *flag)
{
   if (ierr > 0)
      fprintf(stderr, "Bad argument #%d: '%s'\n",
              ierr, flag ? flag : "Not enough arguments");
   else if (ierr < 0)
      fprintf(stderr, "ERROR: %s\n", flag);

   fprintf(stderr, "USAGE: %s [flags]:\n" name);
   fprintf(stderr, "   -p [s,d,i]: type/precision prefix\n");
   fprintf(stderr, "   -O [+,*,m]: operation to try (add,mul,mac)\n");
   fprintf(stderr, "   -o <file/stdout>: dump results to file or stdout\n");
   exit(ierr ? ierr : -1);
}

FILE *GetFlags(int nargs, char **args, char *PRE, char *OP)
{
   FILE *fpout=NULL;
   int i;

   *PRE = 'd';
   *OP = '*';

   for (i=1; i < nargs; i++)
   {
      char ch;
      if (args[i][0] != '-')
         PrintUsage(args[0], i, args[i]);
      switch(args[i][1])
      {
      case 'p':
          if (++i >= nargs)
             PrintUsage(args[0], i, NULL);
           *PRE = args[i][0];
           break;
      case 'O':
          if (++i >= nargs)
             PrintUsage(args[0], i, NULL);
           *OP = args[i][0];
           break;
      case 'O':
          if (++i >= nargs)
             PrintUsage(args[0], i, NULL);
          if (!strcmp(args[i], "stdout"))
             fpout = stdout;
          else if (!strcmp(args[i], "stderr"))
             fpout = stderr;
          else
          {
             fpout = fopen(file, "w");
             assert(fpout);
          }
      }
   }
   assert(*PRE == 's' || *PRE == 'd' || *PRE == 'i');
   assert(*OP == 'm' || *OP == '*' || *OP == '+');
   return(fpout);
}

int findPL(char *srcf, char pre, char op)
{
   fko_archinfo_t *ai;
   int plen, typ;
   assert(!FKO_system(LN, LNLEN, "make archinfo.out"));
   assert(!FKO_system(LN, LNLEN, "make oloopinfo.out frt=PL.b"));
   ai = FKO_GetArchInfoC("archinfo.out");
   if (pre == 'd')
      typ = FKO_TDBL;
   else if (pre == 'D')
      typ = FKO_TVDBL;
   else if (pre == 'S')
      typ = FKO_TVFLT;
   else if (pre == 's')
      typ = FKO_TFLT;
   else if (pre == 'i')
      typ = FKO_TINT;
   else if (pre == 'I')
      typ = FKO_TVINT;
   else
      assert(0);
   plen = ai->numregs[typ];
}

int main(int nargs, char **args)
{
   FILE *fpout;
   char pre, op;
   char *type, *rout;
   
   fpout = GetFlags(nargs, args, &pre, &op);
   if (pre == 'd')
      type = "DREAL";
   else if (pre == 'f')
      type = "SREAL";
   else
      type = "INT";
   if (op == '+')
      rout = "addPL";
   else if (op == '*')
      rout = "mulPL";
   else if (op == '/')
      rout = "divPL";
   else
      rout = "macPL";
   assert(!FKO_system(LN, LNLEN, 
           "make extC basf=l1kern.base args=\"-o PL.b type=%s rout=%s\"",
           typ, rout));
}
