#include "ifko.h"

BLIST *DupBlockList(BLIST *scope, int ivscope)
/*
 * This function duplicates all block info for scope.  CF in duped code
 * that links to blocks duped scope is changed to reference duped code, while
 * links to code outside the duped area are left as is.
 * RETURNS: new block list of duplicated blocks
 * NOTE: all use/set info is left NULL.  
 */
{
   BLIST *bl, *lbase=NULL, *lp;
   BBLOCK *nb, *ob;
   INSTQ *ip;

   for (bl=scope; bl; bl = bl->next)
   {
      ob = bl->blk;
      nb = NewBasicBlock(NULL, NULL);
      lbase = AddBlockToList(lbase, nb);
      nb->bnum = ob->bnum;
      nb->ilab = ob->ilab;
      for (ip=ob->inst1; ip; ip = ip->next)
         InsNewInst(nb, NULL, NULL, ip->inst[0], ip->inst[1],
            ip->inst[2], ip->inst[3]);
   }
/* 
 * Things that are filled in on second pass:
 * nb->[up,down,usucc,csucc,preds]
 */
   for (bl=scope; bl; bl = bl->next)
   {
      ob = bl->blk;
      nb = FindBlockInListByNumber(lbase, ob->bnum);
      assert(nb);
      if (ob->up && BitVecCheck(ivscope, ob->up->bnum-1))
         nb->up = FindBlockInListByNumber(lbase, ob->up->bnum);
      else
         nb->up = ob->up;
      if (ob->down && BitVecCheck(ivscope, ob->down->bnum-1))
         nb->down = FindBlockInListByNumber(lbase, ob->down->bnum);
      else
         nb->down = ob->down;
      if (ob->usucc && BitVecCheck(ivscope, ob->usucc->bnum-1))
         nb->usucc = FindBlockInListByNumber(lbase, ob->usucc->bnum);
      else
         nb->usucc = ob->usucc;
      if (ob->csucc && BitVecCheck(ivscope, ob->csucc->bnum-1))
         nb->csucc = FindBlockInListByNumber(lbase, ob->csucc->bnum);
      else
         nb->csucc = ob->csucc;
      for (lp=bl->blk->preds; lp; lp = lp->next)
      {
         if (BitVecCheck(ivscope, lp->blk->bnum-1))
            nb->preds = AddBlockToList(nb->preds, 
                           FindBlockInListByNumber(lbase, lp->blk->bnum-1));
      }
   }
/* 
 * Things that are left alone:
 * nb->[dom,uses,defs,ins,outs,conin,conout,ignodes,loopq]
 */
   return(lbase);
}

void IndividualizeDuplicatedBlockList(int ndup, BLIST *scope)
/*
 * This function individualizes duped code by finding all labels, and prefacing
 * them with _CD<ndup> to make them unique.  It then finds all references to
 * former label group, and changes them to the new labels
 */
{
   struct locinit *lbase=NULL, *lp;
   INSTQ *ip;
   BLIST *bl;
   char *sp;
   char ln[256];
   short k, op1, op2, op3;

/*
 * Find all labels in block, and change their names
 */
   for (bl=scope; bl; bl = bl->next)
   {
      if (bl->blk->ilab)
      {
         assert(bl->blk->inst1->inst[0] == LABEL && 
                bl->blk->inst1->inst[1] == bl->blk->ilab);
         sp = STname[bl->blk->ilab-1];
/*
 *       Need to increase ndup, not add whole prefix
 */
         if (!strncmp(sp, "_IFKOCD", 7)  && isdigit(sp[7]))
         {
            sp += 7;
            while(*sp && *sp != '_') sp++;
            assert(*sp == '_' && sp[1]);
            sp++;
         }
         sprintf(ln, "_IFKOCD%d_%s", ndup, sp);
         k = STlabellookup(ln);
         lbase = NewLocinit(bl->blk->ilab, k, lbase);
         bl->blk->inst1->inst[1] = bl->blk->ilab = k;
      }
   }
/*
 * Find all refs in block to old labels, and change them to new labels
 */
   for (bl=scope; bl; bl = bl->next)
   {
      for (ip=bl->blk->ainst1; ip != bl->blk->ainstN; ip = ip->next)
      {
         if (ACTIVE_INST(ip->inst[0]))
         {
            op1 = ip->inst[1];
            op2 = ip->inst[2];
            op3 = ip->inst[3];
            if (op1 > 0 || op2 > 0 || op3 > 0)
            {
               for (lp=lbase; lp; lp = lp->next)
               {
                  if (op1 == lp->id)
                     ip->inst[1] = lp->con;
                  if (op2 == lp->id)
                     ip->inst[2] = lp->con;
                  if (op3 == lp->id)
                     ip->inst[3] = lp->con;
               }
            }
         }
      }
   }
   KillAllLocinit(lbase);
}

ILIST *FindIndexRef(BLIST *scope, short I)
/*
 * Finds all LDs from I in given scope.
 * This means all refs of I in code that is straight from h2l.
 */
{
   BLIST *bl;
   INSTQ *ip;
   ILIST *ilbase=NULL;
   for (bl=scope; bl; bl->next)
   {
      for (ip=bl->blk->ainstN; ip; ip = ip->prev)
      {
         if (ip->inst[0] == LD && ip->inst[2] == I)
            ilbase = NewIlist(ip, ilbase);
      }
   }
   return(ilbase);
}

struct ptrinfo *FindMovingPointers(BLIST *scope)
/*
 * Finds pointers that are inc/decremented in scope
 * INFO: where it is inc/dec, #of times inc/dec, whether inc by constant or var
 *       whether inc is contiguous, whether inc or dec
 * NOTE: assumes structure of code generated by HandlePtrArith(), so must be
 *       run before any code transforms.
 */
{
   struct ptrinfo *pbase=NULL, *p;
   BLIST *bl;
   INSTQ *ip;
   short k, i, j;
   int flag;
   for (bl=scope; bl; bl->next)
   {
      for (ip=bl->blk->ainstN; ip; ip = ip->prev)
      {
/*
 *       Look for a store to a pointer, and then see how the pointer was
 *       changed to cause the store
 */
         if (ip->inst[0] == ST)
         {
            k = ip->inst[1]-1;
            flag = STflag[k];
            if (IS_PTR(flag))
            {
               #if IFKO_DEBUG_LEVEL >= 1
                  assert(ip->prev);
                  assert(ip->prev->inst[0] == ADD || ip->prev->inst[0] == SUB);
                  assert(ip->prev->inst[1] == ip->inst[2]);
               #endif
               p = FindPtrinfo(pbase, k+1);
               if (!p)
               {
                  pbase = p = NewPtrinfo(k+1, 0, pbase);
                  p->nupdate = 1;
                  if (ip->prev->inst[0] == ADD)
                     p->flag |= PTRF_INC;
                  if (ip->inst[2] == ip->prev->inst[2])
                  {
                     j = ip->prev->inst[3];
                     if (j > 0 && IS_CONST(STflag[j-1]))
                     {
                        p->flag |= PTRF_CONSTINC;
                        if (SToff[j-1].i == type2len(FLAG2TYPE(flag)))
                           p->flag |= PTRF_CONTIG;
                     }
                  }
               }
               else
               {
                  p->nupdate++;
                  p->flag = 0;
               }
            }
         }
      }
   }
   return(pbase);
}

INSTQ *FindCompilerFlag(BBLOCK *bp, short flag)
{
   INSTQ *iret=NULL, *ip;
   for (ip=bp->inst1; ip; ip = ip->next)
      if (ip->inst[0] == CMPFLAG && ip->inst[1] == flag)
         break;
   return(ip);
}

void KillLoopControl(LOOPQ *lp)
/*
 * This function deletes all the loop control information from loop lp
 * (set, increment & test of index and goto top of loop body)
 */
{
   INSTQ *ip;
   BLIST *bl;

   if (!lp) return;
/*
 * Delete index init that must be in preheader
 */
   ip = FindCompilerFlag(lp->preheader, CF_LOOP_INIT);
   #if IFKO_DEBUG_LEVEL >= 1
      assert(ip);
   #endif
   ip = ip->next;
   while (ip && (ip->inst[0] != CMPFLAG || ip->inst[1] != CF_LOOP_BODY))
      ip = DelInst(ip);
   #if IFKO_DEBUG_LEVEL >= 1
      assert(ip);
   #endif
/*
 * Delete index update, test and branch that must be in all tails
 */
   for (bl=lp->tails; bl; bl = bl->next)
   {
      ip = FindCompilerFlag(bl->blk, CF_LOOP_UPDATE);
      #if IFKO_DEBUG_LEVEL >= 1
         assert(ip);
      #endif
      ip = ip->next;
      while (ip && (ip->inst[0] != CMPFLAG || ip->inst[1] != CF_LOOP_END))
      {
         if (ip->inst[0] != CMPFLAG || ip->inst[1] != CF_LOOP_TEST)
            ip = DelInst(ip);
         else
            ip = ip->next;
      }
      #if IFKO_DEBUG_LEVEL >= 1
         assert(ip);
      #endif
   }
}

static void ForwardLoop(LOOPQ *lp, INSTQ **ipinit, INSTQ **ipupdate,
                        INSTQ **iptest)
/*
 * This loop only used when index value is referenced inside loop.
 */
{
   short r0, r1;
   INSTQ *ip;

   r0 = GetReg(T_INT);
   r1 = GetReg(T_INT);
   if (IS_CONST(STflag[lp->beg-1]))
      *ipinit = ip = NewInst(NULL, NULL, NULL, MOV, -r0, lp->beg, 0);
   else
      *ipinit = ip = NewInst(NULL, NULL, NULL, LD, -r0, lp->beg, 0);
   ip->next = NewInst(NULL, NULL, NULL, ST, lp->I, -r0, 0);
   *ipupdate = ip = NewInst(NULL, NULL, NULL, LD, -r0, lp->I, 0);
   ip = ip->next;
   if (IS_CONST(STflag[lp->inc-1]))
      ip->next = NewInst(NULL, NULL, NULL, ADD, -r0, -r0, lp->inc);
   else
   {
      ip->next = NewInst(NULL, NULL, NULL, LD, -r1, lp->inc, 0);
      ip = ip->next;
      ip->next = NewInst(NULL, NULL, NULL, ADD, -r0, -r0, -r1);
   }
   *iptest = ip = NewInst(NULL, NULL, NULL, LD, -r0, lp->I, 0);
   if (IS_CONST(STflag[lp->end-1]))
      ip->next = NewInst(NULL, NULL, NULL, CMP, -ICC0, -r0, lp->end);
   else
   {
      ip->next = NewInst(NULL, NULL, NULL, LD, -r1, lp->end, 0);
      ip = ip->next;
      ip->next = NewInst(NULL, NULL, NULL, CMP, -ICC0, -r0, -r1);
   }
   ip = ip->next;
   if (lp->flag & L_MINC_BIT)
      InsNewInst(NULL, NULL, NULL, JNE, -PCREG, -ICC0, lp->body_label);
   else if (lp->flag & L_NINC_BIT)
      InsNewInst(NULL, NULL, NULL, JGT, -PCREG, -ICC0, lp->body_label);
   else
      InsNewInst(NULL, NULL, NULL, JLT, -PCREG, -ICC0, lp->body_label);

}

static void SimpleLC(short I, short I0, short N, short inc, short lab,
                     INSTQ **ipinit, INSTQ **ipupdate, INSTQ **iptest)
/*
 * Do simple N..0 loop.
 * NOTE: later specialize to use loop reg on PPC
 *       Assumes inc = 1
 */
{
   INSTQ *ip;
   short r0, r1;

   r0 = GetReg(T_INT);
   r1 = GetReg(T_INT);
   if (IS_CONST(STflag[N-1]) && IS_CONST(STflag[I0-1]))
   {
      *ipinit = ip = NewInst(NULL, NULL, NULL, MOV, -r0, 
                             STiconstlookup(SToff[N-1].i - SToff[I0-1].i), 0);
   }
   else
   {
      if (IS_CONST(STflag[I0-1]))
      {
         *ipinit = ip = NewInst(NULL, NULL, NULL, LD, -r0, N, 0);
         ip->next = NewInst(NULL, NULL, NULL, SUB, -r0, -r0, I0);
      }
      else if (IS_CONST(STflag[N-1]))
      {
         *ipinit = ip = NewInst(NULL, NULL, NULL, LD, -r1, I0, 0);
         ip->next = NewInst(NULL, NULL, NULL, MOV, -r0, N, 0);
         ip = ip->next;
         ip->next = NewInst(NULL, NULL, NULL, SUB, -r0, -r0, -r1);
      }
      else
      {
         *ipinit = ip = NewInst(NULL, NULL, NULL, LD, -r0, N, 0);
         ip->next = NewInst(NULL, NULL, NULL, LD, -r1, I0, 0);
         ip = ip->next;
         ip->next = NewInst(NULL, NULL, NULL, SUB, -r0, -r0, -r1);
      }
      ip = ip->next;
   }
   ip->next = NewInst(NULL, ip, NULL, ST, SToff[I-1].sa[2], -r0, 0);
   *ipupdate = ip = NewInst(NULL, NULL, NULL, LD, -r0, I, 0);
   ip->next = NewInst(NULL, ip, NULL, SUBCC, -r0, -r0, inc);
   *iptest = ip = NewInst(NULL, NULL, NULL, JNE, -PCREG, -ICC0, lab);
   GetReg(-1);
}

int OptimizeLoopControl(LOOPQ *lp, int unroll)
/*
 * attempts to generate optimized loop control for the given loop
 * NOTE: if blind unrolling has been applied, expect that loop counter
 *       is incremented in duplicated bodies, and not in last dup, so
 *       we would call this routine with unroll=1
 */
{
   INSTQ *ipinit, *ipupdate, *iptest;
   int I, beg, end, inc;
   int CHANGE=0;

   if (unroll <= 1) unroll = 1;
/*
 * If I is not referenced in loop, we can do simple N..0 iteration
 */

   I = 0;
   if (!(lp->flag & L_IREF_BIT))
   {
      if (IS_CONST(STflag[lp->inc-1]))
      {
         if (SToff[lp->inc-1].i == 1)
         {
            I = lp->I;
            beg = lp->beg;
            end = lp->end;
         }
         else if (IS_CONST(STflag[lp->end-1]) && IS_CONST(STflag[lp->beg-1]))
         {
            beg = 0;
            end = STiconstlookup((SToff[lp->end-1].i-SToff[lp->beg-1].i) / 
                                 SToff[lp->inc-1].i);
            I = lp->I;
         }
      }
   }
   if (I)
   {
      KillLoopControl(lp);
      SimpleLC(I, beg, end, lp->inc, lp->body_label, &ipinit, &ipupdate, &iptest);
      CHANGE = 1;
   }
   return(CHANGE);
}
