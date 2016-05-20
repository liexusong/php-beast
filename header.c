
/*
 * You can modify this sign to disguise your encrypt file
 */
char encrypt_file_header_sign[] = {
	0xe8, 0x16, 0xa4, 0x0c,
	0xf2, 0xb2, 0x60, 0xee
};

int encrypt_file_header_length = sizeof(encrypt_file_header_sign);
