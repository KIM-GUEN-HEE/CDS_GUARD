#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char Byte;

extern const Byte S[4][256];
extern const Byte KRK[3][16];
void DL(const Byte *i, Byte *o);
void RotXOR(const Byte *s, int n, Byte *t);
int EncKeySetup(const Byte *w0, Byte *roundKeys, int keyBits);
int DecKeySetup(const Byte *w0, Byte *roundKeys, int keyBits);
void Crypt(const Byte *p, int R, const Byte *e, Byte *c);

#ifdef __cplusplus
}
#endif
