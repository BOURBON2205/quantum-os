/* fs.c - RAMFS sederhana (CRUD file teks) + kontrol hak akses per user (owner/other, r/w) */
#include "kernel.h"

#define MAX_FILES 32
#define NAME_LEN  24

struct file { int used; char name[NAME_LEN]; char *data; u32 size, cap; int owner; u8 p_owner, p_other; };
static struct file files[MAX_FILES];
int cur_uid = 1;                                  /* boot sebagai guest */

const char *uid_name(int uid) { return uid == 0 ? "root" : "guest"; }

int user_switch(const char *name, const char *pass) {
    if (!strcmp(name, "guest")) { cur_uid = 1; return 0; }
    if (!strcmp(name, "root"))  { if (pass && !strcmp(pass, "toor")) { cur_uid = 0; return 0; } return E_PERM; }
    return E_NOENT;
}

static struct file *find(const char *n) {
    for (int i = 0; i < MAX_FILES; i++) if (files[i].used && !strcmp(files[i].name, n)) return &files[i];
    return 0;
}

/* root boleh semuanya; selain itu cek bit owner atau other */
static int allowed(struct file *f, int want) {
    if (cur_uid == 0) return 1;
    u8 p = (f->owner == cur_uid) ? f->p_owner : f->p_other;
    return (p & want) == want;
}

static int create_as(const char *name, int uid, int po, int pt) {
    if (find(name)) return E_EXIST;
    if (strlen(name) == 0 || strlen(name) >= NAME_LEN) return E_NOENT;
    for (int i = 0; i < MAX_FILES; i++) if (!files[i].used) {
        struct file *f = &files[i];
        memset(f, 0, sizeof(*f));
        f->used = 1; strncpy(f->name, name, NAME_LEN - 1);
        f->owner = uid; f->p_owner = po; f->p_other = pt;
        return 0;
    }
    return E_FULL;
}

int fs_create(const char *name) { return create_as(name, cur_uid, 6, 4); }

int fs_write(const char *name, const char *txt, int append) {
    struct file *f = find(name);
    if (!f) return E_NOENT;
    if (!allowed(f, P_W)) return E_PERM;
    u32 len = (u32)strlen(txt), base = append ? f->size : 0;
    u32 need = base + len + 1;
    if (need > f->cap) {
        u32 nc = (need + 63) & ~63u;
        char *nb = kmalloc(nc);
        if (!nb) return E_NOMEM;
        if (f->data) { memcpy(nb, f->data, f->size); kfree(f->data); }
        f->data = nb; f->cap = nc;
    }
    memcpy(f->data + base, txt, len);
    f->size = base + len; f->data[f->size] = 0;
    return 0;
}

int fs_read(const char *name, const char **data, u32 *size) {
    struct file *f = find(name);
    if (!f) return E_NOENT;
    if (!allowed(f, P_R)) return E_PERM;
    *data = f->data ? f->data : ""; *size = f->size;
    return 0;
}

int fs_delete(const char *name) {
    struct file *f = find(name);
    if (!f) return E_NOENT;
    if (!allowed(f, P_W)) return E_PERM;
    if (f->data) kfree(f->data);
    memset(f, 0, sizeof(*f));
    return 0;
}

int fs_chmod(const char *name, int owner, int other) {
    struct file *f = find(name);
    if (!f) return E_NOENT;
    if (cur_uid != 0 && f->owner != cur_uid) return E_PERM;
    f->p_owner = owner & 7; f->p_other = other & 7;
    return 0;
}

void fs_list(void) {
    int n = 0;
    kprintf("NAMA                    OWNER  MODE  UKURAN\n");
    for (int i = 0; i < MAX_FILES; i++) if (files[i].used) {
        struct file *f = &files[i];
        kprintf("%s", f->name);
        for (int k = (int)strlen(f->name); k < 24; k++) kputc(' ');
        kprintf("%s", uid_name(f->owner));
        for (int k = (int)strlen(uid_name(f->owner)); k < 7; k++) kputc(' ');
        kprintf("%c%c%c%c%c%c  %u\n",
                f->p_owner & 4 ? 'r' : '-', f->p_owner & 2 ? 'w' : '-', ' ',
                f->p_other & 4 ? 'r' : '-', f->p_other & 2 ? 'w' : '-', ' ', f->size);
        n++;
    }
    kprintf("(%d berkas)\n", n);
}

const char *fs_err(int e) {
    switch (e) {
    case E_NOENT: return "berkas/user tidak ditemukan atau nama tidak valid";
    case E_EXIST: return "berkas sudah ada";
    case E_PERM:  return "AKSES DITOLAK (permission denied)";
    case E_NOMEM: return "memori habis (out of memory)";
    case E_FULL:  return "tabel berkas penuh";
    }
    return "error";
}

void fs_init(void) {
    memset(files, 0, sizeof(files));
    create_as("readme.txt", 0, 6, 4);  
    { int saved = cur_uid; cur_uid = 0;
      fs_write("readme.txt", "Selamat datang di QuantumOS! Ketik 'help' untuk daftar perintah.\n", 0);
      create_as("secret.txt", 0, 6, 0);
      fs_write("secret.txt", "Data rahasia: hanya root yang boleh membaca berkas ini.\n", 0);
      cur_uid = saved; }
}
