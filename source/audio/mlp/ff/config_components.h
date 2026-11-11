#pragma once
#define CONFIG_MLP_DECODER 0
#define CONFIG_TRUEHD_DECODER 0
/* The AC-3 / E-AC-3 decoder (eac3_api.c): E-AC-3 decoding on, and the header parser that
 * ac3_parser.c keeps behind CONFIG_AC3_PARSER (its parser object is compiled but never used). */
#define CONFIG_EAC3_DECODER 1
#define CONFIG_AC3_PARSER 1
