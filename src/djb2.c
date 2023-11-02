#define HASH_LS 5863588
#define HASH_GET 193492613
#define HASH_MKDIR 210720772860
#define HASH_CD 5863276
#define HASH_EXIT 6385204799


unsigned long hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}