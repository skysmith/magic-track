#!/usr/bin/env python3
"""MagicTrack sidecar launcher.

This file stays as the stable entrypoint:
    python3 sidecar/magictrack_sidecar.py
"""

from mt_sidecar import run_server


if __name__ == "__main__":
    run_server()
