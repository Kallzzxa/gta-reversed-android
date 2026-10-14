#!/bin/bash
# RO-MP Master Server — start script (Linux)
# Daftarkan server game ke sini via opsi announce/master_host/master_port
# di server.cfg. Client mengambil daftar server dari sini (server browser).
cd "$(dirname "$0")"
chmod +x bin/linux/romp-master 2>/dev/null
exec ./bin/linux/romp-master -p 7778
