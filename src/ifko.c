#define IFKO_DECLARE
#include "ifko.h"
#include "fko_arch.h"
#include "fko_l2a.h"
#include "fko_loop.h"

int FUNC_FLAG=0, DTnzerod=0, DTabsd=0, DTnzero=0, DTabs=0;

int main(int nargs, char **args)
{
   FILE *fpin;
   extern FILE *yyin;
   extern INSTQ *iqhead;
   struct assmln *abase;

   fpin = fopen(args[1], "r");
   assert(fpin);
   yyin = fpin;
   yyparse();
   fclose(fpin);
   FixFrame();
   abase = lil2ass(iqhead);
   dump_assembly(stdout, abase);
   KillAllAssln(abase);
   #if 0
      abase = DumpData();
      dump_assembly(stdout, abase);
      KillAllAssln(abase);
   #endif
   KillAllInst(iqhead);
   return(0);
}
