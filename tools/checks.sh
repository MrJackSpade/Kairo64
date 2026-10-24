#!/bin/sh
# Kairo64's own checks, after the shared ones (shared/tools/check-app.sh).
set -e
python3 tools/local_patches.py --check
python3 tools/vice_sources.py --check
sh tools/generate_infocontrib.sh --check
python3 tools/notices.py --check
