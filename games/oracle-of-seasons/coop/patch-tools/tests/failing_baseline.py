"""Buildable pre-feature baseline. Deliberately cannot reconstruct a tree."""
import sys

if __name__ == '__main__':
    print('baseline: source-patch packaging is not implemented', file=sys.stderr)
    raise SystemExit(1)
