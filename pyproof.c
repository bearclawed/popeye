/********************* MODIFICATIONS to pyproof.c ***********************
 **
 ** Date       Who  What
 **
 ** 2006/05/17 SE   Changes to allow half-move specification for helpmates using 0.5 notation
 **                 Change for take&make
 **
 ** 2007/05/14 SE   Change for annan
 **
 ** 2008/01/01 SE   Bug fix: Circe Assassin + proof game (reported P.Raican)
 **
 ** 2008/01/01 SE   Bug fix: Circe Parrain + proof game (reported P.Raican)
 **
 **************************** End of List ******************************/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#if defined(__TURBOC__)
# include <mem.h>
#endif
#include "py.h"
#include "pyproc.h"
#include "pydata.h"
#include "pyhash.h"
#include "DHT/dhtbcmem.h"
#include "pyproof.h"
#include "pymsg.h"
#include "pyslice.h"
#include "pyint.h"
#include "platform/maxtime.h"
#include "trace.h"

/* an array to store the position */
static piece ProofPieces[32];
static square ProofSquares[32];
static int ProofNbrAllPieces;
static echiquier ProofBoard, PosA;
static square Proof_rb, Proof_rn, rbA, rnA;
static Flags ProofSpec[nr_squares_on_board], SpecA[nr_squares_on_board];
static imarr Proof_isquare;
static imarr isquareA;

static unsigned int xxxxx[fb+fb+1];
#define ProofNbrPiece (xxxxx+fb)

static int ProofNbrWhitePieces, ProofNbrBlackPieces;

static boolean BlockedBishopc1, BlockedBishopf1, BlockedQueend1,
  BlockedBishopc8, BlockedBishopf8, BlockedQueend8,
  CapturedBishopc1, CapturedBishopf1, CapturedQueend1,
  CapturedBishopc8, CapturedBishopf8, CapturedQueend8;

ProofImpossible_fct_t alternateImpossible;
static ProofImpossible_fct_t seriesImpossible;

void ProofEncode(void)
{
  HashBuffer *hb = &hashBuffers[nbply];
  byte    *position= hb->cmv.Data;
  byte    *bp= position+nr_rows_on_board;
  byte    pieces= 0;
  int       row, col;
  square a_square= square_a1;
  boolean even= false;

  /* detect cases where we encode the same position twice */
  assert(!isHashBufferValid[nbply]);

  /* clear the bits for storing the position of pieces */
  memset(position, 0, nr_rows_on_board);

  for (row=0; row<nr_rows_on_board; row++, a_square+= onerow)
  {
    square curr_square = a_square;
    for (col=0; col<nr_files_on_board; col++, curr_square+=dir_right)
    {
      piece p= e[curr_square];
      if (p!=vide) {
        if (even)
          *bp++ = pieces+(((byte)(p<vide ? 7-p : p))<<(CHAR_BIT/2));
        else
          pieces= (byte)(p<vide ? 7-p : p);
        even= !even;
        position[row] |= BIT(col);
      }
    }
  }

  if (even)
    *bp++ = pieces+(15<<(CHAR_BIT/2));

  *bp++ = castling_flag[nbply];

  if (CondFlag[duellist]) {
    *bp++ = (byte)(whduell[nbply] - square_a1);
    *bp++ = (byte)(blduell[nbply] - square_a1);
  }

  if (CondFlag[blfollow] || CondFlag[whfollow])
    *bp++ = (byte)(move_generation_stack[nbcou].departure - square_a1);

  if (ep[nbply])
    *bp++ = (byte)(ep[nbply] - square_a1);

  hb->cmv.Leng= bp - hb->cmv.Data;

  validateHashBuffer();
}

int proofwkm[square_h8+25-(square_a1-25)+1];
int proofbkm[square_h8+25-(square_a1-25)+1];

#define WhKingMoves  (proofwkm-(square_a1-25))
#define BlKingMoves  (proofbkm-(square_a1-25))

void ProofInitialiseKingMoves(square ProofRB, square ProofRN)
{
  square    *bnp, sq;
  numvec    k;
  int   MoveNbr;
  boolean   GoOn;

  /* set all squares to a maximum */
  for (bnp= boardnum; *bnp; bnp++)
  {
    WhKingMoves[*bnp] = slices[root_slice].u.branch.length;
    BlKingMoves[*bnp] = slices[root_slice].u.branch.length;
  }

  /* mark squares occupied or garded by immobile pawns
     white pawns
  */
  for (sq= square_a2; sq <= square_h2; sq++)
    if (ProofBoard[sq] == pb)
    {
      WhKingMoves[sq]= -1;
      BlKingMoves[sq]= -1;    /* blocked */
      if (eval_white == eval_ortho)
      {
        BlKingMoves[sq+dir_up+dir_left]= -2;
        BlKingMoves[sq+dir_up+dir_right]= -2; /* guarded */
      }
    }

  /* black pawns */
  for (sq= square_a7; sq <= square_h7; sq++)
    if (ProofBoard[sq] == pn)
    {
      BlKingMoves[sq]= -1;
      WhKingMoves[sq]= -1;    /* blocked */
      if (eval_black == eval_ortho)
      {
        WhKingMoves[sq+dir_down+dir_right]= -2;
        WhKingMoves[sq+dir_down+dir_left]= -2;    /* guarded */
      }
    }

  /* cornered bishops */
  if (BlockedBishopc1)
  {
    WhKingMoves[square_c1]= -1;
    BlKingMoves[square_c1]= -1; /* blocked */
  }
  if (BlockedBishopf1)
  {
    WhKingMoves[square_f1]= -1;
    BlKingMoves[square_f1]= -1; /* blocked */
  }
  if (BlockedBishopc8)
  {
    WhKingMoves[square_c8]= -1;
    BlKingMoves[square_c8]= -1; /* blocked */
  }
  if (BlockedBishopf8)
  {
    WhKingMoves[square_f8]= -1;
    BlKingMoves[square_f8]= -1; /* blocked */
  }

  /* initialise wh king */
  WhKingMoves[ProofRB]= 0;
  MoveNbr= 0;
  do
  {
    GoOn= false;
    for (bnp= boardnum; *bnp; bnp++)
    {
      if (WhKingMoves[*bnp] == MoveNbr)
      {
        for (k= vec_queen_end; k>=vec_queen_start; k--)
        {
          sq= *bnp+vec[k];
          if (WhKingMoves[sq] > MoveNbr)
          {
            WhKingMoves[sq]= MoveNbr+1;
            GoOn= true;
          }
          if (calc_whtrans_king) {
            sq= *bnp+vec[k];
            while (e[sq]!=obs && WhKingMoves[sq]!=-1)
            {
              if (WhKingMoves[sq] > MoveNbr)
              {
                WhKingMoves[sq]= MoveNbr+1;
                GoOn= true;
              }
              sq += vec[k];
            }
          } /* trans_king */
        }
        if (calc_whtrans_king)
          for (k= vec_knight_end; k>=vec_knight_start; k--)
          {
            sq= *bnp+vec[k];
            if (e[sq]!=obs && WhKingMoves[sq]>MoveNbr)
            {
              WhKingMoves[sq]= MoveNbr+1;
              GoOn= true;
            }
          }
      }
    }
    MoveNbr++;
  } while(GoOn);

  /* initialise blank king */
  BlKingMoves[ProofRN]= 0;
  MoveNbr= 0;
  do
  {
    GoOn= false;
    for (bnp= boardnum; *bnp; bnp++)
    {
      if (BlKingMoves[*bnp] == MoveNbr)
      {
        for (k= vec_queen_end; k>=vec_queen_start; k--)
        {
          sq= *bnp+vec[k];
          if (BlKingMoves[sq] > MoveNbr)
          {
            BlKingMoves[sq]= MoveNbr+1;
            GoOn= true;
          }
          if (calc_bltrans_king)
          {
            sq= *bnp+vec[k];
            while (e[sq]!=obs && BlKingMoves[sq]!=-1)
            {
              if (BlKingMoves[sq] > MoveNbr)
              {
                BlKingMoves[sq]= MoveNbr+1;
                GoOn= true;
              }
              sq += vec[k];
            }
          } /* trans_king */
        }
        if (calc_bltrans_king)
          for (k= vec_knight_end; k>=vec_knight_start; k--)
          {
            sq= *bnp+vec[k];
            if (e[sq]!=obs && BlKingMoves[sq]>MoveNbr)
            {
              BlKingMoves[sq]= MoveNbr+1;
              GoOn= true;
            }
          }
      }
    }

    MoveNbr++;
  } while(GoOn);
} /* ProofInitialiseKingMoves */

void ProofInitialiseIntelligent(void)
{
  int i;

  ProofNbrWhitePieces = 0;
  ProofNbrBlackPieces = 0;

  for (i = roib; i <= fb; i++)
  {
    ProofNbrWhitePieces += ProofNbrPiece[i];
    ProofNbrBlackPieces += ProofNbrPiece[-i];
  }

  /* determine pieces blocked */
  BlockedBishopc1 = ProofBoard[square_c1] == fb
    && ProofBoard[square_b2] == pb
    && ProofBoard[square_d2] == pb;

  BlockedBishopf1 = ProofBoard[square_f1] == fb
    && ProofBoard[square_e2] == pb
    && ProofBoard[square_g2] == pb;

  BlockedBishopc8 = ProofBoard[square_c8] == fn
    && ProofBoard[square_b7] == pn
    && ProofBoard[square_d7] == pn;

  BlockedBishopf8 = ProofBoard[square_f8] == fn
    && ProofBoard[square_e7] == pn
    && ProofBoard[square_g7] == pn;

  BlockedQueend1 = BlockedBishopc1
    && BlockedBishopf1
    && ProofBoard[square_d1] == db
    && ProofBoard[square_c2] == pb
    && ProofBoard[square_f2] == pb;

  BlockedQueend8 = BlockedBishopc8
    && BlockedBishopf8
    && ProofBoard[square_d8] == dn
    && ProofBoard[square_c7] == pn
    && ProofBoard[square_f7] == pn;

  /* determine pieces captured */
  CapturedBishopc1 = ProofBoard[square_c1] != fb
    && ProofBoard[square_b2] == pb
    && ProofBoard[square_d2] == pb;

  CapturedBishopf1 = ProofBoard[square_f1] != fb
    && ProofBoard[square_e2] == pb
    && ProofBoard[square_g2] == pb;

  CapturedBishopc8 = ProofBoard[square_c8] != fn
    && ProofBoard[square_b7] == pn
    && ProofBoard[square_d7] == pn;

  CapturedBishopf8 = ProofBoard[square_f8] != fn
    && ProofBoard[square_e7] == pn
    && ProofBoard[square_g7] == pn;

  CapturedQueend1 = BlockedBishopc1
    && BlockedBishopf1
    && ProofBoard[square_d1] != db
    && ProofBoard[square_c2] == pb
    && ProofBoard[square_f2] == pb;

  CapturedQueend8 = BlockedBishopc8
    && BlockedBishopf8
    && ProofBoard[square_d8] != dn
    && ProofBoard[square_c7] == pn
    && ProofBoard[square_f7] == pn;

  /* update castling possibilities */
  if (BlockedBishopc1)
    /* wh long castling impossible */
    CLRFLAGMASK(castling_flag[0],ra1_cancastle);

  if (BlockedBishopf1)
    /* wh short castling impossible */
    CLRFLAGMASK(castling_flag[0],rh1_cancastle);

  if (BlockedBishopc8)
    /* blank long castling impossible */
    CLRFLAGMASK(castling_flag[0],ra8_cancastle);

  if (BlockedBishopf8)
    /* blank short castling impossible */
    CLRFLAGMASK(castling_flag[0],rh8_cancastle);

  if (!TSTFLAGMASK(castling_flag[0],ra1_cancastle|rh1_cancastle))
    /* no wh rook can castle, so the wh king cannot either */
    CLRFLAGMASK(castling_flag[0],ke1_cancastle);

  if (!TSTFLAGMASK(castling_flag[0],ra8_cancastle|rh8_cancastle))
    /* no blank rook can castle, so the blank king cannot either */
    CLRFLAGMASK(castling_flag[0],ke8_cancastle);

  castling_flag[2] = castling_flag[1] = castling_flag[0];

  /* initialise king diff_move arrays */
  ProofInitialiseKingMoves(Proof_rb, Proof_rn);
} /* ProofInitialiseIntelligent */

void ProofSaveStartPosition(void)
{
  int i;
  for (i = maxsquare-1; i>=0; i--)
    PosA[i] = e[i];

  for (i = 0; i<nr_squares_on_board; i++)
    SpecA[i] = spec[boardnum[i]];

  rnA = rn;
  rbA = rb;

  for (i = 0; i<maxinum; i++)
    isquareA[i] = isquare[i];
}

static void ProofSaveTargetPosition(void)
{
  int       i;
  piece p;

  Proof_rb = rb;
  Proof_rn = rn;

  for (i = roib; i <= fb; i++)
  {
    ProofNbrPiece[i] = nbpiece[i];
    ProofNbrPiece[-i] = nbpiece[-i];
  }

  for (i = maxsquare - 1; i >= 0; i--)
    ProofBoard[i] = e[i];

  ProofNbrAllPieces = 0;

  for (i = 0; i < nr_squares_on_board; i++)
  {
    ProofSpec[i] = spec[boardnum[i]];
    /* in case continued twinning
     * to other than proof game
     */
    p = e[boardnum[i]];
    if (p != vide)
    {
      ProofPieces[ProofNbrAllPieces] = p;
      ProofSquares[ProofNbrAllPieces] = boardnum[i];
      ++ProofNbrAllPieces;
    }
  }

  if (CondFlag[imitators])
    for (i = 0; i < maxinum; i++)
      Proof_isquare[i] = isquare[i];
}

void ProofRestoreTargetPosition(void)
{
  int i;
  for (i = maxsquare-1; i>=0; i--)
    e[i] = ProofBoard[i];

  for (i= 0; i<nr_squares_on_board; i++)
    spec[boardnum[i]] = ProofSpec[i];

  rn = Proof_rn;
  rb = Proof_rb;
}

static void ProofRestoreStartPosition(void)
{
  slice_index const leaf_slice = 0;
  Goal const goal = slices[leaf_slice].u.leaf.goal;
  int i;

  assert(slices[root_slice].type==STBranchDirect
         || slices[root_slice].type==STBranchHelp
         || slices[root_slice].type==STBranchSeries);
  assert(leaf_slice==slices[root_slice].u.branch.next);
  assert(slices[leaf_slice].type==STLeafDirect
         || slices[leaf_slice].type==STLeafSelf
         || slices[leaf_slice].type==STLeafHelp);

  for (i = 0; i < nr_squares_on_board; i++)
  {
    piece p = goal==goal_atob ? PosA[boardnum[i]] : PAS[i];
    e[boardnum[i]] = p;
    CLEARFL(spec[boardnum[i]]);

    /* We must set spec[] for the PAS.
       This is used in jouecoup for andernachchess!*/
    if (p>=roib)
      SETFLAG(spec[boardnum[i]], White);
    else if (p<=roin)
      SETFLAG(spec[boardnum[i]], Black);
    if (goal==goal_atob)
      spec[boardnum[i]] = SpecA[i];
  }

  /* set the king squares */
  if (!CondFlag[losingchess])
  {
    if (goal==goal_atob)
    {
      rb = rbA;
      rn = rnA;
    }
    else
    {
      rb = square_e1;
      rn = square_e8;
    }
  }

  if (goal==goal_atob && CondFlag[imitators])
    for (i = 0; i < maxinum; i++)
      isquare[i] = isquareA[i];
}

/* a function to store the position and set the PAS */
void ProofInitialise(void)
{
  ProofSaveTargetPosition();
  ProofRestoreStartPosition();
} /* ProofInitialise */

void ProofAtoBWriteStartPosition(void)
{
  if (!OptFlag[noboard])
  {
    char InitialLine[40];
    sprintf(InitialLine,
            "Initial (%s ->):\n",
            PieSpString[ActLang][slice_get_starter(root_slice)]);
    StdString(InitialLine);
    WritePosition();
  }
}

/* function that compares the current position with the desired one
 * and returns true if they are identical. Otherwise it returns false.
 */
boolean ProofIdentical(void)
{
  int i;

  for (i = 0; i < ProofNbrAllPieces; i++)
    if (ProofPieces[i] != e[ProofSquares[i]])
      return false;

  for (i = roib; i <= fb; i++)
    if (ProofNbrPiece[i] != nbpiece[i]
        || ProofNbrPiece[-i] != nbpiece[-i])
      return false;

  if (CondFlag[imitators])
    for (i= 0; i < inum[nbply]; i++)
      if (Proof_isquare[i] != isquare[i])
        return false;

  return true;
}

int ProofKnightMoves[square_h8-square_a1+1]= {
  /*   1-  7 */     0,  3,  2,  3,  2,  3,  4,  5,
  /* dummies  8- 16 */ -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /*  17- 31*/      4,  3,  4,  3,  2,  1,  2,  3,  2, 1, 2, 3, 4, 3, 4,
  /* dummies 32- 40 */ -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /*  41- 55 */     5,  4,  3,  2,  3,  4,  1,  2,  1, 4, 3, 2, 3, 4, 5,
  /* dummies 56- 64 */ -1, -1, -1, -1, -1, -1, -1, -1, -1,
  /*  65- 79*/      4,  3,  4,  3,  2,  3,  2,  3,  2, 3, 2, 3, 4, 3, 4,
  /* dummies 80- 88 */ -1, -1, -1, -1, -1, -1, -1, -1,-1,
  /*  89-103 */     5,  4,  3,  4,  3,  2,  3,  2,  3, 2, 3, 4, 3, 4, 5,
  /* dummies104-112 */ -1, -1, -1, -1, -1, -1, -1, -1,-1,
  /* 113-127 */     4,  5,  4,  3,  4,  3,  4,  3,  4, 3, 4, 3, 4, 5, 4,
  /* dummies128-136 */ -1, -1, -1, -1, -1, -1, -1, -1,-1,
  /* 137-151 */     5,  4,  5,  4,  3,  4,  3,  4,  3, 4, 3, 4, 5, 4, 5,
  /* dummies152-160 */ -1, -1, -1, -1, -1, -1, -1, -1,-1,
  /* 161-175 */     6,  5,  4,  5,  4,  5,  4,  5,  4, 5, 4, 5, 4, 5, 6
};

static int ProofBlKingMovesNeeded(void)
{
  int   cast;
  int   needed= BlKingMoves[rn];

  if (rn==initsquare)
    /* no king in play, or king can be created by promotion
     * -> no optimisation possible */
    return 0;

  if (TSTFLAGMASK(castling_flag[nbply],ke8_cancastle))
  {
    if (TSTFLAGMASK(castling_flag[nbply],ra8_cancastle))
    {
      /* blank long castling */
      /* BlKingMoves is the number of moves the blank king
         still needs after castling. It takes 1 move to castle,
         but we might save a rook move
      */
      cast= BlKingMoves[square_c8];
      if (cast < needed)
        needed= cast;
    }
    if (TSTFLAGMASK(castling_flag[nbply],rh8_cancastle))
    {
      /* blank short castling */
      /* BlKingMoves is the number of moves the blank king still
         needs after castling. It takes 1 move to castle, but we
         might save a rook move
      */
      cast= BlKingMoves[square_g8];
      if (cast < needed)
        needed= cast;
    }
  }
  return needed;
}

static int ProofWhKingMovesNeeded(void)
{
  int   needed = WhKingMoves[rb];
  int   cast;

  if (rb==initsquare)
    /* no king in play, or king can be created by promotion
     * -> no optimisation possible */
    return 0;

  if (TSTFLAGMASK(castling_flag[nbply],ke1_cancastle))
  {
    if (TSTFLAGMASK(castling_flag[nbply],ra1_cancastle))
    {
      /* wh long castling */
      /* WhKingMoves is the number of moves the wh king still
         needs after castling. It takes 1 move to castle, but we
         might save a rook move.
      */
      cast = WhKingMoves[square_c1];
      if (cast<needed)
        needed= cast;
    }
    if (TSTFLAGMASK(castling_flag[nbply],rh1_cancastle))
    {
      /* wh short castling */
      /* WhKingMoves is the number of moves the wh king still
         needs after castling. It takes 1 move to castle, but we
         might save a rook move
      */
      cast = WhKingMoves[square_g1];
      if (cast<needed)
        needed= cast;
    }
  }

  return needed;
}

static void WhPawnMovesFromTo(
  square    from,
  square    to,
  stip_length_type *moves,
  stip_length_type *captures,
  stip_length_type captallowed)
{
  int rank_to= to/onerow;
  int rank_from= from/onerow;

  /* calculate number of captures */
  *captures= abs(to%onerow-from%onerow);

  /* calculate number of moves */
  if (rank_to<rank_from)
    *moves= slices[root_slice].u.branch.length;
  else
  {
    *moves= rank_to-rank_from;
    if (*moves<*captures || *captures>captallowed)
      *moves= slices[root_slice].u.branch.length;
    else if (from<=square_h2 && *captures<*moves-1)
      /* double step possible */
      --*moves;
  }
}

static void BlPawnMovesFromTo(
  square    from,
  square    to,
  stip_length_type *moves,
  stip_length_type *captures,
  stip_length_type captallowed)
{
  int rank_to= to/onerow;
  int rank_from= from/onerow;

  /* calculate number of captures */
  *captures= abs(to%onerow-from%onerow);

  /* calculate number of moves */
  if (rank_from<rank_to)
    *moves= slices[root_slice].u.branch.length;
  else
  {
    *moves= rank_from-rank_to;
    if (*moves<*captures || *captures>captallowed)
      *moves= slices[root_slice].u.branch.length;
    else if (from>=square_a7 && *captures < *moves-1)
      /* double step possible */
      --*moves;
  }
}

static stip_length_type WhPawnMovesNeeded(square sq)
{
  stip_length_type MovesNeeded;
  stip_length_type MovesNeeded1;

  /* The first time ProofWhPawnMovesNeeded is called the following
     test is always false. It has already been checked in
     ProofImpossible. But we need it here for the recursion.
  */
  if (e[sq]==pb && ProofBoard[sq]!=pb)
    return 0;

  if (sq<=square_h2)
    /* there is no pawn at all that can enter this square */
    return slices[root_slice].u.branch.length;

  /* double step */
  if (square_a4<=sq && square_h4>=sq
      && e[sq+2*dir_down] == pb
      && ProofBoard[sq+2*dir_down] != pb)
    return 1;

  if (e[sq+dir_down+dir_right] != obs)
  {
    MovesNeeded= WhPawnMovesNeeded(sq+dir_down+dir_right);
    if (!MovesNeeded)
      /* There is a free pawn on sq+dir_down+dir_right
      ** so it takes just 1 move */
      return 1;
  }
  else
    MovesNeeded= slices[root_slice].u.branch.length;

  if (e[sq+dir_down+dir_left] != obs)
  {
    MovesNeeded1= WhPawnMovesNeeded(sq+dir_down+dir_left);
    if (!MovesNeeded1)
      /* There is a free pawn on sq+dir_down+dir_left
      ** so it takes just 1 move */
      return 1;
    if (MovesNeeded1 < MovesNeeded)
      MovesNeeded= MovesNeeded1;
  }

  MovesNeeded1= WhPawnMovesNeeded(sq+dir_down);
  if (MovesNeeded1<MovesNeeded)
    MovesNeeded= MovesNeeded1;

  return MovesNeeded+1;
}

static stip_length_type BlPawnMovesNeeded(square sq)
{
  stip_length_type MovesNeeded;
  stip_length_type MovesNeeded1;

  /* The first time ProofBlPawnMovesNeeded is called the following
     test is always false. It has already been checked in
     ProofImpossible. But we need it here for the recursion.
  */

  if (e[sq] == pn && ProofBoard[sq] != pn)
    return 0;

  if (sq>=square_a7)
    /* there is no pawn at all that can enter this square */
    return slices[root_slice].u.branch.length;

  /* double step */
  if (square_a5<=sq && square_h5>=sq
      && e[sq+2*dir_up] == pn
      && ProofBoard[sq+2*dir_up] != pn)
    return 1;

  if (e[sq+dir_up+dir_left] != obs)
  {
    MovesNeeded= BlPawnMovesNeeded(sq+dir_up+dir_left);
    if (!MovesNeeded)
      /* There is a free pawn on sq+dir_up+dir_left
      ** so it takes just 1 move */
      return 1;
  }
  else
    MovesNeeded= slices[root_slice].u.branch.length;

  if (e[sq+dir_up+dir_right] != obs)
  {
    MovesNeeded1= BlPawnMovesNeeded(sq+dir_up+dir_right);
    if (!MovesNeeded1)
      /* There is a free pawn on sq+dir_up+dir_right
      ** so it takes just 1 move */
      return 1;
    if (MovesNeeded1 < MovesNeeded)
      MovesNeeded= MovesNeeded1;
  }

  MovesNeeded1= BlPawnMovesNeeded(sq+dir_up);
  if (MovesNeeded1 < MovesNeeded)
    MovesNeeded= MovesNeeded1;

  return MovesNeeded+1;
} /* BlPawnMovesNeeded */

#define BLOCKED(sq)                             \
  (  (e[sq] == pb                               \
      && ProofBoard[sq] == pb                   \
      && WhPawnMovesNeeded(sq)>=slices[root_slice].u.branch.length)       \
     || (e[sq] == pn                            \
         && ProofBoard[sq] == pn                \
         && BlPawnMovesNeeded(sq)>=slices[root_slice].u.branch.length))

static void PieceMovesFromTo(piece p,
                             square from, square to,
                             stip_length_type *moves)
{
  numvec dir;
  int    sqdiff= from-to;

  if (sqdiff==0)
  {
    *moves= 0;
    return;
  }
  switch (abs(p))
  {
  case Knight:
    *moves= ProofKnightMoves[abs(sqdiff)];
    if (*moves > 1)
    {
      square    sqi, sqj;
      int   i, j;
      stip_length_type testmov;
      stip_length_type testmin = slices[root_slice].u.branch.length;
      for (i= vec_knight_start; i<=vec_knight_end; i++)
      {
        sqi= from+vec[i];
        if (!BLOCKED(sqi) && e[sqi] != obs)
          for (j= vec_knight_start; j<=vec_knight_end; j++)
          {
            sqj= to+vec[j];
            if (!BLOCKED(sqj) && e[sqj] != obs)
            {
              testmov= ProofKnightMoves[abs(sqi-sqj)]+2;
              if (testmov == *moves)
                return;
              if (testmov < testmin)
                testmin= testmov;
            }
          }
      }
      *moves= testmin;
    }
    break;

  case Bishop:
    if (SquareCol(from) != SquareCol(to))
      *moves= slices[root_slice].u.branch.length;
    else
    {
      dir= CheckDirBishop[sqdiff];
      if (dir)
      {
        do {
          from-= dir;
        } while (to != from && !BLOCKED(from));
        *moves= to == from ? 1 : 3;
      }
      else
        *moves= 2;
    }
    break;

  case Rook:
    dir= CheckDirRook[sqdiff];
    if (dir)
    {
      do {
        from-= dir;
      } while (to != from && !BLOCKED(from));
      *moves= to == from ? 1 : 3;
    }
    else
      *moves= 2;
    break;

  case Queen:
    dir= CheckDirQueen[sqdiff];
    if (dir)
    {
      do {
        from-= dir;
      } while (to != from && !BLOCKED(from));
      *moves= to == from ? 1 : 2;
    }
    else
      *moves= 2;
    break;

  default:
    StdString("error in PieceMovesFromTo - piece:");WritePiece(p);
    StdString("\n");
  }
} /* PieceMovesFromTo */

static void WhPromPieceMovesFromTo(
    square    from,
    square    to,
    stip_length_type *moves,
    stip_length_type *captures,
    stip_length_type captallowed)
{
  stip_length_type i;
  stip_length_type mov1, mov2, cap1;
  square    cenpromsq;

  cenpromsq= (from%onerow
              + (nr_of_slack_rows_below_board+nr_rows_on_board-1)*onerow);
  *moves= slices[root_slice].u.branch.length;

  WhPawnMovesFromTo(from, cenpromsq, &mov1, &cap1, captallowed);
  PieceMovesFromTo(ProofBoard[to], cenpromsq, to, &mov2);
  if (mov1+mov2 < *moves)
    *moves= mov1+mov2;

  for (i= 1; i<=captallowed; i++)
  {
    if (cenpromsq+i <= square_h8) {
      /* got out of range sometimes ! */
      WhPawnMovesFromTo(from, cenpromsq+i, &mov1, &cap1, captallowed);
      PieceMovesFromTo(ProofBoard[to], cenpromsq+i, to, &mov2);
      if (mov1+mov2 < *moves)
        *moves= mov1+mov2;
    }
    if (cenpromsq-i>=square_a8)
    {
      /* got out of range sometimes ! */
      WhPawnMovesFromTo(from, cenpromsq-i, &mov1, &cap1, captallowed);
      PieceMovesFromTo(ProofBoard[to], cenpromsq-i, to, &mov2);
      if (mov1+mov2 < *moves) {
        *moves= mov1+mov2;
      }
    }
  }

  /* We cannot say for sure how many captures we really need.
  ** We may need 3 moves and 1 capture or 2 moves and 2 captures.
  ** Therefore zero is returned. */
  *captures= 0;
} /* WhPromPieceMovesFromTo */

static void BlPromPieceMovesFromTo(
    square    from,
    square    to,
    stip_length_type *moves,
    stip_length_type *captures,
    stip_length_type captallowed)
{
  square    cenpromsq;
  stip_length_type i, mov1, mov2, cap1;

  cenpromsq= from%onerow + nr_of_slack_rows_below_board*onerow;
  *moves= slices[root_slice].u.branch.length;

  BlPawnMovesFromTo(from, cenpromsq, &mov1, &cap1, captallowed);
  PieceMovesFromTo(ProofBoard[to], cenpromsq, to, &mov2);
  if (mov1+mov2 < *moves)
    *moves= mov1+mov2;

  for (i= 1; i <= captallowed; i++)
  {
    if (cenpromsq+i<=square_h1)
    {
      /* got out of range sometimes !*/
      BlPawnMovesFromTo(from, cenpromsq+i, &mov1, &cap1, captallowed);
      PieceMovesFromTo(ProofBoard[to], cenpromsq+i, to, &mov2);
      if (mov1+mov2 < *moves)
        *moves= mov1+mov2;
    }
    if (cenpromsq-i >= square_a1)
    {
      /* got out of range sometimes ! */
      BlPawnMovesFromTo(from, cenpromsq-i, &mov1, &cap1, captallowed);
      PieceMovesFromTo(ProofBoard[to], cenpromsq-i, to, &mov2);
      if (mov1+mov2 < *moves)
        *moves= mov1+mov2;
    }
  }

  /* We cannot say for sure how many captures we really need.
  ** We may need 3 moves and 1 capture or 2 moves and 2 captures.
  ** Therefore zero is returned. */
  *captures= 0;
} /* BlPromPieceMovesFromTo */

static void WhPieceMovesFromTo(
    square    from,
    square    to,
    stip_length_type *moves,
    stip_length_type *captures,
    stip_length_type captallowed,
    int       captrequ)
{
  piece pfrom= e[from];
  piece pto= ProofBoard[to];

  *moves= slices[root_slice].u.branch.length;

  switch (pto)
  {
  case pb:
    if (pfrom == pb)
      WhPawnMovesFromTo(from, to, moves, captures, captallowed);
    break;

  default:
    if (pfrom == pto)
    {
      PieceMovesFromTo(pfrom, from, to, moves);
      *captures= 0;
    }
    else if (pfrom == pb)
      WhPromPieceMovesFromTo(from,
                             to, moves, captures, captallowed-captrequ);
  }
}

static void BlPieceMovesFromTo(
    square    from,
    square    to,
    stip_length_type *moves,
    stip_length_type *captures,
    stip_length_type captallowed,
    int       captrequ)
{
  piece pfrom, pto;

  pfrom= e[from];
  pto= ProofBoard[to];
  *moves= slices[root_slice].u.branch.length;

  switch (pto)
  {
    case pn:
      if (pfrom == pn)
        BlPawnMovesFromTo(from, to, moves, captures, captallowed);
      break;

    default:
      if (pfrom == pto)
      {
        PieceMovesFromTo(pfrom, from, to, moves);
        *captures= 0;
      }
      else if (pfrom == pn)
        BlPromPieceMovesFromTo(from,
                               to, moves, captures, captallowed-captrequ);
  }
}

typedef struct
{
    int     Nbr;
    square  sq[16];
} PieceList;

typedef struct
{
    int     Nbr;
    stip_length_type moves[16];
    stip_length_type captures[16];
    int     id[16];
} PieceList2;

PieceList ProofWhPawns, CurrentWhPawns,
  ProofWhPieces, CurrentWhPieces,
  ProofBlPawns, CurrentBlPawns,
  ProofBlPieces, CurrentBlPieces;

static stip_length_type ArrangeListedPieces(
  PieceList2    *pl,
  int       nto,
  int       nfrom,
  boolean   *taken,
  stip_length_type CapturesAllowed)
{
  stip_length_type Diff, Diff2;
  int i, id;

  Diff= slices[root_slice].u.branch.length;

  if (nto == 0)
    return 0;

  for (i= 0; i < pl[0].Nbr; i++)
  {
    id= pl[0].id[i];
    if (taken[id] || pl[0].captures[i]>CapturesAllowed)
      continue;

    taken[id]= true;
    Diff2= pl[0].moves[i]
      + ArrangeListedPieces(pl+1, nto-1, nfrom,
                            taken, CapturesAllowed-pl[0].captures[i]);

    if (Diff2 < Diff)
      Diff= Diff2;

    taken[id]= false;
  }

  return Diff;
}

static stip_length_type ArrangePieces(
  stip_length_type CapturesAllowed,
  Side   camp,
  stip_length_type CapturesRequired)
{
  int       ifrom, ito, Diff;
  stip_length_type moves, captures;
  PieceList2    pl[16];
  boolean   taken[16];
  PieceList *from, *to;

  from= camp == White
    ? &CurrentWhPieces
    : &CurrentBlPieces;

  to= camp == White
    ? &ProofWhPieces
    : &ProofBlPieces;

  if (to->Nbr == 0)
    return 0;

  for (ito= 0; ito < to->Nbr; ito++)
  {
    pl[ito].Nbr= 0;
    for (ifrom= 0; ifrom < from->Nbr; ifrom++)
    {
      if (camp == White)
        WhPieceMovesFromTo(from->sq[ifrom],
                           to->sq[ito], &moves, &captures,
                           CapturesAllowed, CapturesRequired);
      else
        BlPieceMovesFromTo(from->sq[ifrom],
                           to->sq[ito], &moves, &captures,
                           CapturesAllowed, CapturesRequired);
      if (moves < slices[root_slice].u.branch.length)
      {
        pl[ito].moves[pl[ito].Nbr]= moves;
        pl[ito].captures[pl[ito].Nbr]= captures;
        pl[ito].id[pl[ito].Nbr]= ifrom;
        pl[ito].Nbr++;
      }
    }
  }

  for (ifrom= 0; ifrom < from->Nbr; ifrom++)
    taken[ifrom]= false;

  /* determine minimal number of moves required */
  Diff= ArrangeListedPieces(pl, to->Nbr, from->Nbr, taken, CapturesAllowed);

  return Diff;
}

static stip_length_type ArrangePawns(
  stip_length_type CapturesAllowed,
  Side   camp,
  stip_length_type *CapturesRequired)
{
  int       ifrom, ito;
  stip_length_type moves, captures, Diff;
  PieceList2    pl[8];
  boolean   taken[8];
  PieceList *from, *to;

  from= camp == White
    ? &CurrentWhPawns
    : &CurrentBlPawns;
  to= camp == White
    ? &ProofWhPawns
    : &ProofBlPawns;

  if (to->Nbr == 0)
  {
    *CapturesRequired= 0;
    return 0;
  }

  for (ito= 0; ito < to->Nbr; ito++)
  {
    pl[ito].Nbr= 0;
    for (ifrom= 0; ifrom<from->Nbr; ifrom++)
    {
      if (camp == White)
        WhPawnMovesFromTo(from->sq[ifrom],
                          to->sq[ito], &moves, &captures, CapturesAllowed);
      else
        BlPawnMovesFromTo(from->sq[ifrom],
                          to->sq[ito], &moves, &captures, CapturesAllowed);
      if (moves < slices[root_slice].u.branch.length)
      {
        pl[ito].moves[pl[ito].Nbr]= moves;
        pl[ito].captures[pl[ito].Nbr]= captures;
        pl[ito].id[pl[ito].Nbr]= ifrom;
        pl[ito].Nbr++;
      }
    }
  }
  for (ifrom= 0; ifrom < from->Nbr; ifrom++)
    taken[ifrom]= false;

  /* determine minimal number of moves required */
  Diff= ArrangeListedPieces(pl,
                            to->Nbr, from->Nbr, taken, CapturesAllowed);

  if (Diff == slices[root_slice].u.branch.length)
    return slices[root_slice].u.branch.length;

  /* determine minimal number of captures required */
  captures= 0;
  while (ArrangeListedPieces(pl, to->Nbr, from->Nbr, taken, captures)
         == slices[root_slice].u.branch.length)
    captures++;

  *CapturesRequired= captures;

  return Diff;
}

static boolean NeverImpossible(void)
{
  return false;
}

static boolean ProofFairyImpossible(void)
{
  square    *bnp, sq;
  piece pparr;
  int   NbrWh, NbrBl;
  int MovesAvailable = BlMovesLeft+WhMovesLeft;

  NbrWh = nbpiece[pb]
    + nbpiece[cb]
    + nbpiece[tb]
    + nbpiece[fb]
    + nbpiece[db]
    + nbpiece[roib];

  NbrBl = nbpiece[pn]
    + nbpiece[cn]
    + nbpiece[tn]
    + nbpiece[fn]
    + nbpiece[dn]
    + nbpiece[roin];

  /* not enough time to capture the remaining pieces */
  if (change_moving_piece)
  {
    if (NbrWh + NbrBl - ProofNbrWhitePieces - ProofNbrBlackPieces
        > MovesAvailable)
      return true;

    if (CondFlag[andernach]
        && !anycirce) {
      int count= 0;
      /* in AndernachChess we need at least 1 capture if a pawn
         residing at his initial square has moved and has to be
         reestablished via a capture of the opposite side.
         has a white pawn on the second rank moved or has it been
         captured?
      */
      for (sq= square_a2; sq <= square_h2; sq++)
        if (e[sq]!=pb && ProofBoard[sq]==pb)
          count++;

      if ((16 - count) < ProofNbrBlackPieces)
        return true;

      count= 0;

      /* has a black pawn on the seventh rank moved or has it
         been captured?
      */
      for (sq= square_a7; sq <= square_h7; sq++)
        if (e[sq]!=pn && ProofBoard[sq]==pn)
          count++;

      if ((16 - count) < ProofNbrWhitePieces)
        return true;
    }
  }
  else
  {
    if (!CondFlag[masand])
    {
      /* not enough time to capture the remaining pieces */
      if (NbrWh-ProofNbrWhitePieces > BlMovesLeft
          || NbrBl-ProofNbrBlackPieces > WhMovesLeft)
        return true;
    }

    pparr = CondFlag[parrain] ? pprise[nbply] : vide;
    if (!CondFlag[sentinelles])
    {
      /* note, that we are in the !change_moving_piece section
         too many pawns captured or promoted
      */
      if (ProofNbrPiece[pb] > nbpiece[pb]+(pparr==pb)
          || ProofNbrPiece[pn] > nbpiece[pn]+(pparr==pn))
        return true;
    }

    if (CondFlag[anti])
    {
      /* note, that we are in the !change_moving_piece section */
      int count= 0;
      /* in AntiCirce we need at least 2 captures if a pawn
         residing at his initial square has moved and has to be
         reborn via capture because we need a second pawn to do
         the same to the other rank NOT ALWAYS TRUE ! Only if
         there's no pawn of the same colour on the same rank has
         a white pawn on the second rank moved or has it been
         captured?
      */
      for (sq= square_a2; sq<=square_h2; sq++)
        if (e[sq] != pb)
        {
          if (ProofBoard[sq]==pb)
          {
            if (ProofBoard[sq+dir_up]!=pb
                && ProofBoard[sq+2*dir_up]!=pb
                && ProofBoard[sq+3*dir_up]!=pb
                && ProofBoard[sq+4*dir_up]!=pb
                && ProofBoard[sq+5*dir_up]!=pb)
              count++;
          }
          else if (ProofBoard[sq+dir_up] == pb
                   && e[sq+dir_up] != pb)
          {
            if (ProofBoard[sq+2*dir_up]!=pb
                && ProofBoard[sq+3*dir_up]!=pb
                && ProofBoard[sq+4*dir_up]!=pb
                && ProofBoard[sq+5*dir_up]!=pb)
              count++;
          }
        }

      if (count%2 == 1)
        count++;

      if ((16 - count) < ProofNbrBlackPieces)
        return true;

      count= 0;
      /* has a black pawn on the seventh rank moved or has it
         been captured?
      */
      for (sq= square_a7; sq <= square_h7; sq++)
        if (e[sq]!=pn)
        {
          if (ProofBoard[sq] == pn)
          {
            if (ProofBoard[sq+dir_down]!=pn
                && ProofBoard[sq+2*dir_down]!=pn
                && ProofBoard[sq+3*dir_down]!=pn
                && ProofBoard[sq+4*dir_down]!=pn
                && ProofBoard[sq+5*dir_down]!=pn)
              count++;
          }
          else if (ProofBoard[sq+dir_down]==pn
                   && e[sq+dir_down]!=pn)
          {
            if (ProofBoard[sq+2*dir_down]!=pn
                && ProofBoard[sq+3*dir_down]!=pn
                && ProofBoard[sq+4*dir_down]!=pn
                && ProofBoard[sq+5*dir_down]!=pn)
              count++;
          }
        }

      if (count%2 == 1)
        count++;
      if ((16 - count) < ProofNbrWhitePieces)
        return true;
    }
  }

  /* find a solution ... */
  MovesAvailable *= 2;

  for (bnp= boardnum; *bnp; bnp++)
  {
    piece const p = ProofBoard[*bnp];
    if (p!=vide && p!=e[*bnp])
      MovesAvailable--;
  }

  return MovesAvailable < 0;
} /* ProofFairyImpossible */

static boolean ProofImpossible(void)
{
  square    *bnp;
  stip_length_type black_moves_left = BlMovesLeft;
  stip_length_type white_moves_left = WhMovesLeft;
  stip_length_type WhPieToBeCapt, BlPieToBeCapt;
  stip_length_type WhCapturesRequired, BlCapturesRequired;
  stip_length_type white_king_moves_needed, black_king_moves_needed;
  piece p1, p2;
  square    sq;
  int       NbrWh, NbrBl;

  /* too many pawns captured or promoted */
  if (ProofNbrPiece[pb] > nbpiece[pb])
  {
    TraceValue("%d ",ProofNbrPiece[pb]);
    TraceValue("%d\n",nbpiece[pb]);
    return true;
  }

  if (ProofNbrPiece[pn] > nbpiece[pn])
  {
    TraceValue("%d ",ProofNbrPiece[pn]);
    TraceValue("%d\n",nbpiece[pn]);
    return true;
  }

  NbrWh = nbpiece[pb]
    + nbpiece[cb]
    + nbpiece[tb]
    + nbpiece[fb]
    + nbpiece[db]
    + nbpiece[roib];

  NbrBl = nbpiece[pn]
    + nbpiece[cn]
    + nbpiece[tn]
    + nbpiece[fn]
    + nbpiece[dn]
    + nbpiece[roin];

  /* too many pieces captured */
  if (NbrWh < ProofNbrWhitePieces)
  {
    TraceValue("%d ",NbrWh);
    TraceValue("%d\n",ProofNbrWhitePieces);
    return true;
  }
  if (NbrBl < ProofNbrBlackPieces)
  {
    TraceValue("%d ",NbrBl);
    TraceValue("%d\n",ProofNbrBlackPieces);
    return true;
  }

  /* check if there is enough time left to capture the
     superfluos pieces
  */

  /* not enough time to capture the remaining pieces */
  WhPieToBeCapt = NbrWh-ProofNbrWhitePieces;
  if (WhPieToBeCapt>black_moves_left)
  {
    TraceValue("%d ",WhPieToBeCapt);
    TraceValue("%d\n",black_moves_left);
    return true;
  }

  BlPieToBeCapt = NbrBl - ProofNbrBlackPieces;
    TraceValue("%d ",BlPieToBeCapt);
    TraceValue("%d ",NbrBl);
    TraceValue("%d ",ProofNbrBlackPieces);
    TraceValue("%d\n",white_moves_left);
  if (BlPieToBeCapt>white_moves_left)
  {
    TraceValue("%d ",BlPieToBeCapt);
    TraceValue("%d\n",white_moves_left);
    return true;
  }

  /* has one of the blocked pieces been captured ? */
  if ((BlockedBishopc1 && ProofBoard[square_c1]!=fb)
      || (BlockedBishopf1 && ProofBoard[square_f1]!=fb)
      || (BlockedBishopc8 && ProofBoard[square_c8]!=fn)
      || (BlockedBishopf8 && ProofBoard[square_f8]!=fn)
      || (BlockedQueend1  && ProofBoard[square_d1]!=db)
      || (BlockedQueend8  && ProofBoard[square_d8]!=dn))
  {
    TraceText("blocked piece was captured\n");
    return true;
  }
  
  /* has a white pawn on the second rank moved or has it
     been captured?
  */
  for (sq= square_a2; sq<=square_h2; sq+=dir_right)
    if (ProofBoard[sq]==pb && e[sq]!=pb)
    {
      TraceValue("%d ",sq);
      TraceText("ProofBoard[sq]==pb && e[sq]!=pb\n");
      return true;
    }

  /* has a black pawn on the seventh rank moved or has it
     been captured?
  */
  for (sq= square_a7; sq<=square_h7; sq+=dir_right)
    if (ProofBoard[sq]==pn && e[sq]!=pn)
    {
      TraceValue("%d ",sq);
      TraceText("ProofBoard[sq]==pn && e[sq]!=pn\n");
      return true;
    }

  white_king_moves_needed = ProofWhKingMovesNeeded();
  if (white_moves_left<white_king_moves_needed)
    return true;
  else
    white_moves_left -= ProofWhKingMovesNeeded();

  black_king_moves_needed = ProofBlKingMovesNeeded();
  if (black_moves_left<black_king_moves_needed)
    return true;
  else
    black_moves_left -= black_king_moves_needed;

  if (CondFlag[haanerchess])
  {
    TraceText("impossible hole created\n");
    return ProofBoard[move_generation_stack[nbcou].departure] != vide;
  }

  /* collect the pieces for further investigations */
  ProofWhPawns.Nbr = 0;
  ProofWhPieces.Nbr = 0;
  ProofBlPawns.Nbr = 0;
  ProofBlPieces.Nbr = 0;
  CurrentWhPawns.Nbr = 0;
  CurrentWhPieces.Nbr = 0;
  CurrentBlPawns.Nbr = 0;
  CurrentBlPieces.Nbr= 0;

  for (bnp= boardnum; *bnp; bnp++)
  {
    p1= ProofBoard[*bnp];
    p2= e[*bnp];

    if (p1 == p2)
      continue;

    if (p1 != vide)
    {
      if (p1 > vide)
      {  /* it's a white piece */
        switch (p1)
        {
          case roib:
            break;

          case pb:
            ProofWhPawns.sq[ProofWhPawns.Nbr]= *bnp;
            ProofWhPawns.Nbr++;
            ProofWhPieces.sq[ProofWhPieces.Nbr]= *bnp;
            ProofWhPieces.Nbr++;
            break;

          default:
            ProofWhPieces.sq[ProofWhPieces.Nbr]= *bnp;
            ProofWhPieces.Nbr++;
            break;
        }
      }
      else
      {  /* it's a black piece */
        switch (p1)
        {
          case roin:
            break;

          case pn:
            ProofBlPawns.sq[ProofBlPawns.Nbr]= *bnp;
            ProofBlPawns.Nbr++;
            ProofBlPieces.sq[ProofBlPieces.Nbr]= *bnp;
            ProofBlPieces.Nbr++;
            break;

          default:
            ProofBlPieces.sq[ProofBlPieces.Nbr]= *bnp;
            ProofBlPieces.Nbr++;
            break;
        }
      }
    } /* p1 != vide */

    if (p2 != vide)
    {
      if (p2 > vide)  /* it's a white piece */
      {
        switch (p2)
        {
          case roib:
            break;

          case pb:
            CurrentWhPawns.sq[CurrentWhPawns.Nbr]= *bnp;
            CurrentWhPawns.Nbr++;
            CurrentWhPieces.sq[CurrentWhPieces.Nbr]= *bnp;
            CurrentWhPieces.Nbr++;
            break;

          default:
            if (!(CapturedBishopc1 && *bnp == square_c1 && p2 == fb)
                &&!(CapturedBishopf1 && *bnp == square_f1 && p2 == fb)
                &&!(CapturedQueend1 && *bnp == square_d1 && p2 == db))
              CurrentWhPieces.sq[CurrentWhPieces.Nbr++]= *bnp;
            break;
        }
      }
      else  /* it's a black piece */
      {
        switch (p2)
        {
          case roin:
            break;
          case pn:
            CurrentBlPawns.sq[CurrentBlPawns.Nbr]= *bnp;
            CurrentBlPawns.Nbr++;
            CurrentBlPieces.sq[CurrentBlPieces.Nbr]= *bnp;
            CurrentBlPieces.Nbr++;
            break;
          default:
            if (!(CapturedBishopc1 && *bnp == square_c1 && p2 == fn)
                &&!(CapturedBishopf1 && *bnp == square_f1 && p2 == fn)
                &&!(CapturedQueend1 && *bnp == square_d1 && p2 == dn))
              CurrentBlPieces.sq[CurrentBlPieces.Nbr++]= *bnp;
            break;
        }
      }
    } /* p2 != vide */
  } /* for (bnp... */

  if (ArrangePawns(BlPieToBeCapt,White,&BlCapturesRequired)>white_moves_left)
  {
    TraceText("ArrangePawns(BlPieToBeCapt,White,&BlCapturesRequired)"
              ">white_moves_left\n");
    return true;
  }

  if (ArrangePawns(WhPieToBeCapt,Black,&WhCapturesRequired)>black_moves_left)
  {
    TraceText("ArrangePawns(WhPieToBeCapt,Black,&WhCapturesRequired)"
              ">black_moves_left");
    return true;
  }

  if (ArrangePieces(BlPieToBeCapt,White,BlCapturesRequired)>white_moves_left)
  {
    TraceText("(ArrangePieces(BlPieToBeCapt,White,BlCapturesRequired)"
              ">white_moves_left");
    return true;
  }

  if (ArrangePieces(WhPieToBeCapt,Black,WhCapturesRequired)>black_moves_left)
  {
    TraceText("ArrangePieces(WhPieToBeCapt,Black,WhCapturesRequired)"
              ">black_moves_left");
    return true;
  }

  TraceText("not ProofImpossible\n");
  return false;
} /* ProofImpossible */

static boolean ProofSeriesImpossible(void)
{
  square    *bnp, sq;
  stip_length_type BlPieToBeCapt, BlCapturesRequired;
  int       NbrBl;
  stip_length_type white_moves_left= BlMovesLeft+WhMovesLeft;
  stip_length_type white_king_moves_needed;

  TraceValue("%d\n",BlMovesLeft+WhMovesLeft);
  /* too many pawns captured or promoted */
  if (ProofNbrPiece[pb]>nbpiece[pb] || ProofNbrPiece[pn]>nbpiece[pn])
    return true;

  NbrBl= nbpiece[pn]
    + nbpiece[cn]
    + nbpiece[tn]
    + nbpiece[fn]
    + nbpiece[dn]
    + nbpiece[roin];

  /* to many pieces captured    or */
  /* not enough time to capture the remaining pieces */
  if (NbrBl<ProofNbrBlackPieces)
    return true;
  else
  {
    BlPieToBeCapt= NbrBl - ProofNbrBlackPieces;
    if (BlPieToBeCapt>white_moves_left)
      return true;
  }
  
  /* has a white pawn on the second rank moved ? */
  for (sq = square_a2; sq<=square_h2; sq += dir_right)
    if (ProofBoard[sq]==pb && e[sq]!=pb)
      return true;

  /* has a black pawn on the seventh rank been captured ? */
  for (sq = square_a7; sq<=square_h7; sq += dir_right)
    if (ProofBoard[sq]==pn && e[sq]!=pn)
      return true;

  /* has a black piece on the eigth rank been captured ? */
  for (sq = square_a8; sq<=square_h8; sq += dir_right)
    if (ProofBoard[sq]<roin && ProofBoard[sq]!=e[sq])
      return true;

  white_king_moves_needed = ProofWhKingMovesNeeded();
  if (white_moves_left<white_king_moves_needed)
    return true;
  else
    white_moves_left -= white_king_moves_needed;

  /* collect the pieces for further investigations */
  ProofWhPawns.Nbr=
    ProofWhPieces.Nbr=
    CurrentWhPawns.Nbr=
    CurrentWhPieces.Nbr= 0;
  
  for (bnp= boardnum; *bnp; bnp++) {
    piece const p1= ProofBoard[*bnp];
    piece const p2= e[*bnp];

    if (p1 != p2) {
      if (p1 > vide) {  /* it's a white piece */
        switch (p1) {
        case roib:
          break;
        case pb:
          ProofWhPawns.sq[ProofWhPawns.Nbr++]= *bnp;
          ProofWhPieces.sq[ProofWhPieces.Nbr++]= *bnp;
          break;
        default:
          ProofWhPieces.sq[ProofWhPieces.Nbr++]= *bnp;
          break;
        }
      } /* p1 > vide */

      if (p2 > vide) {  /* it's a white piece */
        switch (p2) {
        case roib:
          break;
        case pb:
          CurrentWhPawns.sq[CurrentWhPawns.Nbr++]= *bnp;
          CurrentWhPieces.sq[CurrentWhPieces.Nbr++]= *bnp;
          break;
        default:
          CurrentWhPieces.sq[CurrentWhPieces.Nbr++]= *bnp;
          break;
        }
      } /* p2 > vide */
    } /* p1 != p2 */
  } /* for (bnp... */

  if (ArrangePawns(BlPieToBeCapt,White,&BlCapturesRequired)
      > white_moves_left)
    return true;

  if (ArrangePieces(BlPieToBeCapt,White,BlCapturesRequired)
      > white_moves_left)
    return true;

  return false;
} /* ProofSeriesImpossible */

boolean ProofVerifie(void) {
  if (flagfee || PieSpExFlags&(~(BIT(White)+BIT(Black))))
  {
    VerifieMsg(ProofAndFairyPieces);
    return false;
  }

  ProofFairy= change_moving_piece
    || CondFlag[black_oscillatingKs]
    || CondFlag[white_oscillatingKs]
    || CondFlag[republican]
    || anycirce
    || CondFlag[sentinelles]
    || anyanticirce
    || CondFlag[singlebox]
    || CondFlag[blroyalsq]
    || CondFlag[whroyalsq]
    || TSTFLAG(PieSpExFlags, ColourChange)
    || CondFlag[actrevolving]
    || CondFlag[arc]
    || CondFlag[annan]
    || CondFlag[glasgow]
    || CondFlag[takemake]
    || flagAssassin
    || CondFlag[messigny]
    || CondFlag[mars]
    || CondFlag[castlingchess];

  /* TODO Masand can't possibly be the only condition that doesn't
   * allow any optimisation at all.
   */
  if (CondFlag[masand])
  {
    alternateImpossible = &NeverImpossible;
    seriesImpossible = &NeverImpossible;
  }
  else if (ProofFairy)
  {
    alternateImpossible = &ProofFairyImpossible;
    seriesImpossible = &ProofFairyImpossible;
  }
  else
  {
    alternateImpossible = &ProofImpossible;
    seriesImpossible = &ProofSeriesImpossible;
  }

  return true;
} /* ProofVerifie */
