#include <string.h>
#include <assert.h>
#include <rpm/rpmlib.h>
#include "rpmcpio.h"
#include "errexit.h"

static const char *getStringTag(Header h, int tag)
{
   int type;
   int count;
   void *data;
   int rc = headerGetEntry(h, tag, &type, &data, &count);
   if (rc == 1) {
      assert(type == RPM_STRING_TYPE && count == 1);
      return (const char *)data;
   }
   return NULL;
}

struct rpmcpio {
    FD_t fd;
    // n1: current data pos
    // n2: end data pos
    // n3: next entry pos
    int n1, n2, n3;
    struct cpioent ent;
    char fname[4096];
    char rpmfname[];
};

struct rpmcpio *rpmcpio_open(const char *fname)
{
    const char *bn = strrchr(fname, '/');
    bn = bn ? bn + 1 : fname;
    size_t len = strlen(bn);
    struct rpmcpio *cpio = xmalloc(sizeof(*cpio) + len + 1);
    memcpy(cpio->rpmfname, bn, len + 1);

    FD_t fd = Fopen(fname, "r.ufdio");
    if (Ferror(fd))
	die("%s: cannot open", bn);
    Header h;
    int rc = rpmReadPackageHeader(fd, &h, NULL, NULL, NULL);
    if (rc)
	die("%s: cannot read rpm header", bn);

    char mode[] = "r.gzdio";
    const char *compr = getStringTag(h, RPMTAG_PAYLOADCOMPRESSOR);
    if (compr && compr[0] && compr[1] == 'z')
	mode[2] = compr[0];
    cpio->fd = Fdopen(fd, mode);
    if (Ferror(cpio->fd))
	die("%s: cannot open payload", bn);
    if (cpio->fd != fd)
	Fclose(fd);

    cpio->n1 = cpio->n2 = cpio->n3 = 0;
    return cpio;
}

static void rpmcpio_skip(struct rpmcpio *cpio, int n)
{
    assert(n > 0);
    char buf[BUFSIZ];
    do {
	int m = (n > BUFSIZ) ? BUFSIZ : n;
	if (Fread(buf, m, 1, cpio->fd) != 1)
	    die("%s: cannot skip cpio bytes", cpio->rpmfname);
	n -= m;
    }
    while (n > 0);
}

static const unsigned char hex[256] = {
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
};

const struct cpioent *rpmcpio_next(struct rpmcpio *cpio)
{
    if (cpio->n3 > cpio->n1) {
	rpmcpio_skip(cpio, cpio->n3 - cpio->n1);
	cpio->n1 = cpio->n3;
    }
    char buf[110];
    if (Fread(buf, 110, 1, cpio->fd) != 1)
	die("%s: cannot read cpio header", cpio->rpmfname);
    if (memcmp(buf, "070701", 6) != 0)
	die("%s: bad cpio header magic", cpio->rpmfname);
    cpio->n1 += 110;

    unsigned *out = (unsigned *) &cpio->ent;
    for (int i = 0; i < 13; i++) {
	const char *s = buf + 6 + i * 8;
	unsigned u = 0;
	for (int j = 0; j < 8; j++) {
	    unsigned v = hex[(unsigned char) s[j]];
	    if (v == 0xee)
		die("%s: invalid header", cpio->rpmfname);
	    u = (u << 4) | v;
	}
	out[i] = u;
    }

    unsigned fnamesize = ((cpio->ent.fnamelen + 1) & ~3) + 2;
    if (fnamesize > sizeof cpio->fname)
	die("%s: cpio filename too long", cpio->rpmfname);
    if (Fread(cpio->fname, fnamesize, 1, cpio->fd) != 1)
	die("%s: cannot read cpio filename", cpio->rpmfname);

    cpio->n1 += fnamesize;
    cpio->n2 = cpio->n1 + cpio->ent.size;
    cpio->n3 = (cpio->n2 + 3) & ~3;

    if (strcmp(cpio->fname, "TRAILER!!!") == 0)
	return NULL;
    return &cpio->ent;
}
