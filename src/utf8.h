#ifndef UTF8_H_
#define UTF8_H_

/* Mask and bit pattern for a 1-byte character */
#define UTF8_MASK1 0b10000000
#define UTF8_VAL1  0b00000000

/* Mask and bit pattern for a 2-byte character */
#define UTF8_MASK2 0b11100000
#define UTF8_VAL2  0b11000000

/* Mask and bit pattern for a 3-byte character */
#define UTF8_MASK3 0b11110000
#define UTF8_VAL3  0b11100000

/* Mask and bit pattern for a 4-byte character */
#define UTF8_MASK4 0b11111000
#define UTF8_VAL4  0b11110000

/* Mask and bit pattern for a continuation byte */
#define UTF8_CMASK 0b11000000
#define UTF8_CVAL  0b10000000

static inline int utf8_nbytes(char first)
{
	if ((first & UTF8_MASK1) == UTF8_VAL1)
		return 1;
	else if ((first & UTF8_MASK2) == UTF8_VAL2)
		return 2;
	else if ((first & UTF8_MASK3) == UTF8_VAL3)
		return 3;
	else if ((first & UTF8_MASK4) == UTF8_VAL4)
		return 4;
	else
		/* Default case: continuation bytes */
		return 0;
}

#endif // UTF8_H_
