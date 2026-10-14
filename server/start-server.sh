#!/bin/bash
# RO-MP Dedicated Server — start script (Linux)
cd "$(dirname "$0")"
chmod +x bin/linux/romp-server 2>/dev/null
exec ./bin/linux/romp-server -c server.cfg
