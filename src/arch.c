#include "ifko.h"
#define ARCH_DECLARE
#include "fko_arch.h"
struct locinit *LIhead=NULL,       /* Locals to be init to constant vals */
               *ParaDerefQ=NULL;   /* Derefs created for parameters */

#ifdef X86_64
   #define ISIZE 8
#else
   #define ISIZE 4
#endif

void FindRegUsage(BBLOCK *bbase, int *ni0, int *iregs, 
                  int *nf0, int *fregs, int *nd0, int *dregs)
/*
 * Searches through all instructions to find registers that are used
 */
{
   short op;
   int nd, nf=0, ni=0;
   BBLOCK *bp;
   INSTQ *ip;
   const int iend = IREGBEG+TNIR, fend = FREGBEG+TNFR, dend = DREGBEG+TNDR;

   for (nd=0; nd < TNIR; nd++) iregs[nd] = 0;
   for (nd=0; nd < TNFR; nd++) fregs[nd] = 0;
   for (nd=0; nd < TNDR; nd++) dregs[nd] = 0;
   nd = 0;

   for (bp=bbase; bp; bp = bp->down)
   {
      for(ip=bp->inst1; ip; ip = ip->next)
      {
         op = -ip->inst[1];
         if (op >= IREGBEG && op < iend)
         {
            if (!iregs[op-IREGBEG]++) ni++;
         }
         else if (op >= FREGBEG && op < fend)
         {
            if (!fregs[op-FREGBEG]++) nf++;
         }
         else if (op >= DREGBEG && op < dend)
         {
            if (!dregs[op-DREGBEG]++) nd++;
         }
         op = -ip->inst[2];
         if (op >= IREGBEG && op < iend)
         {
            if (!iregs[op-IREGBEG]++) ni++;
         }
         else if (op >= FREGBEG && op < fend)
         {
            if (!fregs[op-FREGBEG]++) nf++;
         }
         else if (op >= DREGBEG && op < dend)
         {
            if (!dregs[op-DREGBEG]++) nd++;
         }
         op = -ip->inst[3];
         if (op >= IREGBEG && op < iend)
         {
            if (!iregs[op-IREGBEG]++) ni++;
         }
         else if (op >= FREGBEG && op < fend)
         {
            if (!fregs[op-FREGBEG]++) nf++;
         }
         else if (op >= DREGBEG && op < dend)
         {
            if (!dregs[op-DREGBEG]++) nd++;
         }
      }
   }
   fprintf(stderr, "\nUSED %d IREGS: ", ni);
   for (op=0; op < TNIR; op++) 
      if (iregs[op]) fprintf(stderr, "%s, ", archiregs[op]);
   fprintf(stderr, "\n");
   fprintf(stderr, "USED %d FREGS: ", nf);
   for (op=0; op < TNFR; op++) 
      if (fregs[op]) fprintf(stderr, "%s, ", archfregs[op]);
   fprintf(stderr, "\n");
   fprintf(stderr, "USED %d DREGS: ", nd);
   for (op=0; op < TNDR; op++)
      if (dregs[op]) fprintf(stderr, "%s, ", archdregs[op]);
   fprintf(stderr, "\n\n");

   *ni0 = ni;
   *nf0 = nf;
   *nd0 = nd;
}

int RemoveNosaveregs(int rstart, int nr, int *regs, int *saves)
/*
 * Zeros reg entry in regs that doesn't need to be saved
 * RETURNS: number of registers removed
 */
{
   int n=0, i;
   for (i=0; i < nr; i++)
   {
      if (!saves[i] && regs[i])
      {
         regs[i] = 0;
         n++;
      }
   }
   return(n);
}

int GetRegSaveList(int rstart, int nr, int *regs)
/*
 * takes list where entry i means save reg i, and return list with
 * each entry indicating the actual register to save
 * RETURNS: number of registers to save.
 */
{
   int i, j;
fprintf(stderr, "rstart=%d, nr=%d\n", rstart, nr);
   for (j=i=0; i < nr; i++)
   {
      if (regs[i])
         regs[j++] = i + rstart;
   }
   return(j);
}

int GetArchAlign(int nvd, int nvf, int nd, int nf, int nl, int ni)
/*
 *  Returns required architectural alignment given the number of
 *  vector double, vector float, double, float, long, and ints you
 *  want to save (actually, these only need to be boolean).
 */
{
   #ifdef X86_64
      return(16);
   #else
   int align = 0;
   if (nvd) align = FKO_DVLEN*8;
   else if (nvf) align = FKO_SVLEN*4;
   #ifdef X86_32
      else if (nd || nf || nl || ni) align = 4;
   #else
      else if (nd || nl) align = 8;
      else if (nf || ni) align = 4;
   #endif
fprintf(stderr, "nvd=%d,nvf=%d,nd=%d,nf=%d,nl=%d,ni=%d, align=%d\n",
        nvd, nvf, nd, nf, nl, ni, align);
   return(align);
   #endif
}
short dName2Reg(char *rname)
{
   short i;
   for (i=0; i < TNDR; i++)
     if (!strcmp(rname, archdregs[i])) return(i+DREGBEG);
   return(0);
}
short fName2Reg(char *rname)
{
   short i;
   for (i=0; i < TNFR; i++)
     if (!strcmp(rname, archfregs[i])) return(i+FREGBEG);
   return(0);
}
short iName2Reg(char *rname)
{
   short i;
   for (i=0; i < TNIR; i++)
     if (!strcmp(rname, archiregs[i])) return(i+IREGBEG);
   return(0);
}
short GetReg(short type)
/*
 * Get registers for local use, dying if we run out
 * NOTE: no handling of long so far
 */
{
   static int dr=0, fr=0, ir=1;
   int iret=0;
   extern int lnno;
   if (type == T_DOUBLE)
   {
      #ifdef SPARC
         if ((fr>>1)<<1 != fr) fr++;
         iret = DREGBEG + (fr>>1);
         fr += 2;
         if (fr > NFR*2)
            fko_error(__LINE__, "Out of double registers on line %d", lnno);
      #else
         iret = DREGBEG + dr;
         if (++dr > NDR)
            fko_error(__LINE__, "Out of double registers on line %d", lnno);
      #endif
   }
   else if (type == T_FLOAT)
   {
      iret = FREGBEG + fr;
      if (++fr > NFR)
         fko_error(__LINE__, "Out of float registers on line %d", lnno);
   }
   else if (type == T_INT)
   {
      iret = IREGBEG + ir;
      if (++ir > NIR)
         fko_error(__LINE__, "Out of integer registers on line %d", lnno);
   }
#ifdef X86_64
   else if (type == T_SHORT)
   {
      iret = IREGBEG + ir;
      if (++ir > NSR)
         fko_error(__LINE__, "Out of short registers on line %d", lnno);
   }
#endif
   else
   {
      assert(type == -1);
      dr = fr = 0;
      ir = 1;
   }
   return(iret);
}

/*
 * Mandated stack pointer alignment by architecture
 */
#ifdef SPARC
   #define ASPALIGN 8
#elif defined(LINUX_PPC)
   #define ASPALIGN 8
#elif defined(OSX_PPC)
   #define ASPALIGN 4
#elif defined(X86_32)
   #define ASPALIGN 4
#elif defined(X86_64)
   #define ASPALIGN 16
#else
   #define ASPALIGN 4
#endif

void CreateSysLocals()
/*
 *  If required, creates any locals needed to support instructions
 */
{
#ifdef X86
   extern int DTnzerod, DTabsd, DTnzero, DTabs;
   if (DTnzerod == -1)
   {
      DTnzerod = STdef("_NEGZEROD", VEC_BIT | T_DOUBLE | LOCAL_BIT, 0);
      SToff[DTnzerod-1].sa[2] = AddDerefEntry(-REG_SP, DTnzerod, -DTnzerod, 0);
   }
   if (DTabsd == -1)
   {
      DTabsd = STdef("_ABSVLD", VEC_BIT | T_DOUBLE | LOCAL_BIT, 0);
      SToff[DTabsd-1].sa[2] = AddDerefEntry(-REG_SP, DTabsd, -DTabsd, 0);
fprintf(stderr, "DTabsd = %d,%d\n", DTabsd, SToff[DTabsd-1].sa[2]);
   }
   if (DTnzero == -1)
   {
      DTnzero = STdef("_NEGZERO", VEC_BIT | T_FLOAT | LOCAL_BIT, 0);
      SToff[DTnzero-1].sa[2] = AddDerefEntry(-REG_SP, DTnzero, -DTnzero, 0);
   }
   if (DTabs == -1)
   {
      DTabs = STdef("_ABSVAL", VEC_BIT | T_FLOAT | LOCAL_BIT, 0);
      SToff[DTabs-1].sa[2] = AddDerefEntry(-REG_SP, DTabs, -DTabs, 0);
   }
#else
#endif
}

void bitload(INSTQ *next, int reg, int nbits, int I)
/*
 * Given a 32 bit value I, load it nbits at a time
 */
{
   int i, j, k, b, r;
   if (!I)
   {
      InsNewInst(NULL, NULL, next, XOR, -reg, -reg, -reg);
      return;
   }
   for (i=0; !(I & (1<<i)); i++);  /* find least sig bit non-zero bit */
   for (j=31; !(I & (1<<j)); j--); /* find most significant non-zero bit */
/*
 * Can we do it with a simple move?
 */
   if (j < nbits)
      InsNewInst(NULL, NULL, next, MOV, -reg, STiconstlookup(I), 0);
/*
 * Must load & shift
 */
   else
   {
      j++;
      k = (j-i+nbits-1) / nbits - 1;  /* number of ors required */
      r = (j-i) % nbits; 
      if (!r) r = nbits;   /* size of first shift */
      b = I >> (j-r);
      InsNewInst(NULL, NULL, next, MOV, -reg, STiconstlookup(b), 0);
      I ^= b << (j-r);
      j -= r;
      for (k; k; k--)
      {
         j -= nbits;
         b = I >> j;
         InsNewInst(NULL, NULL, next, SHL, -reg, -reg, 
                    STiconstlookup(nbits));
         InsNewInst(NULL, NULL, next, OR, -reg, -reg, STiconstlookup(b));
         I ^= b << j;
      }
      if (i) InsNewInst(NULL, NULL, next, SHL, -reg, -reg,
                        STiconstlookup(i));
   }
}

void SignalSet(INSTQ *next, 
               short id,     /* local that has been initialized */
               short deref,  /* deref of id that is not a local */
               short reg)    /* register of same type as id */
/*
 * When we have used a non-native type to initialize id, we issue this
 * load/store pair to signal that id has indeed been set
 */
{
   int type, ld, st;
   type = FLAG2PTYPE(STflag[id-1]);
   if (type == T_FLOAT)
   {
      ld = FLD;
      st = FST;
   }
   else if (type == T_DOUBLE)
   {
      ld = FLDD;
      st = FSTD;
   }
   else fko_error(__LINE__, "Unknown type %d in file %s\n", type, __FILE__);

   InsNewInst(NULL, NULL, next, ld, -reg, deref, 0);
   InsNewInst(NULL, NULL, next, st, SToff[id-1].sa[2], -reg, 0);
}

void FPConstStore(INSTQ *next, short id, short con, 
                  short reg, short freg, short dreg)
{
   int flag;
   int *ip;
   short *sp;
   double d;
   short i, k;
   float f;
#ifdef X86_64
   long *lp;
#endif
   flag = STflag[id-1];
   if (IS_VEC(flag))
   {
      fprintf(stderr, "Vector constants not yet supported!\n");
      exit(-1);
   }
   else if (IS_DOUBLE(flag))
   {
      d = SToff[con-1].d;
      if (d == 0.0)
      {
         #ifndef PPC
            InsNewInst(NULL, NULL, next, FZEROD, -dreg, 0, 0);
            InsNewInst(NULL, NULL, next, FSTD, SToff[id-1].sa[2], -dreg, 0);
         #else
            i = SToff[SToff[id-1].sa[2]-1].sa[3];
            k = AddDerefEntry(-REG_SP, id, -id, i);
            InsNewInst(NULL, NULL, next, XOR, -reg, -reg, -reg);
            InsNewInst(NULL, NULL, next, ST, k, -reg, 0);
            InsNewInst(NULL, NULL, next, ST, 
                       AddDerefEntry(-REG_SP, id, -id, i+4), -reg, 0);
            SignalSet(next, id, k, dreg);
         #endif
      }
      else
      {
         #ifdef X86_32
            ip = (int*) &d;
            i = SToff[SToff[id-1].sa[2]-1].sa[3];
            k = AddDerefEntry(-REG_SP, id, -id, i);
            InsNewInst(NULL, NULL, next, MOV, -reg, STiconstlookup(*ip), 0);
            InsNewInst(NULL, NULL, next, ST, k, -reg, 0);
            InsNewInst(NULL, NULL, next, MOV, -reg, STiconstlookup(ip[1]),
                       0);
            InsNewInst(NULL, NULL, next, ST,
                       AddDerefEntry(-REG_SP, id, -id, i+4), -reg, 0);
         #elif defined(X86_64)
            lp = (long*) &d;
            InsNewInst(NULL, NULL, next, MOV, -reg, STlconstlookup(*lp), 0);
            InsNewInst(NULL, NULL, next, ST, SToff[id-1].sa[2], -reg, 0);
/*
 *       Sparc has 13-bit constants, use 12 to rule out sign prob
 */
         #elif defined(SPARC)
            ip = (int*) &d;
            i = SToff[SToff[id-1].sa[2]-1].sa[3];
            k = AddDerefEntry(-REG_SP, id, -id, i);
            bitload(next, reg, 12, *ip);
            InsNewInst(NULL, NULL, next, ST, k, -reg, 0);
            bitload(next, reg, 12, ip[1]);
            InsNewInst(NULL, NULL, next, ST, 
                       AddDerefEntry(-REG_SP, id, -id, i+4), -reg, 0);
/*
 *       PPC loads 16 bits at a time
 */
         #elif defined(PPC)
            ip = (int*) &d;
            i = SToff[SToff[id-1].sa[2]-1].sa[3];
            k = AddDerefEntry(-REG_SP, id, -id, i);
            bitload(next, reg, 16, *ip);
            InsNewInst(NULL, NULL, next, ST, k, -reg, 0);
            bitload(next, reg, 16, ip[1]);
            InsNewInst(NULL, NULL, next, ST,
                       AddDerefEntry(-REG_SP, id, -id, i+4), -reg, 0);
         #endif
         #ifndef X86_64
            SignalSet(next, id, k, dreg);
         #endif
      }
   }
   else
   {
      assert(IS_FLOAT(flag));
      f = SToff[con-1].f;
      if (f == 0.0e0)
      {
         #ifndef PPC
            InsNewInst(NULL, NULL, next, FZERO, -freg, 0, 0);
            InsNewInst(NULL, NULL, next, FST, SToff[id-1].sa[2], -freg, 0);
         #else
            InsNewInst(NULL, NULL, next, XOR, -reg, -reg, -reg);
            InsNewInst(NULL, NULL, next, ST, SToff[id-1].sa[2], -reg, 0);
         #endif
      }
      else
      {
         ip = (int*) &f;
         #ifdef X86_32
            InsNewInst(NULL, NULL, next, MOV, -reg, STiconstlookup(*ip), 0);
            InsNewInst(NULL, NULL, next, ST, SToff[id-1].sa[2], -reg, 0);
         #elif defined(X86_64)
            assert(reg <= 8);
            InsNewInst(NULL, NULL, next, MOVS, -reg, STiconstlookup(*ip), 0);
            InsNewInst(NULL, NULL, next, STS, SToff[id-1].sa[2], -reg, 0);
         #elif defined(SPARC)
            bitload(next, reg, 12, *ip);
            InsNewInst(NULL, NULL, next, ST, SToff[id-1].sa[2], -reg, 0);
         #elif defined(PPC)
            bitload(next, reg, 16, *ip);
            InsNewInst(NULL, NULL, next, ST, SToff[id-1].sa[2], -reg, 0);
         #endif
      }
   }
}

void IConstStore(INSTQ *next, short id, short con, short reg)
{
   int i, j;

   #ifdef X86_32
      InsNewInst(NULL, NULL, next, MOV, -reg, con, 0);
/*
 * Sparc has 13-bit constants, use 12 to rule out sign prob
 */
   #elif defined(SPARC)
      bitload(next, reg, 12, SToff[con-1].i);
/*
 * Must load constants 16 bits at a time
 */
   #else
      bitload(next, reg, 16, SToff[con-1].i);
   #endif
   InsNewInst(NULL, NULL, next, ST, SToff[id-1].sa[2], -reg, 0);
}

void InitLocalConst(INSTQ *next, short reg, short freg, short dreg)
{
   struct locinit *lp;
   int flag;
   InsNewInst(NULL, NULL, next, COMMENT, 0, 0, 0);
   InsNewInst(NULL, NULL, next, COMMENT, 
              STstrconstlookup("Initialize locals to constants"), 0, 0);
   InsNewInst(NULL, NULL, next, COMMENT, 0, 0, 0);
   for (lp=LIhead; lp; lp = LIhead)
   {
      LIhead = lp->next;
      flag = FLAG2PTYPE(STflag[lp->id-1]);
      if (IS_FLOAT(flag) || IS_DOUBLE(flag))
         FPConstStore(next, lp->id, lp->con, reg, freg, dreg);
      else
         IConstStore(next, lp->id, lp->con, reg);
      free(lp);
   }
}

void Extern2Local(INSTQ *next)
/*
 * After stack frame fully qualified, inserts proper store instructions before
 * next in queue in order to save parameter/system/fp const values to local
 * frame.
 *
 * For some alignments, we may need to address the callers frame by a register
 * other than sp (because we have lost track of size of frame in forcing
 * the alignment beyond the machine's native alignment).  If so, rsav is
 * set to what amounts to the callers sp.  Otherwise, we are indexing by
 *    %sp + fsize 
 */
{
   extern int NPARA, DTnzerod, DTnzero, DTabsd, DTabs;
   short ii, i, j=0, flag, ir, k, kk, reg1=0, freg, dreg;
   const int rsav = 0;
   int USED;
   #ifdef X86_64
      int nof, ni, nd, dr, dreg1;
      char *rpara[6] = {"@rdi", "@rsi", "@rdx", "@rcx", "@r8", "@r9"};
      char fnam[8];
   #elif defined(FKO_ANSIC)
      int ld, st;
   #endif
   int nbytes=0;
   short *paras;
   char nam[8];
   #ifdef PPC
      char fnam[8];
      int fc, fr=0;
   #endif

   dreg = GetReg(T_DOUBLE);
   freg = GetReg(T_FLOAT);
   if (NPARA)
   {
      InsNewInst(NULL, NULL, next, COMMENT, 0, 0, 0);
      InsNewInst(NULL, NULL, next, COMMENT, 
                 STstrconstlookup("Store parameters to local frame"), 0, 0);
      InsNewInst(NULL, NULL, next, COMMENT, 0, 0, 0);
      paras = malloc(NPARA * sizeof(short));
      assert(paras);
   }
   else paras = NULL;
/*
 * Find all parameters, and put them in left-to-right order
 */
   for (i=0; j < NPARA; i++)
   {
      flag = STflag[i];
      if (IS_PARA(flag))
      {
        fprintf(stderr, "para #%d - '%s', I=%d, flag=%d\n", SToff[i].sa[0], 
                 STname[i]?STname[i]:"NULL", i, STflag[i]);
         assert(SToff[i].sa[0] <= NPARA);
         paras[SToff[i].sa[0]-1] = i;
         j++;
      }
   }
/*   MarkUnusedParams(NPARA, paras); */
   #ifdef FKO_ANSIC
      for (i=0; i < NPARA; i++)
      {
         USED = SToff[SToff[paras[i]].sa[2]-1].sa[0];
         if (USED)
            PrintComment(NULL, NULL, next, "para %d, name=%s", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         else
            PrintComment(NULL, NULL, next, "para %d, name=%s: UNUSED", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         flag = STflag[paras[i]];
         k = FLAG2PTYPE(flag);
         switch(k)  /* HERE HERE HERE */
         {
         T_INT:
            ld = LD;
            st = ST;
            break;
         T_FLOAT:
            ld = FLD;
            st = FST;
            break;
         T_DOUBLE:
            ld = FLDD;
            st = FSTD;
            break;
         default:
         }
   #endif
   #ifdef X86_64
      reg1 = GetReg(T_INT);
      while (iparareg[reg1-IREGBEG]) reg1 = GetReg(T_INT);
      fnam[0] = '@';
      fnam[1] = 'x';
      fnam[2] = 'm';
      fnam[3] = 'm';
      fnam[5] = '\0';
      for (i=nof=nd=ni=0; i < NPARA; i++)
      {
         USED = SToff[SToff[paras[i]].sa[2]-1].sa[0];
         if (USED)
            PrintComment(NULL, NULL, next, "para %d, name=%s", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         else
            PrintComment(NULL, NULL, next, "para %d, name=%s: UNUSED", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         flag = STflag[paras[i]];
         if (IS_PTR(flag))
         {
            if (ni < 6) ir = iName2Reg(rpara[ni]);
            else
            {
               ir = reg1;
               k = AddDerefEntry(rsav, 0, 0, nof*8);
               ParaDerefQ = NewLocinit(k, 0, ParaDerefQ);
               if (USED)
                  InsNewInst(NULL, NULL, next, LD, -ir, k, 0);
               nof++;
            }
            if (USED)
               InsNewInst(NULL, NULL, next, ST, SToff[paras[i]].sa[2], -ir, 0);
            ni++;
         }
         else if (IS_INT(flag))
         {
/*
 *          Load 32 bit value
 */
            if (ni < 6) ir = iName2Reg(rpara[ni]);
            else
            {
               ir = reg1;
               k = AddDerefEntry(rsav, 0, 0, nof*8);
               ParaDerefQ = NewLocinit(k, 0, ParaDerefQ);
               if (USED) InsNewInst(NULL, NULL, next, LDS, -ir, k, 0);
               nof++;
            }
/*
 *          Convert to 64-bit value, no conversion required for unsigned
 */
            if (USED && !IS_UNSIGNED(flag))
            {
               k = STiconstlookup(32);
               InsNewInst(NULL, NULL, next, SHL, -ir, -ir, k);
               InsNewInst(NULL, NULL, next, SAR, -ir, -ir, k);
            }
/*
 *          Store 64-bit integer
 */
            if (USED)
               InsNewInst(NULL, NULL, next, ST, SToff[paras[i]].sa[2], -ir, 0);
            ni++;
         }
         else if (IS_FLOAT(flag))
         {
            if (nd < 8)
            {
               fnam[4] = nd + '0';
               dr = fName2Reg(fnam);
               if (USED)
                  InsNewInst(NULL, NULL, next, FST, SToff[paras[i]].sa[2], 
                             -dr, 0);
            }
            else
            {
               ir = reg1;
               if (USED)
               {
                  k = AddDerefEntry(rsav, 0, 0, nof*8);
                  ParaDerefQ = NewLocinit(k, 0, ParaDerefQ);
                  InsNewInst(NULL, NULL, next, FLD, -freg, k, 0);
                  InsNewInst(NULL, NULL, next, FST, SToff[paras[i]].sa[2],
                             -freg, 0);
               }
               nof++;
            }
            nd++;
         }
         else
         {
            if (nd < 8)
            {
               fnam[4] = nd + '0';
               dr = dName2Reg(fnam);
               if (USED) 
                  InsNewInst(NULL, NULL, next, FSTD, SToff[paras[i]].sa[2], 
                             -dr, 0);
            }
            else
            {
               ir = reg1;
               if (USED)
               {
                  k = AddDerefEntry(rsav, 0, 0, nof*8);
                  ParaDerefQ = NewLocinit(k, 0, ParaDerefQ);
                  InsNewInst(NULL, NULL, next, FLDD, -dreg, k, 0);
                  InsNewInst(NULL, NULL, next, FSTD, SToff[paras[i]].sa[2],
                             -dreg, 0);
               }
               nof++;
            }
            nd++;
         }
      }
      InsNewInst(NULL, NULL, next, COMMENT, STstrconstlookup("done paras"),
                 0, 0);
      ir = reg1;
      if (DTnzerod > 0)
      {
         PrintComment(NULL, NULL, next, "Writing -0 to memory for negation");
         InsNewInst(NULL, NULL, next, MOV, -ir,
                    STlconstlookup(0x8000000000000000), 0);
         k = SToff[SToff[DTnzerod-1].sa[2]-1].sa[3];
         InsNewInst(NULL, NULL, next, ST, SToff[DTnzerod-1].sa[2], -ir, 0);
         InsNewInst(NULL, NULL, next, ST, 
                    AddDerefEntry(-REG_SP, DTnzerod, -DTnzerod, k+8), -ir, 0);
      }
      if (DTnzero > 0)
      {
         PrintComment(NULL, NULL, next, "Writing -0 to memory for negation");
         InsNewInst(NULL, NULL, next, MOV, -ir,
                    STlconstlookup(0x8000000080000000), 0);
         InsNewInst(NULL, NULL, next, ST, SToff[DTnzero-1].sa[2], -ir, 0);
         k = ((SToff[DTnzero-1].sa[2]-1)<<2) + 3;
         k = SToff[SToff[DTnzero-1].sa[2]-1].sa[3];
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTnzero, -DTnzero, k+8), -ir, 0);
      }
      if (DTabsd)
      {
         PrintComment(NULL, NULL, next, "Writing ~(-0) to memory for absd");
         InsNewInst(NULL, NULL, next, MOV, -ir,
                    STlconstlookup(0x7FFFFFFFFFFFFFFF), 0);
         k = SToff[SToff[DTabsd-1].sa[2]-1].sa[3];
         InsNewInst(NULL, NULL, next, ST, SToff[DTabsd-1].sa[2], -ir, 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTabsd, -DTabsd, k+8), -ir, 0);
      }
      if (DTabs)
      {
         PrintComment(NULL, NULL, next, "Writing ~(-0) to memory for abss");
         k = SToff[SToff[DTabs-1].sa[2]-1].sa[3];
         InsNewInst(NULL, NULL, next, MOV, -ir,
                    STlconstlookup(0x7fffffff7fffffff), 0);
         InsNewInst(NULL, NULL, next, ST, SToff[DTabs-1].sa[2], -ir, 0);
         InsNewInst(NULL, NULL, next, ST, 
                    AddDerefEntry(-REG_SP, DTabs, -DTabs, k+8), -ir, 0);
      }
      InsNewInst(NULL, NULL, next, COMMENT, STstrconstlookup("done archspec"), 
                 0, 0);
   #endif
   #ifdef X86_32
      reg1 = GetReg(T_INT);
      while (iparareg[reg1-IREGBEG]) reg1 = GetReg(T_INT);
      ir = reg1;
      for (j=i=0; i < NPARA; i++)
      {
         USED = SToff[SToff[paras[i]].sa[2]-1].sa[0];
         if (USED)
            PrintComment(NULL, NULL, next, "para %d, name=%s", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         else
            PrintComment(NULL, NULL, next, "para %d, name=%s: UNUSED", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         flag = STflag[paras[i]];
         if ( IS_DOUBLE(flag) && !IS_PTR(flag))
         {
            if (USED)
            {
               k = AddDerefEntry(rsav, 0, 0, j*4);
               ParaDerefQ = NewLocinit(k, 0, ParaDerefQ);
               InsNewInst(NULL, NULL, next, FLDD, -dreg, k, 0);
               InsNewInst(NULL, NULL, next, FSTD, SToff[paras[i]].sa[2], 
                          -dreg, 0);
            }
            j++;
         }
         else if (USED)
         {
            k = AddDerefEntry(rsav, 0, 0, j*4);
            ParaDerefQ = NewLocinit(k, 0, ParaDerefQ);
            InsNewInst(NULL, NULL, next, LD, -ir, k, 0);
            InsNewInst(NULL, NULL, next, ST, SToff[paras[i]].sa[2], -ir, 0);
         }
         j++;
      }
      if (USED)
         InsNewInst(NULL, NULL, next, COMMENT, STstrconstlookup("done paras"),
                    0, 0);
      if (DTnzerod > 0)
      {
         PrintComment(NULL, NULL, next, "Writing -0 to memory for negation");
         k = SToff[SToff[DTnzerod-1].sa[2]-1].sa[3];
         InsNewInst(NULL, NULL, next, XOR, -ir, -ir, -ir);
         InsNewInst(NULL, NULL, next, ST, SToff[DTnzerod-1].sa[2], -ir, 0);
         InsNewInst(NULL, NULL, next, ST, 
                    AddDerefEntry(-REG_SP, DTnzerod, -DTnzerod, k+8), -ir, 0);
         InsNewInst(NULL, NULL, next, MOV, -ir, STiconstlookup(0x80000000), 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTnzerod, -DTnzerod, k+4), -ir, 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTnzerod, -DTnzerod, k+12), -ir, 0);
      }
      if (DTnzero > 0)
      {
         PrintComment(NULL, NULL, next, "Writing -0 to memory for negation");
         InsNewInst(NULL, NULL, next, MOV, -ir, STiconstlookup(0x80000000), 0);
         InsNewInst(NULL, NULL, next, ST, SToff[DTnzero-1].sa[2], -ir, 0);
         k = SToff[SToff[DTnzero-1].sa[2]-1].sa[3];
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTnzero, -DTnzero, k+4), -ir, 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTnzero, -DTnzero, k+8), -ir, 0);
         InsNewInst(NULL, NULL, next, ST, 
                    AddDerefEntry(-REG_SP, DTnzero, -DTnzero, k+12), -ir, 0);
      }
      if (DTabsd)
      {
         PrintComment(NULL, NULL, next, "Writing ~(-0) to memory for absd");
         k = SToff[SToff[DTabsd-1].sa[2]-1].sa[3];
         InsNewInst(NULL, NULL, next, XOR, -ir, -ir, -ir);
         InsNewInst(NULL, NULL, next, NOT, -ir, -ir, -ir);
         InsNewInst(NULL, NULL, next, ST, SToff[DTabsd-1].sa[2], -ir, 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTabsd, -DTabsd, k+8), -ir, 0);
         InsNewInst(NULL, NULL, next, MOV, -ir, STiconstlookup(0x7fffffff), 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTabsd, -DTabsd, k+4), -ir, 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTabsd, -DTabsd, k+12), -ir, 0);
      }
      if (DTabs)
      {
         PrintComment(NULL, NULL, next, "Writing ~(-0) to memory for abss");
         k = SToff[SToff[DTabs-1].sa[2]-1].sa[3];
         InsNewInst(NULL, NULL, next, MOV, -ir, STiconstlookup(0x7fffffff), 0);
         InsNewInst(NULL, NULL, next, ST, SToff[DTabs-1].sa[2], -ir, 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTabs, -DTabs, k+4), -ir, 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTabs, -DTabs, k+8), -ir, 0);
         InsNewInst(NULL, NULL, next, ST,
                    AddDerefEntry(-REG_SP, DTabs, -DTabs, k+12), -ir, 0);
      }
      InsNewInst(NULL, NULL, next, COMMENT, STstrconstlookup("done archspec"), 0, 0);
   #endif
   #ifdef SPARC
      nam[0] = '@';
      nam[1] = 'i';
      nam[3] = '\0';
      for (j=i=0; i < NPARA; i++)
      {
         USED = SToff[SToff[paras[i]].sa[2]-1].sa[0];
         if (USED)
            PrintComment(NULL, NULL, next, "para %d, name=%s", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         else
            PrintComment(NULL, NULL, next, "para %d, name=%s: UNUSED", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         flag = STflag[paras[i]];
         if (IS_PTR(flag) || IS_INT(flag) || IS_FLOAT(flag))
         {
            if (j < 6)
            {
               nam[2] = j + '0';
               ir = iName2Reg(nam);
               if (USED)
                  InsNewInst(NULL, NULL, next, ST, SToff[paras[i]].sa[2], 
                             -ir, 0);
            }
            else if (USED)
            {
               k = AddDerefEntry(rsav, 0, 0, j*4);
               ParaDerefQ = NewLocinit(k, 0, ParaDerefQ);
               InsNewInst(NULL, NULL, next, LD, -ir, k, 0);
               InsNewInst(NULL, NULL, next, ST, SToff[paras[i]].sa[2], -ir, 0);
            }
            j++;
         }
         else
         {
            assert(IS_DOUBLE(flag));
            if (j < 5)
            {
               nam[2] = j + '0';
               ir = iName2Reg(nam);
               j++;
               if (USED)
               {
                  kk = SToff[SToff[paras[i]].sa[2]-1].sa[3];
                  k = AddDerefEntry(-REG_SP, paras[i], -paras[i], kk);
                  InsNewInst(NULL, NULL, next, ST, k, -ir, 0);
                  nam[2] = j + '0';
                  ir = iName2Reg(nam);
                  InsNewInst(NULL, NULL, next, ST, 
                             AddDerefEntry(-REG_SP, paras[i], -paras[i], kk+4),
                             -ir, 0);
                  SignalSet(next, paras[i]+1, k, dreg);
               }
               j++;
            }
            else if (j == 5)
            {
               nam[2] = j + '0';
               ir = iName2Reg(nam);
               j++;
               if (USED)
               {
                  kk = SToff[SToff[paras[i]].sa[2]-1].sa[3];
                  k = AddDerefEntry(-REG_SP, paras[i], -paras[i], kk);
                  InsNewInst(NULL, NULL, next, ST, SToff[paras[i]].sa[2],
                             -ir, 0);

                  ii = AddDerefEntry(rsav, 0, 0, j*4);
                  ParaDerefQ = NewLocinit(ii, 0, ParaDerefQ);
                  InsNewInst(NULL, NULL, next, LD, -ir, ii, 0);

                  ii = AddDerefEntry(-REG_SP, paras[i], -paras[i], kk+4);
                  InsNewInst(NULL, NULL, next, ST, ii, -ir, 0);
                  SignalSet(next, paras[i]+1, k, dreg);
               }
               j++;
            }
            else
            {
               if (USED)
               {
                  strcpy(nam, archdregs[dreg-DREGBEG]);
                  k = fName2Reg(nam);
                  ii = AddDerefEntry(rsav, 0, 0, j*4);
                  ParaDerefQ = NewLocinit(ii, 0, ParaDerefQ);
                  InsNewInst(NULL, NULL, next, FLD, -k, ii, 0);
                  k++;
                  ii = AddDerefEntry(rsav, 0, 0, j*4+4);
                  ParaDerefQ = NewLocinit(ii, 0, ParaDerefQ);
                  InsNewInst(NULL, NULL, next, FLD, -k, ii, 0);
                  InsNewInst(NULL, NULL, next, FSTD, SToff[paras[i]].sa[2], 
                             -dreg, 0);
               }
               j += 2;
            }
         }
      }
   #endif
   #ifdef OSX_PPC
      nam[0] = 'r';
      nam[3] = nam[2] = '\0';
      fnam[0] = 'f';
      fnam[2] = fnam[3] = '\0';
      for (fc=j=i=0; i < NPARA; i++)
      {
         USED = SToff[SToff[paras[i]].sa[2]-1].sa[0];
         if (USED)
            PrintComment(NULL, NULL, next, "para %d, name=%s", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         else
            PrintComment(NULL, NULL, next, "para %d, name=%s: UNUSED", i, 
                         STname[paras[i]] ? STname[paras[i]] : "NULL");
         flag = STflag[paras[i]];
         if (IS_PTR(flag) || IS_INT(flag))
         {
            if (USED)
            {
               if (j < 8)
               {
                  if (j < 7) nam[1] = j + '3';
                  else { nam[1] = '1'; nam[2] = '0'; }
                  ir = iName2Reg(nam);
                  InsNewInst(NULL, NULL, next, ST, SToff[paras[i]].sa[2], 
                             -ir, 0);
               }
               else
               {
                  ii = AddDerefEntry(rsav, 0, 0, j*4);
                  ParaDerefQ = NewLocinit(ii, 0, ParaDerefQ);
                  if (j == 8)
                  {
                     ir = GetReg(T_INT);
                     while (iparareg[ir-IREGBEG]) ir = GetReg(T_INT);
                  }
                  InsNewInst(NULL, NULL, next, LD, -ir, ii, 0);
                  InsNewInst(NULL, NULL, next, ST, SToff[paras[i]].sa[2], 
                             -ir, 0);
               }
            }
            j++;
         }
         else if (IS_FLOAT(flag))
         {
            if (j == 8)
            {
               ir = GetReg(T_INT);
               while (iparareg[ir-IREGBEG]) ir = GetReg(T_INT);
            }
            if (USED)
            {
               if (fc < 13)
               {
                  if (fc < 9) fnam[1] = '0' + fc + 1;
                  else { fnam[1] = '1'; fnam[2] = '0' + fc - 9; }
                  fr = fName2Reg(fnam);
                  InsNewInst(NULL, NULL, next, FST, SToff[paras[i]].sa[2], 
                             -fr, 0);
               }
               else
               {
                  ii = AddDerefEntry(rsav, 0, 0, j*4);
                  ParaDerefQ = NewLocinit(ii, 0, ParaDerefQ);
                  InsNewInst(NULL, NULL, next, FLD, -freg, ii, 0);
                  InsNewInst(NULL, NULL, next, FST, SToff[paras[i]].sa[2], 
                             -freg, 0);
               }
            }
            fc++;
            j++;
         }
         else
         {
            assert(IS_DOUBLE(flag));
            if (j == 8 || j == 7)
            {
               ir = GetReg(T_INT);
               while (iparareg[ir-IREGBEG]) ir = GetReg(T_INT);
            }
            if (USED)
            {
               if (fc < 13)
               {
                  if (fc < 9) fnam[1] = '0' + fc + 1;
                  else { fnam[1] = '1'; fnam[2] = '0' + fc - 9; }
                  fr = dName2Reg(fnam);
                  InsNewInst(NULL, NULL, next, FSTD, SToff[paras[i]].sa[2], 
                             -fr, 0);
                  j += 2;
               }
               else
               {
                  ii = AddDerefEntry(rsav, 0, 0, j*4);
                  ParaDerefQ = NewLocinit(ii, 0, ParaDerefQ);
                  InsNewInst(NULL, NULL, next, FLDD, -dreg, ii, 0);
                  InsNewInst(NULL, NULL, next, FSTD, SToff[paras[i]].sa[2],
                             -dreg, 0);
                  j += 2;
               }
            }
            else j += 2;
            fc++;
         }
      }
   #endif
/*
 * Initialize constants
 */
   if (!reg1) reg1 = GetReg(T_INT);
   InitLocalConst(next, reg1, freg, dreg);
   GetReg(-1);
}

void FinalizeEpilogue(BBLOCK *bbase,
                      int fsize,  /* frame size of returning func */
                      int Soff,   /* start of reg save area in frame */
                      int savesp, /* offset we saved sp at */
                      int nir,    /* number of int regs saved */
                      int *ir,    /* int regs saved */
                      int nfr,    /* number of single regs saved */
                      int *fr,    /* float regs saved */
                      int ndr,    /* number of double regs saved */
                      int *dr     /* double regs saved */
                      )
{
   int i;
   INSTQ *next;
   BBLOCK *blk;

   i = STlabellookup("_IFKO_EPILOGUE");
   fprintf(stderr, "i=%d\n", i);
   blk = FindBlockWithLabel(bbase, STlabellookup("_IFKO_EPILOGUE"));
   assert(blk);
/* 
 * Find place to insert save statements
 */
   for (next=blk->inst1; next; next = next->next)
      if (next->inst[0] == CMPFLAG && next->inst[1] == CF_REGRESTORE) break;
   assert(next);
/*
/*
 * Restore registers
 */
   for (i=0; i < ndr; i++)
      InsNewInst(blk, NULL, next, FLDD, -dr[i],
                 AddDerefEntry(-REG_SP, 0, 0, Soff+i*8), 0);
   for (i=0; i < nir; i++)
      InsNewInst(blk, NULL, next, LD, -ir[i],
                 AddDerefEntry(-REG_SP, 0,0, Soff+ndr*8+i*ISIZE), 0);
   for (i=0; i < nfr; i++)
      InsNewInst(blk, NULL, next, FLD, -fr[i],
                 AddDerefEntry(-REG_SP, 0, 0, Soff+ndr*8+nir*ISIZE+i*4), 0);
/*
 * Restore stack pointer
 */
   if (savesp >= 0)
      InsNewInst(blk, NULL, next, LD, -REG_SP, 
                 AddDerefEntry(-REG_SP, 0, 0, savesp), 0);
   else
      InsNewInst(blk, NULL, next, ADD, -REG_SP, -REG_SP,
                 STiconstlookup(fsize));
}

void FinalizePrologueEpilogue(BBLOCK *bbase)
/*
 * Calculates required frame size, corrects local and parameter offsets
 * appropriately, and then inserts instructions to save and restore
 * callee-saved regs
 */
{
   INSTQ *ip, *oldhead;
   int k, i, nir, nfr, ndr;
   int ir[TNIR], fr[TNFR], dr[TNDR];
   int Aoff;  /* offset to arguments, from frame pointer */
   int Soff=0; /* system-dependant skip offset */
   int Loff;   /* called routines frame size excluding locals */
   int SAVESP=(-1);  /* must we save SP to stack? */
   int align;        /* local area required byte alignment */
   int lsize;        /* size of all required locals */
   int tsize;        /* total frame size */
   int rsav=0, maxalign, ssize=0;
   int csize=0;/* call parameter area size */
   extern int LOCALIGN, LOCSIZE;

   maxalign = align = LOCALIGN;
   lsize = LOCSIZE;
/*
 * Find registers that need to be saved
 */
   FindRegUsage(bbase, &nir, ir, &nfr, fr, &ndr, dr);
   k = RemoveNosaveregs(IREGBEG, TNIR, ir, icalleesave);
   nir = GetRegSaveList(IREGBEG, TNIR, ir);
fprintf(stderr, "nosave=%d nisav = %d\n", k, nir);
   RemoveNosaveregs(FREGBEG, TNFR, fr, fcalleesave);
   nfr = GetRegSaveList(FREGBEG, TNFR, fr);
   RemoveNosaveregs(DREGBEG, TNDR, dr, dcalleesave);
   ndr = GetRegSaveList(DREGBEG, TNDR, dr);
/*
 * If we return values in a register, no need to save and restore it
 */
   if (rout_flag & IRET_BIT)
   {
      for (i=0; i < nir && ir[i] != IRETREG; i++);
      if (i < nir)
         for (nir--; i < nir; i++) ir[i] = ir[i+1];
   }
   if (rout_flag & FRET_BIT)
   {
      for (i=0; i < nfr && fr[i] != FRETREG; i++);
      if (i < nfr)
         for (nfr--; i < nfr; i++) fr[i] = fr[i+1];
   }
   if (rout_flag & DRET_BIT)
   {
      for (i=0; i < ndr && dr[i] != DRETREG; i++);
      if (i < ndr)
         for (ndr--; i < ndr; i++) dr[i] = dr[i+1];
   }

   #ifdef X86_64
      if (!align) align = 16;
   #else
      if (!align) align = 4;
   #endif

/* 
 * Find place to insert save statements
 */
   for (ip=bbase->inst1; ip; ip = ip->next)
      if (ip->inst[0] == CMPFLAG && ip->inst[1] == CF_REGSAVE) break;
   assert(ip);
   oldhead = ip;
/*
 * For x86-64, save %rbp, if necessary, to the reserved location of 0(%rsp)
 */
   #ifdef X86_64
      k = iName2Reg("%rbp");
      for (i=0; i < nir && ir[i] != k; i++);
      if (i < nir)
      {
         InsNewInst(NULL, NULL, oldhead, ST,
                    AddDerefEntry(-REG_SP, 0, 0, 0), -k, 0);
         for (nir--; i < nir; i++) ir[i] = ir[i+1];
      }
   #endif
/*
 * Figure stack frame, ensuring all parts have correct alignment
 */
   #ifdef SPARC
      Soff = 64;
      Aoff = 68;
      if (csize && csize < 6*4) csize = 6*4;
   #elif defined(X86_64)
      Soff = 8;
      Aoff = 8;
   #elif defined(X86_32)
      Aoff  = 4;
   #elif defined(OSX_PPC)
      if (csize < 32) csize = 32;
      Aoff = 24;
   #elif defined(LINUX_PPC)
      Aoff = 8;
   #endif
/*   tsize = Aoff + csize + ssize + lsize; */
   Soff += csize;
   #ifdef X86_64
      Loff = 8*(nir+ndr) + 4*nfr;
      if (Loff % ASPALIGN) Loff = (Loff/ASPALIGN)*ASPALIGN + ASPALIGN;
      Loff += Soff;
      tsize = Loff + lsize;
      if (tsize % ASPALIGN) tsize = (tsize/ASPALIGN)*ASPALIGN + ASPALIGN;
fprintf(stderr, "tsize=%d, Loff=%d, Soff=%d lsize=%d\n", tsize, Loff, Soff, lsize);
   #else
/*
 *    We assume sp already 4-byte aligned but may need to make 8-byte aligned
 *    if demanded by save
 */
      if (ndr)
      {
         if ((Soff>>3)<<3 != Soff) Soff = 8 + ((Soff>>3)<<3);
         #ifndef X86_32
            if (maxalign < 8) maxalign = 8;
         #endif
      }
      if (maxalign > ASPALIGN)
      {
         Loff = Soff + 8*ndr + 4*nfr + 4*nir + 4;
         SAVESP = Loff-4;
      }
      else Loff = Soff + 8*ndr + 4*nfr + 4*nir;
      if (Loff%align) Loff = (Loff/align)*align + align;
      tsize = Loff + lsize;
      if (tsize % ASPALIGN) tsize = (tsize/ASPALIGN)*ASPALIGN + ASPALIGN;
      if (SAVESP >= 0)
      {
         PrintMajorComment(bbase, NULL, oldhead, 
     "To ensure greater alignment than sp, save old sp to stack and move sp");
         rsav = GetReg(T_INT);
         rsav = GetReg(T_INT);
         while (iparareg[rsav-IREGBEG]) rsav = GetReg(T_INT);
#ifdef X86_64
         assert(rsav <= NSIR);
#endif
         rsav = -rsav;
         InsNewInst(NULL, NULL, oldhead, MOV, rsav, -REG_SP, 0);
      }
      else
   #endif
   {
      PrintMajorComment(bbase, NULL, oldhead, "Adjust sp");
   }
   InsNewInst(NULL, NULL, oldhead, SUB, -REG_SP, -REG_SP,
              STiconstlookup(tsize)); 
   if (SAVESP >= 0)
   {
      i = const2shift(maxalign);
      assert(i >= 3);
      i = STiconstlookup(i);
      InsNewInst(NULL, NULL, oldhead, SHR, -REG_SP, -REG_SP, i);
      InsNewInst(NULL, NULL, oldhead, SHL, -REG_SP, -REG_SP, i);
      InsNewInst(NULL, NULL, oldhead, ST, AddDerefEntry(-REG_SP, 0, 0, SAVESP),
                 rsav, 0);
   }
   PrintMajorComment(bbase, NULL, oldhead, "Save registers");
   fprintf(stderr, "Local offset=%d\n", Loff);
   CorrectLocalOffsets(Loff);
   CorrectParamDerefs(ParaDerefQ, rsav ? rsav : -REG_SP, 
                      rsav ? Aoff : tsize+Aoff);
/*
 * Insert insts in header to save callee-saved registers
 */
   for (i=0; i < ndr; i++)
      InsNewInst(NULL, NULL, oldhead, FSTD,
                 AddDerefEntry(-REG_SP, 0, 0, Soff+i*8), -dr[i], 0);
   for (i=0; i < nir; i++)
      InsNewInst(NULL, NULL, oldhead, ST,
                 AddDerefEntry(-REG_SP, 0, 0, Soff+ndr*8+i*ISIZE), -ir[i], 0);
   for (i=0; i < nfr; i++)
      InsNewInst(NULL, NULL, oldhead, FST,
                 AddDerefEntry(-REG_SP, 0, 0, Soff+ndr*8+nir*ISIZE+i*4),
                 -fr[i], 0);
   GetReg(-1);
   FinalizeEpilogue(bbase, tsize, Soff, SAVESP, nir, ir, nfr, fr, ndr, dr);
}

void CreatePrologue(BBLOCK *bbase)
{
   short prog, k;
   int i;
   INSTQ *ip, *oldhead;
   BBLOCK *bp;

   if (!bbase) return;
/*
 * If we return values in a register, no need to save and restore it
 */
   oldhead = bbase->inst1;
   for (bp=bbase; bp->down; bp = bp->down);
/* 
 * Put routine name label
 */
   prog = STlabellookup(rout_name);
fprintf(stderr, "prog=%d!, rout_name=%s\n", prog, rout_name);
   STflag[prog-1] |= GLOB_BIT;
   InsNewInst(NULL, NULL, oldhead, LABEL, prog, 0, 0);

   InsNewInst(NULL, NULL, oldhead, CMPFLAG, CF_REGSAVE, 0, 0);
   Extern2Local(oldhead);
   InsNewInst(NULL, NULL, oldhead, COMMENT, 0, 0, 0);
   InsNewInst(NULL, NULL, oldhead, COMMENT, 
              STstrconstlookup("END OF FUNCTION PROLOGUE"), 0, 0);
   InsNewInst(NULL, NULL, oldhead, COMMENT, 0, 0, 0);
}
void KillUnusedLocals()  /* HERE, HERE: move to symtab */
{
}

void GenPrologueEpilogueStubs(BBLOCK *bbase)
/*
 * Create partially qualified local and para derefs, and generate function
 * prologue/epilogue stubs
 */
{
   BBLOCK *blk;
   int use=0;

   MarkUnusedLocals(bbase); 
   CreateSysLocals();
   NumberLocalsByType();
   #ifdef X86_64
      UpdateLocalDerefs(8);
   #else
      UpdateLocalDerefs(4);
   #endif
   CreatePrologue(bbase);
   GetReg(-1);
   for (blk=bbase; blk->down; blk = blk->down);

   blk->down = NewBasicBlock(blk, NULL);
   blk = blk->down;
   InsNewInst(blk, NULL, NULL, LABEL, STlabellookup("_IFKO_EPILOGUE"), 
              0, 0);
   if (rout_flag & IRET_BIT)
      use = -IRETREG;
   else if (rout_flag & FRET_BIT)
      use = -FRETREG;
   else if (rout_flag & DRET_BIT)
      use = -DRETREG;
   InsNewInst(blk, NULL, NULL, CMPFLAG, CF_REGRESTORE, 0, 0);
   InsNewInst(blk, NULL, NULL, RET, 0, use, 0);
}
