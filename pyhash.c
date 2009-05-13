/********************* MODIFICATIONS to pyhash.c ***********************
 **
 ** Date       Who  What
 **
 ** 2003/05/12 TLi  hashing bug fixed: h= + intel did not find all solutions .
 **
 ** 2004/03/22 TLi  hashing for exact-* stipulations improved
 **
 ** 2005/02/01 TLi  function hashdefense is not used anymore...
 **
 ** 2005/02/01 TLi  in branch_d_does_attacker_win and invref exchanged the inquiry into the hash
 **                 table for "white can mate" and "white cannot mate" because
 **                 it is more likely that a position has no solution
 **                 This yields an incredible speedup of .5-1%  *fg*
 **
 ** 2006/06/30 SE   New condition: BGL (invented P.Petkov)
 **
 ** 2008/02/10 SE   New condition: Cheameleon Pursuit (invented? : L.Grolman)  
 **
 ** 2009/01/03 SE   New condition: Disparate Chess (invented: R.Bedoni)  
 **
 **************************** End of List ******************************/

/**********************************************************************
 ** We hash.
 **
 ** SmallEncode and LargeEncode are functions to encode the current
 ** position. SmallEncode is used when the starting position contains
 ** less than or equal to eight pieces. LargeEncode is used when more
 ** pieces are present. The function TellSmallEncode and TellLargeEncode
 ** are used to decide what encoding to use. Which function to use is
 ** stored in encode.
 ** SmallEncode encodes for each piece the square where it is located.
 ** LargeEncode uses the old scheme introduced by Torsten: eight
 ** bytes at the beginning give in 64 bits the locations of the pieces
 ** coded after the eight bytes. Both functions give for each piece its
 ** type (1 byte) and specification (2 bytes). After this information
 ** about ep-captures, Duellants and Imitators are coded.
 **
 ** The hash table uses a dynamic hashing scheme which allows dynamic
 ** growth and shrinkage of the hashtable. See the relevant dht* files
 ** for more details. Two procedures are used:
 **   dhtLookupElement: This procedure delivers
 ** a nil pointer, when the given position is not in the hashtable,
 ** or a pointer to a hashelement.
 **   dhtEnterElement:  This procedure enters an encoded position
 ** with its values into the hashtable.
 **
 ** When there is no more memory, or more than MaxPositions positions
 ** are stored in the hash-table, then some positions are removed
 ** from the table. This is done in the compress procedure.
 ** This procedure uses a little improved scheme introduced by Torsten.
 ** The selection of positions to remove is based on the value of
 ** information gathered about this position. The information about
 ** a position "unsolvable in 2 moves" is less valuable than "unsolvable
 ** in 5 moves", since the former can be recomputed faster. For the other
 ** type of information ("solvable") the comparison is the other way round.
 ** The compression of the table is an expensive operation, in a lot
 ** of exeperiments it has shown to be quite effective in keeping the
 ** most valuable information, and speeds up the computation time
 ** considerably. But to be of any use, there must be enough memory to
 ** to store more than 800 positions.
 ** Now Torsten changed popeye so that all stipulations use hashing.
 ** There seems to be no real penalty in using hashing, even if the
 ** hit ratio is very small and only about 5%, it speeds up the
 ** computation time by 30%.
 ** I changed the output of hashstat, since its really informative
 ** to see the hit rate.
 **
 ** inithash()
 **   -- enters the startposition into the hash-table.
 **   -- determines which encode procedure to use
 **   -- Check's for the MaxPostion/MaxMemory settings
 **
 ** closehash()
 **   -- deletes the hashtable and gives back allocated storage.
 **
 ***********************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* TurboC and BorlandC  TLi */
#if defined(__TURBOC__)
# include <mem.h>
# include <alloc.h>
# include <conio.h>
#else
# include <memory.h>
#endif  /* __TURBOC__ */

#include "py.h"
#include "pyproc.h"
#include "pydata.h"
#include "pymsg.h"
#include "pyhash.h"
#include "pyint.h"
#include "DHT/dhtvalue.h"
#include "DHT/dht.h"
#include "pyproof.h"
#include "pystip.h"
#include "platform/maxtime.h"
#include "trace.h"

static struct dht *pyhash;

static char    piece_nbr[PieceCount];
static boolean one_byte_hash;
static unsigned int bytes_per_spec;
static unsigned int bytes_per_piece;

static boolean is_there_slice_with_nonstandard_min_length;

unsigned long int compression_counter;

HashBuffer hashBuffers[maxply+1];

#if !defined(NDEBUG)

boolean isHashBufferValid[maxply+1];

void validateHashBuffer(void)
{
  TraceFunctionEntry(__func__);
  TraceText("\n");

  TraceCurrentHashBuffer();

  isHashBufferValid[nbply] = true;

  TraceFunctionExit(__func__);
  TraceText("\n");
}

void invalidateHashBuffer(boolean guard)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u\n",guard);

  if (guard)
  {
    TraceValue("%u\n",nbply);
    isHashBufferValid[nbply] = false;
  }

  TraceFunctionExit(__func__);
  TraceText("\n");
}

#endif

#if defined(TESTHASH)
#define ifTESTHASH(x)   x
#if defined(__unix)
#include <unistd.h>
static void *OldBreak;
extern int dhtDebug;
#endif /*__unix*/
#else
#define ifTESTHASH(x)
#endif /*TESTHASH*/

#if defined(HASHRATE)
#define ifHASHRATE(x)   x
static unsigned long use_pos, use_all;
#else
#define ifHASHRATE(x)
#endif /*HASHRATE*/

/* New Version for more ply's */
enum
{
  ByteMask = (1u<<CHAR_BIT)-1,
  BitsForPly = 10      /* Up to 1023 ply possible */
};

void (*encode)(void);

typedef unsigned int data_type;

typedef struct
{
	dhtValue Key;
    data_type data;
} element_t;

static slice_index base_slice[max_nr_slices];

void hash_reset_derivations(void)
{
  slice_index si;
  for (si = 0; si!=max_nr_slices; ++si)
    base_slice[si] = no_slice;
}

void hash_slice_is_derived_from(slice_index derived, slice_index base)
{
  base_slice[derived] = base;
}

/* Hashing properties of stipulation slices
 */
typedef struct
{
    boolean is_initialised;
    unsigned int size;
    unsigned int value_size;

    union
    {
        struct
        {
            unsigned int offsetSucc;
            unsigned int maskSucc;
            unsigned int offsetNoSucc;
            unsigned int maskNoSucc;
        } d;
        struct
        {
            unsigned int offsetNoSuccOdd;
            unsigned int maskNoSuccOdd;
            unsigned int offsetNoSuccEven;
            unsigned int maskNoSuccEven;
        } h;
        struct
        {
            unsigned int offsetNoSucc;
            unsigned int maskNoSucc;
        } s;
    } u;
} slice_properties_t;

static slice_properties_t slice_properties[max_nr_slices];

static unsigned int bit_width(unsigned int value)
{
  unsigned int result = 0;
  while (value!=0)
  {
    ++result;
    value /= 2;
  }

  return result;
}

/* Initialise the slice_properties array according to a subtree of the
 * current stipulation slices whose root is a direct slice.
 * @param si root slice of subtree
 * @param length number of attacker's moves of help slice
 * @param nr_bits_left number of bits left over by slices already init
 * @note this is an indirectly recursive function
 */
static void init_slice_properties_direct(slice_index si,
                                         unsigned int length,
                                         unsigned int *nr_bits_left)
{
  unsigned int const size = bit_width(length);
  data_type const mask = (1<<size)-1;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",length);
  TraceFunctionParam("%u\n",*nr_bits_left);

  slice_properties[si].size = size;
  slice_properties[si].value_size = size;

  assert(*nr_bits_left>=size);
  *nr_bits_left -= size;
  slice_properties[si].u.d.offsetNoSucc = *nr_bits_left;
  slice_properties[si].u.d.maskNoSucc = mask << *nr_bits_left;

  assert(*nr_bits_left>=size);
  *nr_bits_left -= size;
  slice_properties[si].u.d.offsetSucc = *nr_bits_left;
  slice_properties[si].u.d.maskSucc = mask << *nr_bits_left;

  TraceFunctionExit(__func__);
  TraceText("\n");
}

/* Initialise the slice_properties array according to a subtree of the
 * current stipulation slices whose root is a help slice.
 * @param si root slice of subtree
 * @param length number of half moves of help slice
 * @param nr_bits_left number of bits left over by slices already init
 * @note this is an indirectly recursive function
 */
static void init_slice_properties_help(slice_index si,
                                       unsigned int length,
                                       unsigned int *nr_bits_left)
{
  unsigned int const size = bit_width((length+1)/2);
  data_type const mask = (1<<size)-1;

  slice_properties[si].size = size;
  slice_properties[si].value_size = size+1;

  assert(*nr_bits_left>=size);
  *nr_bits_left -= size;
  slice_properties[si].u.h.offsetNoSuccOdd = *nr_bits_left;
  slice_properties[si].u.h.maskNoSuccOdd = mask << *nr_bits_left;

  assert(*nr_bits_left>=size);
  *nr_bits_left -= size;
  slice_properties[si].u.h.offsetNoSuccEven = *nr_bits_left;
  slice_properties[si].u.h.maskNoSuccEven = mask << *nr_bits_left;
}

/* Initialise the slice_properties array according to a subtree of the
 * current stipulation slices whose root is a series slice.
 * @param si root slice of subtree
 * @param length number of half moves of series slice
 * @param nr_bits_left number of bits left over by slices already init
 * @note this is an indirectly recursive function
 */
static void init_slice_properties_series(slice_index si,
                                         unsigned int length,
                                         unsigned int *nr_bits_left)
{
  unsigned int const size = bit_width(length);
  data_type const mask = (1<<size)-1;

  slice_properties[si].size = size;
  slice_properties[si].value_size = size;

  assert(*nr_bits_left>=size);
  *nr_bits_left -= size;
  slice_properties[si].u.s.offsetNoSucc = *nr_bits_left;
  slice_properties[si].u.s.maskNoSucc = mask << *nr_bits_left;
}

static void init_slice_properties_recursive(slice_index si,
                                            unsigned int *nr_bits_left);

/* Initialise the slice_properties array according to a subtree of the
 * current stipulation slices
 * @param si root slice of subtree
 * @param nr_bits_left number of bits left over by slices already init
 * @note this is an indirectly recursive function
 */
static void init_slice_properties_recursive(slice_index si,
                                            unsigned int *nr_bits_left)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u\n",*nr_bits_left);

  if (slice_properties[si].is_initialised)
    TraceText("already initialised\n");
  else
  {
    slice_properties[si].is_initialised = true;

    TraceValue("%u\n",slices[si].type);
    switch (slices[si].type)
    {
      case STLeafHelp:
        init_slice_properties_help(si,2,nr_bits_left);
        break;

      case STLeafDirect:
      case STLeafSelf:
        init_slice_properties_direct(si,1,nr_bits_left);
        break;

      case STLeafForced:
        /* nothing */
        break;
      
      case STQuodlibet:
      {
        init_slice_properties_recursive(slices[si].u.quodlibet.op1,
                                        nr_bits_left);
        init_slice_properties_recursive(slices[si].u.quodlibet.op2,
                                        nr_bits_left);

        slice_properties[si].value_size = 0;
      }
      break;

      case STReciprocal:
      {
        slice_index const op1 = slices[si].u.reciprocal.op1;
        slice_index const op2 = slices[si].u.reciprocal.op2;

        init_slice_properties_recursive(op1,nr_bits_left);
        init_slice_properties_recursive(op2,nr_bits_left);

        /* both operand slices must have the same value_size, or the
         * shorter one will dominate the longer one */
        if (slice_properties[op1].value_size>slice_properties[op2].value_size)
          slice_properties[op2].value_size = slice_properties[op1].value_size;
        else
          slice_properties[op1].value_size = slice_properties[op2].value_size;

        break;
      }

      case STNot:
      {
        init_slice_properties_recursive(slices[si].u.not.op,nr_bits_left);
        slice_properties[si].value_size = 0;
        break;
      }

      case STConstant:
        slice_properties[si].value_size = 0;
        break;

      case STBranchDirect:
      {
        slice_index const base = base_slice[si];
        if (base==no_slice)
        {
          unsigned int const length = slices[si].u.branch_d.length;
          slice_index const peer = slices[si].u.branch_d.peer;
          init_slice_properties_direct(si,length,nr_bits_left);
          if (slices[si].u.branch_d.min_length==length
              && length>slack_length_direct+1)
            is_there_slice_with_nonstandard_min_length = true;
          /* check for is_initialised above prefents infinite
           * recursion
           */
          init_slice_properties_recursive(peer,nr_bits_left);
        }
        else
        {
          init_slice_properties_recursive(base,nr_bits_left);
          slice_properties[si] = slice_properties[base];
        }
        break;
      }

      case STBranchDirectDefender:
      {
        slice_index const peer = slices[si].u.branch_d_defender.peer;
        slice_index const next = slices[si].u.branch_d_defender.next;
        init_slice_properties_recursive(peer,nr_bits_left);
        /* check for is_initialised above prevents infinite recursion
         */
        init_slice_properties_recursive(next,nr_bits_left);
        break;
      }

      case STBranchHelp:
      {
        slice_index const base = base_slice[si];
        if (base==no_slice)
        {
          slice_index const next = slices[si].u.branch.next;
          unsigned int const length = slices[si].u.branch.length;
          init_slice_properties_help(si,
                                     length-slack_length_help,
                                     nr_bits_left);
          if (slices[si].u.branch.min_length==length
              && length>slack_length_help+1)
            is_there_slice_with_nonstandard_min_length = true;

          init_slice_properties_recursive(next,nr_bits_left);
        }
        else
        {
          init_slice_properties_recursive(base,nr_bits_left);
          slice_properties[si] = slice_properties[base];
        }
        break;
      }

      case STBranchSeries:
      {
        slice_index const next = slices[si].u.branch.next;
        unsigned int const length = slices[si].u.branch.length;
        init_slice_properties_series(si,
                                     length-slack_length_series,
                                     nr_bits_left);
        if (slices[si].u.branch.min_length==length
            && length>slack_length_series+1)
          is_there_slice_with_nonstandard_min_length = true;

        init_slice_properties_recursive(next,nr_bits_left);
        break;
      }

      case STMoveInverter:
      {
        slice_index const next = slices[si].u.move_inverter.next;
        init_slice_properties_recursive(next,nr_bits_left);
        break;
      }

      default:
        assert(0);
        break;
    }
  }

  TraceFunctionExit(__func__);
  TraceText("\n");
}

/* Initialise the slice_properties array according to the current
 * stipulation slices.
 */
static void init_slice_properties(void)
{
  slice_index const si = root_slice;
  unsigned int nr_bits_left = sizeof(data_type)*CHAR_BIT;
  unsigned int i;

  TraceFunctionEntry(__func__);
  TraceText("\n");

  for (i = 0; i!=max_nr_slices; ++i)
    slice_properties[i].is_initialised = false;

  init_slice_properties_recursive(si,&nr_bits_left);

  TraceFunctionExit(__func__);
  TraceText("\n");
}


/* Pseudo hash table element - template for fast initialization of
 * newly created actual table elements
 */
static dhtElement template_element;


static void set_value_direct_nosucc(dhtElement *he,
                                    slice_index si,
                                    hash_value_type val)
{
  unsigned int const offset = slice_properties[si].u.d.offsetNoSucc;
  unsigned int const bits = val << offset;
  unsigned int const mask = slice_properties[si].u.d.maskNoSucc;
  element_t * const e = (element_t *)he;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u\n",val);
  TraceValue("%u",slice_properties[si].size);
  TraceValue("%u",offset);
  TraceValue("%08x ",mask);
  TracePointerValue("%p ",&e->data);
  TraceValue("pre:%08x ",e->data);
  TraceValue("%08x\n",bits);
  assert((bits&mask)==bits);
  e->data &= ~mask;
  e->data |= bits;
  TraceValue("post:%08x\n",e->data);
  TraceFunctionExit(__func__);
  TraceText("\n");
}

static void set_value_direct_succ(dhtElement *he,
                                  slice_index si,
                                  hash_value_type val)
{
  unsigned int const offset = slice_properties[si].u.d.offsetSucc;
  unsigned int const bits = val << offset;
  unsigned int const mask = slice_properties[si].u.d.maskSucc;
  element_t * const e = (element_t *)he;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u\n",val);
  TraceValue("%u",slice_properties[si].size);
  TraceValue("%u",offset);
  TraceValue("%08x ",mask);
  TracePointerValue("%p ",&e->data);
  TraceValue("pre:%08x ",e->data);
  TraceValue("%08x\n",bits);
  assert((bits&mask)==bits);
  e->data &= ~mask;
  e->data |= bits;
  TraceValue("post:%08x\n",e->data);
  TraceFunctionExit(__func__);
  TraceText("\n");
}

static void set_value_help_odd(dhtElement *he,
                               slice_index si,
                               hash_value_type val)
{
  unsigned int const offset = slice_properties[si].u.h.offsetNoSuccOdd;
  unsigned int const bits = val << offset;
  unsigned int const mask = slice_properties[si].u.h.maskNoSuccOdd;
  element_t * const e = (element_t *)he;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u\n",val);
  TraceValue("%u",slice_properties[si].size);
  TraceValue("%u",offset);
  TraceValue("%08x ",mask);
  TraceValue("pre:%08x ",e->data);
  TraceValue("%08x\n",bits);
  assert((bits&mask)==bits);
  e->data &= ~mask;
  e->data |= bits;
  TraceValue("post:%08x\n",e->data);
  TraceFunctionExit(__func__);
  TraceText("\n");
}

static void set_value_help_even(dhtElement *he,
                                slice_index si,
                                hash_value_type val)
{
  unsigned int const offset = slice_properties[si].u.h.offsetNoSuccEven;
  unsigned int const bits = val << offset;
  unsigned int const mask = slice_properties[si].u.h.maskNoSuccEven;
  element_t * const e = (element_t *)he;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u\n",val);
  TraceValue("%u",slice_properties[si].size);
  TraceValue("%u",offset);
  TraceValue("%08x ",mask);
  TraceValue("pre:%08x ",e->data);
  TraceValue("%08x\n",bits);
  assert((bits&mask)==bits);
  e->data &= ~mask;
  e->data |= bits;
  TraceValue("post:%08x\n",e->data);
  TraceFunctionExit(__func__);
  TraceText("\n");
}

static void set_value_series(dhtElement *he,
                             slice_index si,
                             hash_value_type val)
{
  unsigned int const offset = slice_properties[si].u.s.offsetNoSucc;
  unsigned int const bits = val << offset;
  unsigned int const mask = slice_properties[si].u.s.maskNoSucc;
  element_t * const e = (element_t *)he;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u\n",val);
  TraceValue("%u",slice_properties[si].size);
  TraceValue("%u",offset);
  TraceValue("%08x ",mask);
  TraceValue("pre:%08x ",e->data);
  TraceValue("%08x\n",bits);
  assert((bits&mask)==bits);
  e->data &= ~mask;
  e->data |= bits;
  TraceValue("post:%08x\n",e->data);
  TraceFunctionExit(__func__);
  TraceText("\n");
}

static hash_value_type get_value_direct_succ(dhtElement const *he,
                                             slice_index si)
{
  unsigned int const offset = slice_properties[si].u.d.offsetSucc;
  unsigned int const mask = slice_properties[si].u.d.maskSucc;
  element_t const * const e = (element_t const *)he;
  data_type const result = (e->data & mask) >> offset;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceValue("%08x ",mask);
  TracePointerValue("%p ",&e->data);
  TraceValue("%08x\n",e->data);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u\n",result);
  return result;
}

static hash_value_type get_value_direct_nosucc(dhtElement const *he,
                                               slice_index si)
{
  unsigned int const offset = slice_properties[si].u.d.offsetNoSucc;
  unsigned int const mask = slice_properties[si].u.d.maskNoSucc;
  element_t const * const e = (element_t const *)he;
  data_type const result = (e->data & mask) >> offset;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceValue("%08x ",mask);
  TracePointerValue("%p ",&e->data);
  TraceValue("%08x\n",e->data);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u\n",result);
  return result;
}

static hash_value_type get_value_help_odd(dhtElement const *he,
                                          slice_index si)
{
  unsigned int const offset = slice_properties[si].u.h.offsetNoSuccOdd;
  unsigned int const  mask = slice_properties[si].u.h.maskNoSuccOdd;
  element_t const * const e = (element_t const *)he;
  data_type const result = (e->data & mask) >> offset;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceValue("%08x ",mask);
  TraceValue("%08x\n",e->data);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u\n",result);
  return result;
}

static hash_value_type get_value_help_even(dhtElement const *he,
                                           slice_index si)
{
  unsigned int const offset = slice_properties[si].u.h.offsetNoSuccEven;
  unsigned int const  mask = slice_properties[si].u.h.maskNoSuccEven;
  element_t const * const e = (element_t const *)he;
  data_type const result = (e->data & mask) >> offset;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceValue("%08x ",mask);
  TraceValue("%08x\n",e->data);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u\n",result);
  return result;
}

static hash_value_type get_value_series(dhtElement const *he,
                                        slice_index si)
{
  unsigned int const offset = slice_properties[si].u.s.offsetNoSucc;
  unsigned int const mask = slice_properties[si].u.s.maskNoSucc;
  element_t const * const e = (element_t const *)he;
  data_type const result = (e->data & mask) >> offset;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceValue("%08x ",mask);
  TraceValue("%08x\n",e->data);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u\n",result);
  return result;
}

/* Determine the contribution of a direct slice (or leaf slice with
 * direct end) to the value of a hash table element node.
 * @param he address of hash table element to determine value of
 * @param si slice index of slice
 * @param length length of slice
 * @return value of contribution of slice si to *he's value
 */
static hash_value_type own_value_of_data_direct(dhtElement const *he,
                                                slice_index si,
                                                stip_length_type length)
{
  hash_value_type result;

  hash_value_type const succ = get_value_direct_succ(he,si);
  hash_value_type const nosucc = get_value_direct_nosucc(he,si);
  hash_value_type const succ_neg = length-succ;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%p",he);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u\n",length);

  TraceValue("%u",succ);
  TraceValue("%u\n",nosucc);

  assert(succ<=length);
  result = succ_neg>nosucc ? succ_neg : nosucc;
  
  TraceFunctionExit(__func__);
  TraceFunctionResult("%u\n",result);
  return result;
}

/* Determine the contribution of a help slice (or leaf slice with help
 * end) to the value of a hash table element node.
 * @param he address of hash table element to determine value of
 * @param si slice index of help slice
 * @return value of contribution of slice si to *he's value
 */
static hash_value_type own_value_of_data_help(dhtElement const *he,
                                              slice_index si)
{
  hash_value_type const odd = get_value_help_odd(he,si);
  hash_value_type const even = get_value_help_even(he,si);
  hash_value_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%p ",he);
  TraceFunctionParam("%u\n",si);

  TraceValue("%u",odd);
  TraceValue("%u\n",even);

  result = even>odd ? even*2 : odd*2+1;

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u\n",result);
  return result;
}

/* Determine the contribution of a series slice to the value of
 * a hash table element node.
 * @param he address of hash table element to determine value of
 * @param si slice index of series slice
 * @return value of contribution of slice si to *he's value
 */
static hash_value_type own_value_of_data_series(dhtElement const *he,
                                                slice_index si)
{
  return get_value_series(he,si);
}

/* Determine the contribution of a leaf slice to the value of
 * a hash table element node.
 * @param he address of hash table element to determine value of
 * @param leaf slice index of composite slice
 * @return value of contribution of the leaf slice to *he's value
 */
static hash_value_type own_value_of_data_leaf(dhtElement const *he,
                                              slice_index leaf)
{
  switch (slices[leaf].type)
  {
    case STLeafHelp:
      return own_value_of_data_help(he,leaf);

    case STLeafDirect:
    case STLeafSelf:
      return own_value_of_data_direct(he,leaf,1);

    case STLeafForced:
      return 0;

    default:
      assert(0);
      return 0;
  }
}

/* Determine the contribution of a composite slice to the value of
 * a hash table element node.
 * @param he address of hash table element to determine value of
 * @param si slice index of composite slice
 * @return value of contribution of the slice si to *he's value
 */
static hash_value_type own_value_of_data_composite(dhtElement const *he,
                                                   slice_index si)
{
  hash_value_type result = 0;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%p ",he);
  TraceFunctionParam("%u\n",si);

  switch (slices[si].type)
  {
    case STBranchDirect:
      result = own_value_of_data_direct(he,si,slices[si].u.branch_d.length);
      break;

    case STBranchHelp:
      result = own_value_of_data_help(he,si);
      break;

    case STBranchSeries:
      result = own_value_of_data_series(he,si);
      break;

    default:
      assert(0);
      break;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%08x\n",result);
  return result;
}

/* Determine the contribution of a stipulation subtree to the value of
 * a hash table element node.
 * @param he address of hash table element to determine value of
 * @param offset bit offset for subtree
 * @param si slice index of subtree root slice
 * @return value of contribuation of the subtree to *he's value
 */
static hash_value_type value_of_data_recursive(dhtElement const *he,
                                               unsigned int offset,
                                               slice_index si)
{
  hash_value_type result = 0;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%p ",he);
  TraceFunctionParam("%u",offset);
  TraceFunctionParam("%u\n",si);

  if (base_slice[si]==no_slice)
  {
    offset -= slice_properties[si].value_size;
    TraceValue("%u",slices[si].type);
    TraceValue("%u",slice_properties[si].value_size);
    TraceValue("->%u\n",offset);

    switch (slices[si].type)
    {
      case STLeafSelf:
      case STLeafDirect:
      case STLeafHelp:
      {
        result = own_value_of_data_leaf(he,si) << offset;
        break;
      }

      case STLeafForced:
        result = 0;
        break;

      case STQuodlibet:
      {
        slice_index const op1 = slices[si].u.quodlibet.op1;
        slice_index const op2 = slices[si].u.quodlibet.op2;

        hash_value_type const nested_value1 = value_of_data_recursive(he,
                                                                      offset,
                                                                      op1);
        hash_value_type const nested_value2 = value_of_data_recursive(he,
                                                                      offset,
                                                                      op2);

        result = nested_value1>nested_value2 ? nested_value1 : nested_value2;
        break;
      }

      case STReciprocal:
      {
        slice_index const op1 = slices[si].u.reciprocal.op1;
        slice_index const op2 = slices[si].u.reciprocal.op2;

        hash_value_type const nested_value1 = value_of_data_recursive(he,
                                                                      offset,
                                                                      op1);
        hash_value_type const nested_value2 = value_of_data_recursive(he,
                                                                      offset,
                                                                      op2);

        result = nested_value1>nested_value2 ? nested_value1 : nested_value2;
        break;
      }

      case STNot:
      {
        slice_index const op = slices[si].u.not.op;
        result = value_of_data_recursive(he,offset,op);
        break;
      }

      case STMoveInverter:
      {
        slice_index const next = slices[si].u.move_inverter.next;
        result = value_of_data_recursive(he,offset,next);
        break;
      }

      case STBranchDirect:
      {
        hash_value_type const own_value = own_value_of_data_composite(he,si);
        slice_index const peer = slices[si].u.branch_d.peer;
        hash_value_type const nested_value = value_of_data_recursive(he,
                                                                     offset,
                                                                     peer);
        result = (own_value << offset) + nested_value;
        break;
      }

      case STBranchDirectDefender:
      {
        slice_index const next = slices[si].u.branch_d_defender.next;
        result = value_of_data_recursive(he,offset,next);
        break;
      }

      case STBranchHelp:
      case STBranchSeries:
      {
        hash_value_type const own_value = own_value_of_data_composite(he,si);

        slice_index const next = slices[si].u.branch.next;
        hash_value_type const nested_value =
            value_of_data_recursive(he,offset,next);
        TraceValue("%x ",own_value);
        TraceValue("%x\n",nested_value);

        result = (own_value << offset) + nested_value;
        break;
      }

      default:
        assert(0);
        break;
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%08x\n",result);
  return result;
}

/* How much is element *he worth to us? This information is used to
 * determine which elements to discard from the hash table if it has
 * reached its capacity.
 * @param he address of hash table element to determine value of
 * @return value of *he
 */
static hash_value_type value_of_data(dhtElement const *he)
{
  unsigned int const offset = sizeof(data_type)*CHAR_BIT;
  hash_value_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%p\n",he);

  TraceValue("%08x\n",((element_t *)he)->data);

  result = value_of_data_recursive(he,offset,root_slice);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%08x\n",result);
  return result;
}

static unsigned long totalRemoveCount = 0;

static void compresshash (void)
{
  dhtElement *he;
  hash_value_type x;
  unsigned long RemoveCnt, ToDelete;
#if defined(TESTHASH)
  unsigned long initCnt, visitCnt, runCnt;
#endif
  unsigned int val_step = 1;

  ++compression_counter;
  
  he= dhtGetFirstElement(pyhash);
  if (he!=0)
  {
    hash_value_type min_val = value_of_data(he);
    he = dhtGetNextElement(pyhash);
    while (he)
    {
      x = value_of_data(he);
      if (x<min_val)
        min_val = x;
      he= dhtGetNextElement(pyhash);
    }
    RemoveCnt= 0;
    ToDelete= dhtKeyCount(pyhash)/16 + 1;
    if (ToDelete >= dhtKeyCount(pyhash))
      ToDelete= dhtKeyCount(pyhash);
    /* this is a pathological case: it may only occur, when we are so
     * low on memory, that only one or no position can be stored.
     */

    while ((val_step&min_val)==0)
      val_step <<= 1;

#if defined(TESTHASH)
    printf("\nmin_val: %08x\n", min_val);
    printf("\nval_step: %08x\n", val_step);
    printf("ToDelete: %ld\n", ToDelete);
    fflush(stdout);
    initCnt= dhtKeyCount(pyhash);
    runCnt= 0;
#endif  /* TESTHASH */

    while (RemoveCnt < ToDelete)
    {
      min_val += val_step;

#if defined(TESTHASH)
      printf("min_val: %08x\n", min_val);
      printf("RemoveCnt: %ld\n", RemoveCnt);
      fflush(stdout);
      visitCnt= 0;
#endif  /* TESTHASH */

      for (he = dhtGetFirstElement(pyhash);
           he!=0;
           he= dhtGetNextElement(pyhash))
        if (value_of_data(he)<=min_val)
        {
          RemoveCnt++;
          totalRemoveCount++;
          dhtRemoveElement(pyhash, he->Key);
#if defined(TESTHASH)
          if (RemoveCnt + dhtKeyCount(pyhash) != initCnt)
          {
            fprintf(stdout,
                    "dhtRemove failed on %ld-th element of run %ld. "
                    "This was the %ld-th call to dhtRemoveElement.\n"
                    "RemoveCnt=%ld, dhtKeyCount=%ld, initCnt=%ld\n",
                    visitCnt, runCnt, totalRemoveCount,
                    RemoveCnt, dhtKeyCount(pyhash), initCnt);
            exit(1);
          }
#endif  /* TESTHASH */
        }
#if defined(TESTHASH)
      visitCnt++;
#endif  /* TESTHASH */
#if defined(TESTHASH)
      runCnt++;
      printf("run=%ld, RemoveCnt: %ld, missed: %ld\n",
             runCnt, RemoveCnt, initCnt-visitCnt);
      {
        int l, counter[16];
        int KeyCount=dhtKeyCount(pyhash);
        dhtBucketStat(pyhash, counter, 16);
        for (l=0; l< 16-1; l++)
          fprintf(stdout, "%d %d %d\n", KeyCount, l+1, counter[l]);
        printf("%d %d %d\n\n", KeyCount, l+1, counter[l]);
        if (runCnt > 9)
          printf("runCnt > 9 after %ld-th call to  dhtRemoveElement\n",
                 totalRemoveCount);
        dhtDebug= runCnt == 9;
      }
      fflush(stdout);
#endif  /* TESTHASH */

    }
#if defined(TESTHASH)
    printf("%ld;", dhtKeyCount(pyhash));
#if defined(HASHRATE)
    printf(" usage: %ld", use_pos);
    printf(" / %ld", use_all);
    printf(" = %ld%%", (100 * use_pos) / use_all);
#endif
#if defined(FREEMAP) && defined(FXF)
    PrintFreeMap(stdout);
#endif /*FREEMAP*/
#if defined(__TURBOC__)
    gotoxy(1, wherey());
#else
    printf("\n");
#endif /*__TURBOC__*/
#if defined(FXF)
    printf("\n after compression:\n");
    fxfInfo(stdout);
#endif /*FXF*/
#endif /*TESTHASH*/
  }
} /* compresshash */

#if defined(HASHRATE)
/* Level = 0: No output of HashStat
 * Level = 1: Output with every trace output
 * Level = 2: Output at each table compression
 * Level = 3: Output at every 1000th hash entry
 * a call to HashStats with a value of 0 will
 * always print
 */
static unsigned int HashRateLevel = 0;

void IncHashRateLevel(void)
{
  ++HashRateLevel;
  StdString("  ");
  PrintTime();
  logIntArg(HashRateLevel);
  Message(IncrementHashRateLevel);
  HashStats(0, "\n");
}

void DecHashRateLevel(void)
{
  if (HashRateLevel>0)
    --HashRateLevel;
  StdString("  ");
  PrintTime();
  logIntArg(HashRateLevel);
  Message(DecrementHashRateLevel);
  HashStats(0, "\n");
}

#else

void IncHashRateLevel(void)
{
  /* intentionally nothing */
}

void DecHashRateLevel(void)
{
  /* intentionally nothing */
}

#endif

void HashStats(unsigned int level, char *trailer)
{
#if defined(HASHRATE)
  int pos=dhtKeyCount(pyhash);
  char rate[60];

  if (level<=HashRateLevel)
  {
    StdString("  ");
    pos= dhtKeyCount(pyhash);
    logIntArg(pos);
    Message(HashedPositions);
    if (use_all > 0)
    {
      if (use_all < 10000)
        sprintf(rate, " %ld/%ld = %ld%%",
                use_pos, use_all, (use_pos*100) / use_all);
      else
        sprintf(rate, " %ld/%ld = %ld%%",
                use_pos, use_all, use_pos / (use_all/100));
    }
    else
      sprintf(rate, " -");
    StdString(rate);
    if (HashRateLevel > 3)
    {
      unsigned long msec;
      unsigned long Seconds;
      StopTimer(&Seconds,&msec);
      if (Seconds > 0)
      {
        sprintf(rate, ", %lu pos/s", use_all/Seconds);
        StdString(rate);
      }
    }
    if (trailer)
      StdString(trailer);
  }
#endif /*HASHRATE*/
}

static int estimateNumberOfHoles(slice_index si)
{
  int result = 0;

  /*
   * I assume an average of (nr_files_on_board*nr_rows_on_board -
   * number of pieces)/2 additional holes per position.
   */
  switch (slices[si].type)
  {
    case STBranchDirect:
      result = 2*slices[si].u.branch_d.length;
      break;

    case STBranchDirectDefender:
      result = 2*slices[si].u.branch_d_defender.length;
      break;

    case STBranchHelp:
      result = 2*slices[si].u.branch.length;
      break;

    case STBranchSeries:
      /* That's far too much. In a ser-h#5 there won't be more
       * than 5 holes in hashed positions.      TLi
       */
      result = slices[si].u.branch.length;
      break;

    case STMoveInverter:
      result = estimateNumberOfHoles(slices[si].u.move_inverter.next);
      break;

    case STQuodlibet:
    {
      int const result1 = estimateNumberOfHoles(slices[si].u.quodlibet.op1);
      int const result2 = estimateNumberOfHoles(slices[si].u.quodlibet.op2);
      result = result1>result2 ? result1 : result2;
      break;
    }

    case STReciprocal:
    {
      int const result1 = estimateNumberOfHoles(slices[si].u.reciprocal.op1);
      int const result2 = estimateNumberOfHoles(slices[si].u.reciprocal.op2);
      result = result1>result2 ? result1 : result2;
      break;
    }

    case STNot:
      result = estimateNumberOfHoles(slices[si].u.not.op);
      break;

    default:
      printf("%u\n",slices[si].type);
      assert(0);
      break;
  }

  return result;
}

static int TellCommonEncodePosLeng(int len, int nbr_p)
{
  len++; /* Castling_Flag */

  if (CondFlag[haanerchess])
  {
    int nbr_holes = estimateNumberOfHoles(root_slice);
    if (nbr_holes > (nr_files_on_board*nr_rows_on_board-nbr_p)/2)
      nbr_holes= (nr_files_on_board*nr_rows_on_board-nbr_p)/2;
    len += bytes_per_piece*nbr_holes;
  }

  if (CondFlag[messigny])
    len+= 2;

  if (CondFlag[duellist])
    len+= 2;

  if (CondFlag[blfollow] || CondFlag[whfollow] || CondFlag[champursue])
    len++;

  if (flag_synchron)
    len++;

  if (CondFlag[imitators])
  {
    unsigned int imi_idx;
    for (imi_idx = 0; imi_idx<inum[nbply]; imi_idx++)
      len++;

    /* coding of no. of imitators and average of one
       imitator-promotion assumed.
    */
    len+=2;
  }

  if (CondFlag[parrain])
    /*
    ** only one out of three positions with a capture
    ** assumed.
    */
    len++;

  if (OptFlag[nontrivial])
    len++;

  if (is_there_slice_with_nonstandard_min_length)
    len++;

  if (CondFlag[disparate])
    len++;

  return len;
} /* TellCommonEncodePosLeng */

static int TellLargeEncodePosLeng(void)
{
  square    *bnp;
  int       nbr_p= 0, len= 8;

  for (bnp= boardnum; *bnp; bnp++)
    if (e[*bnp] != vide)
    {
      len += bytes_per_piece;
      nbr_p++;  /* count no. of pieces and holes */
    }

  if (CondFlag[BGL])
    len+= sizeof BGL_white + sizeof BGL_black;

  len += nr_ghosts*bytes_per_piece;

  return TellCommonEncodePosLeng(len, nbr_p);
} /* TellLargeEncodePosLeng */

static int TellSmallEncodePosLeng(void)
{
  square  *bnp;
  int nbr_p= 0, len= 0;

  for (bnp= boardnum; *bnp; bnp++)
  {
    /* piece    p;
    ** Flags    pspec;
    */
    if (e[*bnp] != vide)
    {
      len += 1 + bytes_per_piece;
      nbr_p++;            /* count no. of pieces and holes */
    }
  }

  len += nr_ghosts*bytes_per_piece;
  
  return TellCommonEncodePosLeng(len, nbr_p);
} /* TellSmallEncodePosLeng */

static byte *CommonEncode(byte *bp)
{
  if (CondFlag[messigny]) {
    if (move_generation_stack[nbcou].capture == messigny_exchange) {
      *bp++ = (byte)(move_generation_stack[nbcou].arrival - square_a1);
      *bp++ = (byte)(move_generation_stack[nbcou].departure - square_a1);
    }
    else {
      *bp++ = (byte)(0);
      *bp++ = (byte)(0);
    }
  }
  if (CondFlag[duellist]) {
    *bp++ = (byte)(whduell[nbply] - square_a1);
    *bp++ = (byte)(blduell[nbply] - square_a1);
  }

  if (CondFlag[blfollow] || CondFlag[whfollow] || CondFlag[champursue])
    *bp++ = (byte)(move_generation_stack[nbcou].departure - square_a1);

  if (flag_synchron)
    *bp++= (byte)(sq_num[move_generation_stack[nbcou].departure]
                  -sq_num[move_generation_stack[nbcou].arrival]
                  +64);

  if (CondFlag[imitators])
  {
    unsigned int imi_idx;

    /* The number of imitators has to be coded too to avoid
     * ambiguities.
     */
    *bp++ = (byte)inum[nbply];
    for (imi_idx = 0; imi_idx<inum[nbply]; imi_idx++)
      *bp++ = (byte)(isquare[imi_idx]-square_a1);
  }

  if (OptFlag[nontrivial])
    *bp++ = (byte)(max_nr_nontrivial);

  if (CondFlag[parrain]) {
    /* a piece has been captured and can be reborn */
    *bp++ = (byte)(move_generation_stack[nbcou].capture - square_a1);
    if (one_byte_hash) {
      *bp++ = (byte)(pprispec[nbply])
          + ((byte)(piece_nbr[abs(pprise[nbply])]) << (CHAR_BIT/2));
    }
    else {
      *bp++ = pprise[nbply];
      *bp++ = (byte)(pprispec[nbply]>>CHAR_BIT);
      *bp++ = (byte)(pprispec[nbply]&ByteMask);
    }
  }

  if (is_there_slice_with_nonstandard_min_length)
    *bp++ = (byte)(nbply);

  if (ep[nbply]!=initsquare)
    *bp++ = (byte)(ep[nbply] - square_a1);

  *bp++ = castling_flag[nbply];     /* Castling_Flag */

  if (CondFlag[BGL]) {
    memcpy(bp, &BGL_white, sizeof BGL_white);
    bp += sizeof BGL_white;
    memcpy(bp, &BGL_black, sizeof BGL_black);
    bp += sizeof BGL_black;
  }

  if (CondFlag[disparate]) {
    *bp++ = (byte)(nbply>=2?pjoue[nbply]:vide);
  }

  return bp;
} /* CommonEncode */

static byte *LargeEncodePiece(byte *bp, byte *position,
                              int row, int col,
                              piece p, Flags pspec)
{
  if (!TSTFLAG(pspec, Neutral))
    SETFLAG(pspec, (p < vide ? Black : White));
  p = abs(p);
  if (one_byte_hash)
    *bp++ = (byte)pspec + ((byte)piece_nbr[p] << (CHAR_BIT/2));
  else
  {
    unsigned int i;
    *bp++ = p;
    for (i = 0; i<bytes_per_spec; i++)
      *bp++ = (byte)((pspec>>(CHAR_BIT*i)) & ByteMask);
  }

  position[row] |= BIT(col);

  return bp;
}

static void LargeEncode(void)
{
  HashBuffer *hb = &hashBuffers[nbply];
  byte *position = hb->cmv.Data;
  byte *bp = position+nr_rows_on_board;
  int row, col;
  square a_square = square_a1;
  ghost_index_type gi;

  /* detect cases where we encode the same position twice */
  assert(!isHashBufferValid[nbply]);

  /* clear the bits for storing the position of pieces */
  memset(position,0,nr_rows_on_board);

  for (row=0; row<nr_rows_on_board; row++, a_square+= onerow)
  {
    square curr_square = a_square;
    for (col=0; col<nr_files_on_board; col++, curr_square+= dir_right)
    {
      piece const p = e[curr_square];
      if (p!=vide)
        bp = LargeEncodePiece(bp,position,row,col,p,spec[curr_square]);
    }
  }

  for (gi = 0; gi<nr_ghosts; ++gi)
  {
    square s = (ghosts[gi].ghost_square
                - nr_of_slack_rows_below_board*onerow
                - nr_of_slack_files_left_of_board);
    row = s/onerow;
    col = s%onerow;
    bp = LargeEncodePiece(bp,position,
                          row,col,
                          ghosts[gi].ghost_piece,ghosts[gi].ghost_flags);
  }

  /* Now the rest of the party */
  bp = CommonEncode(bp);

  assert(bp-hb->cmv.Data<=UCHAR_MAX);
  hb->cmv.Leng = (unsigned char)(bp-hb->cmv.Data);

  validateHashBuffer();
} /* LargeEncode */

static byte *SmallEncodePiece(byte *bp,
                              int row, int col,
                              piece p, Flags pspec)
{
  if (!TSTFLAG(pspec,Neutral))
    SETFLAG(pspec, (p < vide ? Black : White));
  p= abs(p);
  *bp++= (byte)((row<<(CHAR_BIT/2))+col);
  if (one_byte_hash)
    *bp++ = (byte)pspec + ((byte)piece_nbr[p] << (CHAR_BIT/2));
  else
  {
    unsigned int i;
    *bp++ = p;
    for (i = 0; i<bytes_per_spec; i++)
      *bp++ = (byte)((pspec>>(CHAR_BIT*i)) & ByteMask);
  }

  return bp;
}

static void SmallEncode(void)
{
  HashBuffer *hb = &hashBuffers[nbply];
  byte *bp = hb->cmv.Data;
  square a_square = square_a1;
  int row;
  int col;
  ghost_index_type gi;

  /* detect cases where we encode the same position twice */
  assert(!isHashBufferValid[nbply]);

  for (row=0; row<nr_rows_on_board; row++, a_square += onerow)
  {
    square curr_square= a_square;
    for (col=0; col<nr_files_on_board; col++, curr_square += dir_right)
    {
      piece const p = e[curr_square];
      if (p!=vide)
        bp = SmallEncodePiece(bp,row,col,p,spec[curr_square]);
    }
  }

  for (gi = 0; gi<nr_ghosts; ++gi)
  {
    square s = (ghosts[gi].ghost_square
                - nr_of_slack_rows_below_board*onerow
                - nr_of_slack_files_left_of_board);
    row = s/onerow;
    col = s%onerow;
    bp = SmallEncodePiece(bp,
                          row,col,
                          ghosts[gi].ghost_piece,ghosts[gi].ghost_flags);
  }

  /* Now the rest of the party */
  bp = CommonEncode(bp);

  assert(bp-hb->cmv.Data<=UCHAR_MAX);
  hb->cmv.Leng = (unsigned char)(bp-hb->cmv.Data);

  validateHashBuffer();
}

boolean inhash(slice_index si, hashwhat what, hash_value_type val)
{
  boolean result = false;
  HashBuffer *hb = &hashBuffers[nbply];
  dhtElement const *he;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",what);
  TraceFunctionParam("%u\n",val);

  TraceValue("%u\n",nbply);

  assert(isHashBufferValid[nbply]);

  ifHASHRATE(use_all++);

  /* TODO create hash slice(s) that are only active if we can
   * allocated the hash table. */
  he = pyhash==0 ? dhtNilElement : dhtLookupElement(pyhash, (dhtValue)hb);
  if (he==dhtNilElement)
    result = false;
  else
    switch (what)
    {
      case SerNoSucc:
      {
        hash_value_type const nosucc = get_value_series(he,si);
        if (nosucc>=val
            && (nosucc+slices[si].u.branch.min_length
                <=val+slices[si].u.branch.length))
        {
          ifHASHRATE(use_pos++);
          result = true;
        }
        else
          result = false;
        break;
      }
      case HelpNoSuccOdd:
      {
        hash_value_type const nosucc = get_value_help_odd(he,si);
        if (nosucc>=val
            && (nosucc+slices[si].u.branch.min_length
                <=val+slices[si].u.branch.length))
        {
          ifHASHRATE(use_pos++);
          result = true;
        }
        else
          result = false;
        break;
      }
      case HelpNoSuccEven:
      {
        hash_value_type const nosucc = get_value_help_even(he,si);
        if (nosucc>=val
            && (nosucc+slices[si].u.branch.min_length
                <=val+slices[si].u.branch.length))
        {
          ifHASHRATE(use_pos++);
          result = true;
        }
        else
          result = false;
        break;
      }
      case DirNoSucc:
      {
        hash_value_type const nosucc = get_value_direct_nosucc(he,si);
        if (nosucc>=val
            && (nosucc+slices[si].u.branch_d.min_length
                <=val+slices[si].u.branch_d.length))
        {
          ifHASHRATE(use_pos++);
          result = true;
        } else
          result = false;
        break;
      }
      case DirSucc:
      {
        hash_value_type const succ = get_value_direct_succ(he,si);
        if (succ<=val
            && (succ+slices[si].u.branch_d.length
                >=val+slices[si].u.branch_d.min_length))
        {
          ifHASHRATE(use_pos++);
          result = true;
        } else
          result = false;
        break;
      }

      default:
        assert(0);
        break;
    }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u\n",result);
  return result; /* avoid compiler warning */
} /* inhash */

/* Initialise the bits representing a direct slice in a hash table
 * element's data field with null values
 * @param he address of hash table element
 * @param si slice index of series slice
 */
static void init_element_direct(dhtElement *he,
                                slice_index si,
                                unsigned int length)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%p",he);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u\n",length);

  set_value_direct_nosucc(he,si,0);
  set_value_direct_succ(he,si,length);

  TraceFunctionExit(__func__);
  TraceText("\n");
}

/* Initialise the bits representing a help slice in a hash table
 * element's data field with null values
 * @param he address of hash table element
 * @param si slice index of series slice
 */
static void init_element_help(dhtElement *he, slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%p",he);
  TraceFunctionParam("%u\n",si);

  set_value_help_even(he,si,0);
  set_value_help_odd(he,si,0);

  TraceFunctionExit(__func__);
  TraceText("\n");
}

/* Initialise the bits representing a series slice in a hash table
 * element's data field with null values
 * @param he address of hash table element
 * @param si slice index of series slice
 */
static void init_element_series(dhtElement *he, slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%p",he);
  TraceFunctionParam("%u\n",si);

  set_value_series(he,si,0);

  TraceFunctionExit(__func__);
  TraceText("\n");
}

/* Initialise the bits representing a slice (including its possible
 * descendants) in a hash table element's data field with null values
 * @param he address of hash table element
 * @param si slice index of slice
 * @param element_initialised remembers which slices have already been
 *                            visited (to avoid infinite recursion)
 * @note this is a recursive function
 */
static void init_element(dhtElement *he,
                         slice_index si,
                         boolean element_initialised[max_nr_slices])
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%p",he);
  TraceFunctionParam("%u\n",si);

  element_initialised[si] = true;

  TraceValue("%u\n",slices[si].type);
  switch (slices[si].type)
  {
    case STLeafHelp:
      init_element_help(he,si);
      break;

    case STLeafDirect:
    case STLeafSelf:
      init_element_direct(he,si,1);
      break;

    case STLeafForced:
      /* nothing */
      break;

    case STReciprocal:
      init_element(he,slices[si].u.reciprocal.op1,element_initialised);
      init_element(he,slices[si].u.reciprocal.op2,element_initialised);
      break;

    case STQuodlibet:
      init_element(he,slices[si].u.quodlibet.op1,element_initialised);
      init_element(he,slices[si].u.quodlibet.op2,element_initialised);
      break;

    case STNot:
      init_element(he,slices[si].u.not.op,element_initialised);
      break;

    case STBranchDirect:
      if (base_slice[si]==no_slice)
      {
        init_element_direct(he,si,slices[si].u.branch_d.length);
        if (!element_initialised[slices[si].u.branch_d.peer])
          init_element(he,slices[si].u.branch_d.peer,element_initialised);
      }
      break;

    case STBranchDirectDefender:
      init_element(he,slices[si].u.branch_d_defender.next,element_initialised);
      if (!element_initialised[slices[si].u.branch_d_defender.peer])
        init_element(he,slices[si].u.branch_d_defender.peer,element_initialised);
      break;

    case STBranchHelp:
      if (base_slice[si]==no_slice)
      {
        init_element_help(he,si);
        init_element(he,slices[si].u.branch.next,element_initialised);
      }
      break;
      
    case STBranchSeries:
      init_element_series(he,si);
      init_element(he,slices[si].u.branch.next,element_initialised);
      break;

    case STMoveInverter:
    {
      slice_index const next = slices[si].u.move_inverter.next;
      init_element(he,next,element_initialised);
      break;
    }

    case STConstant:
      /* nothing */
      break;

    default:
      assert(0);
      break;
  }

  TraceFunctionExit(__func__);
  TraceText("\n");
}

/* Initialise the bits representing all slices in a hash table
 * element's data field with null values 
 * @param he address of hash table element
 */
static void init_elements(dhtElement *he)
{
  boolean element_initialised[max_nr_slices] = { false };
  init_element(he,root_slice,element_initialised);
}

/* (attempt to) allocate a hash table element - compress the current
 * hash table if necessary; exit()s if allocation is not possible
 * in spite of compression
 * @param hb has value (basis for calculation of key)
 * @return address of element
 */
static dhtElement *allocDHTelement(dhtValue hb)
{
  dhtElement *result= dhtEnterElement(pyhash, (dhtValue)hb, 0);
  unsigned long nrKeys = dhtKeyCount(pyhash);
  while (result==dhtNilElement)
  {
    compresshash();
    if (dhtKeyCount(pyhash)==nrKeys)
    {
      /* final attempt */
      inithash();
      result = dhtEnterElement(pyhash, (dhtValue)hb, 0);
      break;
    }
    else
    {
      nrKeys = dhtKeyCount(pyhash);
      result = dhtEnterElement(pyhash, (dhtValue)hb, 0);
    }
  }

  if (result==dhtNilElement)
  {
    fprintf(stderr,
            "Sorry, cannot enter more hashelements "
            "despite compression\n");
    exit(-2);
  }

  return result;
}

void addtohash(slice_index si, hashwhat what, hash_value_type val)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",what);
  TraceFunctionParam("%u\n",val);

  TraceValue("%u\n",nbply);

  /* TODO create hash slice(s) that are only active if we can
   * allocated the hash table. */
  if (pyhash!=0)
  {
    HashBuffer *hb = &hashBuffers[nbply];
    dhtElement *he = dhtLookupElement(pyhash, (dhtValue)hb);

    assert(isHashBufferValid[nbply]);

    if (he == dhtNilElement)
    {
      /* the position is new */
      he = allocDHTelement((dhtValue)hb);
      he->Data = template_element.Data;

      switch (what)
      {
        case SerNoSucc:
          set_value_series(he,si,val);
          break;

        case HelpNoSuccOdd:
          set_value_help_odd(he,si,val);
          break;

        case HelpNoSuccEven:
          set_value_help_even(he,si,val);
          break;

        case DirSucc:
          set_value_direct_succ(he,si,val);
          break;

        case DirNoSucc:
          set_value_direct_nosucc(he,si,val);
          break;

        default:
          assert(0);
          break;
      }
    }
    else
      switch (what)
      {
        /* TODO use optimized operation? */
        case SerNoSucc:
          if (get_value_series(he,si)<val)
            set_value_series(he,si,val);
          break;

        case HelpNoSuccOdd:
          if (get_value_help_odd(he,si)<val)
            set_value_help_odd(he,si,val);
          break;

        case HelpNoSuccEven:
          if (get_value_help_even(he,si)<val)
            set_value_help_even(he,si,val);
          break;

        case DirSucc:
          if (get_value_direct_succ(he,si)>val)
            set_value_direct_succ(he,si,val);
          break;

        case DirNoSucc:
          if (get_value_direct_nosucc(he,si)<val)
            set_value_direct_nosucc(he,si,val);
          break;

        default:
          assert(0);
          break;
      }
  }
  
  TraceFunctionExit(__func__);
  TraceText("\n");

#if defined(HASHRATE)
  if (dhtKeyCount(pyhash)%1000 == 0)
    HashStats(3, "\n");
#endif /*HASHRATE*/
} /* addtohash */

void inithash(void)
{
  int Small, Large;
  int i, j;

  TraceFunctionEntry(__func__);
  TraceText("\n");

  ifTESTHASH(
      sprintf(GlobalStr, "calling inithash\n");
      StdString(GlobalStr)
      );

#if defined(__unix) && defined(TESTHASH)
  OldBreak= sbrk(0);
#endif /*__unix,TESTHASH*/

#if defined(FXF)
  if (fxfInit(MaxMemory) == -1) /* we didn't get hashmemory ... */
    FtlMsg(NoMemory);
  ifTESTHASH(fxfInfo(stdout));
#endif /*FXF*/

  is_there_slice_with_nonstandard_min_length = false;

  compression_counter = 0;

  init_slice_properties();
  init_elements(&template_element);

  dhtRegisterValue(dhtBCMemValue, 0, &dhtBCMemoryProcs);
  dhtRegisterValue(dhtSimpleValue, 0, &dhtSimpleProcs);
  pyhash= dhtCreate(dhtBCMemValue, dhtCopy, dhtSimpleValue, dhtNoCopy);
  if (pyhash==0)
  {
    TraceValue("%s\n",dhtErrorMsg());
  }

  ifHASHRATE(use_pos = use_all = 0);

  /* check whether a piece can be coded in a single byte */
  j = 0;
  for (i = PieceCount; Empty < i; i--)
    if (exist[i])
      piece_nbr[i] = j++;

  if (CondFlag[haanerchess])
    piece_nbr[obs]= j++;

  one_byte_hash = j<(1<<(CHAR_BIT/2)) && PieSpExFlags<(1<<(CHAR_BIT/2));

  bytes_per_spec= 1;
  if ((PieSpExFlags >> CHAR_BIT) != 0)
    bytes_per_spec++;
  if ((PieSpExFlags >> 2*CHAR_BIT) != 0)
    bytes_per_spec++;

  bytes_per_piece= one_byte_hash ? 1 : 1+bytes_per_spec;

  if (isIntelligentModeActive)
  {
    one_byte_hash = false;
    bytes_per_spec= 5; /* TODO why so high??? */
  }

  if (slices[1].u.leaf.goal==goal_proof
      || slices[1].u.leaf.goal==goal_atob)
  {
    encode = ProofEncode;
    if (MaxMemory>0 && MaxPositions==0)
      MaxPositions= MaxMemory/(24+sizeof(char *)+1);
  }
  else
  {
    Small= TellSmallEncodePosLeng();
    Large= TellLargeEncodePosLeng();
    if (Small <= Large) {
      encode= SmallEncode;
      if (MaxMemory>0 && MaxPositions==0)
        MaxPositions= MaxMemory/(Small+sizeof(char *)+1);
    }
    else
    {
      encode= LargeEncode;
      if (MaxMemory>0 && MaxPositions==0)
        MaxPositions= MaxMemory/(Large+sizeof(char *)+1);
    }
  }

#if defined(FXF)
  ifTESTHASH(printf("MaxPositions: %7lu\n", MaxPositions));
  assert(MaxMemory/1024<UINT_MAX);
  ifTESTHASH(printf("MaxMemory:    %7u KB\n", (unsigned int)(MaxMemory/1024)));
#else
  ifTESTHASH(
      printf("room for up to %lu positions in hash table\n", MaxPositions));
#endif /*FXF*/

  invalidateHashBuffer(true); /* prevent the following line from firing an
                                 assert() */
  (*encode)(); /* TODO why is this necessary*/

  TraceFunctionExit(__func__);
  TraceText("\n");
} /* inithash */

void    closehash(void)
{
#if defined(TESTHASH)
  sprintf(GlobalStr, "calling closehash\n");
  StdString(GlobalStr);

#if defined(HASHRATE)
  sprintf(GlobalStr, "%ld enquiries out of %ld successful. ",
          use_pos, use_all);
  StdString(GlobalStr);
  if (use_all) {
    sprintf(GlobalStr, "Makes %ld%%\n", (100 * use_pos) / use_all);
    StdString(GlobalStr);
  }
#endif
#if defined(__unix)
  {
#if defined(FXF)
    unsigned long const HashMem = fxfTotal();
#else
    unsigned long const HashMem = sbrk(0)-OldBreak;
#endif /*FXF*/
    unsigned long const HashCount = pyhash==0 ? 0 : dhtKeyCount(pyhash);
    if (HashCount>0)
    {
      unsigned long const BytePerPos = (HashMem*100)/HashCount;
      sprintf(GlobalStr,
              "Memory for hash-table: %ld, "
              "gives %ld.%02ld bytes per position\n",
              HashMem, BytePerPos/100, BytePerPos%100);
    }
    else
      sprintf(GlobalStr, "Nothing in hashtable\n");
    StdString(GlobalStr);
#endif /*__unix*/
  }
#endif /*TESTHASH*/

  /* TODO create hash slice(s) that are only active if we can
   * allocated the hash table. */
  if (pyhash!=0)
  {
    dhtDestroy(pyhash);
    pyhash = 0;
  }

#if defined(TESTHASH) && defined(FXF)
  fxfInfo(stdout);
#endif /*TESTHASH,FXF*/
} /* closehash */

/* assert()s below this line must remain active even in "productive"
 * executables. */
#undef NDEBUG
#include <assert.h>

/* Check assumptions made in the hashing module. Abort if one of them
 * isn't met.
 * This is called from checkGlobalAssumptions() once at program start.
 */
void check_hash_assumptions(void)
{
  /* SmallEncode uses 1 byte for both row and file of a square */
  assert(nr_rows_on_board<1<<(CHAR_BIT/2));
  assert(nr_files_on_board<1<<(CHAR_BIT/2));

  /* LargeEncode() uses 1 bit per square */
  assert(nr_files_on_board<=CHAR_BIT);

  /* the encoding functions encode Flags as 2 bytes */
  assert(PieSpCount<=2*CHAR_BIT);
}
