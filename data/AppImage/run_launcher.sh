#!/bin/bash
export PATH="$APPDIR/usr/bin:$PATH"
exec "$APPDIR/usr/bin/apdoom-launcher" "$@"
