/*	$OpenBSD: rf_pqdegdags.c,v 1.1 1999/01/11 14:29:40 niklas Exp $	*/
/*	$NetBSD: rf_pqdegdags.c,v 1.1 1998/11/13 04:20:32 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * rf_pqdegdags.c
 * Degraded mode dags for double fault cases. 
*/

/*
 * :  
 * Log: rf_pqdegdags.c,v 
 * Revision 1.31  1996/11/05 21:10:40  jimz
 * failed pda generalization
 *
 * Revision 1.30  1996/07/31  16:30:05  jimz
 * asm/asmap fix
 *
 * Revision 1.29  1996/07/31  15:35:15  jimz
 * evenodd changes; bugfixes for double-degraded archs, generalize
 * some formerly PQ-only functions
 *
 * Revision 1.28  1996/07/28  20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.27  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.26  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.25  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.24  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.23  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.22  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.21  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.20  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.19  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.18  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.17  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.16  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.15  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.14  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.13  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.12  1996/05/08  21:01:24  jimz
 * fixed up enum type names that were conflicting with other
 * enums and function names (ie, "panic")
 * future naming trends will be towards RF_ and rf_ for
 * everything raidframe-related
 *
 * Revision 1.11  1996/05/03  19:47:50  wvcii
 * removed include of rf_redstripe.h
 *
 * Revision 1.10  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.9  1995/11/30  16:17:57  wvcii
 * added copyright info
 *
 * Revision 1.8  1995/11/07  15:33:25  wvcii
 * dag creation routines now generate term node
 * added asserts
 * encoded commit point nodes, antecedence types into dags
 * didn't add commit barrier - the code is a mess and needs to
 * be cleand up first
 *
 */

#include "rf_archs.h"

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagfuncs.h"
#include "rf_dagutils.h"
#include "rf_etimer.h"
#include "rf_acctrace.h"
#include "rf_general.h"
#include "rf_pqdegdags.h"
#include "rf_pq.h"
#include "rf_sys.h"

static void applyPDA(RF_Raid_t *raidPtr, RF_PhysDiskAddr_t *pda, RF_PhysDiskAddr_t *ppda,
	RF_PhysDiskAddr_t *qpda, void *bp);

/*
   Two data drives have failed, and we are doing a read that covers one of them. 
   We may also be reading some of the surviving drives. 
   

 *****************************************************************************************
 *
 * creates a DAG to perform a degraded-mode read of data within one stripe.
 * This DAG is as follows:
 *
 *                                      Hdr
 *                                       |
 *                                     Block
 *                       /         /           \         \     \   \
 *                      Rud  ...  Rud         Rrd  ...  Rrd    Rp  Rq
 *                      | \       | \         | \       | \    | \ | \
 *
 *                                 |                 |
 *                              Unblock              X
 *                                  \               /
 *                                   ------ T ------
 *
 * Each R node is a successor of the L node
 * One successor arc from each R node goes to U, and the other to X
 * There is one Rud for each chunk of surviving user data requested by the user,
 * and one Rrd for each chunk of surviving user data _not_ being read by the user
 * R = read, ud = user data, rd = recovery (surviving) data, p = P data, q = Qdata
 * X = pq recovery node, T = terminate
 *
 * The block & unblock nodes are leftovers from a previous version.  They
 * do nothing, but I haven't deleted them because it would be a tremendous
 * effort to put them back in.
 *
 * Note:  The target buffer for the XOR node is set to the actual user buffer where the
 * failed data is supposed to end up.  This buffer is zero'd by the code here.  Thus,
 * if you create a degraded read dag, use it, and then re-use, you have to be sure to
 * zero the target buffer prior to the re-use.
 *
 * Every buffer read is passed to the pq recovery node, whose job it is to sort out whats
 * needs and what's not.
 ****************************************************************************************/
/*   init a disk node with 2 successors and one predecessor */
#define INIT_DISK_NODE(node,name) \
rf_InitNode(node, rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc, rf_GenericWakeupFunc, 2,1,4,0, dag_h, name, allocList); \
(node)->succedents[0] = unblockNode; \
(node)->succedents[1] = recoveryNode; \
(node)->antecedents[0] = blockNode; \
(node)->antType[0] = rf_control

#define DISK_NODE_PARAMS(_node_,_p_) \
  (_node_).params[0].p = _p_ ; \
  (_node_).params[1].p = (_p_)->bufPtr; \
  (_node_).params[2].v = parityStripeID; \
  (_node_).params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru)

#define DISK_NODE_PDA(node)  ((node)->params[0].p)

RF_CREATE_DAG_FUNC_DECL(rf_PQ_DoubleDegRead)
{
  rf_DoubleDegRead(raidPtr, asmap, dag_h, bp, flags, allocList,
    "Rq", "PQ Recovery", rf_PQDoubleRecoveryFunc);
}
  
static void applyPDA(raidPtr,pda,ppda,qpda, bp)
  RF_Raid_t          *raidPtr;
  RF_PhysDiskAddr_t  *pda;
  RF_PhysDiskAddr_t  *ppda;
  RF_PhysDiskAddr_t  *qpda;
  void               *bp;
{
  RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
  RF_RaidAddr_t s0off = rf_StripeUnitOffset(layoutPtr, ppda->startSector);
  RF_SectorCount_t s0len = ppda->numSector, len;
  RF_SectorNum_t suoffset;
  unsigned coeff;
  char *pbuf = ppda->bufPtr;
  char *qbuf = qpda->bufPtr;
  char *buf;
  int delta;

  suoffset = rf_StripeUnitOffset(layoutPtr, pda->startSector);
  len = pda->numSector;
  /* see if pda intersects a recovery pda */
  if ((suoffset < s0off+s0len) && ( suoffset+len > s0off))
    {
      buf = pda->bufPtr;
      coeff = rf_RaidAddressToStripeUnitID(&(raidPtr->Layout),pda->raidAddress);
      coeff = (coeff % raidPtr->Layout.numDataCol);

      if (suoffset < s0off)
	{
	  delta = s0off - suoffset;
	  buf += rf_RaidAddressToStripeUnitID(&(raidPtr->Layout),delta);
	  suoffset = s0off;
	  len -= delta;
	}
      if (suoffset > s0off)
	{
	  delta = suoffset - s0off;
	  pbuf += rf_RaidAddressToStripeUnitID(&(raidPtr->Layout),delta);
	  qbuf += rf_RaidAddressToStripeUnitID(&(raidPtr->Layout),delta);
	}
      if ((suoffset + len) > (s0len + s0off))
	len = s0len + s0off - suoffset;

      /* src, dest, len */
      rf_bxor(buf,pbuf,rf_RaidAddressToByte(raidPtr,len), bp);
	  
      /* dest, src, len, coeff */
      rf_IncQ((unsigned long *)qbuf,(unsigned long *)buf,rf_RaidAddressToByte(raidPtr,len),coeff);
    }
}
/*
   Recover data in the case of a double failure. There can be two
   result buffers, one for each chunk of data trying to be recovered.
   The params are pda's that have not been range restricted or otherwise
   politely massaged - this should be done here. The last params are the
   pdas of P and Q, followed by the raidPtr. The list can look like

   pda, pda, ... , p pda, q pda, raidptr, asm
   
   or

   pda, pda, ... , p_1 pda, p_2 pda, q_1 pda, q_2 pda, raidptr, asm

   depending on wether two chunks of recovery data were required.

   The second condition only arises if there are two failed buffers
   whose lengths do not add up a stripe unit.
*/


int rf_PQDoubleRecoveryFunc(node)
  RF_DagNode_t  *node;
{
  int np = node->numParams;
  RF_AccessStripeMap_t *asmap = (RF_AccessStripeMap_t *) node->params[np-1].p;
  RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[np-2].p;
  RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) &(raidPtr->Layout);
  int d, i;
  unsigned coeff;
  RF_RaidAddr_t sosAddr, suoffset;
  RF_SectorCount_t len, secPerSU = layoutPtr->sectorsPerStripeUnit;
  int two = 0;
  RF_PhysDiskAddr_t *ppda,*ppda2,*qpda,*qpda2,*pda,npda;
  char *buf;
  int numDataCol = layoutPtr->numDataCol;
  RF_Etimer_t timer;
  RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;

  RF_ETIMER_START(timer);

  if (asmap->failedPDAs[1] && 
      (asmap->failedPDAs[1]->numSector + asmap->failedPDAs[0]->numSector < secPerSU))
    {
      RF_ASSERT(0);
      ppda  = node->params[np-6].p;
      ppda2 = node->params[np-5].p;
      qpda  = node->params[np-4].p;
      qpda2 = node->params[np-3].p;
      d = (np-6);
      two = 1;
    }
  else
    {
      ppda = node->params[np-4].p;
      qpda = node->params[np-3].p;
      d = (np-4);
    }

  for (i=0; i < d; i++)
    {
      pda = node->params[i].p;
      buf = pda->bufPtr;
      suoffset = rf_StripeUnitOffset(layoutPtr, pda->startSector);
      len = pda->numSector;
      coeff = rf_RaidAddressToStripeUnitID(layoutPtr,pda->raidAddress);
      /* compute the data unit offset within the column */
      coeff = (coeff % raidPtr->Layout.numDataCol);
      /* see if pda intersects a recovery pda */
      applyPDA(raidPtr,pda,ppda,qpda,node->dagHdr->bp);
      if (two)
	applyPDA(raidPtr,pda,ppda,qpda,node->dagHdr->bp);
    }

  /* ok, we got the parity back to the point where we can recover.
     We now need to determine the coeff of the columns that need to be
     recovered. We can also only need to recover a single stripe unit.
     */
  
  if (asmap->failedPDAs[1] == NULL)
    { /* only a single stripe unit to recover. */
      pda = asmap->failedPDAs[0];
      sosAddr      = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
      /* need to determine the column of the other failed disk */
      coeff = rf_RaidAddressToStripeUnitID(layoutPtr,pda->raidAddress);
      /* compute the data unit offset within the column */
      coeff = (coeff % raidPtr->Layout.numDataCol);
      for (i=0; i < numDataCol; i++)
	{
	  npda.raidAddress = sosAddr + (i * secPerSU);
	  (raidPtr->Layout.map->MapSector)(raidPtr,npda.raidAddress, &(npda.row), &(npda.col), &(npda.startSector), 0);
	  /* skip over dead disks */
	  if (RF_DEAD_DISK(raidPtr->Disks[npda.row][npda.col].status))
	    if (i != coeff) break;
	}
      RF_ASSERT (i < numDataCol);
      RF_ASSERT (two==0);
      /* recover the data. Since we need only want to recover one column, we overwrite the
	 parity with the other one. */
      if (coeff < i) /* recovering 'a' */
	rf_PQ_recover((unsigned long *)ppda->bufPtr,(unsigned long *)qpda->bufPtr,(unsigned long *)pda->bufPtr,(unsigned long *)ppda->bufPtr,rf_RaidAddressToByte(raidPtr,pda->numSector), coeff, i);
      else /* recovering 'b' */
	rf_PQ_recover((unsigned long *)ppda->bufPtr,(unsigned long *)qpda->bufPtr,(unsigned long *)ppda->bufPtr,(unsigned long *)pda->bufPtr,rf_RaidAddressToByte(raidPtr,pda->numSector), i, coeff);
    }
  else
    RF_PANIC();

  RF_ETIMER_STOP(timer);
  RF_ETIMER_EVAL(timer);
  if (tracerec)
    tracerec->q_us += RF_ETIMER_VAL_US(timer);
  rf_GenericWakeupFunc(node,0);
  return(0);
}

int rf_PQWriteDoubleRecoveryFunc(node)
  RF_DagNode_t  *node;
{
  /* The situation:

         We are doing a write that hits only one
	 failed data unit.
	 The other failed data unit is not being overwritten, so
	 we need to generate it.
	 
	 For the moment, we assume all the nonfailed data being
	 written is in the shadow of the failed data unit.
	 (i.e,, either a single data unit write or the entire
	 failed stripe unit is being overwritten. )
	 
	 Recovery strategy: 
	     apply the recovery data to the parity and q.
	     Use P & Q to recover the second failed data unit in P.
	     Zero fill Q, then apply the recovered data to p.
	     Then apply the data being written to the failed drive.
	     Then walk through the surviving drives, applying new data
	     when it exists, othewise the recovery data. Quite a mess.


	The params

	read pda0, read pda1, ... read pda (numDataCol-3), 
	write pda0, ... , write pda (numStripeUnitAccess - numDataFailed),
	failed pda, raidPtr, asmap
   */

  int np = node->numParams;
  RF_AccessStripeMap_t *asmap = (RF_AccessStripeMap_t *) node->params[np-1].p;
  RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[np-2].p;
  RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) &(raidPtr->Layout);
  int i;
  RF_RaidAddr_t sosAddr;
  unsigned coeff;
  RF_StripeCount_t secPerSU = layoutPtr->sectorsPerStripeUnit;
  RF_PhysDiskAddr_t *ppda,*qpda,*pda,npda;
  int numDataCol = layoutPtr->numDataCol;
  RF_Etimer_t timer;
  RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;

  RF_ASSERT(node->numResults == 2);
  RF_ASSERT(asmap->failedPDAs[1] == NULL);
  RF_ETIMER_START(timer);
  ppda = node->results[0];
  qpda = node->results[1];
  /* apply the recovery data */
  for (i=0; i < numDataCol-2; i++)
    applyPDA(raidPtr,node->params[i].p,ppda,qpda, node->dagHdr->bp);

  /* determine the other failed data unit */
  pda = asmap->failedPDAs[0];
  sosAddr      = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
  /* need to determine the column of the other failed disk */
  coeff = rf_RaidAddressToStripeUnitID(layoutPtr,pda->raidAddress);
  /* compute the data unit offset within the column */
  coeff = (coeff % raidPtr->Layout.numDataCol);
  for (i=0; i < numDataCol; i++)
    {
      npda.raidAddress = sosAddr + (i * secPerSU);
      (raidPtr->Layout.map->MapSector)(raidPtr,npda.raidAddress, &(npda.row), &(npda.col), &(npda.startSector), 0);
      /* skip over dead disks */
      if (RF_DEAD_DISK(raidPtr->Disks[npda.row][npda.col].status))
	if (i != coeff) break;
    }
  RF_ASSERT (i < numDataCol);
  /* recover the data. The column we want to recover we write over the parity.
     The column we don't care about we dump in q. */
  if (coeff < i) /* recovering 'a' */
    rf_PQ_recover((unsigned long *)ppda->bufPtr,(unsigned long *)qpda->bufPtr,(unsigned long *)ppda->bufPtr,(unsigned long *)qpda->bufPtr,rf_RaidAddressToByte(raidPtr,pda->numSector), coeff, i);
  else /* recovering 'b' */
    rf_PQ_recover((unsigned long *)ppda->bufPtr,(unsigned long *)qpda->bufPtr,(unsigned long *)qpda->bufPtr,(unsigned long *)ppda->bufPtr,rf_RaidAddressToByte(raidPtr,pda->numSector), i, coeff);
  
  /* OK. The valid data is in P. Zero fill Q, then inc it into it. */
  bzero(qpda->bufPtr,rf_RaidAddressToByte(raidPtr,qpda->numSector));
  rf_IncQ((unsigned long *)qpda->bufPtr,(unsigned long *)ppda->bufPtr,rf_RaidAddressToByte(raidPtr,qpda->numSector),i);

  /* now apply all the write data to the buffer */
  /* single stripe unit write case: the failed data is only thing we are writing. */
  RF_ASSERT(asmap->numStripeUnitsAccessed == 1);
  /* dest, src, len, coeff */
  rf_IncQ((unsigned long *)qpda->bufPtr,(unsigned long *)asmap->failedPDAs[0]->bufPtr,rf_RaidAddressToByte(raidPtr,qpda->numSector),coeff);
  rf_bxor(asmap->failedPDAs[0]->bufPtr,ppda->bufPtr,rf_RaidAddressToByte(raidPtr,ppda->numSector),node->dagHdr->bp);
  
  /* now apply all the recovery data */
  for (i=0; i < numDataCol-2; i++)
    applyPDA(raidPtr,node->params[i].p,ppda,qpda, node->dagHdr->bp);

  RF_ETIMER_STOP(timer);
  RF_ETIMER_EVAL(timer);
  if (tracerec)
    tracerec->q_us += RF_ETIMER_VAL_US(timer);
	
  rf_GenericWakeupFunc(node,0);
  return(0);
}

RF_CREATE_DAG_FUNC_DECL(rf_PQ_DDLargeWrite)
{
  RF_PANIC();
}

/*
   Two lost data unit write case.

   There are really two cases here:

   (1) The write completely covers the two lost data units. 
       In that case, a reconstruct write that doesn't write the
       failed data units will do the correct thing. So in this case,
       the dag looks like

            full stripe read of surviving data units (not being overwriten)
	    write new data (ignoring failed units)   compute P&Q
	                                             write P&Q


   (2) The write does not completely cover both failed data units
       (but touches at least one of them). Then we need to do the 
       equivalent of a reconstruct read to recover the missing data
       unit from the other stripe. 
     
       For any data we are writing that is not in the "shadow"
       of the failed units, we need to do a four cycle update.
       PANIC on this case. for now

*/

RF_CREATE_DAG_FUNC_DECL(rf_PQ_200_CreateWriteDAG)
{
  RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
  RF_SectorCount_t sectorsPerSU = layoutPtr->sectorsPerStripeUnit;
  int sum;
  int nf = asmap->numDataFailed;

  sum = asmap->failedPDAs[0]->numSector;
  if (nf == 2)
    sum += asmap->failedPDAs[1]->numSector;

  if ((nf == 2) && ( sum == (2*sectorsPerSU)))
    {
      /* large write case */
      rf_PQ_DDLargeWrite(raidPtr, asmap, dag_h, bp, flags, allocList);
      return;
    }

  
  if ((nf == asmap->numStripeUnitsAccessed) || (sum >= sectorsPerSU))
    {
      /* small write case, no user data not in shadow */
      rf_PQ_DDSimpleSmallWrite(raidPtr, asmap, dag_h, bp, flags, allocList);
      return;
    }
  RF_PANIC();
}

RF_CREATE_DAG_FUNC_DECL(rf_PQ_DDSimpleSmallWrite)
{
  rf_DoubleDegSmallWrite(raidPtr, asmap, dag_h, bp, flags, allocList, "Rq", "Wq", "PQ Recovery", rf_PQWriteDoubleRecoveryFunc);
}

#endif /* (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0) */
