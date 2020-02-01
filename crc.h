// $Id$


/**
 * This is an implementation of CRC32. See ISO 3309 and ITU-T V.42 
 * for a formal specification
 *
 * This file is partly taken from Crypto++ library (http://www.cryptopp.com)
 * and http://www.di-mgt.com.au/crypto.html#CRC.
 *
 * Since the original version of the code is put in public domain,
 * this file is put on public domain as well.
 */
/**
 * @file 
 * @brief This implements CRC32 algorithm. See ITU-T V.42 for the formal specification.
 */


#ifndef CRC_H_
#define CRC_H_

#include <stdint.h>
#include <stddef.h>


/** CRC32 context. */
typedef struct crc32_context_t
{
    uint32_t	crc_state;	/**< Current state. */
}crc32_context_t;


/**
 * \brief Initialize CRC32 context.
 *
 * @param ctx	    CRC32 context.
 */
void crc32_init(crc32_context_t *ctx);


/**
 * \brief Feed data incrementally to the CRC32 algorithm.
 *
 * @param ctx	    CRC32 context.
 * @param data	    Input data.
 * @param nbytes    Length of the input data.
 *
 * @return	    The current CRC32 value.
 */
uint32_t crc32_update(crc32_context_t *ctx, const uint8_t *data, size_t nbytes);


/**
 * Finalize CRC32 calculation and retrieve the CRC32 value.
 *
 * @param ctx	    CRC32 context.
 *
 * @return	    The current CRC value.
 */
uint32_t crc32_final(crc32_context_t *ctx);


/**
 * \brief Perform one-off CRC32 calculation to the specified data.
 *
 * @param data	     Input data.
 * @param nbytes    Length of input data.
 *
 * @return	    CRC value of the data.
 */
uint32_t crc32_calculate(const uint8_t *data, size_t nbytes);

#endif
