# Termux-MiningDroid

CPU miner frontend untuk Android/Termux dengan CLI `./droider`, memakai backend CPU VerusHash 2.2.

> Catatan penting: `droider` adalah launcher/controller C. Hashing VerusHash yang benar-benar dikirim ke pool dilakukan oleh backend CPU upstream yang diambil dan dibangun oleh script. Jadi ini bukan simulator hashrate atau benchmark palsu.

## CLI

```bash
./droider -a verushash -o stratum+tcp://POOL:PORT -u WALLET.worker -t 4 -p 50 -P x
```

| Opsi | Fungsi |
|---|---|
| `-a verushash` | Algoritma VerusHash 2.2 |
| `-o` | URL Stratum pool |
| `-u` | Address wallet Verus, opsional `.worker` |
| `-t` | Jumlah thread mining. Jika dihilangkan, default memakai jumlah CPU online dikurangi satu (minimal 1) agar Android masih punya ruang untuk UI. |
| `-p` | Target duty-cycle CPU 1-100%; default 80% agar perangkat lebih responsif |
| `-P` | Password pool, biasanya `x` |

Perhatikan typo manusia yang klasik: protokolnya **`stratum+tcp://`**, bukan `startum+tcp://`.

## Struktur source

```text
termux-miningdroid/
├── README.md
├── LICENSE
├── src/
│   └── droider.c
└── scripts/
    └── build-termux.sh
```

Saat selesai build, dua file executable akan dibuat:

```text
./droider
./droider-backend
```

`droider` menangani CLI dan CPU duty-cycle. `droider-backend` adalah executable mining VerusHash 2.2 yang dibangun dari backend upstream.

## Build dari file mentah sampai ./droider

### 1. Install Termux

Gunakan Termux versi yang masih mendapat dukungan paket Android yang sesuai.

### 2. Install dependency

```bash
pkg update
pkg upgrade
pkg install git clang make autoconf automake libtool pkg-config openssl libcurl
```

Cek arsitektur:

```bash
uname -m
```

Android 64-bit ARM biasanya menghasilkan:

```text
aarch64
```

### 3. Ambil repository

```bash
git clone https://github.com/Aditya01103/termux-miningdroid.git
cd termux-miningdroid
```

### 4. Build otomatis

Beri permission:

```bash
chmod +x scripts/build-termux.sh
```

Jalankan:

```bash
./scripts/build-termux.sh
```

Script melakukan urutan berikut:

1. Membuat direktori `build/`.
2. Clone branch `Verus2.2` dari backend CPU.
3. Menampilkan commit backend yang sedang dibangun.
4. Menjalankan build backend dengan toolchain Termux.
5. Menyalin hasil backend menjadi `./droider-backend`.
6. Mengompilasi `src/droider.c` menggunakan `clang`.
7. Menghasilkan executable `./droider`.

### 5. Cek hasil build

```bash
ls -lh ./droider ./droider-backend
file ./droider ./droider-backend
./droider -h
```

Kalau `./droider -h` menampilkan help, launcher C sudah jadi.

## Build manual launcher C

Kalau mau melihat proses dari source mentah tanpa script:

```bash
clang -O2 -Wall -Wextra -std=c11 src/droider.c -o droider
chmod +x droider
```

Tetapi executable `droider` saja belum cukup. Ia membutuhkan `./droider-backend` untuk melakukan hashing dan komunikasi Stratum.

## Cara menjalankan mining

Format:

```bash
./droider \
  -a verushash \
  -o stratum+tcp://POOL:PORT \
  -u WALLET_ADDRESS.worker01 \
  -t 4 \
  -p 50 \
  -P x
```

Contoh endpoint LuckPool:

```bash
./droider \
  -a verushash \
  -o stratum+tcp://ap.luckpool.net:3956 \
  -u YOUR_VERUS_ADDRESS.phone01 \
  -t 4 \
  -p 50 \
  -P x
```

Port dan endpoint pool dapat berubah. Gunakan endpoint yang saat ini diberikan oleh pool.

## Arti -p

`-p` di `droider` adalah **CPU duty-cycle**, bukan pengaturan frekuensi CPU. Default-nya 80% jika opsi ini dihilangkan, untuk mengurangi beban berkelanjutan dan memberi Android ruang bernapas.

Contoh:

```bash
# Full duty-cycle
./droider -a verushash -o stratum+tcp://POOL:PORT -u WALLET.worker -t 4 -p 100 -P x

# Kira-kira separuh waktu proses aktif
./droider -a verushash -o stratum+tcp://POOL:PORT -u WALLET.worker -t 4 -p 50 -P x

# Kira-kira seperempat waktu proses aktif
./droider -a verushash -o stratum+tcp://POOL:PORT -u WALLET.worker -t 2 -p 25 -P x
```

Jika opsi `-t` tidak diberikan, launcher memakai jumlah CPU online dikurangi satu (minimal 1) agar sistem punya ruang untuk tugas Android lain. Atur `-t` secara eksplisit bila ingin memilih sendiri jumlah thread. Kontrol dilakukan dengan `SIGSTOP/SIGCONT` pada process group miner dalam interval pendek. Pemeriksaan proses launcher dibuat lebih jarang untuk mengurangi overhead polling. Karena scheduler Android, aplikasi lain, governor CPU, dan thermal throttling ikut bermain, nilai ini **bukan jaminan persentase CPU sistem yang persis**. Duty-cycle rendah juga dapat mengurangi hashrate secara signifikan.

## Menjalankan di background

Install tmux:

```bash
pkg install tmux
tmux new -s mining
```

Jalankan miner:

```bash
./droider -a verushash -o stratum+tcp://POOL:PORT -u WALLET.worker -t 4 -p 50 -P x
```

Detach:

```text
Ctrl-b
d
```

Masuk lagi:

```bash
tmux attach -t mining
```

Android tertentu agresif membunuh proses background. Battery optimization juga dapat mengganggu mining jangka panjang.

## Wallet dan keamanan

Gunakan hanya public receiving/mining address.

Jangan pernah memasukkan:

- seed phrase
- private key
- recovery phrase

ke command miner atau repository GitHub.

Jangan menjalankan miner pada perangkat yang bukan milik lo atau tanpa izin.

## Kenapa backend tidak ditulis ulang dari nol?

VerusHash adalah proof-of-work yang menjadi bagian dari consensus. Menulis implementasi hash baru hanya demi membuat repo terlihat "mandiri" adalah cara yang sangat efisien untuk menghasilkan miner yang salah.

Karena itu proyek ini memisahkan:

- **C launcher:** CLI `droider`, validasi argumen, thread count, dan CPU duty-cycle.
- **VerusHash backend:** implementasi mining CPU yang dibangun dari source upstream.

Dengan pemisahan ini, kode custom tetap kecil dan fungsi hashing tidak diganti dengan implementasi eksperimental.

## License

Kode launcher/integrasi di repo ini menggunakan MIT License.

Backend VerusHash/ccminer yang diambil oleh script memiliki lisensi upstream sendiri. Lisensi dan kewajiban atribusinya tetap berlaku.
