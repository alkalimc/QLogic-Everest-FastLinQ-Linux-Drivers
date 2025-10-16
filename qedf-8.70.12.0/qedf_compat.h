/*
 *  QLogic FCoE Offload Driver
 *  Copyright (c) 2015-2018 Cavium Inc.
 *  
 *  See LICENSE.qedf for copyright and licensing details.
 */
#ifndef _QEDF_COMPAT_H_
#define _QEDF_COMPAT_H_

#ifndef ETHER_ADDR_EQUAL
/**
 * ether_addr_equal - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two ethernet addresses, returns true if equal
 */
static inline bool ether_addr_equal(const u8 *addr1, const u8 *addr2)
{
	return !compare_ether_addr(addr1, addr2);
}
#endif /* ETHER_ADDR_EQUAL */

#ifndef ETHER_ADDR_COPY
/**
 ** ether_addr_copy - Copy an Ethernet address
 ** @dst: Pointer to a six-byte array Ethernet address destination
 ** @src: Pointer to a six-byte array Ethernet address source
 **
 ** Please note: dst & src must both be aligned to u16.
 **/
static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	*(u32 *)dst = *(const u32 *)src;
	*(u16 *)(dst + 4) = *(const u16 *)(src + 4);
#else
	u16 *a = (u16 *)dst;
	const u16 *b = (const u16 *)src;

	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
#endif
}
#endif /* ETHER_ADDR_COPY */

#ifndef ETH_ZERO_ADDR
/**
 * eth_zero_addr - Assign zero address
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Assign the zero address to the given address array.
 */
static inline void eth_zero_addr(u8 *addr)
{
	memset(addr, 0x00, ETH_ALEN);
}
#endif /* ETH_ZERO_ADDR */

#ifndef KMALLOC_ARRAY
#define kmalloc_array(_n, _size, _gfp) kmalloc((_n) * (_size), _gfp)
#endif /* KMALLOC_ARRAY */

#ifndef FC_DISC_CONFIG
#define fc_disc_config(_vn_port, _priv)
#endif /* FC_DISC_CONFIG */

#endif /* _QEDF_COMPAT_H_ */
