/*
 * Contributed to the OpenSSL Project by the American Registry for
 * Internet Numbers ("ARIN").
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 */

/*
 * Implementation of RFC 3779 section 2.2.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cryptlib.h"
#include <openssl/conf.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/buffer.h>
#include <openssl/x509v3.h>

#ifndef OPENSSL_NO_RFC3779

/*
 * OpenSSL ASN.1 template translation of RFC 3779 2.2.3.
 */

ASN1_SEQUENCE(IPAddressRange) = {
  ASN1_SIMPLE(IPAddressRange, min, ASN1_BIT_STRING),
  ASN1_SIMPLE(IPAddressRange, max, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END(IPAddressRange)

ASN1_CHOICE(IPAddressOrRange) = {
  ASN1_SIMPLE(IPAddressOrRange, u.addressPrefix, ASN1_BIT_STRING),
  ASN1_SIMPLE(IPAddressOrRange, u.addressRange,  IPAddressRange)
} ASN1_CHOICE_END(IPAddressOrRange)

ASN1_CHOICE(IPAddressChoice) = {
  ASN1_SIMPLE(IPAddressChoice,      u.inherit,           ASN1_NULL),
  ASN1_SEQUENCE_OF(IPAddressChoice, u.addressesOrRanges, IPAddressOrRange)
} ASN1_CHOICE_END(IPAddressChoice)

ASN1_SEQUENCE(IPAddressFamily) = {
  ASN1_SIMPLE(IPAddressFamily, addressFamily,   ASN1_OCTET_STRING),
  ASN1_SIMPLE(IPAddressFamily, ipAddressChoice, IPAddressChoice)
} ASN1_SEQUENCE_END(IPAddressFamily)

ASN1_ITEM_TEMPLATE(IPAddrBlocks) = 
  ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0,
			IPAddrBlocks, IPAddressFamily)
ASN1_ITEM_TEMPLATE_END(IPAddrBlocks)

IMPLEMENT_ASN1_FUNCTIONS(IPAddressRange)
IMPLEMENT_ASN1_FUNCTIONS(IPAddressOrRange)
IMPLEMENT_ASN1_FUNCTIONS(IPAddressChoice)
IMPLEMENT_ASN1_FUNCTIONS(IPAddressFamily)

/*
 * How much buffer space do we need for a raw address?
 */
#define ADDR_RAW_BUF_LEN	16

/*
 * What's the address length associated with this AFI?
 */
static int length_from_afi(const unsigned afi)
{
  switch (afi) {
  case IANA_AFI_IPV4:
    return 4;
  case IANA_AFI_IPV6:
    return 16;
  default:
    return 0;
  }
}

/*
 * Extract the AFI from an IPAddressFamily.
 */
unsigned int v3_addr_get_afi(const IPAddressFamily *f)
{
  return ((f != NULL &&
	   f->addressFamily != NULL &&
	   f->addressFamily->data != NULL)
	  ? ((f->addressFamily->data[0] << 8) |
	     (f->addressFamily->data[1]))
	  : 0);
}

/*
 * Expand the bitstring form of an address into a raw byte array.
 * At the moment this is coded for simplicity, not speed.
 */
static void addr_expand(unsigned char *addr,
			const ASN1_BIT_STRING *bs,
			const int length,
			const unsigned char fill)
{
  OPENSSL_assert(bs->length >= 0 && bs->length <= length);
  if (bs->length > 0) {
    memcpy(addr, bs->data, bs->length);
    if ((bs->flags & 7) != 0) {
      unsigned char mask = 0xFF >> (8 - (bs->flags & 7));
      if (fill == 0)
	addr[bs->length - 1] &= ~mask;
      else
	addr[bs->length - 1] |= mask;
    }
  }
  memset(addr + bs->length, fill, length - bs->length);
}

/*
 * Extract the prefix length from a bitstring.
 */
#define addr_prefixlen(bs) ((int) ((bs)->length * 8 - ((bs)->flags & 7)))

/*
 * i2r handler for one address bitstring.
 */
static int i2r_address(BIO *out,
		       const unsigned afi,
		       const unsigned char fill,
		       const ASN1_BIT_STRING *bs)
{
  unsigned char addr[ADDR_RAW_BUF_LEN];
  int i, n;

  if (bs->length < 0)
    return 0;
  switch (afi) {
  case IANA_AFI_IPV4:
    if (bs->length > 4)
      return 0;
    addr_expand(addr, bs, 4, fill);
    BIO_printf(out, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
    break;
  case IANA_AFI_IPV6:
    if (bs->length > 16)
      return 0;
    addr_expand(addr, bs, 16, fill);
    for (n = 16; n > 1 && addr[n-1] == 0x00 && addr[n-2] == 0x00; n -= 2)
      ;
    for (i = 0; i < n; i += 2)
      BIO_printf(out, "%x%s", (addr[i] << 8) | addr[i+1], (i < 14 ? ":" : ""));
    if (i < 16)
      BIO_puts(out, ":");
    if (i == 0)
      BIO_puts(out, ":");
    break;
  default:
    for (i = 0; i < bs->length; i++)
      BIO_printf(out, "%s%02x", (i > 0 ? ":" : ""), bs->data[i]);
    BIO_printf(out, "[%d]", (int) (bs->flags & 7));
    break;
  }
  return 1;
}

/*
 * i2r handler for a sequence of addresses and ranges.
 */
static int i2r_IPAddressOrRanges(BIO *out,
				 const int indent,
				 const IPAddressOrRanges *aors,
				 const unsigned afi)
{
  int i;
  for (i = 0; i < sk_IPAddressOrRange_num(aors); i++) {
    const IPAddressOrRange *aor = sk_IPAddressOrRange_value(aors, i);
    BIO_printf(out, "%*s", indent, "");
    switch (aor->type) {
    case IPAddressOrRange_addressPrefix:
      if (!i2r_address(out, afi, 0x00, aor->u.addressPrefix))
	return 0;
      BIO_printf(out, "/%d\n", addr_prefixlen(aor->u.addressPrefix));
      continue;
    case IPAddressOrRange_addressRange:
      if (!i2r_address(out, afi, 0x00, aor->u.addressRange->min))
	return 0;
      BIO_puts(out, "-");
      if (!i2r_address(out, afi, 0xFF, aor->u.addressRange->max))
	return 0;
      BIO_puts(out, "\n");
      continue;
    }
  }
  return 1;
}

/*
 * i2r handler for an IPAddrBlocks extension.
 */
static int i2r_IPAddrBlocks(const X509V3_EXT_METHOD *method,
			    void *ext,
			    BIO *out,
			    int indent)
{
  const IPAddrBlocks *addr = ext;
  int i;
  for (i = 0; i < sk_IPAddressFamily_num(addr); i++) {
    IPAddressFamily *f = sk_IPAddressFamily_value(addr, i);
    const unsigned int afi = v3_addr_get_afi(f);
    switch (afi) {
    case IANA_AFI_IPV4:
      BIO_printf(out, "%*sIPv4", indent, "");
      break;
    case IANA_AFI_IPV6:
      BIO_printf(out, "%*sIPv6", indent, "");
      break;
    default:
      BIO_printf(out, "%*sUnknown AFI %u", indent, "", afi);
      break;
    }
    if (f->addressFamily->length > 2) {
      switch (f->addressFamily->data[2]) {
      case   1:
	BIO_puts(out, " (Unicast)");
	break;
      case   2:
	BIO_puts(out, " (Multicast)");
	break;
      case   3:
	BIO_puts(out, " (Unicast/Multicast)");
	break;
      case   4:
	BIO_puts(out, " (MPLS)");
	break;
      case  64:
	BIO_puts(out, " (Tunnel)");
	break;
      case  65:
	BIO_puts(out, " (VPLS)");
	break;
      case  66:
	BIO_puts(out, " (BGP MDT)");
	break;
      case 128:
	BIO_puts(out, " (MPLS-labeled VPN)");
	break;
      default:  
	BIO_printf(out, " (Unknown SAFI %u)",
		   (unsigned) f->addressFamily->data[2]);
	break;
      }
    }
    switch (f->ipAddressChoice->type) {
    case IPAddressChoice_inherit:
      BIO_puts(out, ": inherit\n");
      break;
    case IPAddressChoice_addressesOrRanges:
      BIO_puts(out, ":\n");
      if (!i2r_IPAddressOrRanges(out,
				 indent + 2,
				 f->ipAddressChoice->u.addressesOrRanges,
				 afi))
	return 0;
      break;
    }
  }
  return 1;
}

/*
 * Sort comparison function for a sequence of IPAddressOrRange
 * elements.
 */
static int IPAddressOrRange_cmp(const IPAddressOrRange *a,
				const IPAddressOrRange *b,
				const int length)
{
  unsigned char addr_a[ADDR_RAW_BUF_LEN], addr_b[ADDR_RAW_BUF_LEN];
  int prefixlen_a = 0, prefixlen_b = 0;
  int r;

  switch (a->type) {
  case IPAddressOrRange_addressPrefix:
    addr_expand(addr_a, a->u.addressPrefix, length, 0x00);
    prefixlen_a = addr_prefixlen(a->u.addressPrefix);
    break;
  case IPAddressOrRange_addressRange:
    addr_expand(addr_a, a->u.addressRange->min, length, 0x00);
    prefixlen_a = length * 8;
    break;
  }

  switch (b->type) {
  case IPAddressOrRange_addressPrefix:
    addr_expand(addr_b, b->u.addressPrefix, length, 0x00);
    prefixlen_b = addr_prefixlen(b->u.addressPrefix);
    break;
  case IPAddressOrRange_addressRange:
    addr_expand(addr_b, b->u.addressRange->min, length, 0x00);
    prefixlen_b = length * 8;
    break;
  }

  if ((r = memcmp(addr_a, addr_b, length)) != 0)
    return r;
  else
    return prefixlen_a - prefixlen_b;
}

/*
 * IPv4-specific closure over IPAddressOrRange_cmp, since sk_sort()
 * comparision routines are only allowed two arguments.
 */
static int v4IPAddressOrRange_cmp(const IPAddressOrRange * const *a,
				  const IPAddressOrRange * const *b)
{
  return IPAddressOrRange_cmp(*a, *b, 4);
}

/*
 * IPv6-specific closure over IPAddressOrRange_cmp, since sk_sort()
 * comparision routines are only allowed two arguments.
 */
static int v6IPAddressOrRange_cmp(const IPAddressOrRange * const *a,
				  const IPAddressOrRange * const *b)
{
  return IPAddressOrRange_cmp(*a, *b, 16);
}

/*
 * Calculate whether a range collapses to a prefix.
 * See last paragraph of RFC 3779 2.2.3.7.
 */
static int range_should_be_prefix(const unsigned char *min,
				  const unsigned char *max,
				  const int length)
{
  unsigned char mask;
  int i, j;

  for (i = 0; i < length && min[i] == max[i]; i++)
    ;
  for (j = length - 1; j >= 0 && min[j] == 0x00 && max[j] == 0xFF; j--)
    ;
  if (i < j)
    return -1;
  if (i > j)
    return i * 8;
  mask = min[i] ^ max[i];
  switch (mask) {
  case 0x01: j = 7; break;
  case 0x03: j = 6; break;
  case 0x07: j = 5; break;
  case 0x0F: j = 4; break;
  case 0x1F: j = 3; break;
  case 0x3F: j = 2; break;
  case 0x7F: j = 1; break;
  default:   return -1;
  }
  if ((min[i] & mask) != 0 || (max[i] & mask) != mask)
    return -1;
  else
    return i * 8 + j;
}

/*
 * Construct a prefix.
 */
static int make_addressPrefix(IPAddressOrRange **result,
			      unsigned char *addr,
			      const int prefixlen)
{
  int bytelen = (prefixlen + 7) / 8, bitlen = prefixlen % 8;
  IPAddressOrRange *aor = IPAddressOrRange_new();

  if (aor == NULL)
    return 0;
  aor->type = IPAddressOrRange_addressPrefix;
  if (aor->u.addressPrefix == NULL &&
      (aor->u.addressPrefix = ASN1_BIT_STRING_new()) == NULL)
    goto err;
  if (!ASN1_BIT_STRING_set(aor->u.addressPrefix, addr, bytelen))
    goto err;
  aor->u.addressPrefix->flags &= ~7;
  aor->u.addressPrefix->flags |= ASN1_STRING_FLAG_BITS_LEFT;
  if (bitlen > 0) {
    aor->u.addressPrefix->data[bytelen - 1] &= ~(0xFF >> bitlen);
    aor->u.addressPrefix->flags |= 8 - bitlen;
  }
  
  *result = aor;
  return 1;

 err:
  IPAddressOrRange_free(aor);
  return 0;
}

/*
 * Construct a range.  If it can be expressed as a prefix,
 * return a prefix instead.  Doing this here simplifies
 * the rest of the code considerably.
 */
static int make_addressRange(IPAddressOrRange **result,
			     unsigned char *min,
			     unsigned char *max,
			     const int length)
{
  IPAddressOrRange *aor;
  int i, prefixlen;

  if ((prefixlen = range_should_be_prefix(min, max, length)) >= 0)
    return make_addressPrefix(result, min, prefixlen);

  if ((aor = IPAddressOrRange_new()) == NULL)
    return 0;
  aor->type = IPAddressOrRange_addressRange;
  OPENSSL_assert(aor->u.addressRange == NULL);
  if ((aor->u.addressRange = IPAddressRange_new()) == NULL)
    goto err;
  if (aor->u.addressRange->min == NULL &&
      (aor->u.addressRange->min = ASN1_BIT_STRING_new()) == NULL)
    goto err;
  if (aor->u.addressRange->max == NULL &&
      (aor->u.addressRange->max = ASN1_BIT_STRING_new()) == NULL)
    goto err;

  for (i = length; i > 0 && min[i - 1] == 0x00; --i)
    ;
  if (!ASN1_BIT_STRING_set(aor->u.addressRange->min, min, i))
    goto err;
  aor->u.addressRange->min->flags &= ~7;
  aor->u.addressRange->min->flags |= ASN1_STRING_FLAG_BITS_LEFT;
  if (i > 0) {
    unsigned char b = min[i - 1];
    int j = 1;
    while ((b & (0xFFU >> j)) != 0) 
      ++j;
    aor->u.addressRange->min->flags |= 8 - j;
  }

  for (i = length; i > 0 && max[i - 1] == 0xFF; --i)
    ;
  if (!ASN1_BIT_STRING_set(aor->u.addressRange->max, max, i))
    goto err;
  aor->u.addressRange->max->flags &= ~7;
  aor->u.addressRange->max->flags |= ASN1_STRING_FLAG_BITS_LEFT;
  if (i > 0) {
    unsigned char b = max[i - 1];
    int j = 1;
    while ((b & (0xFFU >> j)) != (0xFFU >> j))
      ++j;
    aor->u.addressRange->max->flags |= 8 - j;
  }

  *result = aor;
  return 1;

 err:
  IPAddressOrRange_free(aor);
  return 0;
}

/*
 * Construct a new address family or find an existing one.
 */
static IPAddressFamily *make_IPAddressFamily(IPAddrBlocks *addr,
					     const unsigned afi,
					     const unsigned *safi)
{
  IPAddressFamily *f;
  unsigned char key[3];
  unsigned keylen;
  int i;

  key[0] = (afi >> 8) & 0xFF;
  key[1] = afi & 0xFF;
  if (safi != NULL) {
    key[2] = *safi & 0xFF;
    keylen = 3;
  } else {
    keylen = 2;
  }

  for (i = 0; i < sk_IPAddressFamily_num(addr); i++) {
    f = sk_IPAddressFamily_value(addr, i);
    OPENSSL_assert(f->addressFamily->data != NULL);
    if (f->addressFamily->length == keylen &&
	!memcmp(f->addressFamily->data, key, keylen))
      return f;
  }

  if ((f = IPAddressFamily_new()) == NULL)
    goto err;
  if (f->ipAddressChoice == NULL &&
      (f->ipAddressChoice = IPAddressChoice_new()) == NULL)
    goto err;
  if (f->addressFamily == NULL && 
      (f->addressFamily = ASN1_OCTET_STRING_new()) == NULL)
    goto err;
  if (!ASN1_OCTET_STRING_set(f->addressFamily, key, keylen))
    goto err;
  if (!sk_IPAddressFamily_push(addr, f))
    goto err;

  return f;

 err:
  IPAddressFamily_free(f);
  return NULL;
}

/*
 * Add an inheritance element.
 */
int v3_addr_add_inherit(IPAddrBlocks *addr,
			const unsigned afi,
			const unsigned *safi)
{
  IPAddressFamily *f = make_IPAddressFamily(addr, afi, safi);
  if (f == NULL ||
      f->ipAddressChoice == NULL ||
      (f->ipAddressChoice->type == IPAddressChoice_addressesOrRanges &&
       f->ipAddressChoice->u.addressesOrRanges != NULL))
    return 0;
  if (f->ipAddressChoice->type == IPAddressChoice_inherit &&
      f->ipAddressChoice->u.inherit != NULL)
    return 1;
  if (f->ipAddressChoice->u.inherit == NULL &&
      (f->ipAddressChoice->u.inherit = ASN1_NULL_new()) == NULL)
    return 0;
  f->ipAddressChoice->type = IPAddressChoice_inherit;
  return 1;
}

/*
 * Construct an IPAddressOrRange sequence, or return an existing one.
 */
static IPAddressOrRanges *make_prefix_or_range(IPAddrBlocks *addr,
					       const unsigned afi,
					       const unsigned *safi)
{
  IPAddressFamily *f = make_IPAddressFamily(addr, afi, safi);
  IPAddressOrRanges *aors = NULL;

  if (f == NULL ||
      f->ipAddressChoice == NULL ||
      (f->ipAddressChoice->type == IPAddressChoice_inherit &&
       f->ipAddressChoice->u.inherit != NULL))
    return NULL;
  if (f->ipAddressChoice->type == IPAddressChoice_addressesOrRanges)
    aors = f->ipAddressChoice->u.addressesOrRanges;
  if (aors != NULL)
    return aors;
  if ((aors = sk_IPAddressOrRange_new_null()) == NULL)
    return NULL;
  switch (afi) {
  case IANA_AFI_IPV4:
    sk_IPAddressOrRange_set_cmp_func(aors, v4IPAddressOrRange_cmp);
    break;
  case IANA_AFI_IPV6:
    sk_IPAddressOrRange_set_cmp_func(aors, v6IPAddressOrRange_cmp);
    break;
  }
  f->ipAddressChoice->type = IPAddressChoice_addressesOrRanges;
  f->ipAddressChoice->u.addressesOrRanges = aors;
  return aors;
}

/*
 * Add a prefix.
 */
int v3_addr_add_prefix(IPAddrBlocks *addr,
		       const unsigned afi,
		       const unsigned *safi,
		       unsigned char *a,
		       const int prefixlen)
{
  IPAddressOrRanges *aors = make_prefix_or_range(addr, afi, safi);
  IPAddressOrRange *aor;
  if (aors == NULL || !make_addressPrefix(&aor, a, prefixlen))
    return 0;
  if (sk_IPAddressOrRange_push(aors, aor))
    return 1;
  IPAddressOrRange_free(aor);
  return 0;
}

/*
 * Add a range.
 */
int v3_addr_add_range(IPAddrBlocks *addr,
		      const unsigned afi,
		      const unsigned *safi,
		      unsigned char *min,
		      unsigned char *max)
{
  IPAddressOrRanges *aors = make_prefix_or_range(addr, afi, safi);
  IPAddressOrRange *aor;
  int length = length_from_afi(afi);
  if (aors == NULL)
    return 0;
  if (!make_addressRange(&aor, min, max, length))
    return 0;
  if (sk_IPAddressOrRange_push(aors, aor))
    return 1;
  IPAddressOrRange_free(aor);
  return 0;
}

/*
 * Extract min and max values from an IPAddressOrRange.
 */
static void extract_min_max(IPAddressOrRange *aor,
			    unsigned char *min,
			    unsigned char *max,
			    int length)
{
  OPENSSL_assert(aor != NULL && min != NULL && max != NULL);
  switch (aor->type) {
  case IPAddressOrRange_addressPrefix:
    addr_expand(min, aor->u.addressPrefix, length, 0x00);
    addr_expand(max, aor->u.addressPrefix, length, 0xFF);
    return;
  case IPAddressOrRange_addressRange:
    addr_expand(min, aor->u.addressRange->min, length, 0x00);
    addr_expand(max, aor->u.addressRange->max, length, 0xFF);
    return;
  }
}

/*
 * Public wrapper for extract_min_max().
 */
int v3_addr_get_range(IPAddressOrRange *aor,
		      const unsigned afi,
		      unsigned char *min,
		      unsigned char *max,
		      const int length)
{
  int afi_length = length_from_afi(afi);
  if (aor == NULL || min == NULL || max == NULL ||
      afi_length == 0 || length < afi_length ||
      (aor->type != IPAddressOrRange_addressPrefix &&
       aor->type != IPAddressOrRange_addressRange))
    return 0;
  extract_min_max(aor, min, max, afi_length);
  return afi_length;
}

/*
 * Sort comparision function for a sequence of IPAddressFamily.
 *
 * The last paragraph of RFC 3779 2.2.3.3 is slightly ambiguous about
 * the ordering: I can read it as meaning that IPv6 without a SAFI
 * comes before IPv4 with a SAFI, which seems pretty weird.  The
 * examples in appendix B suggest that the author intended the
 * null-SAFI rule to apply only within a single AFI, which is what I
 * would have expected and is what the following code implements.
 */
static int IPAddressFamily_cmp(const IPAddressFamily * const *a_,
			       const IPAddressFamily * const *b_)
{
  const ASN1_OCTET_STRING *a = (*a_)->addressFamily;
  const ASN1_OCTET_STRING *b = (*b_)->addressFamily;
  int len = ((a->length <= b->length) ? a->length : b->length);
  int cmp = memcmp(a->data, b->data, len);
  return cmp ? cmp : a->length - b->length;
}

/*
 * Check whether an IPAddrBLocks is in canonical form.
 */
int v3_addr_is_canonical(IPAddrBlocks *addr)
{
  unsigned char a_min[ADDR_RAW_BUF_LEN], a_max[ADDR_RAW_BUF_LEN];
  unsigned char b_min[ADDR_RAW_BUF_LEN], b_max[ADDR_RAW_BUF_LEN];
  IPAddressOrRanges *aors;
  int i, j, k;

  /*
   * Empty extension is cannonical.
   */
  if (addr == NULL)
    return 1;

  /*
   * Check whether the top-level list is in order.
   */
  for (i = 0; i < sk_IPAddressFamily_num(addr) - 1; i++) {
    const IPAddressFamily *a = sk_IPAddressFamily_value(addr, i);
    const IPAddressFamily *b = sk_IPAddressFamily_value(addr, i + 1);
    if (IPAddressFamily_cmp(&a, &b) >= 0)
      return 0;
  }

  /*
   * Top level's ok, now check each address family.
   */
  for (i = 0; i < sk_IPAddressFamily_num(addr); i++) {
    IPAddressFamily *f = sk_IPAddressFamily_value(addr, i);
    int length = length_from_afi(v3_addr_get_afi(f));

    /*
     * Inheritance is canonical.  Anything other than inheritance or
     * a SEQUENCE OF IPAddressOrRange is an ASN.1 error or something.
     */
    if (f == NULL || f->ipAddressChoice == NULL)
      return 0;
    switch (f->ipAddressChoice->type) {
    case IPAddressChoice_inherit:
      continue;
    case IPAddressChoice_addressesOrRanges:
      break;
    default:
      return 0;
    }

    /*
     * It's an IPAddressOrRanges sequence, check it.
     */
    aors = f->ipAddressChoice->u.addressesOrRanges;
    if (sk_IPAddressOrRange_num(aors) == 0)
      return 0;
    for (j = 0; j < sk_IPAddressOrRange_num(aors) - 1; j++) {
      IPAddressOrRange *a = sk_IPAddressOrRange_value(aors, j);
      IPAddressOrRange *b = sk_IPAddressOrRange_value(aors, j + 1);

      extract_min_max(a, a_min, a_max, length);
      extract_min_max(b, b_min, b_max, length);

      /*
       * Punt misordered list, overlapping start, or inverted range.
       */
      if (memcmp(a_min, b_min, length) >= 0 ||
	  memcmp(a_min, a_max, length) > 0 ||
	  memcmp(b_min, b_max, length) > 0)
	return 0;

      /*
       * Punt if adjacent or overlapping.  Check for adjacency by
       * subtracting one from b_min first.
       */
      for (k = length - 1; k >= 0 && b_min[k]-- == 0x00; k--)
	;
      if (memcmp(a_max, b_min, length) >= 0)
	return 0;

      /*
       * Check for range that should be expressed as a prefix.
       */
      if (a->type == IPAddressOrRange_addressRange &&
	  range_should_be_prefix(a_min, a_max, length) >= 0)
	return 0;
    }

    /*
     * Check final range to see if it should be a prefix.
     */
    j = sk_IPAddressOrRange_num(aors) - 1;
    {
      IPAddressOrRange *a = sk_IPAddressOrRange_value(aors, j);
      if (a->type == IPAddressOrRange_addressRange) {
	extract_min_max(a, a_min, a_max, length);
	if (range_should_be_prefix(a_min, a_max, length) >= 0)
	  return 0;
      }
    }
  }

  /*
   * If we made it through all that, we're happy.
   */
  return 1;
}

/*
 * Whack an IPAddressOrRanges into canonical form.
 */
static int IPAddressOrRanges_canonize(IPAddressOrRanges *aors,
				      const unsigned afi)
{
  int i, j, length = length_from_afi(afi);

  /*
   * Sort the IPAddressOrRanges sequence.
   */
  sk_IPAddressOrRange_sort(aors);

  /*
   * Clean up representation issues, punt on duplicates or overlaps.
   */
  for (i = 0; i < sk_IPAddressOrRange_num(aors) - 1; i++) {
    IPAddressOrRange *a = sk_IPAddressOrRange_value(aors, i);
    IPAddressOrRange *b = sk_IPAddressOrRange_value(aors, i + 1);
    unsigned char a_min[ADDR_RAW_BUF_LEN], a_max[ADDR_RAW_BUF_LEN];
    unsigned char b_min[ADDR_RAW_BUF_LEN], b_max[ADDR_RAW_BUF_LEN];

    extract_min_max(a, a_min, a_max, length);
    extract_min_max(b, b_min, b_max, length);

    /*
     * Punt overlaps.
     */
    if (memcmp(a_max, b_min, length) >= 0)
      return 0;

    /*
     * Merge if a and b are adjacent.  We check for
     * adjacency by subtracting one from b_min first.
     */
    for (j = length - 1; j >= 0 && b_min[j]-- == 0x00; j--)
      ;
    if (memcmp(a_max, b_min, length) == 0) {
      IPAddressOrRange *merged;
      if (!make_addressRange(&merged, a_min, b_max, length))
	return 0;
      sk_IPAddressOrRange_set(aors, i, merged);
      sk_IPAddressOrRange_delete(aors, i + 1);
      IPAddressOrRange_free(a);
      IPAddressOrRange_free(b);
      --i;
      continue;
    }
  }

  return 1;
}

/*
 * Whack an IPAddrBlocks extension into canonical form.
 */
int v3_addr_canonize(IPAddrBlocks *addr)
{
  int i;
  for (i = 0; i < sk_IPAddressFamily_num(addr); i++) {
    IPAddressFamily *f = sk_IPAddressFamily_value(addr, i);
    if (f->ipAddressChoice->type == IPAddressChoice_addressesOrRanges &&
	!IPAddressOrRanges_canonize(f->ipAddressChoice->u.addressesOrRanges,
				    v3_addr_get_afi(f)))
      return 0;
  }
  sk_IPAddressFamily_set_cmp_func(addr, IPAddressFamily_cmp);
  sk_IPAddressFamily_sort(addr);
  OPENSSL_assert(v3_addr_is_canonical(addr));
  return 1;
}

/*
 * v2i handler for the IPAddrBlocks extension.
 */
static void *v2i_IPAddrBlocks(const struct v3_ext_method *method,
			      struct v3_ext_ctx *ctx,
			      STACK_OF(CONF_VALUE) *values)
{
  static const char v4addr_chars[] = "0123456789.";
  static const char v6addr_chars[] = "0123456789.:abcdefABCDEF";
  IPAddrBlocks *addr = NULL;
  char *s = NULL, *t;
  int i;
  
  if ((addr = sk_IPAddressFamily_new(IPAddressFamily_cmp)) == NULL) {
    X509V3err(X509V3_F_V2I_IPADDRBLOCKS, ERR_R_MALLOC_FAILURE);
    return NULL;
  }

  for (i = 0; i < sk_CONF_VALUE_num(values); i++) {
    CONF_VALUE *val = sk_CONF_VALUE_value(values, i);
    unsigned char min[ADDR_RAW_BUF_LEN], max[ADDR_RAW_BUF_LEN];
    unsigned afi, *safi = NULL, safi_;
    const char *addr_chars;
    int prefixlen, i1, i2, delim, length;

    if (       !name_cmp(val->name, "IPv4")) {
      afi = IANA_AFI_IPV4;
    } else if (!name_cmp(val->name, "IPv6")) {
      afi = IANA_AFI_IPV6;
    } else if (!name_cmp(val->name, "IPv4-SAFI")) {
      afi = IANA_AFI_IPV4;
      safi = &safi_;
    } else if (!name_cmp(val->name, "IPv6-SAFI")) {
      afi = IANA_AFI_IPV6;
      safi = &safi_;
    } else {
      X509V3err(X509V3_F_V2I_IPADDRBLOCKS, X509V3_R_EXTENSION_NAME_ERROR);
      X509V3_conf_err(val);
      goto err;
    }

    switch (afi) {
    case IANA_AFI_IPV4:
      addr_chars = v4addr_chars;
      break;
    case IANA_AFI_IPV6:
      addr_chars = v6addr_chars;
      break;
    }

    length = length_from_afi(afi);

    /*
     * Handle SAFI, if any, and BUF_strdup() so we can null-terminate
     * the other input values.
     */
    if (safi != NULL) {
      *safi = strtoul(val->value, &t, 0);
      t += strspn(t, " \t");
      if (*safi > 0xFF || *t++ != ':') {
	X509V3err(X509V3_F_V2I_IPADDRBLOCKS, X509V3_R_INVALID_SAFI);
	X509V3_conf_err(val);
	goto err;
      }
      t += strspn(t, " \t");
      s = BUF_strdup(t);
    } else {
      s = BUF_strdup(val->value);
    }
    if (s == NULL) {
      X509V3err(X509V3_F_V2I_IPADDRBLOCKS, ERR_R_MALLOC_FAILURE);
      goto err;
    }

    /*
     * Check for inheritance.  Not worth additional complexity to
     * optimize this (seldom-used) case.
     */
    if (!strcmp(s, "inherit")) {
      if (!v3_addr_add_inherit(addr, afi, safi)) {
	X509V3err(X509V3_F_V2I_IPADDRBLOCKS, X509V3_R_INVALID_INHERITANCE);
	X509V3_conf_err(val);
	goto err;
      }
      OPENSSL_free(s);
      s = NULL;
      continue;
    }

    i1 = strspn(s, addr_chars);
    i2 = i1 + strspn(s + i1, " \t");
    delim = s[i2++];
    s[i1] = '\0';

    if (a2i_ipadd(min, s) != length) {
      X509V3err(X509V3_F_V2I_IPADDRBLOCKS, X509V3_R_INVALID_IPADDRESS);
      X509V3_conf_err(val);
      goto err;
    }

    switch (delim) {
    case '/':
      prefixlen = (int) strtoul(s + i2, &t, 10);
      if (t == s + i2 || *t != '\0') {
	X509V3err(X509V3_F_V2I_IPADDRBLOCKS, X509V3_R_EXTENSION_VALUE_ERROR);
	X509V3_conf_err(val);
	goto err;
      }
      if (!v3_addr_add_prefix(addr, afi, safi, min, prefixlen)) {
	X509V3err(X509V3_F_V2I_IPADDRBLOCKS, ERR_R_MALLOC_FAILURE);
	goto err;
      }
      break;
    case '-':
      i1 = i2 + strspn(s + i2, " \t");
      i2 = i1 + strspn(s + i1, addr_chars);
      if (i1 == i2 || s[i2] != '\0') {
	X509V3err(X509V3_F_V2I_IPADDRBLOCKS, X509V3_R_EXTENSION_VALUE_ERROR);
	X509V3_conf_err(val);
	goto err;
      }
      if (a2i_ipadd(max, s + i1) != length) {
	X509V3err(X509V3_F_V2I_IPADDRBLOCKS, X509V3_R_INVALID_IPADDRESS);
	X509V3_conf_err(val);
	goto err;
      }
      if (!v3_addr_add_range(addr, afi, safi, min, max)) {
	X509V3err(X509V3_F_V2I_IPADDRBLOCKS, ERR_R_MALLOC_FAILURE);
	goto err;
      }
      break;
    case '\0':
      if (!v3_addr_add_prefix(addr, afi, safi, min, length * 8)) {
	X509V3err(X509V3_F_V2I_IPADDRBLOCKS, ERR_R_MALLOC_FAILURE);
	goto err;
      }
      break;
    default:
      X509V3err(X509V3_F_V2I_IPADDRBLOCKS, X509V3_R_EXTENSION_VALUE_ERROR);
      X509V3_conf_err(val);
      goto err;
    }

    OPENSSL_free(s);
    s = NULL;
  }

  /*
   * Canonize the result, then we're done.
   */
  if (!v3_addr_canonize(addr))
    goto err;    
  return addr;

 err:
  OPENSSL_free(s);
  sk_IPAddressFamily_pop_free(addr, IPAddressFamily_free);
  return NULL;
}

/*
 * OpenSSL dispatch
 */
const X509V3_EXT_METHOD v3_addr = {
  NID_sbgp_ipAddrBlock,		/* nid */
  0,				/* flags */
  ASN1_ITEM_ref(IPAddrBlocks),	/* template */
  0, 0, 0, 0,			/* old functions, ignored */
  0,				/* i2s */
  0,				/* s2i */
  0,				/* i2v */
  v2i_IPAddrBlocks,		/* v2i */
  i2r_IPAddrBlocks,		/* i2r */
  0,				/* r2i */
  NULL				/* extension-specific data */
};

/*
 * Figure out whether extension sues inheritance.
 */
int v3_addr_inherits(IPAddrBlocks *addr)
{
  int i;
  if (addr == NULL)
    return 0;
  for (i = 0; i < sk_IPAddressFamily_num(addr); i++) {
    IPAddressFamily *f = sk_IPAddressFamily_value(addr, i);
    if (f->ipAddressChoice->type == IPAddressChoice_inherit)
      return 1;
  }
  return 0;
}

/*
 * Figure out whether parent contains child.
 */
static int addr_contains(IPAddressOrRanges *parent,
			 IPAddressOrRanges *child,
			 int length)
{
  unsigned char p_min[ADDR_RAW_BUF_LEN], p_max[ADDR_RAW_BUF_LEN];
  unsigned char c_min[ADDR_RAW_BUF_LEN], c_max[ADDR_RAW_BUF_LEN];
  int p, c;

  if (child == NULL || parent == child)
    return 1;
  if (parent == NULL)
    return 0;

  p = 0;
  for (c = 0; c < sk_IPAddressOrRange_num(child); c++) {
    extract_min_max(sk_IPAddressOrRange_value(child, c),
		    c_min, c_max, length);
    for (;; p++) {
      if (p >= sk_IPAddressOrRange_num(parent))
	return 0;
      extract_min_max(sk_IPAddressOrRange_value(parent, p),
		      p_min, p_max, length);
      if (memcmp(p_max, c_max, length) < 0)
	continue;
      if (memcmp(p_min, c_min, length) > 0)
	return 0;
      break;
    }
  }

  return 1;
}

/*
 * Test whether a is a subset of b.
 */
int v3_addr_subset(IPAddrBlocks *a, IPAddrBlocks *b)
{
  int i;
  if (a == NULL || a == b)
    return 1;
  if (b == NULL || v3_addr_inherits(a) || v3_addr_inherits(b))
    return 0;
  sk_IPAddressFamily_set_cmp_func(b, IPAddressFamily_cmp);
  for (i = 0; i < sk_IPAddressFamily_num(a); i++) {
    IPAddressFamily *fa = sk_IPAddressFamily_value(a, i);
    int j = sk_IPAddressFamily_find(b, fa);
    IPAddressFamily *fb;
    fb = sk_IPAddressFamily_value(b, j);
    if (fb == NULL)
       return 0;
    if (!addr_contains(fb->ipAddressChoice->u.addressesOrRanges, 
		       fa->ipAddressChoice->u.addressesOrRanges,
		       length_from_afi(v3_addr_get_afi(fb))))
      return 0;
  }
  return 1;
}

/*
 * Validation error handling via callback.
 */
#define validation_err(_err_)		\
  do {					\
    if (ctx != NULL) {			\
      ctx->error = _err_;		\
      ctx->error_depth = i;		\
      ctx->current_cert = x;		\
      ret = ctx->verify_cb(0, ctx);	\
    } else {				\
      ret = 0;				\
    }					\
    if (!ret)				\
      goto done;			\
  } while (0)

/*
 * Core code for RFC 3779 2.3 path validation.
 */
static int v3_addr_validate_path_internal(X509_STORE_CTX *ctx,
					  STACK_OF(X509) *chain,
					  IPAddrBlocks *ext)
{
  IPAddrBlocks *child = NULL;
  int i, j, ret = 1;
  X509 *x;

  OPENSSL_assert(chain != NULL && sk_X509_num(chain) > 0);
  OPENSSL_assert(ctx != NULL || ext != NULL);
  OPENSSL_assert(ctx == NULL || ctx->verify_cb != NULL);

  /*
   * Figure out where to start.  If we don't have an extension to
   * check, we're done.  Otherwise, check canonical form and
   * set up for walking up the chain.
   */
  if (ext != NULL) {
    i = -1;
    x = NULL;
  } else {
    i = 0;
    x = sk_X509_value(chain, i);
    OPENSSL_assert(x != NULL);
    if ((ext = x->rfc3779_addr) == NULL)
      goto done;
  }
  if (!v3_addr_is_canonical(ext))
    validation_err(X509_V_ERR_INVALID_EXTENSION);
  sk_IPAddressFamily_set_cmp_func(ext, IPAddressFamily_cmp);
  if ((child = sk_IPAddressFamily_dup(ext)) == NULL) {
    X509V3err(X509V3_F_V3_ADDR_VALIDATE_PATH_INTERNAL, ERR_R_MALLOC_FAILURE);
    ret = 0;
    goto done;
  }

  /*
   * Now walk up the chain.  No cert may list resources that its
   * parent doesn't list.
   */
  for (i++; i < sk_X509_num(chain); i++) {
    x = sk_X509_value(chain, i);
    OPENSSL_assert(x != NULL);
    if (!v3_addr_is_canonical(x->rfc3779_addr))
      validation_err(X509_V_ERR_INVALID_EXTENSION);
    if (x->rfc3779_addr == NULL) {
      for (j = 0; j < sk_IPAddressFamily_num(child); j++) {
	IPAddressFamily *fc = sk_IPAddressFamily_value(child, j);
	if (fc->ipAddressChoice->type != IPAddressChoice_inherit) {
	  validation_err(X509_V_ERR_UNNESTED_RESOURCE);
	  break;
	}
      }
      continue;
    }
    sk_IPAddressFamily_set_cmp_func(x->rfc3779_addr, IPAddressFamily_cmp);
    for (j = 0; j < sk_IPAddressFamily_num(child); j++) {
      IPAddressFamily *fc = sk_IPAddressFamily_value(child, j);
      int k = sk_IPAddressFamily_find(x->rfc3779_addr, fc);
      IPAddressFamily *fp = sk_IPAddressFamily_value(x->rfc3779_addr, k);
      if (fp == NULL) {
	if (fc->ipAddressChoice->type == IPAddressChoice_addressesOrRanges) {
	  validation_err(X509_V_ERR_UNNESTED_RESOURCE);
	  break;
	}
	continue;
      }
      if (fp->ipAddressChoice->type == IPAddressChoice_addressesOrRanges) {
	if (fc->ipAddressChoice->type == IPAddressChoice_inherit ||
	    addr_contains(fp->ipAddressChoice->u.addressesOrRanges, 
			  fc->ipAddressChoice->u.addressesOrRanges,
			  length_from_afi(v3_addr_get_afi(fc))))
	  sk_IPAddressFamily_set(child, j, fp);
	else
	  validation_err(X509_V_ERR_UNNESTED_RESOURCE);
      }
    }
  }

  /*
   * Trust anchor can't inherit.
   */
  OPENSSL_assert(x != NULL);
  if (x->rfc3779_addr != NULL) {
    for (j = 0; j < sk_IPAddressFamily_num(x->rfc3779_addr); j++) {
      IPAddressFamily *fp = sk_IPAddressFamily_value(x->rfc3779_addr, j);
      if (fp->ipAddressChoice->type == IPAddressChoice_inherit &&
	  sk_IPAddressFamily_find(child, fp) >= 0)
	validation_err(X509_V_ERR_UNNESTED_RESOURCE);
    }
  }

 done:
  sk_IPAddressFamily_free(child);
  return ret;
}

#undef validation_err

/*
 * RFC 3779 2.3 path validation -- called from X509_verify_cert().
 */
int v3_addr_validate_path(X509_STORE_CTX *ctx)
{
  return v3_addr_validate_path_internal(ctx, ctx->chain, NULL);
}

/*
 * RFC 3779 2.3 path validation of an extension.
 * Test whether chain covers extension.
 */
int v3_addr_validate_resource_set(STACK_OF(X509) *chain,
				  IPAddrBlocks *ext,
				  int allow_inheritance)
{
  if (ext == NULL)
    return 1;
  if (chain == NULL || sk_X509_num(chain) == 0)
    return 0;
  if (!allow_inheritance && v3_addr_inherits(ext))
    return 0;
  return v3_addr_validate_path_internal(NULL, chain, ext);
}

#endif /* OPENSSL_NO_RFC3779 */
