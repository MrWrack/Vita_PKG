#include "../include/vpk_convert.h"
#include "../include/app.h"
#include "../include/sha256.h"

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define ZIP_EOCD_SIG   0x06054B50u
#define ZIP_CDIR_SIG   0x02014B50u
#define ZIP_LOCAL_SIG  0x04034B50u
#define IO_BUF_SIZE    32768
#define MAX_ZIP_NAME   480

typedef struct {
    char name[MAX_ZIP_NAME];
    uint16_t method;
    uint16_t flags;
    uint32_t crc32;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint32_t local_offset;
} ZipEntry;

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int wr_all(SceUID fd, const void *data, unsigned size) {
    const uint8_t *p = (const uint8_t *)data;
    unsigned done = 0;
    while (done < size) {
        int n = sceIoWrite(fd, p + done, size - done);
        if (n <= 0)
            return -1;
        done += (unsigned)n;
    }
    return 0;
}

static int wr32(SceUID fd, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)v,
        (uint8_t)(v >> 8),
        (uint8_t)(v >> 16),
        (uint8_t)(v >> 24)
    };
    return wr_all(fd, b, sizeof(b));
}

static int wr64(SceUID fd, uint64_t v) {
    uint8_t b[8];
    for (int i = 0; i < 8; ++i)
        b[i] = (uint8_t)(v >> (i * 8));
    return wr_all(fd, b, sizeof(b));
}

static int safe_rel(const char *p) {
    if (!p || !*p || p[0] == '/' || p[0] == '\\' || strchr(p, ':'))
        return 0;

    if (!strcmp(p, "..") ||
        !strncmp(p, "../", 3) ||
        !strncmp(p, "..\\", 3) ||
        strstr(p, "/../") ||
        strstr(p, "\\..\\"))
        return 0;

    return 1;
}

static int is_directory_name(const char *name) {
    size_t n = strlen(name);
    return n > 0 && (name[n - 1] == '/' || name[n - 1] == '\\');
}

static const char *base_name(const char *path) {
    const char *a = strrchr(path, '/');
    const char *b = strrchr(path, '\\');
    const char *p = a > b ? a : b;
    return p ? p + 1 : path;
}

static void make_output_path(const char *vpk, char *out, unsigned out_size) {
    char name[160];
    snprintf(name, sizeof(name), "%s", base_name(vpk));

    char *dot = strrchr(name, '.');
    if (dot)
        *dot = 0;

    snprintf(out, out_size, "%s/%s.pkg", MRW_PKG_ROOT, name);
}

static int find_eocd(SceUID fd, uint32_t *cd_offset, uint16_t *entries) {
    SceOff end = sceIoLseek(fd, 0, SCE_SEEK_END);
    if (end < 22)
        return -300;

    unsigned tail_size = (unsigned)(end > 65557 ? 65557 : end);
    uint8_t *tail = malloc(tail_size);
    if (!tail)
        return -301;

    if (sceIoLseek(fd, end - tail_size, SCE_SEEK_SET) < 0 ||
        sceIoRead(fd, tail, tail_size) != (int)tail_size) {
        free(tail);
        return -302;
    }

    for (int i = (int)tail_size - 22; i >= 0; --i) {
        if (le32(tail + i) == ZIP_EOCD_SIG) {
            uint16_t disk = le16(tail + i + 4);
            uint16_t cd_disk = le16(tail + i + 6);
            uint16_t disk_entries = le16(tail + i + 8);
            uint16_t total_entries = le16(tail + i + 10);
            uint32_t offset = le32(tail + i + 16);

            free(tail);

            if (disk != 0 || cd_disk != 0 || disk_entries != total_entries)
                return -303; /* multi-disk ZIP unsupported */

            *cd_offset = offset;
            *entries = total_entries;
            return 0;
        }
    }

    free(tail);
    return -304;
}

static int read_entries(
    SceUID fd,
    ZipEntry **out_entries,
    unsigned *out_count
) {
    uint32_t cd_offset = 0;
    uint16_t total = 0;
    int r = find_eocd(fd, &cd_offset, &total);
    if (r < 0)
        return r;

    ZipEntry *items = calloc(total ? total : 1, sizeof(ZipEntry));
    if (!items)
        return -305;

    if (sceIoLseek(fd, cd_offset, SCE_SEEK_SET) < 0) {
        free(items);
        return -306;
    }

    unsigned count = 0;

    for (unsigned i = 0; i < total; ++i) {
        uint8_t h[46];
        if (sceIoRead(fd, h, sizeof(h)) != sizeof(h) ||
            le32(h) != ZIP_CDIR_SIG) {
            free(items);
            return -307;
        }

        uint16_t flags = le16(h + 8);
        uint16_t method = le16(h + 10);
        uint32_t crc = le32(h + 16);
        uint32_t comp = le32(h + 20);
        uint32_t uncomp = le32(h + 24);
        uint16_t name_len = le16(h + 28);
        uint16_t extra_len = le16(h + 30);
        uint16_t comment_len = le16(h + 32);
        uint32_t local_offset = le32(h + 42);

        if (name_len == 0 || name_len >= MAX_ZIP_NAME) {
            free(items);
            return -308;
        }

        char name[MAX_ZIP_NAME];
        if (sceIoRead(fd, name, name_len) != name_len) {
            free(items);
            return -309;
        }
        name[name_len] = 0;

        if (sceIoLseek(fd, extra_len + comment_len, SCE_SEEK_CUR) < 0) {
            free(items);
            return -310;
        }

        if (is_directory_name(name))
            continue;

        if (!safe_rel(name)) {
            free(items);
            return -311;
        }

        /* Bit 0 means traditional ZIP encryption. */
        if (flags & 1) {
            free(items);
            return -312;
        }

        if (method != 0 && method != 8) {
            free(items);
            return -313;
        }

        ZipEntry *e = &items[count++];
        snprintf(e->name, sizeof(e->name), "%s", name);
        e->method = method;
        e->flags = flags;
        e->crc32 = crc;
        e->comp_size = comp;
        e->uncomp_size = uncomp;
        e->local_offset = local_offset;
    }

    *out_entries = items;
    *out_count = count;
    return 0;
}

static int local_data_offset(SceUID fd, const ZipEntry *e, SceOff *out) {
    uint8_t h[30];

    if (sceIoLseek(fd, e->local_offset, SCE_SEEK_SET) < 0 ||
        sceIoRead(fd, h, sizeof(h)) != sizeof(h) ||
        le32(h) != ZIP_LOCAL_SIG)
        return -320;

    uint16_t name_len = le16(h + 26);
    uint16_t extra_len = le16(h + 28);

    *out = (SceOff)e->local_offset + 30 + name_len + extra_len;
    return 0;
}

static int copy_stored(
    SceUID in,
    SceUID out,
    uint32_t size,
    MrwSha256 *sha
) {
    uint8_t *buf = malloc(IO_BUF_SIZE);
    if (!buf)
        return -321;

    uint32_t remain = size;
    while (remain) {
        unsigned chunk = remain > IO_BUF_SIZE ? IO_BUF_SIZE : remain;
        int n = sceIoRead(in, buf, chunk);
        if (n != (int)chunk) {
            free(buf);
            return -322;
        }

        mrw_sha256_update(sha, buf, chunk);

        if (wr_all(out, buf, chunk) < 0) {
            free(buf);
            return -323;
        }

        remain -= chunk;
    }

    free(buf);
    return 0;
}

static int copy_deflated(
    SceUID in,
    SceUID out,
    uint32_t comp_size,
    uint32_t expected_size,
    MrwSha256 *sha
) {
    uint8_t *inbuf = malloc(IO_BUF_SIZE);
    uint8_t *outbuf = malloc(IO_BUF_SIZE);

    if (!inbuf || !outbuf) {
        free(inbuf);
        free(outbuf);
        return -324;
    }

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        free(inbuf);
        free(outbuf);
        return -325;
    }

    uint32_t comp_left = comp_size;
    uint64_t written = 0;
    int zret = Z_OK;

    while (zret != Z_STREAM_END) {
        if (zs.avail_in == 0 && comp_left) {
            unsigned chunk = comp_left > IO_BUF_SIZE ? IO_BUF_SIZE : comp_left;
            int n = sceIoRead(in, inbuf, chunk);
            if (n != (int)chunk) {
                inflateEnd(&zs);
                free(inbuf);
                free(outbuf);
                return -326;
            }

            zs.next_in = inbuf;
            zs.avail_in = chunk;
            comp_left -= chunk;
        }

        zs.next_out = outbuf;
        zs.avail_out = IO_BUF_SIZE;

        zret = inflate(&zs, Z_NO_FLUSH);
        if (zret != Z_OK && zret != Z_STREAM_END) {
            inflateEnd(&zs);
            free(inbuf);
            free(outbuf);
            return -327;
        }

        unsigned produced = IO_BUF_SIZE - zs.avail_out;
        if (produced) {
            mrw_sha256_update(sha, outbuf, produced);
            if (wr_all(out, outbuf, produced) < 0) {
                inflateEnd(&zs);
                free(inbuf);
                free(outbuf);
                return -328;
            }
            written += produced;
        }

        if (zs.avail_in == 0 && comp_left == 0 && produced == 0 &&
            zret != Z_STREAM_END) {
            inflateEnd(&zs);
            free(inbuf);
            free(outbuf);
            return -329;
        }
    }

    inflateEnd(&zs);
    free(inbuf);
    free(outbuf);

    return written == expected_size ? 0 : -330;
}

int mrw_convert_vpk_to_pkg(
    const char *vpk_path,
    char *output_path,
    unsigned output_path_size,
    MrwInstallProgress *progress
) {
    if (!vpk_path || !output_path || output_path_size == 0)
        return -340;

    if (progress) {
        progress->percent = 1;
        snprintf(progress->stage, sizeof(progress->stage), "Convert");
        snprintf(progress->message, sizeof(progress->message), "Reading VPK directory");
    }

    sceIoMkdir(MRW_DATA_ROOT, 0777);
    sceIoMkdir(MRW_PKG_ROOT, 0777);

    SceUID in = sceIoOpen(vpk_path, SCE_O_RDONLY, 0);
    if (in < 0)
        return in;

    ZipEntry *entries = NULL;
    unsigned count = 0;
    int r = read_entries(in, &entries, &count);
    if (r < 0) {
        sceIoClose(in);
        return r;
    }

    if (count == 0) {
        free(entries);
        sceIoClose(in);
        return -341;
    }

    make_output_path(vpk_path, output_path, output_path_size);

    char tmp[MRW_MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.tmp", output_path);
    sceIoRemove(tmp);

    SceUID out = sceIoOpen(
        tmp,
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
        0666
    );
    if (out < 0) {
        free(entries);
        sceIoClose(in);
        return out;
    }

    static const uint8_t magic[8] = {
        'M','R','W','P','K','G','1',0
    };

    char manifest[256];
    int manifest_len = snprintf(
        manifest,
        sizeof(manifest),
        "{\"format\":\"MRW-PKG\",\"version\":2,\"file_count\":%u,\"source\":\"%s\"}",
        count,
        base_name(vpk_path)
    );

    if (manifest_len <= 0 || manifest_len >= (int)sizeof(manifest) ||
        wr_all(out, magic, sizeof(magic)) < 0 ||
        wr32(out, 2) < 0 ||
        wr32(out, (uint32_t)manifest_len) < 0 ||
        wr_all(out, manifest, (unsigned)manifest_len) < 0 ||
        wr32(out, count) < 0) {
        r = -342;
        goto fail;
    }

    for (unsigned i = 0; i < count; ++i) {
        ZipEntry *e = &entries[i];
        unsigned name_len = (unsigned)strlen(e->name);

        SceOff data_offset = 0;
        r = local_data_offset(in, e, &data_offset);
        if (r < 0)
            goto fail;

        if (sceIoLseek(in, data_offset, SCE_SEEK_SET) < 0) {
            r = -343;
            goto fail;
        }

        if (wr32(out, name_len) < 0 ||
            wr_all(out, e->name, name_len) < 0 ||
            wr64(out, e->uncomp_size) < 0) {
            r = -344;
            goto fail;
        }

        SceOff sha_pos = sceIoLseek(out, 0, SCE_SEEK_CUR);
        uint8_t zero_sha[32] = {0};
        if (sha_pos < 0 || wr_all(out, zero_sha, 32) < 0) {
            r = -345;
            goto fail;
        }

        MrwSha256 sha;
        uint8_t digest[32];
        mrw_sha256_init(&sha);

        if (e->method == 0)
            r = copy_stored(in, out, e->uncomp_size, &sha);
        else
            r = copy_deflated(
                in,
                out,
                e->comp_size,
                e->uncomp_size,
                &sha
            );

        if (r < 0)
            goto fail;

        mrw_sha256_final(&sha, digest);

        SceOff end_pos = sceIoLseek(out, 0, SCE_SEEK_CUR);
        if (end_pos < 0 ||
            sceIoLseek(out, sha_pos, SCE_SEEK_SET) < 0 ||
            wr_all(out, digest, sizeof(digest)) < 0 ||
            sceIoLseek(out, end_pos, SCE_SEEK_SET) < 0) {
            r = -346;
            goto fail;
        }

        if (progress) {
            progress->percent = 5 + (int)(((i + 1) * 90U) / count);
            snprintf(progress->stage, sizeof(progress->stage), "Convert");
            snprintf(
                progress->message,
                sizeof(progress->message),
                "%u/%u files packed",
                i + 1,
                count
            );
        }
    }

    sceIoClose(out);
    sceIoClose(in);
    free(entries);

    sceIoRemove(output_path);
    r = sceIoRename(tmp, output_path);
    if (r < 0) {
        sceIoRemove(tmp);
        return r;
    }

    if (progress) {
        progress->percent = 100;
        snprintf(progress->stage, sizeof(progress->stage), "Done");
        snprintf(progress->message, sizeof(progress->message), "VPK converted to MRW-PKG");
    }

    return 0;

fail:
    sceIoClose(out);
    sceIoClose(in);
    free(entries);
    sceIoRemove(tmp);

    if (progress) {
        snprintf(progress->stage, sizeof(progress->stage), "Error");
        snprintf(progress->message, sizeof(progress->message), "VPK conversion failed: %d", r);
    }

    return r;
}
