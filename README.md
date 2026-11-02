# QuantumOS - Mini OS Tugas Besar Sistem Operasi

Kernel i386 (protected mode) ditulis dengan C + GNU assembler, dijalankan di QEMU.

## Setup (Windows: pakai WSL2 + Ubuntu)
1. PowerShell (admin): `wsl --install -d Ubuntu`, restart.
2. VSCode: install extension **WSL**, **C/C++**; buka folder ini lewat "WSL: Open Folder in WSL".
3. Di terminal WSL:
   `sudo apt update && sudo apt install -y build-essential gcc-multilib qemu-system-x86 grub-pc-bin grub-common xorriso mtools gdb`

## Build & jalankan
| Perintah | Fungsi |
|---|---|
| `make` | build `build/quantum.elf` |
| `make run` | QEMU + jendela VGA (keyboard) + serial di terminal |
| `make run-serial` | QEMU tanpa jendela (semua di terminal; keluar: Ctrl+A lalu X) |
| `make run-iso` | boot lewat **GRUB** sungguhan (ISO) |
| `make debug` | QEMU menunggu gdb (F5 di VSCode: "Debug QuantumOS") |

## Peta file -> komponen tugas
| File | Komponen |
|---|---|
| `boot.S`, `linker.ld`, `main.c`, `console.c`, `shell.c` | Bootstrapping & CLI (VGA + serial + keyboard PS/2) |
| `interrupts.c`, `isr.S` | GDT, IDT, PIC, PIT 100 Hz, exception handler |
| `thread.c` | TCB, status NEW/READY/RUNNING/WAITING/TERMINATED, Round Robin preemptive, context switch (`switch_context` di `isr.S`) |
| `sync.c` | Mutex, semaphore, deteksi deadlock (wait-for graph) |
| `demo.c` | Skenario uji: thread, race condition, deadlock, memori |
| `mem.c` | Alokator heap: first-fit, split, coalesce, OOM aman |
| `fs.c` | RAMFS CRUD + hak akses user (root/guest, owner/other, r/w) |

## Skrip demo (urutan untuk presentasi)
```
help
threads              # 2 thread, perubahan status ditampilkan
sched ; ps
race                 # tanpa lock: hasil < 2.000.000 | dengan mutex: 2.000.000
deadlock             # 2 thread saling tunggu + deteksi circular wait
ps ; kill <tid> ...  # bersihkan
deadlock fix         # urutan mutex sama -> selesai
meminfo ; alloc 1000 ; free 0 ; memtest      # OOM aman
touch a.txt ; write a.txt halo ; cat a.txt ; append a.txt !! ; rm a.txt ; ls
cat secret.txt       # DITOLAK (guest)
write readme.txt x   # DITOLAK (guest)
su root toor ; cat secret.txt                # berhasil
```
User awal `guest`. Password root: `toor`.
